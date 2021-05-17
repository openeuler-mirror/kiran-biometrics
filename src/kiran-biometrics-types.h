#ifndef __KIRAN_BIOMETRICS_TYPES_H__
#define __KIRAN_BIOMETRICS_TYPES_H__

#define FPRINT_DIR "/etc/kiran-fprint"
#define FACE_DIR "/etc/kiran-faces"

typedef enum {
    FPRINT_ERROR_NOT_FOUND_DEVICE, /* 未找到设备 */
    FPRINT_ERROR_DEVICE_BUSY, /* 设备忙 */
    FPRINT_ERROR_INTERNAL, /* 内部错误 */
    FPRINT_ERROR_PERMISSION_DENIED, /* 没有权限 */
    FPRINT_ERROR_NO_ENROLLED_PRINTS, /* 未录入指纹*/
    FPRINT_ERROR_NO_ACTION_IN_PROGRESS, /* 当前没有对应的操作*/
} FprintError;

typedef enum {
    FACE_ERROR_NOT_FOUND_DEVICE, /* 未找到设备 */
    FACE_ERROR_DEVICE_BUSY, /* 设备忙 */
    FACE_ERROR_INTERNAL, /* 内部错误 */
    FACE_ERROR_PERMISSION_DENIED, /* 没有权限 */
    FACE_ERROR_NO_FACE_TRACKER, /* 未录入人脸*/
    FACE_ERROR_NO_ACTION_IN_PROGRESS, /* 当前没有对应的操作*/
} FaceError;

typedef enum {
    FPRINT_RESULT_UNSUPPORT = -1, //此接口不支持
    FPRINT_RESULT_OK = 0, //成功
    FPRINT_RESULT_FAIL = 1, //失败
    FPRINT_RESULT_TIMEOUT = 2, //超时
    FPRINT_RESULT_NO_DEVICE = 3, //设备不存在
    FPRINT_RESULT_OPEN_DEVICE_FAIL = 4, //打开设备失败
    //For fprint
    FPRINT_RESULT_ENROLL_RETRY_TOO_SHORT = 5,
    FPRINT_RESULT_ENROLL_RETRY_CENTER_FINGER = 6,
    FPRINT_RESULT_ENROLL_RETRY_REMOVE_FINGER = 7,
    FPRINT_RESULT_ENROLL_RETRY = 8,
    FPRINT_RESULT_ENROLL_PASS = 9,
    FPRINT_RESULT_ENROLL_COMPLETE = 10,
} FprintResult;

typedef enum {
    FACE_RESULT_OK = 0, //成功
    FACE_RESULT_FAIL = 1, //失败
    FACE_RESULT_TIMEOUT = 2, //超时
    FACE_RESULT_NO_DEVICE = 3, //设备不存在
    FACE_RESULT_OPEN_DEVICE_FAIL = 4, //打开设备失败
} FaceResult;

#endif /*__KIRAN_BIOMETRICS_TYPES_H__ */
