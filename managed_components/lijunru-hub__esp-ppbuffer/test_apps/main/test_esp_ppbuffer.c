/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "unity.h"
#include "esp_random.h"
#include "ppbuffer.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-3000)

static size_t before_free_8bit;
static size_t before_free_32bit;

TEST_CASE("test ppbuffer", "[ppbuffer]")
{
    PingPongBuffer_t *ppbuf = (PingPongBuffer_t *)malloc(sizeof(PingPongBuffer_t));
    char *buf0 = NULL;
    char *buf1 = NULL;
    void *buf = NULL;
    buf0 = malloc(64);
    buf1 = malloc(64);
    ppbuffer_create(ppbuf, buf0, buf1);
    ppbuffer_get_write_buf(ppbuf, &buf);
    sprintf(buf, "Hello World");
    ppbuffer_set_write_done(ppbuf);

    ppbuffer_get_read_buf(ppbuf, &buf);
    TEST_ASSERT_EQUAL_STRING("Hello World", buf);
    ppbuffer_set_read_done(ppbuf);

    free(buf0);
    free(buf1);
}

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    /**
     *   ___ ___ ___     ___ ___ ___ _   _ ___ ___ ___ ___ 
     *  | __/ __| _ \___| _ \ _ \ _ ) | | | __| __| __| _ \
     *  | _|\__ \  _/___|  _/  _/ _ \ |_| | _|| _|| _||   /
     *  |___|___/_|     |_| |_| |___/\___/|_| |_| |___|_|_\
     */
    printf(" ___ ___ ___     ___ ___ ___ _   _ ___ ___ ___ ___\n");
    printf("| __/ __| _ \\___| _ \\ _ \\ _ ) | | | __| __| __| _ \\\n");
    printf("| _|\\__ \\  _/___|  _/  _/ _ \\ |_| | _|| _|| _||   /\n");
    printf("|___|___/_|     |_| |_| |___/\\___/|_| |_| |___|_|_\\\n");

    unity_run_menu();
}