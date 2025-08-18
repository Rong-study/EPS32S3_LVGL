#include "lv_video_ui.h"

LV_IMG_DECLARE(Vectorback_next)
LV_IMG_DECLARE(Vectorback_pro)
lv_video_ui_t video_cfig;
extern lv_obj_t * back_btn;
AVI_INFO g_avix;                                        /* avi文件相关信息 */
char *const AVI_VIDS_FLAG_TBL[2] = {"00dc", "01dc"};    /* 视频编码标志字符串,00dc/01dc */
char *const AVI_AUDS_FLAG_TBL[2] = {"00wb", "01wb"};    /* 音频编码标志字符串,00wb/01wb */

uint16_t video_curindex;
FILINFO *vfileinfo;
uint8_t *video_pname;
uint16_t totavinum;
uint8_t *framebuf;
uint32_t *voffsettbl;
FF_DIR vdir;
FIL *video_favi;


/* VIDEO 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define VIDEO_PRIO      24                              /* 任务优先级 */
#define VIDEO_STK_SIZE  5 * 1024                        /* 任务堆栈大小 */
TaskHandle_t            VIDEOTask_Handler;              /* 任务句柄 */
void video(void *pvParameters);                         /* 任务函数 */

static const char *TAG = "lv_video_ui";

/**
 * @brief       avi解码初始化
 * @param       buf  : 输入缓冲区
 * @param       size : 缓冲区大小
 * @retval      res
 *    @arg      OK,avi文件解析成功
 *    @arg      其他,错误代码
 */
