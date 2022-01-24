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
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <syslog.h>

#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib-lowlevel.h>

#define PAM_SM_AUTH
#include <security/pam_modules.h>

#include "config.h"
#include "kiran-biometrics-proxy.h"
#include "kiran-pam.h"
#include "kiran-pam-msg.h"
#include "kiran-biometrics-types.h"

#include "marshal.h"

typedef struct {
    char *result;
    gboolean match;
    pam_handle_t *pamh;
    GMainLoop *loop;
    gboolean should_handle;
} verify_data;

static DBusGConnection *
get_dbus_connection (pam_handle_t *pamh, GMainLoop **ret_loop)
{
    DBusGConnection *connection;
    DBusConnection *conn;
    GMainLoop *loop;
    GMainContext *ctx;
    DBusError error;

    connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);

    if (connection != NULL)
            dbus_g_connection_unref (connection);

    dbus_error_init (&error);
    conn = dbus_bus_get_private (DBUS_BUS_SYSTEM, &error);
    if (conn == NULL) {
        D(pamh, "Error with getting the bus: %s", error.message);
        dbus_error_free (&error);
        return NULL;
    }

    ctx = g_main_context_new ();
    loop = g_main_loop_new (ctx, FALSE);
    dbus_connection_setup_with_g_main (conn, ctx);

    connection = dbus_connection_get_g_connection (conn);
    *ret_loop = loop;

    return connection;
}

static void 
verify_result(GObject *object, const char *result, gboolean done, gboolean match ,gpointer user_data)
{
    verify_data *data = user_data;
    const char *msg;

    if (!data->should_handle)
    {
	data->should_handle = TRUE;
	return;
    }

    D(data->pamh, "Face verify result: %s\n", result);
    data->match = match;
    if (done != FALSE) {
            data->result = g_strdup (result);
            g_main_loop_quit (data->loop);
            return;
    }
    send_info_msg (data->pamh, result);
}

static gboolean 
verify_timeout_cb (gpointer user_data)
{
    verify_data *data = user_data;

    send_info_msg (data->pamh, "Face verification timed out");
    g_main_loop_quit (data->loop);

    return FALSE;
}

static int 
do_verify(GMainLoop *loop, pam_handle_t *pamh, DBusGProxy *biometrics, const char *auth)
{
    verify_data *data;
    GError *error = NULL;
    GSource *source;
    int ret;

    data = g_new0 (verify_data, 1);
    data->pamh = pamh;
    data->loop = loop;
    data->result = NULL;
    data->match = FALSE;
    data->should_handle = TRUE;

    dbus_g_proxy_add_signal(biometrics, 
		            "VerifyFaceStatus", 
			    G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, NULL);
    dbus_g_proxy_connect_signal(biometrics, 
		               "VerifyFaceStatus", 
			       G_CALLBACK(verify_result),
                               data, NULL);
    ret = PAM_AUTH_ERR;
    D(data->pamh, "Verify id: %s\n", auth);

    if(!com_kylinsec_Kiran_SystemDaemon_Biometrics_verify_face_start (biometrics, auth, &error))
    {
	if (dbus_g_error_has_name (error, "com.kylinsec.Kiran.SystemDaemon.Biometrics.Error.DeviceBusy"))
	{
	    //取消先前的认证
	    data->should_handle = FALSE;
    	    com_kylinsec_Kiran_SystemDaemon_Biometrics_verify_face_stop(biometrics, NULL);
            g_error_free (error);
	}

	error = NULL;
	if(!com_kylinsec_Kiran_SystemDaemon_Biometrics_verify_face_start (biometrics, auth, &error))
	{
            D(pamh, "VerifyFaceStart failed: %s", error->message);
    	    send_info_msg (pamh,  error->message);
            g_error_free (error);
    
            g_free (data->result);
    	    g_free (data);
    	    return PAM_AUTH_ERR;
	}
    }

    source = g_timeout_source_new_seconds (120);
    g_source_attach (source, g_main_loop_get_context (loop));
    g_source_set_callback (source, verify_timeout_cb, data, NULL);
    
    g_main_loop_run (loop);

    g_source_destroy (source);
    g_source_unref (source);

    com_kylinsec_Kiran_SystemDaemon_Biometrics_verify_face_stop(biometrics, NULL);
    dbus_g_proxy_disconnect_signal(biometrics, "VerifyFaceStatus", G_CALLBACK(verify_result), data);

    if (data->match)
    {
	//认证成功
        ret = PAM_SUCCESS;
        send_info_msg (data->pamh, data->result);
    }
    else
    {
        ret = PAM_AUTH_ERR;
        send_err_msg (data->pamh, data->result);
    }

    g_free (data->result);
    g_free (data);

    return ret;
}

