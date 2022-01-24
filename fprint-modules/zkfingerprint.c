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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <dlfcn.h>  //Linux动态库的显示调用
#include "kiran-fprint-module.h"
#include "zk/zkinterface.h"
#include "zk/libzkfperrdef.h"
#include "zk/libzkfptype.h"
#include "zk/libzkfp.h"

static HANDLE m_libHandle = NULL;
static HANDLE m_hDBCache = NULL;  //算法缓冲区

static int do_acquire = 0;

unsigned int 
GetTickCount()  //获取当前时间
{
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);
    return (tv.tv_sec*1000 + tv.tv_usec/1000);
}

static int
loadlib()
{
    m_libHandle = dlopen("libzkfp.so", RTLD_NOW);
    if (NULL == m_libHandle)
    {
	return -1;
    }

    ZKFPM_Init = (T_ZKFPM_Init)dlsym(m_libHandle, "ZKFPM_Init");
    if (NULL == ZKFPM_Init)
    {
	return -1;
    }

    ZKFPM_Terminate = (T_ZKFPM_Terminate)dlsym(m_libHandle, "ZKFPM_Terminate");
    ZKFPM_GetDeviceCount = (T_ZKFPM_GetDeviceCount)dlsym(m_libHandle, "ZKFPM_GetDeviceCount");
    ZKFPM_OpenDevice = (T_ZKFPM_OpenDevice)dlsym(m_libHandle, "ZKFPM_OpenDevice");
    ZKFPM_CloseDevice = (T_ZKFPM_CloseDevice)dlsym(m_libHandle, "ZKFPM_CloseDevice");
    ZKFPM_SetParameters = (T_ZKFPM_SetParameters)dlsym(m_libHandle, "ZKFPM_SetParameters");
    ZKFPM_GetParameters = (T_ZKFPM_GetParameters)dlsym(m_libHandle, "ZKFPM_GetParameters");
    ZKFPM_AcquireFingerprint = (T_ZKFPM_AcquireFingerprint)dlsym(m_libHandle, "ZKFPM_AcquireFingerprint");
    ZKFPM_DBInit = (T_ZKFPM_DBInit)dlsym(m_libHandle, "ZKFPM_DBInit");
    ZKFPM_DBFree = (T_ZKFPM_DBFree)dlsym(m_libHandle, "ZKFPM_DBFree");
    ZKFPM_DBMerge = (T_ZKFPM_DBMerge)dlsym(m_libHandle, "ZKFPM_DBMerge");
    ZKFPM_DBDel = (T_ZKFPM_DBDel)dlsym(m_libHandle, "ZKFPM_DBDel");
    ZKFPM_DBAdd = (T_ZKFPM_DBAdd)dlsym(m_libHandle, "ZKFPM_DBAdd");
    ZKFPM_DBClear = (T_ZKFPM_DBClear)dlsym(m_libHandle, "ZKFPM_DBClear");
    ZKFPM_DBCount = (T_ZKFPM_DBCount)dlsym(m_libHandle, "ZKFPM_DBCount");
    ZKFPM_DBIdentify = (T_ZKFPM_DBIdentify)dlsym(m_libHandle, "ZKFPM_DBIdentify");
    ZKFPM_DBMatch = (T_ZKFPM_DBMatch)dlsym(m_libHandle, "ZKFPM_DBMatch");
    ZKFPM_SetLogLevel=(T_ZKFPM_SetLogLevel)dlsym(m_libHandle, "ZKFPM_SetLogLevel");
    ZKFPM_ConfigLog=(T_ZKFPM_ConfigLog)dlsym(m_libHandle, "ZKFPM_ConfigLog");

    return 0;
}

int kiran_fprint_init()
{
    int ret;
    
    ret = loadlib();
    if (ret != 0)
	return FPRINT_RESULT_FAIL;

    ret = ZKFPM_Init();

    if (ret != ZKFP_ERR_OK)
	return FPRINT_RESULT_FAIL;

    m_hDBCache = ZKFPM_DBInit(); //创建算法缓冲区   返回值：缓冲区句柄
    if (NULL == m_hDBCache)
    {
        ZKFPM_Terminate();  //释放资源
        return FPRINT_RESULT_FAIL;
    }

    return FPRINT_RESULT_OK;
}

int kiran_fprint_finalize()
{
    int ret;

    if (m_hDBCache)
    {
        ZKFPM_DBFree(m_hDBCache);
        m_hDBCache = NULL;
    }

    ret = ZKFPM_Terminate();

    if (m_libHandle)
    {
	dlclose (m_libHandle);
	m_libHandle= NULL;
    }

    return ret;
}

int kiran_fprint_get_dev_count()
{
    return ZKFPM_GetDeviceCount();
}

HANDLE kiran_fprint_open_device (int index)
{
    return ZKFPM_OpenDevice (index);
}

