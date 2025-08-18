#include "lv_photo_ui.h"


LV_IMG_DECLARE(Vectorback_next)
LV_IMG_DECLARE(Vectorback_pro)
lv_photo_ui_t photo_cfig;
extern lv_obj_t * back_btn;

extern lv_group_t *ctrl_g;
extern lv_indev_t *indev_keypad;    /* 按键组 */

/* PIC Task Configuration
 * Including: task handle, task priority, stack size, creation task
 */
#define PIC_PRIO      10                                /* task priority */
#define PIC_STK_SIZE  5 * 1024                          /* task stack size */
TaskHandle_t          PICTask_Handler;                  /* task handle */
void pic(void *pvParameters);                           /* Task function */

/**
 * @brief       Obtain the total number of target files in the path path
 * @param       path : path
 * @retval      Total number of valid files
 */
uint16_t pic_get_tnum(char *path)
{
    uint8_t res;
    uint16_t rval = 0;
    FF_DIR tdir;
    FILINFO *tfileinfo;
    tfileinfo = (FILINFO *)malloc(sizeof(FILINFO));
    res = f_opendir(&tdir, (const TCHAR *)path);

    if (res == FR_OK && tfileinfo)
    {
        while (1)
        {
            res = f_readdir(&tdir, tfileinfo);

            if (res != FR_OK || tfileinfo->fname[0] == 0)break;
            res = exfuns_file_type(tfileinfo->fname);

            if ((res & 0X0F) != 0X00)
            {
                rval++;
            }
        }
    }

    free(tfileinfo);
    return rval;
}

/**
 * @brief       转换
 * @param       fs:文件系统对象
 * @param       clst:转换
 * @retval      =0:扇区号，0:失败
 */
static LBA_t atk_clst2sect(FATFS *fs, DWORD clst)
{
    clst -= 2;  /* Cluster number is origin from 2 */

    if (clst >= fs->n_fatent - 2)
    {
        return 0;   /* Is it invalid cluster number? */
    }

    return fs->database + (LBA_t)fs->csize * clst;  /* Start sector number of the cluster */
}

/**
 * @brief       偏移
 * @param       dp:指向目录对象
 * @param       Offset:目录表的偏移量
 * @retval      FR_OK(0):成功，!=0:错误
 */
FRESULT atk_photo_dir_sdi(FF_DIR *dp, DWORD ofs)
{
    DWORD clst;
    FATFS *fs = dp->obj.fs;

    if (ofs >= (DWORD)((FF_FS_EXFAT && fs->fs_type == FS_EXFAT) ? 0x10000000 : 0x200000) || ofs % 32)
    {
        /* Check range of offset and alignment */
        return FR_INT_ERR;
    }

    dp->dptr = ofs;         /* Set current offset */
    clst = dp->obj.sclust;  /* Table start cluster (0:root) */

    if (clst == 0 && fs->fs_type >= FS_FAT32)
    {	/* Replace cluster# 0 with root cluster# */
        clst = (DWORD)fs->dirbase;

        if (FF_FS_EXFAT)
        {
            dp->obj.stat = 0;
        }
        /* exFAT: Root dir has an FAT chain */
    }

    if (clst == 0)
    {	/* Static table (root-directory on the FAT volume) */
        if (ofs / 32 >= fs->n_rootdir)
        {
            return FR_INT_ERR;  /* Is index out of range? */
        }

        dp->sect = fs->dirbase;

    }
    else
    {   /* Dynamic table (sub-directory or root-directory on the FAT32/exFAT volume) */
        dp->sect = atk_clst2sect(fs, clst);
    }

    dp->clust = clst;   /* Current cluster# */

    if (dp->sect == 0)
    {
        return FR_INT_ERR;
    }

    dp->sect += ofs / fs->ssize;             /* Sector# of the directory entry */
    dp->dir = fs->win + (ofs % fs->ssize);   /* Pointer to the entry in the win[] */

    return FR_OK;
}

lv_img_dsc_t img_pic_dsc = {
    .header.always_zero = 0,
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .data = NULL,
};

/**
 * @brief       PNG/BMPJPEG/JPG decoding
 * @param       filename : file name
 * @param       width    : image width
 * @param       height   : image height
 * @retval      None
 */
void lv_pic_png_bmp_jpeg_decode(uint16_t w,uint16_t h,uint8_t * pic_buf)
{
    img_pic_dsc.header.w = w;
    img_pic_dsc.header.h = h;
    img_pic_dsc.data_size = w * h * 2;
    img_pic_dsc.data = (const uint8_t *)pic_buf;
    lv_img_set_src(photo_cfig.photo_obj_t.photo_img,&img_pic_dsc);
}

/**
 * @brief       pic task
 * @param       pvParameters : parameters (not used)
 * @retval      None
 */
