#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

#include <sys/types.h>
#include <pwd.h>
#include <errno.h>
#include <zlog_ex.h>

#include "kiran-biometrics.h"
#include "kiran-fprint-manager.h"
#include "kiran-biometrics-types.h"

#ifdef HAVE_KIRAN_FACE
#include "kiran-face-manager.h"
#endif /* HAVE_KIRAN_FACE */

#ifndef MAX_TRY_COUNT 
#define MAX_TRY_COUNT 50               /* 最大尝试次数 */
#endif

#define DEFAULT_TIME_OUT  600000          /* 一次等待指纹时间，单位毫秒*/
#define BUFFER_SIZE 1024

#define SUPPORT_FINGER_NUMBER 100       /* 最大指纹模板数目 */ 

GQuark fprint_error_quark(void);
GType fprint_error_get_type(void);

#ifdef HAVE_KIRAN_FACE
GQuark face_error_quark(void);
GType face_error_get_type(void);
#endif /* HAVE_KIRAN_FACE */

#define FPRINT_TYPE_ERROR fprint_error_get_type()
#define FPRINT_ERROR_DBUS_INTERFACE "com.kylinsec.Kiran.SystemDaemon.Biometrics.Error"

#define FPRINT_ERROR fprint_error_quark()

#ifdef HAVE_KIRAN_FACE
#define FACE_TYPE_ERROR face_error_get_type()
#define FACE_ERROR_DBUS_INTERFACE "com.kylinsec.Kiran.SystemDaemon.Biometrics.Error"

#define FACE_ERROR face_error_quark()
#endif /* HAVE_KIRAN_FACE */

typedef enum {
        ACTION_NONE = 0,
        FP_ACTION_VERIFY,
        FP_ACTION_ENROLL,
        FACE_ACTION_VERIFY,
        FACE_ACTION_ENROLL,
} FprintAction;

struct _KiranBiometricsPrivate
{ 
    KiranFprintManager* kfpmanager;
    gboolean fprint_busy;
    FprintAction fp_action;

    GThread *fprint_enroll_thread;
    GThread *fprint_verify_thread;

#ifdef HAVE_KIRAN_FACE
    KiranFaceManager* kfamanager;
    FprintAction face_action;
    gboolean face_busy;

    GThread *face_capture_thread;
#endif /* HAVE_KIRAN_FACE */
};

static void kiran_biometrics_enroll_fprint_start (KiranBiometrics *kirBiometrics, 
						  DBusGMethodInvocation *context);
static void kiran_biometrics_enroll_fprint_stop (KiranBiometrics *kirBiometrics, 
						 DBusGMethodInvocation *context);
static void kiran_biometrics_verify_fprint_start (KiranBiometrics *kirBiometrics, 
						  DBusGMethodInvocation *context);
static void kiran_biometrics_verify_fprint_stop (KiranBiometrics *kirBiometrics, 
						 DBusGMethodInvocation *context);
static void kiran_biometrics_delete_enrolled_finger (KiranBiometrics *kirBiometrics, 
						     const char *id,
						     DBusGMethodInvocation *context);
static void kiran_biometrics_enroll_face_start (KiranBiometrics *kirBiometrics, 
						DBusGMethodInvocation *context);
static void kiran_biometrics_enroll_face_stop (KiranBiometrics *kirBiometrics, 
					       DBusGMethodInvocation *context);
static void kiran_biometrics_verify_face_start (KiranBiometrics *kirBiometrics, 
						const char *id,
						DBusGMethodInvocation *context);
static void kiran_biometrics_verify_face_stop (KiranBiometrics *kirBiometrics, 
					       DBusGMethodInvocation *context);
static void kiran_biometrics_delete_enrolled_face (KiranBiometrics *kirBiometrics, 
						   const char *id,
						   DBusGMethodInvocation *context);
#include "kiran-biometrics-stub.h"

#define KIRAN_BIOMETRICS_GET_PRIVATE(O) \
	(G_TYPE_INSTANCE_GET_PRIVATE((O), KIRAN_TYPE_BIOMETRICS, KiranBiometricsPrivate))

enum kiran_biometrics_signals {
    SIGNAL_FPRINT_VERIFY_STATUS,
    SIGNAL_FPRINT_ENROLL_STATUS,
    SIGNAL_FACE_VERIFY_STATUS,
    SIGNAL_FACE_ENROLL_STATUS,
    NUM_SIGNAL,
};

static guint signals[NUM_SIGNAL] = {0, };

G_DEFINE_TYPE (KiranBiometrics, kiran_biometrics, G_TYPE_OBJECT);

static void
kiran_biometrics_finalize (GObject *object)
{
    KiranBiometrics *kiranBiometrics;
    KiranBiometricsPrivate *priv;

    kiranBiometrics = KIRAN_BIOMETRICS (object);
    priv = kiranBiometrics->priv;

    g_object_unref (priv->kfpmanager);
#ifdef HAVE_KIRAN_FACE
    g_object_unref (priv->kfamanager);
#endif /* HAVE_KIRAN_FACE */

    G_OBJECT_CLASS (kiran_biometrics_parent_class)->finalize (object);
}

