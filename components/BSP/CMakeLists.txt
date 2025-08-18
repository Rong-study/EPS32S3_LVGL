set(src_dirs
            MYIIC
            LCD
            LED
            MYSPI
            XL9555
            KEY
            MYI2S
            SPI_SD
            TOUCH
            RTC
            ES8311
            USART)

set(include_dirs
            MYIIC
            LCD
            LED
            MYSPI
            XL9555
            KEY
            MYI2S
            SPI_SD
            TOUCH
            RTC
            ES8311
            USART)

set(requires
            driver
            esp_lcd
            fatfs
            newlib
            esp_lcd)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)