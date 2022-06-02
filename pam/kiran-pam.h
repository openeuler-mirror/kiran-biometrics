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

#ifndef __KIRAN_PAM_H__
#define __KIRAN_PAM_H__

#define FINGER_MODE "KiranFingerAuthMode"
#define FACE_MODE "KiranFaceAuthMode"
#define PASSWD_MODE "KiranPasswordAuthMode"

#define NEED_DATA "KiranNeedAuth"
#define NOT_NEED_DATA "KiranNotNeedAuth"

#include <glib.h>
#include <security/pam_modules.h>

#define D(pamh, ...)                      \
    {                                     \
        char *s;                          \
        s = g_strdup_printf(__VA_ARGS__); \
        send_debug_msg(pamh, s);          \
        g_free(s);                        \
    }

char *request_respone(pam_handle_t *pamh,
                      int echocode,
                      const char *prompt);

gboolean send_info_msg(pam_handle_t *pamh,
                       const char *msg);

gboolean send_err_msg(pam_handle_t *pamh,
                      const char *msg);

void send_debug_msg(pam_handle_t *pamh,
                    const char *msg);

#endif /* __KIRAN_PAM_H__ */