static void
kiran_biometrics_class_init (KiranBiometricsClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec *pspec;

    gobject_class->finalize = kiran_biometrics_finalize;

    dbus_g_object_type_install_info (KIRAN_TYPE_BIOMETRICS,
        		&dbus_glib_SystemDaemon_object_info);

    g_type_class_add_private (klass, sizeof (KiranBiometricsPrivate));
#ifdef HAVE_KIRAN_FACE
    dbus_g_error_domain_register (FPRINT_ERROR, FPRINT_ERROR_DBUS_INTERFACE, FPRINT_TYPE_ERROR);
    dbus_g_error_domain_register (FACE_ERROR, FACE_ERROR_DBUS_INTERFACE, FACE_TYPE_ERROR);
#endif /* HAVE_KIRAN_FACE */

    signals[SIGNAL_FPRINT_VERIFY_STATUS] = 
	                g_signal_new ("verify-fprint-status",
		    	               G_TYPE_FROM_CLASS (gobject_class), 
		    	               G_SIGNAL_RUN_LAST, 
				       0, 
				       NULL, NULL, NULL, 
				       G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_STRING);

    signals[SIGNAL_FPRINT_ENROLL_STATUS] = 
	    		g_signal_new ("enroll-fprint-status",
		    		       G_TYPE_FROM_CLASS (gobject_class), 
		    	               G_SIGNAL_RUN_LAST, 
				       0, 
				       NULL, NULL, NULL, 
				       G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_BOOLEAN);

    signals[SIGNAL_FACE_VERIFY_STATUS] = 
	                g_signal_new ("verify-face-status",
		    	               G_TYPE_FROM_CLASS (gobject_class), 
		    	               G_SIGNAL_RUN_LAST, 
				       0, 
				       NULL, NULL, NULL, 
				       G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);

    signals[SIGNAL_FACE_ENROLL_STATUS] = 
	    		g_signal_new ("enroll-face-status",
		    		       G_TYPE_FROM_CLASS (gobject_class), 
		    	               G_SIGNAL_RUN_LAST, 
				       0, 
				       NULL, NULL, NULL, 
				       G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_BOOLEAN);
}

#ifdef HAVE_KIRAN_FACE
static void
face_enroll_status_cb (KiranBiometrics *kirBiometrics,
		gint quality,
		gchar *id,
		gint progress,
		gpointer user_data)
{
    KiranBiometricsPrivate *priv = kirBiometrics->priv;

    g_message ("face_enroll_status_cb progress: %d, id: %s\n",  progress, id);
    dzlog_debug ("face_enroll_status_cb progress: %d, id: %s\n",  progress, id);

    if (progress == 100 && id) //采集完成
    {
         g_signal_emit(kirBiometrics,
                          signals[SIGNAL_FACE_ENROLL_STATUS], 0,
                          _("Successed enroll face!"), id, progress,
                          TRUE);


	 //关闭采集
	 priv->face_action = ACTION_NONE;

         if (priv->face_capture_thread)
         {
             g_thread_join (priv->face_capture_thread);
             priv->face_capture_thread = NULL;
         }
     
         priv->face_busy = FALSE;

    }
    else
    {
	 gchar *msg = _("Please look the camera!");

	 if (quality > 0)
	 {
	    msg = _("Please stay from the camera!");
	 }
	 else if (quality < 0)
	 {
	    msg = _("Please get closer to the camera!");
	 }

         g_signal_emit(kirBiometrics,
                          signals[SIGNAL_FACE_ENROLL_STATUS], 0,
                          msg, "", progress,
                          FALSE);
    }
}

static void
face_verify_status_cb (KiranBiometrics *kirBiometrics,
		       gboolean match,
		       gpointer user_data)
{
    KiranBiometricsPrivate *priv = kirBiometrics->priv;

    g_message ("face_enroll_verify_cb result is : %d\n",  match);

    if (match) //人脸验证匹配
    {
	 g_signal_emit(kirBiometrics,
                          signals[SIGNAL_FACE_VERIFY_STATUS], 0,
                          _("Face match!"), TRUE, TRUE);


         //关闭采集
         priv->face_action = ACTION_NONE;

         if (priv->face_capture_thread)
         {
             g_thread_join (priv->face_capture_thread);
             priv->face_capture_thread = NULL;
         }

         priv->face_busy = FALSE;
    }
    else
    {
	g_signal_emit(kirBiometrics,
                          signals[SIGNAL_FACE_VERIFY_STATUS], 0,
                          _("Face not match! Please look the camera!"), FALSE, FALSE);
    }
}
#endif /* HAVE_KIRAN_FACE */