int kiran_fprint_acquire_finger_print (HANDLE hDevice,
			    	       unsigned char **fpTemplate,
				       unsigned int *cbTemplate,
				       unsigned int timeout)
{
    unsigned int preTime = GetTickCount();
    char paramValue[4] = {0x0};
    unsigned int cbParamValue = 4;
    int imageBufferSize = 0;
    unsigned char *m_pImgBuf = NULL;
    unsigned char szTemplate[MAX_TEMPLATE_SIZE];
    unsigned int tempLen = MAX_TEMPLATE_SIZE;
    unsigned int curTime;
    int ret;

    do_acquire = 0;

    memset(paramValue, 0x0, 4);  //初始化paramValue[4]
    cbParamValue = 4;  //初始化cbParamValue
                                /* |   设备  |   参数类型     |  参数值     |  参数数据长度  */
    ZKFPM_GetParameters(hDevice, 1, (unsigned char*)paramValue, &cbParamValue);//获取采集器参数 图像宽
 
    memset(paramValue, 0x0, 4);  //初始化paramValue[4]
    cbParamValue =4;  //初始化cbParamValue
                            /* |   设备  |   参数类型     |  参数值     |  参数数据长度  */
    ZKFPM_GetParameters(hDevice, 2, (unsigned char*)paramValue, &cbParamValue);//获取采集器参数 图像高

    memset(paramValue, 0x0, 4);  //初始化paramValue[4]
    cbParamValue =4;  //初始化cbParamValue
                            /* |   设备  |   参数类型     |  参数值     |  参数数据长度  */
    ZKFPM_GetParameters(hDevice, 106, (unsigned char*)paramValue, &cbParamValue);//获取采集器参数 图像数据大小
    if(ret != 0)
	return FPRINT_RESULT_FAIL;

    imageBufferSize = *((int *)paramValue);
    m_pImgBuf = (unsigned char *)malloc(imageBufferSize);
    if (m_pImgBuf == NULL)
	return FPRINT_RESULT_FAIL;

    ret = FPRINT_RESULT_FAIL;

    while (0 == do_acquire)
    {

        ret = ZKFPM_AcquireFingerprint (hDevice, 
		    	                    m_pImgBuf, imageBufferSize,
			                    szTemplate, &tempLen);
	if (ret == 0)
	{
	    break;
	}

	curTime = GetTickCount();
	if (curTime - preTime >= timeout)
	{
	    ret = FPRINT_RESULT_FAIL;
	    break;
	}

	usleep(100000);
    }

    if (ret == 0)
    {
	*cbTemplate = tempLen;
	*fpTemplate = (unsigned char *)malloc(*cbTemplate);
	memcpy (*fpTemplate, szTemplate, *cbTemplate);
    }
    else
    {
	*cbTemplate = 0;
	*fpTemplate = NULL;
    }

    free(m_pImgBuf);
    m_pImgBuf = NULL;

    return ret;
}

void 
kiran_fprint_acquire_finger_print_stop(HANDLE hDevice)
{
    do_acquire = -1;
}

int kiran_fprint_close_device (HANDLE hDevice)
{
    return ZKFPM_CloseDevice(hDevice);
}

int kiran_fprint_template_merge (HANDLE hDevice,
				 unsigned char *fpTemplate1,
			         unsigned char *fpTemplate2,
				 unsigned char *fpTemplate3,
				 unsigned char **regTemplate,
				 unsigned int *cbRegTemplate)
{ 
    unsigned char szTemplate[MAX_TEMPLATE_SIZE];
    unsigned int tempLen = MAX_TEMPLATE_SIZE;
    int ret;
    
    ret = FPRINT_RESULT_FAIL;
    ret = ZKFPM_DBMerge(m_hDBCache, 
		         fpTemplate1, fpTemplate2, fpTemplate3,
			 szTemplate, &tempLen);  //将3枚预登记指纹模板合并为一枚登记指纹

    if (ret == 0)
    {
	*cbRegTemplate = tempLen;
	*regTemplate = (unsigned char *)malloc(*cbRegTemplate);
	 memcpy (*regTemplate, szTemplate, *cbRegTemplate);
    }
    else
    {
	*cbRegTemplate = 0;
	*regTemplate = NULL;
    }		   

    return ret;
}

int kiran_fprint_template_match (HANDLE hDevice,
			         unsigned char *fpTemplate1,
				 unsigned int cbfpTemplate1,
			         unsigned char *fpTemplate2,
				 unsigned int cbfpTemplate2)
{
    int score = 0;

    score = ZKFPM_DBMatch(m_hDBCache, 
		          fpTemplate1, cbfpTemplate1, 
			  fpTemplate2, cbfpTemplate2); 

    return score > 0 ? FPRINT_RESULT_OK : FPRINT_RESULT_FAIL;
}

int kiran_fprint_verify_finger_print (HANDLE hDevice,
                                      unsigned char **fpTemplate,
                                      unsigned int *cbTemplate,
                                      unsigned int *number,
                                      unsigned int timeout)
{
    return FPRINT_RESULT_UNSUPPORT;
}