static void 
close_and_unref (DBusGConnection *connection)
{
    DBusConnection *conn;

    conn = dbus_g_connection_get_connection (connection);
    dbus_connection_close (conn);
    dbus_g_connection_unref (connection);
}

static void 
unref_loop (GMainLoop *loop)
{
    GMainContext *ctx;

    ctx = g_main_loop_get_context (loop);
    g_main_loop_unref (loop);
    g_main_context_unref (ctx);
}

static int 
do_auth(pam_handle_t *pamh, const char *username, const char *auth)
{
    DBusGConnection *connection;
    DBusGProxy *biometrics;
    DBusConnection *conn;
    GMainLoop *loop;
    char *rep;
    int ret;

    connection = get_dbus_connection (pamh, &loop);
    if (connection == NULL)
         return PAM_AUTHINFO_UNAVAIL;

    biometrics = dbus_g_proxy_new_for_name(connection,
                                        SERVICE_NAME,
                                        SERVICE_PATH,
                                        SERVICE_INTERFACE);
    if (biometrics == NULL)
    {
         D(pamh, "Error with connect the service: %s", SERVICE_NAME);
         return PAM_AUTHINFO_UNAVAIL;
    }

    //请求人脸人认证界面
    rep = request_respone(pamh,
                          PAM_PROMPT_ECHO_ON,
                          ASK_FACE);
    if (rep && g_strcmp0(rep, REP_FACE) == 0)
    {
	//认证界面准备完毕	
        ret = do_verify (loop, pamh, biometrics, auth);
    }
    else
    {
        ret =  PAM_AUTHINFO_UNAVAIL;
    }
   
    unref_loop (loop);
    g_object_unref (biometrics);
    close_and_unref (connection);

    return ret;
}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc,
                                   const char **argv)
{
    const char *rhost = NULL;
    const char *username;
    guint i;
    int r;
    const void *auth;

#if !GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init();
#endif

    dbus_g_object_register_marshaller (biometrics_marshal_VOID__STRING_BOOLEAN_BOOLEAN,
                                       G_TYPE_NONE, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_INVALID);

    pam_get_item(pamh, PAM_RHOST, (const void **)(const void*) &rhost);

    if (rhost != NULL &&
        *rhost != '\0' &&
        strcmp (rhost, "localhost") != 0) {
            return PAM_AUTHINFO_UNAVAIL;
    }

    r = pam_get_user(pamh, &username, NULL);
    if (r != PAM_SUCCESS)
        return PAM_AUTHINFO_UNAVAIL;

    r = pam_get_data (pamh, FACE_MODE, &auth);
    if (r == PAM_SUCCESS && auth != NULL)
    {
        if (g_strcmp0 (auth, NOT_NEED_DATA) == 0)
        {
	    return PAM_SUCCESS;
        }
    }

    r = do_auth (pamh, username, auth);

    return r;
}

int pam_sm_setcred(pam_handle_t *pamh, int flags,
                   int argc, const char **argv)
{
    return PAM_SUCCESS;
}

/* Account Management API's */
int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags,
                     int argc, const char **argv)
{
    return PAM_SUCCESS;
}

/* Session Management API's */
int pam_sm_open_session(pam_handle_t *pamh, int flags,
                        int argc, const char **argv)
{
    return PAM_SUCCESS;
}

int pam_sm_close_session(pam_handle_t *pamh, int flags,
                         int argc, const char **argv)
{
    return PAM_SUCCESS;
}

/* Password Management API's */
int pam_sm_chauthtok(pam_handle_t *pamh, int flags,
                     int argc, const char **argv)
{
    return PAM_SUCCESS;
}