static void
kiran_biometrics_init (KiranBiometrics *self)
{
    KiranBiometricsPrivate *priv;

    priv = self->priv = KIRAN_BIOMETRICS_GET_PRIVATE (self);
    priv->fprint_busy = FALSE;
    priv->kfpmanager = kiran_fprint_manager_new ();
    priv->fp_action = ACTION_NONE;
    priv->fprint_enroll_thread = NULL;
    priv->fprint_verify_thread = NULL;

#ifdef HAVE_KIRAN_FACE
    priv->face_busy = FALSE;
    priv->kfamanager = kiran_face_manager_new ();
    priv->face_action = ACTION_NONE;
    priv->face_capture_thread = NULL;
    g_signal_connect_swapped (priv->kfamanager,
		              "enroll-face-status",
		              G_CALLBACK (face_enroll_status_cb),
		              self);

    g_signal_connect_swapped (priv->kfamanager,
		              "verify-face-status",
		              G_CALLBACK (face_verify_status_cb),
		              self);
#endif /* HAVE_KIRAN_FACE */
}
static int
kiran_biometrics_remove_fprint (const gchar *md5)
{
    gchar *path;
    int ret;

    path = g_strdup_printf ("%s/%s.bat", FPRINT_DIR, md5);
    if (!g_file_test (path, G_FILE_TEST_EXISTS))
    {
	g_free (path);
	return -1;
    }

    ret = g_remove (path);
    g_free(path);

    return ret;
}

static int
kiran_biometrics_get_save_fprint (unsigned char **template,
		                  unsigned int  *length,
                                  char *path)
{
    GError *error = NULL;
    gint ret = 0;
    gsize read_size;
    gboolean result;

    result = g_file_get_contents(path,
                                 (gchar **)template,
                                 (gsize *)length,
                                 &error);

    if (!result)
    {
        dzlog_debug ("get file %s failed:", path, error->message);
        g_error_free (error);
        ret = -1; 
    }
    
    return ret;
}

static int
kiran_biometrics_get_save_fprints (unsigned char **saveTemplates,
                                   unsigned int *saveTemplateLens,
                                   int *save_number
                                   )
{
    GError *error = NULL;
    GDir *dir = NULL;
    gint ret = 0;
    const char *name;

    *save_number = 0;
    dir = g_dir_open (FPRINT_DIR, 
                      0,
                      &error); 

    if (dir == NULL)
    {
        dzlog_debug ("open dir %s failed:", FPRINT_DIR, error->message);
        g_error_free (error);
        return -1;
    }

    while ((name = g_dir_read_name (dir)))
    {

        if (*save_number == SUPPORT_FINGER_NUMBER)
        {
            dzlog_debug ("read save fprint reach %d:", SUPPORT_FINGER_NUMBER);
            break;
        }

        if (g_str_has_suffix (name, ".bat"))
        {
            unsigned char *saveTemplate = NULL;
            unsigned int saveTemplateLen = 0;
            int ret = 0;
            gchar *path;

            path = g_strdup_printf ("%s/%s", FPRINT_DIR, name);
            ret = kiran_biometrics_get_save_fprint (&saveTemplate, &saveTemplateLen, path);

            if (ret == 0)
            {
	       saveTemplates[*save_number] = saveTemplate;
               saveTemplateLens[*save_number] = saveTemplateLen;
               *save_number = *save_number + 1;
            }

            g_free(path);
        }
    }

    g_dir_close (dir);

    if (*save_number == 0)
        return -1;    

    return ret;
}

static int
kiran_biometrics_save_fprint (unsigned char *template,
		              unsigned int length,
			      gchar **md5)
{
    GFile *file; 
    GFileIOStream *stream;
    GOutputStream *output_stream;
    GError *error = NULL;
    gint ret = 0;
    gchar *dir;
    gchar *path;

    *md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5,
		    			 template,
					 length);

    dir = g_strdup_printf (FPRINT_DIR);
    if (!g_file_test (dir, G_FILE_TEST_IS_DIR))
    {
	g_mkdir(dir, S_IRWXU);
    }

    path = g_strdup_printf ("%s/%s.bat", dir, *md5);

    file = g_file_new_for_path (path);
    stream = g_file_create_readwrite (file, 
		   		      G_FILE_CREATE_PRIVATE,
				      NULL,
				      &error);
    if (error)
    {
	ret = -1;
	g_warning("create fingerprint file io stream fail: %s", error->message);
	g_error_free (error);
    }
    else
    {
	output_stream = g_io_stream_get_output_stream (G_IO_STREAM (stream));
	error = NULL;
	g_output_stream_write (output_stream,
			       template,
			       length,
			       NULL,
			       &error);
	if (error)
	{
	    ret = -1;
	    g_warning("write fingerprint file fail: %s", error->message);
	    g_error_free (error);
	}

	g_object_unref (stream);
    }
    
    g_free (dir);
    g_free (path);
    g_object_unref (file);

    return ret;
}

