#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

#include <sys/types.h>
#include <pwd.h>
#include <errno.h>

#include "kiran-biometrics.h"
#include "kiran-fprint-manager.h"
#include "kiran-biometrics-types.h"
#include "kiran-face-manager.h"

#ifndef MAX_TRY_COUNT 
#define MAX_TRY_COUNT 20               /* 最大尝试次数 */
#endif

#define DEFAULT_TIME_OUT 5000          /* 一次等待指纹时间，单位毫秒*/
#define MAX_FPRINT_TEMPLATE  10240     /* 最大指纹模板长度 */

GQuark fprint_error_quark(void);
GType fprint_error_get_type(void);

GQuark face_error_quark(void);
GType face_error_get_type(void);

#define FPRINT_TYPE_ERROR fprint_error_get_type()
#define FPRINT_ERROR_DBUS_INTERFACE "com.kylinsec.Kiran.SystemDaemon.Biometrics.Error"

#define FPRINT_ERROR fprint_error_quark()

#define FACE_TYPE_ERROR face_error_get_type()
#define FACE_ERROR_DBUS_INTERFACE "com.kylinsec.Kiran.SystemDaemon.Biometrics.Error"

#define FACE_ERROR face_error_quark()

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
    gchar *fprint_verify_id;

    KiranFaceManager* kfamanager;
    FprintAction face_action;
    gboolean face_busy;

    GThread *face_enroll_thread;
    GThread *face_verify_thread;
};

static void kiran_biometrics_enroll_fprint_start (KiranBiometrics *kirBiometrics, 
						  DBusGMethodInvocation *context);
static void kiran_biometrics_enroll_fprint_stop (KiranBiometrics *kirBiometrics, 
						 DBusGMethodInvocation *context);
