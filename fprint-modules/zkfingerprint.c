#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include "kiran-fprint-module.h"

#ifndef MAX_TEMPLATE_SIZE
#define MAX_TEMPLATE_SIZE 2048          /* 指纹模板最大长度 */
#endif

static HANDLE m_hDBCache = NULL;  //算法缓冲区

unsigned int 
GetTickCount()  //获取当前时间
{
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);
    return (tv.tv_sec*1000 + tv.tv_usec/1000);
}

int kiran_fprint_init()
{
    int ret = ZKFPM_Init();

    if (ret == 0)
    {
        m_hDBCache = ZKFPM_DBInit(); //创建算法缓冲区   返回值：缓冲区句柄
        if (NULL == m_hDBCache)
	{
	    ZKFPM_Terminate(); 
	    return FPRINT_RESULT_FAIL;
        }
    }

    return ret;
}

int kiran_fprint_finalize()
{
    if (m_hDBCache)
    {
        ZKFPM_DBFree(m_hDBCache);
        m_hDBCache = NULL;
    }

    return ZKFPM_Terminate();
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
    int ret;

    ret = ZKFPM_GetParameters(hDevice, 106, (unsigned char*)paramValue, &cbParamValue);//获取采集器参数 图像宽
    if(ret != 0)
	return FPRINT_RESULT_FAIL;

    imageBufferSize = *((int *)paramValue);
    m_pImgBuf = (unsigned char *)malloc(imageBufferSize);
    if (m_pImgBuf == NULL)
	return FPRINT_RESULT_FAIL;

    ret = FPRINT_RESULT_FAIL;

    while (1)
    {
	unsigned int curTime;
        int ret = ZKFPM_AcquireFingerprint (hDevice, 
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

    free(m_pImgBuf);
    m_pImgBuf = NULL;
    if (ret == 0)
    {
	*cbTemplate = tempLen;
	*fpTemplate = (unsigned char *)malloc(*cbTemplate);
	memecpy (*fpTemplate, szTemplate, *cbTemplate);
    }
    else
    {
	*cbTemplate = 0;
	*fpTemplate = NULL;
    }

    return ret;
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
	 memecpy (*regTemplate, szTemplate, *cbRegTemplate);
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

    return score;
}
