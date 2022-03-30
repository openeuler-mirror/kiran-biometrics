/**
 * Copyright (c) 2020 ~ 2021 KylinSec Co., Ltd. 
 * kiran-cc-daemon is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2. 
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2 
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, 
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, 
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.  
 * See the Mulan PSL v2 for more details.  
 * 
 * Author:     wangxiaoqing <wangxiaoqing@kylinos.com.cn>
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "aratek/aratek.h"
#include "kiran-fprint-module.h"

#ifndef FEATURELEN
#define FEATURELEN 1024
#endif

static int do_acquire = 0;

unsigned int
GetTickCount()  //获取当前时间
{
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);
    return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

int kiran_fprint_init()
{
    return ARAFPSCAN_GlobalInit();
}

int kiran_fprint_finalize()
{
    return ARAFPSCAN_GlobalFree();
}

int kiran_fprint_get_dev_count()
{
    int count = 0;

    ARAFPSCAN_GetDeviceCount(&count);

    return count;
}

HANDLE kiran_fprint_open_device(int index)
{
    HANDLE handle = NULL;

    ARAFPSCAN_OpenDevice(&handle, index);

    return handle;
}

int kiran_fprint_acquire_finger_print(HANDLE hDevice,
                                      unsigned char **fpTemplate,
                                      unsigned int *cbTemplate,
                                      unsigned int timeout)
{
    unsigned int preTime = GetTickCount();
    unsigned char featureBuff[FEATURELEN];
    int width = 0;
    int height = 0;
    int dpi = 0;
    unsigned char *rawdate = NULL;
    int ret = FPRINT_RESULT_FAIL;
    unsigned int curTime;
    int quality;

    do_acquire = 0;

    ret = ARAFPSCAN_GetImageInfo(hDevice, &width, &height, &dpi);
    if (ret != FPRINT_RESULT_OK)
        return FPRINT_RESULT_FAIL;

    rawdate = malloc(width * height);
    if (rawdate == NULL)
        return FPRINT_RESULT_FAIL;

    while (0 == do_acquire)
    {
        curTime = GetTickCount();
        if (curTime - preTime >= timeout)
        {
            ret = FPRINT_RESULT_FAIL;
            break;
        }

        usleep(100000);

        ret = ARAFPSCAN_CaptureRawData(hDevice, 5, rawdate);
        if (ret != FPRINT_RESULT_OK)
            continue;

        quality = 0;
        ARAFPSCAN_ImgQuality(width, height, rawdate, &quality);
        if (quality < 100)
            continue;

        ret = ARAFPSCAN_ExtractFeature(hDevice, 0, featureBuff);
        if (ret != FPRINT_RESULT_OK)
            continue;
        else
            break;
    }

    if (ret == FPRINT_RESULT_OK)
    {
        *cbTemplate = FEATURELEN;
        *fpTemplate = (unsigned char *)malloc(*cbTemplate);
        memcpy(*fpTemplate, featureBuff, *cbTemplate);
    }
    else
    {
        *cbTemplate = 0;
        *fpTemplate = NULL;
    }

    free(rawdate);

    return ret;
}

void kiran_fprint_acquire_finger_print_stop(HANDLE hDevice)
{
    do_acquire = -1;
}

int kiran_fprint_close_device(HANDLE hDevice)
{
    return ARAFPSCAN_CloseDevice(&hDevice);
}

int kiran_fprint_template_merge(HANDLE hDevice,
                                unsigned char *fpTemplate1,
                                unsigned char *fpTemplate2,
                                unsigned char *fpTemplate3,
                                unsigned char **regTemplate,
                                unsigned int *cbRegTemplate)
{
    *cbRegTemplate = 3 * FEATURELEN;
    *regTemplate = (unsigned char *)malloc(*cbRegTemplate);
    if (*regTemplate == NULL)
        return FPRINT_RESULT_FAIL;

    memcpy(*regTemplate, fpTemplate1, FEATURELEN);
    memcpy(*regTemplate + FEATURELEN, fpTemplate2, FEATURELEN);
    memcpy(*regTemplate + 2 * FEATURELEN, fpTemplate3, FEATURELEN);

    return FPRINT_RESULT_OK;
}

int kiran_fprint_template_match(HANDLE hDevice,
                                unsigned char *fpTemplate1,
                                unsigned int cbfpTemplate1,
                                unsigned char *fpTemplate2,
                                unsigned int cbfpTemplate2)
{
    int ret;
    int pnSimilarity;
    int pMatch;
    int num = 0;

    num = cbfpTemplate2 / FEATURELEN;
    ret = ARAFPSCAN_VerifyExt(hDevice,
                              4,
                              fpTemplate1,
                              num,
                              fpTemplate2,
                              &pnSimilarity,
                              &pMatch);
    return pMatch;
}

int kiran_fprint_verify_finger_print(HANDLE hDevice,
                                     unsigned char **fpTemplate,
                                     unsigned int *cbTemplate,
                                     unsigned int *number,
                                     unsigned int timeout)
{
    return FPRINT_RESULT_UNSUPPORT;
}