AVISTATUS avi_init(uint8_t *buf, uint32_t size)
{
    uint16_t offset;
    uint8_t *tbuf;
    AVISTATUS res = AVI_OK;
    AVI_HEADER *aviheader;
    LIST_HEADER *listheader;
    AVIH_HEADER *avihheader;
    STRH_HEADER *strhheader;

    STRF_BMPHEADER *bmpheader;
    STRF_WAVHEADER *wavheader;

    tbuf = buf;
    aviheader = (AVI_HEADER *)buf;
    if (aviheader->RiffID != AVI_RIFF_ID)
    {
        return AVI_RIFF_ERR;        /* RIFF ID错误 */
    }

    if (aviheader->AviID != AVI_AVI_ID)
    {
        return AVI_AVI_ERR;         /* AVI ID错误 */
    }

    buf += sizeof(AVI_HEADER);      /* 偏移 */
    listheader = (LIST_HEADER *)(buf);
    if (listheader->ListID != AVI_LIST_ID)
    {
        return AVI_LIST_ERR;        /* LIST ID错误 */
    }

    if (listheader->ListType != AVI_HDRL_ID)
    {
        return AVI_HDRL_ERR;        /* HDRL ID错误 */
    }

    buf += sizeof(LIST_HEADER);     /* 偏移 */
    avihheader = (AVIH_HEADER *)(buf);
    if (avihheader->BlockID != AVI_AVIH_ID)
    {
        return AVI_AVIH_ERR;        /* AVIH ID错误 */
    }

    g_avix.SecPerFrame = avihheader->SecPerFrame;   /* 得到帧间隔时间 */
    g_avix.TotalFrame = avihheader->TotalFrame;     /* 得到总帧数 */
    buf += avihheader->BlockSize + 8;               /* 偏移 */
    listheader = (LIST_HEADER *)(buf);
    if (listheader->ListID != AVI_LIST_ID)
    {
        return AVI_LIST_ERR;        /* LIST ID错误 */
    }

    if (listheader->ListType != AVI_STRL_ID)
    {
        return AVI_STRL_ERR;        /* STRL ID错误 */
    }

    strhheader = (STRH_HEADER *)(buf + 12);
    if (strhheader->BlockID != AVI_STRH_ID)
    {
        return AVI_STRH_ERR;        /* STRH ID错误 */
    }

    if (strhheader->StreamType == AVI_VIDS_STREAM)  /* 视频帧在前 */
    {
        if (strhheader->Handler != AVI_FORMAT_MJPG) /* 非MJPG视频流,不支持 */
        {
            return AVI_FORMAT_ERR;
        }

        g_avix.VideoFLAG = AVI_VIDS_FLAG_TBL[0];    /* 视频流标记  "00dc" */
        g_avix.AudioFLAG = AVI_AUDS_FLAG_TBL[1];    /* 音频流标记  "01wb" */
        bmpheader = (STRF_BMPHEADER *)(buf + 12 + strhheader->BlockSize + 8);   /* strf */
        if (bmpheader->BlockID != AVI_STRF_ID)
        {
            return AVI_STRF_ERR;    /* STRF ID错误 */
        }

        g_avix.Width = bmpheader->bmiHeader.Width;
        g_avix.Height = bmpheader->bmiHeader.Height;
        buf += listheader->BlockSize + 8;       /* 偏移 */
        listheader = (LIST_HEADER *)(buf);
        if (listheader->ListID != AVI_LIST_ID)  /* 是不含有音频帧的视频文件 */
        {
            g_avix.SampleRate = 0;              /* 音频采样率 */
            g_avix.Channels = 0;                /* 音频通道数 */
            g_avix.AudioType = 0;               /* 音频格式 */
        }
        else
        {
            if (listheader->ListType != AVI_STRL_ID)
            {
                return AVI_STRL_ERR;    /* STRL ID错误 */
            }

            strhheader = (STRH_HEADER *)(buf + 12);
            if (strhheader->BlockID != AVI_STRH_ID)
            {
                return AVI_STRH_ERR;    /* STRH ID错误 */
            }

            if (strhheader->StreamType != AVI_AUDS_STREAM)
            {
                return AVI_FORMAT_ERR;  /* 格式错误 */
            }

            wavheader = (STRF_WAVHEADER *)(buf + 12 + strhheader->BlockSize + 8);   /* strf */
            if (wavheader->BlockID != AVI_STRF_ID)
            {
                return AVI_STRF_ERR;    /* STRF ID错误 */
            }

            g_avix.SampleRate = wavheader->SampleRate;      /* 音频采样率 */
            g_avix.Channels = wavheader->Channels;          /* 音频通道数 */
            g_avix.AudioType = wavheader->FormatTag;        /* 音频格式 */
        }
    }
    else if (strhheader->StreamType == AVI_AUDS_STREAM)     /* 音频帧在前 */
    { 
        g_avix.VideoFLAG = AVI_VIDS_FLAG_TBL[1];            /* 视频流标记  "01dc" */
        g_avix.AudioFLAG = AVI_AUDS_FLAG_TBL[0];            /* 音频流标记  "00wb" */
        wavheader = (STRF_WAVHEADER *)(buf + 12 + strhheader->BlockSize + 8);   /* strf */
        if (wavheader->BlockID != AVI_STRF_ID)
        {
            return AVI_STRF_ERR;                            /* STRF ID错误 */
        }
 
        g_avix.SampleRate = wavheader->SampleRate;          /* 音频采样率 */
        g_avix.Channels = wavheader->Channels;              /* 音频通道数 */
        g_avix.AudioType = wavheader->FormatTag;            /* 音频格式 */
        buf += listheader->BlockSize + 8;                   /* 偏移 */
        listheader = (LIST_HEADER *)(buf);
        if (listheader->ListID != AVI_LIST_ID)
        {
            return AVI_LIST_ERR;    /* LIST ID错误 */
        }

        if (listheader->ListType != AVI_STRL_ID)
        {
            return AVI_STRL_ERR;    /* STRL ID错误 */
        }

        strhheader = (STRH_HEADER *)(buf + 12);
        if (strhheader->BlockID != AVI_STRH_ID)
        {
            return AVI_STRH_ERR;    /* STRH ID错误 */
        }

        if (strhheader->StreamType != AVI_VIDS_STREAM)
        {
            return AVI_FORMAT_ERR;  /* 格式错误 */
        }

        bmpheader = (STRF_BMPHEADER *)(buf + 12 + strhheader->BlockSize + 8);   /* strf */
        if (bmpheader->BlockID != AVI_STRF_ID)
        {
            return AVI_STRF_ERR;    /* STRF ID错误 */
        }

        if (bmpheader->bmiHeader.Compression != AVI_FORMAT_MJPG)
        {
            return AVI_FORMAT_ERR;  /* 格式错误 */
        }

        g_avix.Width = bmpheader->bmiHeader.Width;
        g_avix.Height = bmpheader->bmiHeader.Height;
    }

    offset = avi_srarch_id(tbuf, size, "movi");     /* 查找movi ID */
    if (offset == 0)
    {
        return AVI_MOVI_ERR;        /* MOVI ID错误 */
    }

    if (g_avix.SampleRate)          /* 有音频流,才查找 */
    {
        tbuf += offset;
        offset = avi_srarch_id(tbuf, size, g_avix.AudioFLAG);   /* 查找音频流标记 */
        if (offset == 0)
        {
            return AVI_STREAM_ERR;
        }
        tbuf += offset + 4;
        g_avix.AudioBufSize = *((uint16_t *)tbuf);              /* 得到音频流buf大小. */
    }

    ESP_LOGI(TAG, "avi init ok\r\n");
    ESP_LOGI(TAG, "g_avix.SecPerFrame:%ld\r\n", g_avix.SecPerFrame);
    ESP_LOGI(TAG, "g_avix.TotalFrame:%ld\r\n", g_avix.TotalFrame);
    ESP_LOGI(TAG, "g_avix.Width:%ld\r\n", g_avix.Width);
    ESP_LOGI(TAG, "g_avix.Height:%ld\r\n", g_avix.Height);
    ESP_LOGI(TAG, "g_avix.AudioType:%d\r\n", g_avix.AudioType);
    ESP_LOGI(TAG, "g_avix.SampleRate:%ld\r\n", g_avix.SampleRate);
    ESP_LOGI(TAG, "g_avix.Channels:%d\r\n", g_avix.Channels);
    ESP_LOGI(TAG, "g_avix.AudioBufSize:%d\r\n", g_avix.AudioBufSize);
    ESP_LOGI(TAG, "g_avix.VideoFLAG:%s\r\n", g_avix.VideoFLAG);
    ESP_LOGI(TAG, "g_avix.AudioFLAG:%s\r\n", g_avix.AudioFLAG);

    return res;
}