static gpointer
do_finger_enroll (gpointer data)
{
    KiranBiometrics *kirBiometrics = KIRAN_BIOMETRICS (data);
    KiranBiometricsPrivate *priv = kirBiometrics->priv;
    int ret = FPRINT_RESULT_FAIL;
    int i;
    int try_count = 0;
    int progress = 0;
    unsigned char *templates[3];
    unsigned int templateLens[3];
    unsigned char *regTemplate = NULL;
    unsigned int length;
    int index = 0;
    int enroll_number = 0;
    
    for (i = 0; i < 3; i++)
    {
        templates[i] = NULL;
    }
    
    for (i = 0; i < 3 && (priv->fp_action == FP_ACTION_ENROLL);)
    {
	char *msg = _("Please place the finger again!");
        char pass_message[BUFFER_SIZE] = {0};

        if (templates[i])
        {
    	    //重复录入时，释放内存
            g_free (templates[i]);
    	    templates[i] = NULL;
        }

	if (i == 0)
	{
           msg = _("Please place the finger!");

	}
            
        switch (ret)
        {
	    case FPRINT_RESULT_ENROLL_RETRY_TOO_SHORT:
                msg = _("Your swipe was too short, please try again.");
                break;
 
	    case FPRINT_RESULT_ENROLL_RETRY_REMOVE_FINGER:
                msg = _("Scan failed, please remove your finger and then try again.");
                break;

	    case FPRINT_RESULT_ENROLL_RETRY_CENTER_FINGER:
                msg = _("Didn't catch that, please center your finger on the sensor and try again.");
                break;

	    case FPRINT_RESULT_ENROLL_RETRY:
                msg = _("Didn't quite catch that. Please try again.");
                break;

	    case FPRINT_RESULT_ENROLL_PASS:
                enroll_number++;
                snprintf(pass_message, BUFFER_SIZE, _("Enroll stage passed %d. Please place the finger again!"), enroll_number);
                msg = pass_message;
                break;

            default:
               break;
        }

        //计算进度， 0, 25, 50, 75, 100这几个期间
        progress = 25 * i + enroll_number * 8;
        if (progress > 99) //最大96, 只有保存了才能成功后才能100
           progress = 99;

        g_signal_emit(kirBiometrics, 
              	      signals[SIGNAL_FPRINT_ENROLL_STATUS], 0,
                      msg, "", progress, 
              	      FALSE);
    
        ret = kiran_fprint_manager_acquire_finger_print (priv->kfpmanager,
    	                                                 &templates[i],
                                                         &length,
    					                 DEFAULT_TIME_OUT);

        dzlog_debug ("kiran_fprint_manager_acquire_finger_print ret is %d, len %d\n", ret, length);

        templateLens[i] = length;
        if (ret == FPRINT_RESULT_OK)
        {
            if (i > 0)
            {
                ret = kiran_fprint_manager_template_match (priv->kfpmanager,
            		    			       templates[0],
            					       templateLens[0],
            		    			       templates[i],
            					       templateLens[i]);
            
                dzlog_debug ("kiran_fprint_manager_template_match ret is %d\n", ret);
                if (ret == FPRINT_RESULT_UNSUPPORT)//不支持指纹比对
                    ret = FPRINT_RESULT_OK;
            
                if (ret != FPRINT_RESULT_OK)
                {
            	    g_signal_emit(kirBiometrics, 
                  		      signals[SIGNAL_FPRINT_ENROLL_STATUS], 0,
                                      _("Please place the same finger!"), "", progress, 
                  		      FALSE);
                    i = 0;
                }
                else
                {
            	    i++;
                }
            }
            else
            {
                i++;
            }
        } else if (ret == FPRINT_RESULT_ENROLL_COMPLETE) //录入内部完成
        {
             i++;
	     break;
        }
    
        if (try_count >= MAX_TRY_COUNT)
        {
    	    ret = FPRINT_RESULT_FAIL;
    	    break;
        }
    
        try_count++;
    }
    
    if (ret == FPRINT_RESULT_OK)
    {
        if (templates[0] && templates[1] && templates[2])
            ret = kiran_fprint_manager_template_merge (priv->kfpmanager,
    					           templates[0], 
    		                                   templates[1],
    					           templates[2],
    					           &regTemplate,
    					           &length);
        dzlog_debug ("kiran_fprint_manager_template_merge ret is %d, len is %d\n", ret, length);
    }
    else if (ret == FPRINT_RESULT_ENROLL_COMPLETE) //不支持指纹合成
    {
        length = templateLens[0];
        regTemplate = (unsigned char *)malloc(length);
        if (regTemplate != NULL)
        memcpy (regTemplate, templates[0], length);
        dzlog_debug ("fingger enroll complete with date len [%d]", length);
    }
    
    if (ret == FPRINT_RESULT_OK)
    {
        ret = kiran_fprint_manager_template_match (priv->kfpmanager,
                                                           templates[0],
                                                           templateLens[0],
                                                           regTemplate,
                                                           length);

    }
    else if (ret == FPRINT_RESULT_ENROLL_COMPLETE) //不支持两个指纹模板比对
    {
        ret = FPRINT_RESULT_OK;
    }

    
    if (ret == FPRINT_RESULT_OK)
    {
        gchar *id = NULL;
        //进行指纹保存
        ret = kiran_biometrics_save_fprint (regTemplate, length, &id);
        if (ret == 0)
        {
            g_signal_emit(kirBiometrics, 
                  	  signals[SIGNAL_FPRINT_ENROLL_STATUS],  0,
                          _("Successed enroll finger!"), id, 100, 
                          TRUE);
        }
        g_free(id);
    }

    if (ret != FPRINT_RESULT_OK)
    {
	if (priv->fp_action != FP_ACTION_ENROLL)
	{
            g_signal_emit(kirBiometrics, 
    	      	      signals[SIGNAL_FPRINT_ENROLL_STATUS], 0,
    	              _("Failed enroll finger!"), "", progress, 
    	      	      TRUE);
	}
	else
	{
            g_signal_emit(kirBiometrics,
                      signals[SIGNAL_FPRINT_ENROLL_STATUS], 0,
                      _("Cancel fprint enroll!"), "", 0,
                      TRUE);
	}
    }
    
    for (index = 0; index < i; index++)
        g_free(templates[index]);
    
    g_free (regTemplate);

    //完成采集
    kiran_fprint_manager_close (priv->kfpmanager);
    priv->fprint_busy = FALSE;
    priv->fp_action = ACTION_NONE;

    g_thread_exit (0);
}

