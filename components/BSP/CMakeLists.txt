set(src_dirs
            MYIIC
            LCD
            LED
            XL9555
            USART)

set(include_dirs
            MYIIC
            LCD
            LED
            XL9555
            USART)

set(requires
            driver
            esp_lcd)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)