/**
 * @brief       查找 ID
 * @param       buf  : 输入缓冲区
 * @param       size : 缓冲区大小
 * @param       id   : 要查找的id, 必须是4字节长度
 * @retval      执行结果
 *   @arg       0     , 没找到
 *   @arg       其他  , movi ID偏移量
 */
uint32_t avi_srarch_id(uint8_t *buf, uint32_t size, char *id)
{
    uint32_t i;
    uint32_t idsize = 0;
    size -= 4;
    for (i = 0; i < size; i++)
    {
        if ((buf[i] == id[0]) &&
            (buf[i + 1] == id[1]) &&
            (buf[i + 2] == id[2]) &&
            (buf[i + 3] == id[3]))
        {
            idsize = MAKEDWORD(buf + i + 4);    /* 得到帧大小,必须大于16字节,才返回,否则不是有效数据 */

            if (idsize > 0X10)return i;         /* 找到"id"所在的位置 */
        }
    }

    return 0;
}

/**
 * @brief       得到stream流信息
 * @param       buf  : 流开始地址(必须是01wb/00wb/01dc/00dc开头)
 * @retval      执行结果
 *   @arg       AVI_OK, AVI文件解析成功
 *   @arg       其他  , 错误代码
 */
AVISTATUS avi_get_streaminfo(uint8_t *buf)
{
    g_avix.StreamID = MAKEWORD(buf + 2);    /* 得到流类型 */
    g_avix.StreamSize = MAKEDWORD(buf + 4); /* 得到流大小 */

    if (g_avix.StreamSize > AVI_MAX_FRAME_SIZE) /* 帧大小太大了,直接返回错误 */
    {
        printf("FRAME SIZE OVER:%ld\r\n", g_avix.StreamSize);
        g_avix.StreamSize = 0;
        return AVI_STREAM_ERR;
    }
    
    if (g_avix.StreamSize % 2)
    {
        g_avix.StreamSize++;    /* 奇数加1(g_avix.StreamSize,必须是偶数) */
    }

    if (g_avix.StreamID == AVI_VIDS_FLAG || g_avix.StreamID == AVI_AUDS_FLAG)
    {
        return AVI_OK;
    }

    return AVI_STREAM_ERR;
}

/**
 * @brief       显示当前播放时间
 * @param       favi   : 当前播放的视频文件
 * @param       aviinfo: avi控制结构体
 * @retval      无
 */