static void
kiran_biometrics_enroll_fprint_start (KiranBiometrics *kirBiometrics, 
			              DBusGMethodInvocation *context)
{
    KiranBiometricsPrivate *priv = kirBiometrics->priv;
    g_autoptr(GError) error = NULL;
    int ret;

    if (priv->fprint_busy)
    {
	g_set_error (&error, FPRINT_ERROR, 
		     FPRINT_ERROR_DEVICE_BUSY, _("Fingerprint Device Busy"));
	dbus_g_method_return_error (context, error);
	return;
    }

    ret = kiran_fprint_manager_open (priv->kfpmanager);
    dzlog_debug ("kiran_fprint_manager_open ret is %d\n", ret);
    if (ret == FPRINT_RESULT_OK)
    {
        priv->fprint_busy = TRUE;
        priv->fp_action = FP_ACTION_ENROLL; 

	priv->fprint_enroll_thread = g_thread_new (NULL,
		                            do_finger_enroll,
				            kirBiometrics); 
    }
    else
    {
	const char *msg;

	if (ret == FPRINT_RESULT_NO_DEVICE)
	    msg = _("Fingerprint Device Not Found");
	else
	    msg = _("Open Fingerprint Device Fail!");

	g_set_error (&error, FPRINT_ERROR, 
		     FPRINT_ERROR_NOT_FOUND_DEVICE, msg);
	dbus_g_method_return_error (context, error);
	return;
    }

    dbus_g_method_return(context);
}

static void 
kiran_biometrics_enroll_fprint_stop (KiranBiometrics *kirBiometrics, 
			             DBusGMethodInvocation *context)
{
    KiranBiometricsPrivate *priv = kirBiometrics->priv;
    g_autoptr(GError) error = NULL;

    if (priv->fp_action != FP_ACTION_ENROLL)
    {
	g_set_error (&error, FPRINT_ERROR, 
		     FPRINT_ERROR_NO_ACTION_IN_PROGRESS, _("No Action In Progress"));
	dbus_g_method_return_error (context, error);
	return;
    }

    priv->fp_action = ACTION_NONE;

    if (priv->fprint_enroll_thread)
    {
        kiran_fprint_manager_acquire_finger_print_stop (priv->kfpmanager);
        g_thread_join (priv->fprint_enroll_thread);
        priv->fprint_enroll_thread = NULL;
    }

    dbus_g_method_return(context);
}