void pic(void *pvParameters)
{
    pvParameters = pvParameters;
    uint8_t res = 0;
    uint16_t temp = 0;

    while(1)
    {
        res = f_opendir(&photo_cfig.picdir, "0:/PICTURE");

        if (res == FR_OK)
        {
            photo_cfig.pic_curindex = 0;

            while (1)
            {
                temp = photo_cfig.picdir.dptr;
                res = f_readdir(&photo_cfig.picdir, photo_cfig.pic_picfileinfo);
                if (res != FR_OK || photo_cfig.pic_picfileinfo->fname[0] == 0)break;

                res = exfuns_file_type(photo_cfig.pic_picfileinfo->fname);

                if ((res & 0X0F) != 0X00)
                {
                    photo_cfig.pic_picoffsettbl[photo_cfig.pic_curindex] = temp;
                    photo_cfig.pic_curindex++;
                }
            }
        }

        photo_cfig.pic_curindex = 0;
        res = f_opendir(&photo_cfig.picdir, (const TCHAR *)"0:/PICTURE");

        while (res == FR_OK)
        {
            atk_photo_dir_sdi(&photo_cfig.picdir, photo_cfig.pic_picoffsettbl[photo_cfig.pic_curindex]);
            res = f_readdir(&photo_cfig.picdir, photo_cfig.pic_picfileinfo);

            if (res != FR_OK || photo_cfig.pic_picfileinfo->fname[0] == 0)break;

            strcpy((char *)photo_cfig.pic_pname, "0:/PICTURE/");
            strcat((char *)photo_cfig.pic_pname, (const char *)photo_cfig.pic_picfileinfo->fname);

            temp = exfuns_file_type(photo_cfig.pic_pname);
            lvgl_mux_lock(200);
            switch (temp)
            {
                case T_BMP:
                    bmp_decode(photo_cfig.pic_pname,lv_obj_get_width(lv_scr_act()),lv_obj_get_height(lv_scr_act()),(void *)lv_pic_png_bmp_jpeg_decode);    /* BMP decode */
                    break;
                case T_JPG:
                case T_JPEG:
                    jpeg_decode(photo_cfig.pic_pname,lv_obj_get_width(lv_scr_act()),lv_obj_get_height(lv_scr_act()) - 20,(void *)lv_pic_png_bmp_jpeg_decode);   /* JPG/JPEG decode */
                    break;
                case T_PNG:
                    png_decode(photo_cfig.pic_pname,lv_obj_get_width(lv_scr_act()),lv_obj_get_height(lv_scr_act()) - 20,(void *)lv_pic_png_bmp_jpeg_decode);    /* PNG decode */
                    break;
                default:
                    photo_cfig.pic_state = PIC_NEXT;                                                                 /* Non image format */
                    break;
            }

            lvgl_mux_unlock();

            while (1)
            {
                while (lv_smail_icon_get_state(TF_STATE) == 0 && photo_cfig.pic_start == 0x01)
                {
                    app_obj_general.app_state = DEL_STATE;
                    vTaskDelay(10);
                }

                if (photo_cfig.pic_state == PIC_PREV)
                {
                    if (photo_cfig.pic_curindex)
                    {
                        photo_cfig.pic_curindex--;
                    }
                    else
                    {
                        photo_cfig.pic_curindex = photo_cfig.pic_totpicnum - 1;
                    }

                    photo_cfig.pic_state = PIC_NULL;
                    break;
                }
                else if (photo_cfig.pic_state == PIC_NEXT)
                {
                    photo_cfig.pic_curindex++;

                    if (photo_cfig.pic_curindex >= photo_cfig.pic_totpicnum)
                    {
                        photo_cfig.pic_curindex = 0;
                    }

                    photo_cfig.pic_state = PIC_NULL;
                    break;
                }
                vTaskDelay(10);
            }

        }
    }
}

/**
 * @brief  相册播放事件回调
 * @param  *e ：事件相关参数的集合，它包含了该事件的所有数据
 * @return 无
 */
static void pic_play_event_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);      /* 获取触发源 */
    lv_event_code_t code = lv_event_get_code(e);    /* 获取事件类型 */
    
    if (target == photo_cfig.photo_obj_t.photo_next)                   /* 下一张 */
    {
        if (code == LV_EVENT_CLICKED)
        {
            photo_cfig.pic_state = PIC_PREV;
        }
    }
    else if (target == photo_cfig.photo_obj_t.photo_pre)              /* 上一张 */
    {
        if (code == LV_EVENT_CLICKED)
        {
            photo_cfig.pic_state = PIC_NEXT;
        }
    }
}