void video_time_show(FIL *favi, AVI_INFO *aviinfo)
{
    static uint32_t oldsec;                                         /* 上一次的播放时间 */
    
    uint32_t totsec = 0;                                            /* video文件总时间 */
    uint32_t cursec;                                                /* 当前播放时间 */
    
    totsec = (aviinfo->SecPerFrame / 1000) * aviinfo->TotalFrame;   /* 歌曲总长度(单位:ms) */
    totsec /= 1000;                                                 /* 秒钟数. */
    cursec = ((double)favi->fptr / favi->obj.objsize) * totsec;     /* 获取当前播放到多少秒 */
    
    if (oldsec != cursec)                                           /* 需要更新显示时间 */
    {
        oldsec = cursec;
    }
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
FRESULT atk_video_dir_sdi(FF_DIR *dp, DWORD ofs)
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

lv_img_dsc_t video_img_dsc = {
    .header.always_zero = 0,
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .data = NULL,
};

/**
 * @brief       video显示视频
 * @param       无
 * @retval      无
 */
void video_show(uint32_t w,uint32_t h,uint8_t * video_buf)
{
    video_img_dsc.header.w = w;
    video_img_dsc.header.h = h;
    video_img_dsc.data_size = w * h * 2;
    video_img_dsc.data = (const uint8_t *)video_buf;
    lv_img_set_src(video_cfig.video_obj_t.video_img,&video_img_dsc);
    //video_time_show(video_favi, &g_avix);
}

/**
 * @brief       video task
 * @param       pvParameters : 传入参数(未用到)
 * @retval      无
 */
void video(void *pvParameters)
{
    pvParameters = pvParameters;
    uint8_t res = 0;
    uint8_t key = 0;
    uint8_t temp = 0;
    uint8_t *pbuf = NULL;
    uint32_t nr = 0;
    uint16_t offset = 0;

    res = (uint8_t)f_opendir(&vdir, "0:/VIDEO");            /* 打开目录 */

    if (res == FR_OK)
    {
        video_curindex = 0;

        while (1)
        {
            temp = vdir.dptr;                               /* 记录当前dptr偏移 */
            res = (uint8_t)f_readdir(&vdir, vfileinfo);     /* 读取下一个文件 */
            
            if ((res != 0) || (vfileinfo->fname[0] == 0))   /* 错误或到末尾，退出 */
            {
                break;
            }
            
            res = exfuns_file_type(vfileinfo->fname);
            
            if ((res & 0xF0) == 0x60)                       /* 是视频文件 */
            {
                voffsettbl[video_curindex] = temp;          /* 记录索引 */
                video_curindex++;
            }
        }

        video_curindex = 0;
        res = (uint8_t)f_opendir(&vdir, "0:/VIDEO");        /* 打开目录 */

        while (1)
        {
            atk_video_dir_sdi(&vdir, voffsettbl[video_curindex]);   /* 改变当前目录索引 */
            res = (uint8_t)f_readdir(&vdir, vfileinfo);             /* 读取目录的下一个文件 */
            
            if ((res != 0) || (vfileinfo->fname[0] == 0))           /* 错误或到末尾，退出 */
            {
                printf("err\r\n");
                break;
            }
            
            strcpy((char *)video_pname, "0:/VIDEO/");                       /* 复制路径（目录） */
            strcat((char *)video_pname, (const char *)vfileinfo->fname);    /* 将文件名接在后面 */
            printf("%s\n",video_pname);
            memset(framebuf, 0, AVI_MAX_FRAME_SIZE);
            
            res = (uint8_t)f_open(video_favi, (const TCHAR *)video_pname, FA_READ);

            if (res == FR_OK)
            {
                pbuf = framebuf;
                res = (uint8_t)f_read(video_favi, pbuf, AVI_MAX_FRAME_SIZE, (UINT*)&nr);    /* 开始读取 */

                if (res != 0)
                {
                    printf("fread error:%d\r\n", res);
                    break;
                }

                res = avi_init(pbuf, AVI_MAX_FRAME_SIZE);                           /* AVI解析 */

                if (res != 0)
                {
                    printf("avi error:%d\r\n", res);
                    break;
                }

                esptim_int_init( 1, g_avix.SecPerFrame);
                i2s_set_samplerate_bits_sample(g_avix.SampleRate, I2S_BITS_PER_SAMPLE_16BIT);   /* 设置采样率和数据位宽 */
                i2s_trx_start();                /* I2S TRX启动 */ 
                offset = avi_srarch_id(pbuf, AVI_MAX_FRAME_SIZE, "movi");   /* 寻找movi ID */
                avi_get_streaminfo(pbuf + offset + 4);                      /* 获取流信息 */
                f_lseek(video_favi, offset + 12);                           /* 跳过标志ID，读地址偏移到流数据开始处 */
                res = mjpegdec_init(0,0);                                   /* 初始化JPG解码 */

                if (res != 0)
                {
                    printf("mjpegdec Fail\r\n");
                    break;
                }

                if (Windows_Width * Windows_Height == g_avix.Width * g_avix.Height)
                {
                    continue;
                }
                else
                {
                    /* 定义图像的宽高 */
                    Windows_Width = g_avix.Width;
                    Windows_Height = g_avix.Height;

                    if (video_buf == NULL)
                    {
                        mjpegdec_malloc();
                    }
                }

                if (g_avix.SampleRate)                                                  /* 定义图像的宽高 */
                {

                }

                while (1)
                {
                    lvgl_mux_lock(10);
                    if (g_avix.StreamID == AVI_VIDS_FLAG)                               /* 视频流 */
                    {
                        pbuf = framebuf;
                        f_read(video_favi, pbuf, g_avix.StreamSize + 8, (UINT*)&nr);    /* 读取整帧+下一帧数据流ID信息 */
                        res = mjpegdec_decode(pbuf, g_avix.StreamSize,video_show);
                        
                        if (res != 0)
                        {
                            printf("decode error!\r\n");
                        }

                        while (frameup == 0);                                           /* 等待播放时间到达 */
                        frameup = 0;
                    }
                    else
                    {
                        f_read(video_favi, framebuf, g_avix.StreamSize + 8, (UINT*)&nr);       /* 填充psaibuf */
                        pbuf = framebuf;
                        i2s_tx_write(framebuf, g_avix.StreamSize);
                    }
                    
                    lvgl_mux_unlock();

                    
                    if (avi_get_streaminfo(pbuf + g_avix.StreamSize) != 0)
                    {
                        key = VIDEO_NEXT;
                        break;
                    }

                    vTaskDelay(pdMS_TO_TICKS(5));   /* 延时5毫秒 */
                }

                if (key == VIDEO_PREV)              /* 上一曲 */
                {
                    if (video_curindex)
                    {
                        video_curindex--;
                    }
                    else
                    {
                        video_curindex = totavinum - 1;
                    }
                }
                else if (key == VIDEO_NEXT)         /* 下一曲 */
                {
                    video_curindex++;

                    if (video_curindex >= totavinum)
                    {
                        video_curindex = 0;         /* 到末尾的时候,自动从头开始 */
                    }

                }
                i2s_trx_stop();
                i2s_deinit();
                esp_timer_delete(esp_tim_handle);
                mjpegdec_free();
                f_close(video_favi);
            }
        }
    }
}

/**
 * @brief       获取指定路径下有效视频文件的数量
 * @param       path: 指定路径
 * @retval      有效视频文件的数量
 */
static uint16_t video_get_tnum(char *path)
{
    uint8_t res;
    uint16_t rval = 0;
    FF_DIR tdir;
    FILINFO *tfileinfo;
    
    tfileinfo = (FILINFO *)malloc(sizeof(FILINFO));             /* 申请内存 */
    res = (uint8_t)f_opendir(&tdir, (const TCHAR *)path);       /* 打开目录 */
    
    if ((res == 0) && tfileinfo)
    {
        while (1)                                               /* 查询总的有效文件数 */
        {
            res = (uint8_t)f_readdir(&tdir, tfileinfo);         /* 读取目录下的一个文件 */
            
            if ((res != 0) || (tfileinfo->fname[0] == 0))       /* 错误或到末尾，退出 */
            {
                break;
            }
            
            res = exfuns_file_type(tfileinfo->fname);
            
            if ((res & 0xF0) == 0x60)                           /* 是视频文件 */
            {
                rval++;
            }
        }
    }
    
    free(tfileinfo);                                            /* 释放内存 */
    
    return rval;
}

/**
 * @brief  视频演示
 * @param  无
 * @return 无
 */
void lv_video_ui(void)
{
    video_curindex = 0;     /* 当前索引 */
    vfileinfo = 0;          /* 文件信息 */
    video_pname = 0;        /* 带路径的文件名 */
    totavinum = 0;          /* 音乐文件总数 */
    framebuf = NULL;
    voffsettbl = 0;
    video_favi = NULL;

    xl9555_pin_write(SPK_CTRL_IO, 1);   /* 打开喇叭 */

    if (lv_smail_icon_get_state(TF_STATE))
    {
        if (f_opendir(&vdir, "0:/VIDEO"))
        {
            lv_msgbox("MUSIC folder error");
            return ;
        }
        
        totavinum = video_get_tnum("0:/VIDEO");

        if (totavinum == 0)
        {
            lv_msgbox("No music files");
            return ;
        }

        vfileinfo = (FILINFO*)malloc(sizeof(FILINFO));
        video_pname = (uint8_t *)malloc(255 * 2 + 1);
        voffsettbl = (uint32_t *)malloc(4 * totavinum);

        if ((vfileinfo == NULL) || (video_pname == NULL) || (voffsettbl == NULL))
        {
            lv_msgbox("memory allocation failed");
            return ;
        }

        framebuf = (uint8_t *)malloc(AVI_MAX_FRAME_SIZE);
        video_favi = (FIL *)malloc(sizeof(FIL));

        if ((framebuf == NULL) || (video_favi == NULL))
        {
            lv_msgbox("memory error!");
            return ;
        }

        memset(framebuf, 0, AVI_MAX_FRAME_SIZE);
        memset(video_pname, 0, 255 * 2 + 1);
        memset(voffsettbl, 0, 4 * totavinum);

        /* 隐藏box */
        lv_hidden_box();

        video_cfig.video_start = 0x01;
        video_cfig.video_box = lv_obj_create(lv_scr_act());
        lv_obj_set_size(video_cfig.video_box,lv_obj_get_width(lv_scr_act()),lv_obj_get_height(lv_scr_act()) - 20);
        lv_obj_set_style_bg_color(video_cfig.video_box, lv_color_make(0,0,0), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(video_cfig.video_box,LV_OPA_100,LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(video_cfig.video_box,LV_OPA_0,LV_STATE_DEFAULT);
        lv_obj_set_style_radius(video_cfig.video_box, 0, LV_STATE_DEFAULT);
        lv_obj_set_pos(video_cfig.video_box,0,20);
        lv_obj_clear_flag(video_cfig.video_box, LV_OBJ_FLAG_SCROLLABLE);

        app_obj_general.del_parent = video_cfig.video_box;
        app_obj_general.APP_Function = lv_video_del;
        app_obj_general.app_state = NOT_DEL_STATE;

        video_cfig.video_obj_t.video_img = lv_img_create(video_cfig.video_box);
        lv_obj_set_style_bg_color(video_cfig.video_obj_t.video_img, lv_color_make(50,52,67), LV_STATE_DEFAULT);
        lv_obj_center(video_cfig.video_obj_t.video_img);
        lv_obj_move_foreground(back_btn);

        if (VIDEOTask_Handler == NULL)
        {
            xTaskCreatePinnedToCore((TaskFunction_t )video,
                                    (const char*    )"video",
                                    (uint16_t       )VIDEO_STK_SIZE,
                                    (void*          )NULL,
                                    (UBaseType_t    )VIDEO_PRIO,
                                    (TaskHandle_t*  )&VIDEOTask_Handler,
                                    (BaseType_t     ) 1);
        }
    }
    else
    {
        lv_msgbox("SD device not detected");
    }

}

void lv_video_del(void)
{
    i2s_trx_stop();
    i2s_deinit();
    vTaskDelete(VIDEOTask_Handler);
    VIDEOTask_Handler = NULL;
    esp_timer_delete(esp_tim_handle);
    mjpegdec_free();                                                /* 删除一个定时器实例 */

    if ((framebuf != NULL))
    {
        free(framebuf);
        framebuf = NULL;
    }

    if ((vfileinfo != NULL) || (video_pname != NULL) || (voffsettbl != NULL))
    {
        free(vfileinfo);
        free(video_pname);
        free(voffsettbl);
    }

    lv_obj_del(app_obj_general.del_parent);
    app_obj_general.del_parent = NULL;

    lv_display_box();
}