static gpointer
do_finger_verify (gpointer data)
{
    KiranBiometrics *kirBiometrics = KIRAN_BIOMETRICS (data);
    KiranBiometricsPrivate *priv = kirBiometrics->priv;
    unsigned char *saveTemplates[SUPPORT_FINGER_NUMBER];
    unsigned int saveTemplateLens[SUPPORT_FINGER_NUMBER];
    int save_number = 0;
    int number = 0;
    unsigned char *template = NULL;
    unsigned int templateLen;
    char *md5 = NULL;
    int i = 0;
    int ret = 0;

    memset(saveTemplates, 0x00, sizeof(unsigned char *) * SUPPORT_FINGER_NUMBER);

    ret = kiran_biometrics_get_save_fprints (saveTemplates, saveTemplateLens, &save_number);
    if (ret != FPRINT_RESULT_OK)
    {
        g_signal_emit(kirBiometrics, 
                      signals[SIGNAL_FPRINT_VERIFY_STATUS], 0,
                       _("Not Found The Id Fprint Template!"), TRUE, FALSE, "");

        kiran_fprint_manager_close (priv->kfpmanager);
        priv->fprint_busy = FALSE;
        priv->fp_action = ACTION_NONE;

        return NULL ;
    }
    
    for (i = 0; i < MAX_TRY_COUNT && (priv->fp_action == FP_ACTION_VERIFY); i++)
    {
        if (template)
        {
    	    //重复录入时，释放内存
            g_free (template);
    	    template = NULL;
        }

	if (i == 0)
	{
            g_signal_emit(kirBiometrics, 
                      signals[SIGNAL_FPRINT_VERIFY_STATUS], 0,
                      _("Please place the finger!"), FALSE, FALSE, "");
	}

        //首先调用指纹内部接口进行比对
        number = save_number;
        ret = kiran_fprint_manager_verify_finger_print (priv->kfpmanager,
                                                        saveTemplates,
                                                        saveTemplateLens,
                                                        &number,
                                                        DEFAULT_TIME_OUT);

        dzlog_debug ("kiran_fprint_verify_acquire_finger_print ret is %d\n", ret);
        if (ret == FPRINT_RESULT_UNSUPPORT) //指纹内部认证接口不支持， 调用其它接口认证
        {
            ret = kiran_fprint_manager_acquire_finger_print (priv->kfpmanager,
                                                             &template,
                                                             &templateLen,
                                                             DEFAULT_TIME_OUT);
            dzlog_debug ("kiran_fprint_manager_acquire_finger_print ret is %d, len %d\n", ret, templateLen);

            if (ret == FPRINT_RESULT_OK)
            {
                int j = 0;

                for (j = 0; j < save_number; j++)
                { 
                    ret = kiran_fprint_manager_template_match (priv->kfpmanager,
                                                               template,
                                                               templateLen,
                                                               saveTemplates[j],
                                                               saveTemplateLens[j]);

                   if (ret == FPRINT_RESULT_OK)
                   {
                   
                      md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5,
		    			                   saveTemplates[j],
					                   saveTemplateLens[j]);
                      break;
                   }
                }

                dzlog_debug ("kiran_fprint_manager_template_match ret is %d\n", ret);
            }
        }
        else if (ret == FPRINT_RESULT_OK)
        {

            if (number >=0 && number < save_number)
            {
                md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5,
	    			                     saveTemplates[number],
				                     saveTemplateLens[number]);
            }
        }

        if (md5 != NULL)
        {
           g_signal_emit(kirBiometrics, 
              	      signals[SIGNAL_FPRINT_VERIFY_STATUS], 0,
                      _("Fingerprint match!"), FALSE, TRUE, md5);
           g_free (md5);
        }
        else
        {
            char *msg = _("Fingerprint not match, place again!");

            switch (ret)
            {
                case FPRINT_RESULT_ENROLL_RETRY_TOO_SHORT:
                    msg = _("Your swipe was too short, please try again.");
                    break;

                case FPRINT_RESULT_ENROLL_RETRY_REMOVE_FINGER:
                    msg = _("Scan failed, please remove your finger and then try again.");
                    break;

                case FPRINT_RESULT_ENROLL_RETRY_CENTER_FINGER:
                    msg = _("Didn't catch that, please center your finger on the sensor and try again.");
                    break;

                case FPRINT_RESULT_ENROLL_RETRY:
                    msg = _("Didn't quite catch that. Please try again.");
                    break;

                default:
                   break;
            }

            g_signal_emit(kirBiometrics, 
              	      signals[SIGNAL_FPRINT_VERIFY_STATUS], 0,
                          msg, FALSE, FALSE, "");
        }
    }

    if (ret != FPRINT_RESULT_OK)
    {

	if (priv->fp_action != FP_ACTION_VERIFY)
	{
            g_signal_emit(kirBiometrics,
                          signals[SIGNAL_FPRINT_VERIFY_STATUS], 0,
                          _("Cancel fprint verify!"), TRUE, FALSE, "");
	}
	else
	{
            g_signal_emit(kirBiometrics, 
                      signals[SIGNAL_FPRINT_VERIFY_STATUS], 0,
                      _("Fingerprint over max try count!"), TRUE, FALSE, "");
	}
    }

    kiran_fprint_manager_close (priv->kfpmanager);
    priv->fprint_busy = FALSE;
    priv->fp_action = ACTION_NONE;

    for (i = 0; i++; i < save_number)
        g_free(saveTemplates[i]);

    g_free (template);

    g_thread_exit (0);
}

static void 
kiran_biometrics_verify_fprint_start (KiranBiometrics *kirBiometrics, 
				      DBusGMethodInvocation *context)
{
    KiranBiometricsPrivate *priv = kirBiometrics->priv;
    g_autoptr(GError) error = NULL;
    int ret;

    if (priv->fprint_busy)
    {
        g_set_error (&error, FPRINT_ERROR, 
                         FPRINT_ERROR_DEVICE_BUSY, _("Fingerprint Device Busy"));
	dbus_g_method_return_error (context, error);
        return;
    }

    ret = kiran_fprint_manager_open (priv->kfpmanager);
    dzlog_debug ("kiran_fprint_manager_open ret is %d\n", ret);
    if (ret == FPRINT_RESULT_OK)
    {
        priv->fprint_busy = TRUE;
        priv->fp_action = FP_ACTION_VERIFY;

	priv->fprint_verify_thread = g_thread_new (NULL,
		                            do_finger_verify,
				            kirBiometrics); 
    }
    else
    {
	g_set_error (&error, FPRINT_ERROR, 
		     FPRINT_ERROR_NOT_FOUND_DEVICE, _("Fingerprint Device Not Found"));
	dbus_g_method_return_error (context, error);
	return;
    }

    dbus_g_method_return(context);
}

static void 
kiran_biometrics_verify_fprint_stop (KiranBiometrics *kirBiometrics, 
				     DBusGMethodInvocation *context)
{
    KiranBiometricsPrivate *priv = kirBiometrics->priv;
    g_autoptr(GError) error = NULL;

    if (priv->fp_action != FP_ACTION_VERIFY)
    {
        g_set_error (&error, FPRINT_ERROR, 
                     FPRINT_ERROR_NO_ACTION_IN_PROGRESS, _("No Action In Progress"));
	dbus_g_method_return_error (context, error);
        return;
    }

    priv->fp_action = ACTION_NONE;

    if (priv->fprint_verify_thread)
    {
        kiran_fprint_manager_acquire_finger_print_stop (priv->kfpmanager);
        g_thread_join (priv->fprint_verify_thread);
        priv->fprint_verify_thread = NULL;
    }

    dbus_g_method_return(context);
}