static void kiran_biometrics_verify_fprint_start (KiranBiometrics *kirBiometrics, 
						  const char *id,
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
    g_object_unref (priv->kfamanager);
    g_free (priv->fprint_verify_id);

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
    dbus_g_error_domain_register (FPRINT_ERROR, FPRINT_ERROR_DBUS_INTERFACE, FPRINT_TYPE_ERROR);

    signals[SIGNAL_FPRINT_VERIFY_STATUS] = 
	                g_signal_new ("verify-fprint-status",
		    	               G_TYPE_FROM_CLASS (gobject_class), 
		    	               G_SIGNAL_RUN_LAST, 
				       0, 
				       NULL, NULL, NULL, 
				       G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);

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

static void
face_enroll_status_cb (KiranBiometrics *kirBiometrics,
		gchar *id,
		gint progress,
		gpointer user_data)
{
    KiranBiometricsPrivate *priv = kirBiometrics->priv;

    g_message ("face_enroll_status_cb progress: %d, id: %s\n",  progress, id);

    if (progress == 100 && id) //采集完成
    {
         g_signal_emit(kirBiometrics,
                          signals[SIGNAL_FACE_ENROLL_STATUS], 0,
                          _("Successed enroll face!"), "", progress,
                          TRUE);


	 //关闭采集
	 priv->face_action = ACTION_NONE;

         if (priv->face_enroll_thread)
         {
             g_thread_join (priv->face_enroll_thread);
             priv->face_enroll_thread = NULL;
         }
     
         priv->face_busy = FALSE;

    }
    else
    {
         g_signal_emit(kirBiometrics,
                          signals[SIGNAL_FACE_ENROLL_STATUS], 0,
                          _("Please look the camera!"), "", progress,
                          FALSE);
    }
}

static void
kiran_biometrics_init (KiranBiometrics *self)
{
    KiranBiometricsPrivate *priv;

    priv = self->priv = KIRAN_BIOMETRICS_GET_PRIVATE (self);
    priv->fprint_busy = FALSE;
    priv->face_busy = FALSE;
    priv->kfpmanager = kiran_fprint_manager_new ();
    priv->kfamanager = kiran_face_manager_new ();
    priv->fp_action = ACTION_NONE;
    priv->face_action = ACTION_NONE;
    priv->fprint_enroll_thread = NULL;
    priv->fprint_verify_thread = NULL;
    priv->fprint_verify_id = NULL;

    g_signal_connect_swapped (priv->kfamanager,
		              "enroll-face-status",
		              G_CALLBACK (face_enroll_status_cb),
		              self);
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
kiran_biometrics_get_fprint (unsigned char *template,
		             unsigned int  *length,
			     const gchar *md5)
{
    GFile *file;
    GFileInputStream *input_stream;
    GError *error = NULL;
    gint ret = 0;
    gchar *path;
    gsize read_size;

    path = g_strdup_printf ("%s/%s.bat", FPRINT_DIR, md5);
    if (!g_file_test (path, G_FILE_TEST_EXISTS))
    {
	g_free (path);
	return -1;
    }

    file = g_file_new_for_path (path);
    input_stream = g_file_read (file, 
				NULL,
				&error);

    if (error)
    {
	ret = -1;
	g_warning("open file io stream fail: %s", error->message);
	g_error_free (error);
    }
    else
    {
	g_input_stream_read_all (G_INPUT_STREAM (input_stream),
			         template,
				 *length,
				 &read_size,
				 NULL,
				 &error);
	if (error)
	{
	    ret = -1;
	    g_warning("read file fail: %s", error->message);
	    g_error_free (error);
	}
	*length = read_size;
    }

    g_free(path);
    g_object_unref (input_stream);
    g_object_unref (file);
    
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
    int ret;
    int i;
    int try_count = 0;
    int progress = 0;
    unsigned char *templates[3];
    unsigned int templateLens[3];
    unsigned char *regTemplate = NULL;
    unsigned int length;
    
    for (i = 0; i < 3; i++)
    {
        templates[i] = NULL;
    }
    
    for (i = 0; i < 3 && (priv->fp_action == FP_ACTION_ENROLL);)
    {
        if (templates[i])
        {
    	    //重复录入时，释放内存
            g_free (templates[i]);
    	    templates[i] = NULL;
        }
        progress = 25 * i;
	if (i == 0)
	{
            g_signal_emit(kirBiometrics, 
              	          signals[SIGNAL_FPRINT_ENROLL_STATUS], 0,
                          _("Please place the finger!"), "", progress, 
              	          FALSE);
	}
	else
	{
            g_signal_emit(kirBiometrics, 
              	         signals[SIGNAL_FPRINT_ENROLL_STATUS], 0,
                         _("Please place the finger again!"), "", progress, 
              	         FALSE);
	}
    
        ret = kiran_fprint_manager_acquire_finger_print (priv->kfpmanager,
    	                                                 &templates[i],
                                                         &length,
    					                 DEFAULT_TIME_OUT);
        g_message ("kiran_fprint_manager_acquire_finger_print ret is %d, len %d\n", ret, length);
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
            
                g_message ("kiran_fprint_manager_template_match ret is %d\n", ret);
            
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
        g_message ("kiran_fprint_manager_template_merge ret is %d, len is %d\n", ret, length);
    }
    
    if (ret == FPRINT_RESULT_OK)
    {
        ret = kiran_fprint_manager_template_match (priv->kfpmanager,
                                                           templates[0],
                                                           templateLens[0],
                                                           regTemplate,
                                                           length);
        
        if (ret == FPRINT_RESULT_OK)
        {
    	    gchar *id = NULL;
    
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
    
    for (i = 0; i < 3; i++)
        g_free(templates[i]);
    
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
    g_message ("kiran_fprint_manager_open ret is %d\n", ret);
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
	g_set_error (&error, FPRINT_ERROR, 
		     FPRINT_ERROR_NOT_FOUND_DEVICE, _("Fingerprint Device Not Found"));
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
    unsigned char saveTemplate[MAX_FPRINT_TEMPLATE] = {0};
    unsigned int saveTemplateLen = MAX_FPRINT_TEMPLATE;
    unsigned char *template;
    unsigned int templateLen;
    int i = 0;
    int ret = 0;

    ret = kiran_biometrics_get_fprint (saveTemplate, &saveTemplateLen, priv->fprint_verify_id);
    g_message ("kiran_biometrics_get_fprint ret is %d and len is %d\n", ret, saveTemplateLen);
    if (ret != FPRINT_RESULT_OK)
    {
        g_signal_emit(kirBiometrics, 
                      signals[SIGNAL_FPRINT_VERIFY_STATUS], 0,
                       _("Not Found The Id Fprint Template!"), TRUE, FALSE);

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
                      _("Please place the finger!"), FALSE, FALSE);
	}

        ret = kiran_fprint_manager_acquire_finger_print (priv->kfpmanager,
                                                         &template,
                                                         &templateLen,
                                                         DEFAULT_TIME_OUT);
        g_message ("kiran_fprint_manager_acquire_finger_print ret is %d, len %d\n", ret, templateLen);
        if (ret == FPRINT_RESULT_OK)
        {
            ret = kiran_fprint_manager_template_match (priv->kfpmanager,
                                                               template,
                                                               templateLen,
                                                               saveTemplate,
                                                               saveTemplateLen);
            g_message ("kiran_fprint_manager_template_match ret is %d\n", ret);
            if (ret == FPRINT_RESULT_OK)
            {
                g_signal_emit(kirBiometrics, 
                  	      signals[SIGNAL_FPRINT_VERIFY_STATUS], 0,
                              _("Fingerprint match!"), TRUE, TRUE);
                //指纹匹配
                break;
            }
            else
            {
                g_signal_emit(kirBiometrics, 
                  	      signals[SIGNAL_FPRINT_VERIFY_STATUS], 0,
                              _("Fingerprint not match, place again!"), FALSE, TRUE);
            }
        }
    }

    if (ret != FPRINT_RESULT_OK)
    {

	if (priv->fp_action != FP_ACTION_VERIFY)
	{
            g_signal_emit(kirBiometrics,
                          signals[SIGNAL_FPRINT_VERIFY_STATUS], 0,
                          _("Cancel fprint verify!"), TRUE, FALSE);
	}
	else
	{
            g_signal_emit(kirBiometrics, 
                      signals[SIGNAL_FPRINT_VERIFY_STATUS], 0,
                      _("Fingerprint over max try count!"), TRUE, FALSE);
	}
    }

    kiran_fprint_manager_close (priv->kfpmanager);
    priv->fprint_busy = FALSE;
    priv->fp_action = ACTION_NONE;

    g_thread_exit (0);
}

static void 
kiran_biometrics_verify_fprint_start (KiranBiometrics *kirBiometrics, 
				      const char *id,
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
    g_message ("kiran_fprint_manager_open ret is %d\n", ret);
    if (ret == FPRINT_RESULT_OK)
    {
        priv->fprint_busy = TRUE;
        priv->fp_action = FP_ACTION_VERIFY;

	g_free (priv->fprint_verify_id);
	priv->fprint_verify_id = g_strdup (id);

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

static gpointer
do_face_enroll (gpointer data)
{
    KiranBiometrics *kirBiometrics = KIRAN_BIOMETRICS (data);
    KiranBiometricsPrivate *priv = kirBiometrics->priv;
    int ret;

    ret = FACE_RESULT_OK;
    while (priv->face_action == FACE_ACTION_ENROLL
	   && ret == FACE_RESULT_OK)
    {
        ret = kiran_face_manager_capture_face (priv->kfamanager);
	usleep(10000);
    }

    kiran_face_manager_stop (priv->kfamanager);
    g_message ("kiran_face_manager_stop##################### \n");
    priv->face_busy = FALSE;
    priv->face_action = ACTION_NONE;

    g_thread_exit (0);
}

static void 
kiran_biometrics_enroll_face_start (KiranBiometrics *kirBiometrics, 
				    DBusGMethodInvocation *context)
{
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

	    priv->fprint_enroll_thread = g_thread_new (NULL,
		                            do_face_enroll,
				            kirBiometrics); 
	}

        dbus_g_method_return(context, kiran_face_manager_get_addr (priv->kfamanager));
	return;
    }

    g_set_error (&error, FACE_ERROR,
                     FACE_ERROR_NOT_FOUND_DEVICE, _("Face Device Not Found"));
    dbus_g_method_return_error (context, error);
}

static void 
kiran_biometrics_enroll_face_stop (KiranBiometrics *kirBiometrics, 
			           DBusGMethodInvocation *context)
{
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

    if (priv->face_enroll_thread)
    {
        g_thread_join (priv->face_enroll_thread);
        priv->face_enroll_thread = NULL;
    }

    priv->face_busy = FALSE;

    dbus_g_method_return(context);
}

static void 
kiran_biometrics_verify_face_start (KiranBiometrics *kirBiometrics, 
				    const char *id,
				    DBusGMethodInvocation *context)
{
    dbus_g_method_return(context);
}

static void 
kiran_biometrics_verify_face_stop (KiranBiometrics *kirBiometrics, 
			           DBusGMethodInvocation *context)
{
    dbus_g_method_return(context);
}

static void 
kiran_biometrics_delete_enrolled_face (KiranBiometrics *kirBiometrics, 
				       const char *id,
				       DBusGMethodInvocation *context)
{
    dbus_g_method_return(context);
}

GQuark fprint_error_quark(void)
{
    static GQuark quark = 0;
    if (!quark)
            quark = g_quark_from_static_string("kiran-fprintd-error-quark");
    return quark;
}

GQuark face_error_quark(void)
{
    static GQuark quark = 0;
    if (!quark)
            quark = g_quark_from_static_string("kiran-face-error-quark");
    return quark;
}

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

KiranBiometrics *
kiran_biometrics_new()
{
    return g_object_new (KIRAN_TYPE_BIOMETRICS, NULL);
}
