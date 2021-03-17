#ifndef __KIRAN_BIOMETRICS_TYPES_H__
#define __KIRAN_BIOMETRICS_TYPES_H__

typedef enum {
    FPRINT_ERROR_NOT_FOUND_DEVICE, /* 未找到设备 */
    FPRINT_ERROR_DEVICE_BUSY, /* 设备忙 */
    FPRINT_ERROR_INTERNAL, /* 内部错误 */
    FPRINT_ERROR_PERMISSION_DENIED, /* 没有权限 */
    FPRINT_ERROR_NO_ENROLLED_PRINTS, /* 未录入指纹*/
    FPRINT_ERROR_NO_ACTION_IN_PROGRESS, /* 当前没有对应的操作*/
} FprintError;

typedef enum {
    FPRINT_RESULT_OK = 0, //成功
    FPRINT_RESULT_FAIL = 1, //失败
    FPRINT_RESULT_TIMEOUT = 2, //超时
    FPRINT_RESULT_NO_DEVICE = 3, //设备不存在
    FPRINT_RESULT_OPEN_DEVICE_FAIL = 4, //打开设备失败
} FprintResult;

#endif /*__KIRAN_BIOMETRICS_TYPES_H__ */