void lv_photo_ui(void)
{
    if (lv_smail_icon_get_state(TF_STATE))
    {
        if (f_opendir(&photo_cfig.picdir, "0:/PICTURE"))
        {
            lv_msgbox("PICTURE folder error");
            return ;
        }
        
        photo_cfig.pic_totpicnum = pic_get_tnum("0:/PICTURE");

        if (photo_cfig.pic_totpicnum == 0)
        {
            lv_msgbox("No pic files");
            return ;
        }

        photo_cfig.pic_picfileinfo = (FILINFO *)malloc(sizeof(FILINFO));
        photo_cfig.pic_pname = malloc(255 * 2 + 1);
        photo_cfig.pic_picoffsettbl = malloc(4 * photo_cfig.pic_totpicnum);

        if (!photo_cfig.pic_picfileinfo || !photo_cfig.pic_pname || !photo_cfig.pic_picoffsettbl)
        {
            lv_msgbox("memory allocation failed");
            return ;
        }

        /* 隐藏box */
        lv_hidden_box();

        photo_cfig.pic_start = 0x01;
        photo_cfig.photo_box = lv_obj_create(lv_scr_act());
        lv_obj_set_size(photo_cfig.photo_box,lv_obj_get_width(lv_scr_act()),lv_obj_get_height(lv_scr_act()) - 20);
        lv_obj_set_style_bg_color(photo_cfig.photo_box, lv_color_make(0,0,0), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(photo_cfig.photo_box,LV_OPA_100,LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(photo_cfig.photo_box,LV_OPA_0,LV_STATE_DEFAULT);
        lv_obj_set_style_radius(photo_cfig.photo_box, 0, LV_STATE_DEFAULT);
        lv_obj_set_pos(photo_cfig.photo_box,0,20);
        lv_obj_clear_flag(photo_cfig.photo_box, LV_OBJ_FLAG_SCROLLABLE);

        app_obj_general.del_parent = photo_cfig.photo_box;
        app_obj_general.APP_Function = lv_pic_del;
        app_obj_general.app_state = NOT_DEL_STATE;

        photo_cfig.photo_obj_t.photo_img = lv_img_create(photo_cfig.photo_box);
        lv_obj_set_style_bg_color(photo_cfig.photo_obj_t.photo_img, lv_color_make(50,52,67), LV_STATE_DEFAULT);
        lv_obj_center(photo_cfig.photo_obj_t.photo_img);
        lv_obj_move_background(photo_cfig.photo_obj_t.photo_img);

        photo_cfig.photo_obj_t.photo_pre = lv_img_create(photo_cfig.photo_box);
        lv_img_set_src(photo_cfig.photo_obj_t.photo_pre,&Vectorback_pro);
        lv_obj_align(photo_cfig.photo_obj_t.photo_pre,LV_ALIGN_LEFT_MID,0,0);
        lv_obj_set_size(photo_cfig.photo_obj_t.photo_pre, Vectorback_pro.header.w, Vectorback_pro.header.h);
        lv_obj_add_flag(photo_cfig.photo_obj_t.photo_pre,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(photo_cfig.photo_obj_t.photo_pre, pic_play_event_cb, LV_EVENT_ALL, NULL);

        photo_cfig.photo_obj_t.photo_next = lv_img_create(photo_cfig.photo_box);
        lv_img_set_src(photo_cfig.photo_obj_t.photo_next,&Vectorback_next);
        lv_obj_align(photo_cfig.photo_obj_t.photo_next,LV_ALIGN_RIGHT_MID,0,0);
        lv_obj_set_size(photo_cfig.photo_obj_t.photo_next, Vectorback_next.header.w, Vectorback_next.header.h);
        lv_obj_add_flag(photo_cfig.photo_obj_t.photo_next,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(photo_cfig.photo_obj_t.photo_next, pic_play_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_move_foreground(back_btn);

        if (indev_keypad != NULL)
        {
            lv_group_add_obj(ctrl_g, photo_cfig.photo_obj_t.photo_pre);
            lv_group_add_obj(ctrl_g, photo_cfig.photo_obj_t.photo_next);
            lv_group_focus_obj(photo_cfig.photo_obj_t.photo_pre);  /* 聚焦第一个APP */
        }

        if (PICTask_Handler == NULL)
        {
            xTaskCreatePinnedToCore((TaskFunction_t )pic,
                                    (const char*    )"pic",
                                    (uint16_t       )PIC_STK_SIZE,
                                    (void*          )NULL,
                                    (UBaseType_t    )PIC_PRIO,
                                    (TaskHandle_t*  )&PICTask_Handler,
                                    (BaseType_t     ) 1);
        }

    }
    else
    {
        lv_msgbox("SD device not detected");
    }
}

/**
  * @brief  del pic
  * @param  None
  * @retval None
  */
void lv_pic_del(void)
{
    vTaskDelete(PICTask_Handler);
    
    PICTask_Handler = NULL;

    lv_obj_del(photo_cfig.photo_box);

    if (photo_cfig.pic_picfileinfo || photo_cfig.pic_pname || photo_cfig.pic_picoffsettbl)
    {
        free(photo_cfig.pic_picfileinfo);
        free(photo_cfig.pic_pname);
        free(photo_cfig.pic_picoffsettbl);
    }

    lv_display_box();
}