static void 
kiran_biometrics_delete_enrolled_finger (KiranBiometrics *kirBiometrics, 
					 const char *id,
					 DBusGMethodInvocation *context)
{
    KiranBiometricsPrivate *priv = kirBiometrics->priv;
    g_autoptr(GError) error = NULL;
    int ret;

    ret = kiran_biometrics_remove_fprint (id);
    if (ret != FPRINT_RESULT_OK)
    {
        g_set_error (&error, FPRINT_ERROR,
                     FPRINT_ERROR_INTERNAL, _("Internal Error"));
	dbus_g_method_return_error (context, error);
    }
    
    dbus_g_method_return(context);
}

#ifdef HAVE_KIRAN_FACE
static gpointer
do_face_capture (gpointer data)
{
    KiranBiometrics *kirBiometrics = KIRAN_BIOMETRICS (data);
    KiranBiometricsPrivate *priv = kirBiometrics->priv;
    int ret;

    ret = FACE_RESULT_OK;
    while ((priv->face_action == FACE_ACTION_ENROLL
	    || priv->face_action == FACE_ACTION_VERIFY)
	   && ret == FACE_RESULT_OK)
    {
        ret = kiran_face_manager_capture_face (priv->kfamanager);
	usleep(100000);
    }

    kiran_face_manager_stop (priv->kfamanager);
    dzlog_debug ("stop face caputer\n");
    priv->face_busy = FALSE;
    priv->face_action = ACTION_NONE;

    g_thread_exit (0);
}
#endif /* HAVE_KIRAN_FACE */

static void 
kiran_biometrics_enroll_face_start (KiranBiometrics *kirBiometrics, 
				    DBusGMethodInvocation *context)
{
#ifdef HAVE_KIRAN_FACE
    KiranBiometricsPrivate *priv = kirBiometrics->priv;
    g_autoptr(GError) error = NULL;
    int ret;

    if (priv->face_busy)
    {
        g_set_error (&error, FACE_ERROR,
                     FACE_ERROR_DEVICE_BUSY, _("Face Device Busy"));
        dbus_g_method_return_error (context, error);
        return;
    }

    ret = kiran_face_manager_start (priv->kfamanager);
    if (ret == FACE_RESULT_OK)
    {
	ret = kiran_face_manager_do_enroll (priv->kfamanager);
	if (ret == FACE_RESULT_OK)
	{
            priv->face_busy = TRUE;
            priv->face_action = FACE_ACTION_ENROLL; 

	    priv->face_capture_thread = g_thread_new (NULL,
		                            do_face_capture,
				            kirBiometrics); 
	}

        dbus_g_method_return(context, kiran_face_manager_get_addr (priv->kfamanager));
	return;
    }

    g_set_error (&error, FACE_ERROR,
                     FACE_ERROR_NOT_FOUND_DEVICE, _("Face Device Not Found"));
    dbus_g_method_return_error (context, error);
#endif /* HAVE_KIRAN_FACE */
}

static void 
kiran_biometrics_enroll_face_stop (KiranBiometrics *kirBiometrics, 
			           DBusGMethodInvocation *context)
{
#ifdef HAVE_KIRAN_FACE
    KiranBiometricsPrivate *priv = kirBiometrics->priv;
    g_autoptr(GError) error = NULL;

    if (priv->face_action != FACE_ACTION_ENROLL)
    {
        g_set_error (&error, FPRINT_ERROR,
                     FPRINT_ERROR_NO_ACTION_IN_PROGRESS, _("No Action In Progress"));
        dbus_g_method_return_error (context, error);
        return;
    }

    priv->face_action = ACTION_NONE;

    if (priv->face_capture_thread)
    {
        g_thread_join (priv->face_capture_thread);
        priv->face_capture_thread = NULL;
    }

    priv->face_busy = FALSE;

    dbus_g_method_return(context);
#endif /* HAVE_KIRAN_FACE */
}

static void 
kiran_biometrics_verify_face_start (KiranBiometrics *kirBiometrics, 
				    const char *id,
				    DBusGMethodInvocation *context)
{
#ifdef HAVE_KIRAN_FACE
    KiranBiometricsPrivate *priv = kirBiometrics->priv;
    g_autoptr(GError) error = NULL;
    int ret;

    if (priv->face_busy)
    {
        g_set_error (&error, FACE_ERROR,
                     FACE_ERROR_DEVICE_BUSY, _("Face Device Busy"));
        dbus_g_method_return_error (context, error);
        return;
    }

    ret = kiran_face_manager_start (priv->kfamanager);
    if (ret == FACE_RESULT_OK)
    {
        ret = kiran_face_manager_do_verify (priv->kfamanager, id);
        if (ret == FACE_RESULT_OK)
        {
	    g_signal_emit(kirBiometrics,
                          signals[SIGNAL_FACE_VERIFY_STATUS], 0,
                          _("Looking for you face, Please look the camera!"), FALSE, FALSE);

            priv->face_busy = TRUE;
            priv->face_action = FACE_ACTION_VERIFY;

            priv->face_capture_thread = g_thread_new (NULL,
                                            do_face_capture,
                                            kirBiometrics);
        }

        dbus_g_method_return(context, kiran_face_manager_get_addr (priv->kfamanager));
        return;
    }

    g_set_error (&error, FACE_ERROR,
                     FACE_ERROR_NOT_FOUND_DEVICE, _("Face Device Not Found"));
    dbus_g_method_return_error (context, error);
#endif /* HAVE_KIRAN_FACE */
}

