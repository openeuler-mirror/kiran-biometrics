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

#ifndef __KIRAN_FACE_MANAGER_H__
#define __KIRAN_FACE_MANAGER_H__

#include <glib-object.h>
#include <glib.h>

#define KIRAN_TYPE_FACE_MANAGER (kiran_face_manager_get_type())
#define KIRAN_FACE_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                                            KIRAN_TYPE_FACE_MANAGER, KiranFaceManager))
#define KIRAN_FACE_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
                                                                 KIRAN_TYPE_FACE_MANAGER, KiranFaceManagerClass))
#define KIRAN_IS_FACE_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                                                               KIRAN_TYPE_FACE_MANAGER))
#define KIRAN_IS_FACE_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
                                                                    KIRAN_TYPE_FACE_MANAGER))
#define KIRAN_FACE_MANAGER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
                                                                     KIRAN_TYPE_FACE_MANAGER, KiranFaceManagerClass))

typedef struct _KiranFaceManager KiranFaceManager;
typedef struct _KiranFaceManagerClass KiranFaceManagerClass;
typedef struct _KiranFaceManagerPrivate KiranFaceManagerPrivate;

struct _KiranFaceManager
{
    GObject parent;

    KiranFaceManagerPrivate *priv;
};

struct _KiranFaceManagerClass
{
    GObjectClass parent;
};

GType kiran_face_manager_get_type();
KiranFaceManager *kiran_face_manager_new();

int kiran_face_manager_start(KiranFaceManager *kfamanager);
int kiran_face_manager_capture_face(KiranFaceManager *kfamanager);
int kiran_face_manager_stop(KiranFaceManager *kfamanager);
int kiran_face_manager_do_enroll(KiranFaceManager *kfamanager);
int kiran_face_manager_do_verify(KiranFaceManager *kfamanager,
                                 const gchar *id);
char *kiran_face_manager_get_addr(KiranFaceManager *kfamanager);
int kiran_face_manager_delete(const gchar *id);

#endif /* __KIRAN_FACE_MANAGER_H__ */
