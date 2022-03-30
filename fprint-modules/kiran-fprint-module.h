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

#ifndef __KIRAN_FPRINT_MODULE_H__
#define __KIRAN_FPRINT_MODULE_H__

typedef void *HANDLE;

enum
{
    FPRINT_RESULT_UNSUPPORT = -1,        //此接口不支持
    FPRINT_RESULT_OK = 0,                //成功
    FPRINT_RESULT_FAIL = 1,              //失败
    FPRINT_RESULT_TIMEOUT = 2,           //超时
    FPRINT_RESULT_NO_DEVICE = 3,         //设备不存在
    FPRINT_RESULT_OPEN_DEVICE_FAIL = 4,  //打开设备失败
    //For fprint
    FPRINT_RESULT_ENROLL_RETRY_TOO_SHORT = 5,
    FPRINT_RESULT_ENROLL_RETRY_CENTER_FINGER = 6,
    FPRINT_RESULT_ENROLL_RETRY_REMOVE_FINGER = 7,
    FPRINT_RESULT_ENROLL_RETRY = 8,
    FPRINT_RESULT_ENROLL_PASS = 9,
    FPRINT_RESULT_ENROLL_COMPLETE = 10,
};

/*
 * 指纹模块头文件，每个指纹厂家都需要实现这些函数才能使用
 */

/*
 * [功能]
 * 初始化资源
 *
 * [参数]
 * 无
 *
 * [返回值]
 * 0 表示成功
 * 其它表示失败
 */
int kiran_fprint_init();

/*
 * [功能]
 * 释放资源
 *
 * [参数]
 * 无
 *
 * [返回值]
 * 0 表示成功
 * 其它表示失败
 */
int kiran_fprint_finalize();

/*
 * [功能]
 * 获取设备数
 *
 * [参数]
 * 无
 *
 * [返回值]
 *
 *  >= 0 表示设备数目
 *  < 0 表示调用失败
 */
int kiran_fprint_get_dev_count();

/*
 * [功能]
 * 打开设备
 *
 * [参数]
 * index
 * 	设备索引
 *
 * [返回值]
 * 设备操作句柄
 * 
 */
HANDLE kiran_fprint_open_device(int index);

/*
 * [功能]
 * 采集指纹模板
 *
 * [参数]
 * hDevice
 *       设备操作实例句柄
 * fpTemplate [out]
 *       指向指纹模板地址
 *
 * cbTemplate [out]
 *       实际返回指纹模板数据大小
 *
 * timeout
 * 超时时间, 单位为毫秒
 *
 * [返回值]
 * 0 表示成功
 * FPRINT_RESULT_ENROLL_COMPLETE 表示指纹驱动内部进行采集完成时返回，如libfprint库
 * 其它表示失败
 */
int kiran_fprint_acquire_finger_print(HANDLE hDevice,
                                      unsigned char **fpTemplate,
                                      unsigned int *cbTemplate,
                                      unsigned int timeout);
/* 
 * [功能]
 * 停止采集指纹模板
 *
 * [参数]
 * hDevice
 *       设备操作实例句柄
 */
void kiran_fprint_acquire_finger_print_stop(HANDLE hDevice);

/*
 * [功能]
 * 指纹比对, 此函数直接调用指纹接口内部获取指纹进行比对
 *
 * [参数]
 * hDevice
 *       设备操作实例句柄
 * fpTemplate [in]
 *       指向指纹模板数组地址
 *
 * cbTemplate [in]
 *       指纹模板长度数组
 *
 * number [in]
 *       作为输入时表示指纹模板个数
 *       作为输出时表示匹配的指纹模板下标
 *
 * timeout
 * 超时时间, 单位为毫秒
 *
 * [返回值]
 * 0 匹配
 * -1 表示不支持
 *  其它表示失败
 */
int kiran_fprint_verify_finger_print(HANDLE hDevice,
                                     unsigned char **fpTemplate,
                                     unsigned int *cbTemplate,
                                     unsigned int *number,
                                     unsigned int timeout);

/*
 * [功能]
 * 关闭设备
 *
 * [参数]
 * hDevice 
 *       设备操作实例句柄
 *
 * [返回值]
 * 0 表示成功
 * 其它表示失败
 */
int kiran_fprint_close_device(HANDLE hDevice);

/*
 * [功能]
 * 将3枚指纹模板合并为一枚
 *
 * [参数]
 * fpTemplate1
 *       指纹模板1
 *
 * fpTemplate2
 *       指纹模板2
 *
 * fpTemplate3
 *       指纹模板3
 *
 * regfpTemplate [out]
 *       合并后的指纹模板
 *
 * cbRegTemplate [out]
 *       实际返回指纹模板数据大小
 *
 * timeout
 * 超时时间, 单位为毫秒
 *
 * [返回值]
 * 0 表示成功
 * 其它表示失败
 */
int kiran_fprint_template_merge(HANDLE hDevice,
                                unsigned char *fpTemplate1,
                                unsigned char *fpTemplate2,
                                unsigned char *fpTemplate3,
                                unsigned char **regTemplate,
                                unsigned int *cbRegTemplate);

/*
 * [功能]
 * 对比两枚指纹是否匹配
 *
 * [参数]
 * fpTemplate1
 *       指纹模板1
 *
 * cbfpTemplate1
 *       指纹模板1数据长度
 *
 * fpTemplate2
 *       指纹模板2
 *
 * cbfpTemplate2
 *       指纹模板2数据长度
 *
 * [返回值]
 * 0 表示匹配
 * 其它表示失败
 */
int kiran_fprint_template_match(HANDLE hDevice,
                                unsigned char *fpTemplate1,
                                unsigned int cbfpTemplate1,
                                unsigned char *fpTemplate2,
                                unsigned int cbfpTemplate2);

#endif /* __KIRAN_FPRINT_MODULE_H__ */