static void 
kiran_biometrics_verify_face_stop (KiranBiometrics *kirBiometrics, 
			           DBusGMethodInvocation *context)
{
#ifdef HAVE_KIRAN_FACE
    KiranBiometricsPrivate *priv = kirBiometrics->priv;
    g_autoptr(GError) error = NULL;

    if (priv->face_action != FACE_ACTION_VERIFY)
    {
        g_set_error (&error, FPRINT_ERROR,
                     FPRINT_ERROR_NO_ACTION_IN_PROGRESS, _("No Action In Progress"));
        dbus_g_method_return_error (context, error);
        return;
    }

    priv->face_action = ACTION_NONE;

    if (priv->face_capture_thread)
    {
        g_thread_join (priv->face_capture_thread);
        priv->face_capture_thread = NULL;
    }

    priv->face_busy = FALSE;

    dbus_g_method_return(context);
#endif /* HAVE_KIRAN_FACE */
}

static void 
kiran_biometrics_delete_enrolled_face (KiranBiometrics *kirBiometrics, 
				       const char *id,
				       DBusGMethodInvocation *context)
{
#ifdef HAVE_KIRAN_FACE
    KiranBiometricsPrivate *priv = kirBiometrics->priv;

    g_autoptr(GError) error = NULL;
    int ret;

    ret = kiran_face_manager_delete (id);
    if (ret != FACE_RESULT_OK)
    {
        g_set_error (&error, FACE_ERROR,
                     FACE_ERROR_INTERNAL, _("Internal Error"));
        dbus_g_method_return_error (context, error);
	return;
    }

    dbus_g_method_return(context);
#endif /* HAVE_KIRAN_FACE */
}

GQuark fprint_error_quark(void)
{
    static GQuark quark = 0;
    if (!quark)
            quark = g_quark_from_static_string("kiran-fprintd-error-quark");
    return quark;
}

#ifdef HAVE_KIRAN_FACE
GQuark face_error_quark(void)
{
    static GQuark quark = 0;
    if (!quark)
            quark = g_quark_from_static_string("kiran-face-error-quark");
    return quark;
}
#endif /* HAVE_KIRAN_FACE */

#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }
GType
fprint_error_get_type (void)
{
    static GType etype = 0;
 
    if (etype == 0) {
            static const GEnumValue values[] =
            {
                    ENUM_ENTRY (FPRINT_ERROR_NOT_FOUND_DEVICE, "NoSuchDevice"),
                    ENUM_ENTRY (FPRINT_ERROR_DEVICE_BUSY, "DeviceBusy"),
                    ENUM_ENTRY (FPRINT_ERROR_INTERNAL, "Internal"),
                    ENUM_ENTRY (FPRINT_ERROR_PERMISSION_DENIED, "PermissionDenied"),
                    ENUM_ENTRY (FPRINT_ERROR_NO_ENROLLED_PRINTS, "NoEnrolledPrints"),
                    ENUM_ENTRY (FPRINT_ERROR_NO_ACTION_IN_PROGRESS, "NoActionInProgress"),
                    { 0, 0, 0 }
            };
            etype = g_enum_register_static ("FprintError", values);
    }
    return etype;
}

#ifdef HAVE_KIRAN_FACE
GType
face_error_get_type (void)
{
    static GType etype = 0;
 
    if (etype == 0) {
            static const GEnumValue values[] =
            {
                    ENUM_ENTRY (FACE_ERROR_NOT_FOUND_DEVICE, "NoSuchDevice"),
                    ENUM_ENTRY (FACE_ERROR_DEVICE_BUSY, "DeviceBusy"),
                    ENUM_ENTRY (FACE_ERROR_INTERNAL, "Internal"),
                    ENUM_ENTRY (FACE_ERROR_PERMISSION_DENIED, "PermissionDenied"),
                    ENUM_ENTRY (FACE_ERROR_NO_FACE_TRACKER, "NoFaceTracker"),
                    ENUM_ENTRY (FACE_ERROR_NO_ACTION_IN_PROGRESS, "NoActionInProgress"),
                    { 0, 0, 0 }
            };
            etype = g_enum_register_static ("FaceError", values);
    }
    return etype;
}
#endif /* HAVE_KIRAN_FACE */

KiranBiometrics *
kiran_biometrics_new()
{
    return g_object_new (KIRAN_TYPE_BIOMETRICS, NULL);
}
