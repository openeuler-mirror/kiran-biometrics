#include <opencv-glib/opencv-glib.h>
#include <glib/gstdio.h>
#include <zmq.h>

#include "config.h"
#include "kiran-biometrics-types.h"
#include "kiran-face-manager.h"
#include "kiran-face-msg.h"
#include "json-glib/json-glib.h"

#define FACE_CAS_FILE "/usr/share/OpenCV/haarcascades/haarcascade_frontalface_default.xml"
#define EYE_CAS_FILE "/usr/share/OpenCV/haarcascades/haarcascade_eye_tree_eyeglasses.xml"
#define DEFAULT_ZMP_ADDR "ipc://KiranFaceService.ipc"
#define ENROLL_FACE_NUM 10

struct _KiranFaceManagerPrivate
{
    GCVCamera *camera;
    GCVCascadeClassifier *face_cas;
    GCVCascadeClassifier *eye_cas;
    GCVImage *detect_image;

    GThread *detect_thread;
    gboolean detect;

    GMutex mutex;
    GCond cond;

    gpointer ctx;
    gpointer service;
    gchar *addr;

    gboolean do_enroll;
    gboolean do_verify;
    gint enroll_face_count;

    GThread *face_thread;
    GList *enroll_images;
    GCVImage *face;

    GMutex face_mutex;
    GCond face_cond;
};

enum kiran_biometrics_signals {
    SIGNAL_FACE_VERIFY_STATUS,
    SIGNAL_FACE_ENROLL_STATUS,
    NUM_SIGNAL,
};

static guint signals[NUM_SIGNAL] = {0, };

#define KIRAN_FACE_MANAGER_GET_PRIVATE(O) \
        (G_TYPE_INSTANCE_GET_PRIVATE((O), KIRAN_TYPE_FACE_MANAGER, KiranFaceManagerPrivate))

G_DEFINE_TYPE (KiranFaceManager, kiran_face_manager, G_TYPE_OBJECT);

static void
kiran_face_manager_finalize (GObject *object)
{
    KiranFaceManager *manager;
    KiranFaceManagerPrivate *priv;

    manager = KIRAN_FACE_MANAGER (object);
    priv = manager->priv;

    if (priv->camera)
        g_object_unref (priv->camera);

    priv->camera = NULL;
    g_object_unref (priv->face_cas);
    g_object_unref (priv->eye_cas);

    g_mutex_clear (&priv->mutex);
    g_cond_clear (&priv->cond);

    g_mutex_clear (&priv->face_mutex);
    g_cond_clear (&priv->face_cond);

    g_free (priv->addr);
    zmq_close (priv->service);
    zmq_ctx_term (priv->ctx);

    G_OBJECT_CLASS (kiran_face_manager_parent_class)->finalize (object);
}

static void
kiran_face_manager_class_init (KiranFaceManagerClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);

    gobject_class->finalize = kiran_face_manager_finalize;

    g_type_class_add_private (class, sizeof (KiranFaceManagerPrivate));

    signals[SIGNAL_FACE_VERIFY_STATUS] =
                        g_signal_new ("verify-face-status",
                                       G_TYPE_FROM_CLASS (gobject_class),
                                       G_SIGNAL_RUN_LAST,
                                       0,
                                       NULL, NULL, NULL,
                                       G_TYPE_NONE, 0);

    signals[SIGNAL_FACE_ENROLL_STATUS] =
                        g_signal_new ("enroll-face-status",
                                       G_TYPE_FROM_CLASS (gobject_class),
                                       G_SIGNAL_RUN_LAST,
                                       0,
                                       NULL, NULL, NULL,
                                       G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_INT);
}

static void
send_faces_axis (KiranFaceManager *manager, 
		 GList *faces)
{
    KiranFaceManagerPrivate *priv = manager->priv;
    GList *iter;
    JsonArray *array;
    JsonGenerator *generator;
    JsonNode *root;
    gsize len;
    gchar *data;
    struct face_axis *axis;
    int ret;
    gsize total_len;

    array = json_array_new ();
    generator = json_generator_new();
    root = json_node_new (JSON_NODE_ARRAY);

    for (iter = faces; iter; iter = iter->next)
    {
	JsonNode *node;
	JsonNode *child_node;
	JsonObject *object;
	GCVRectangle *rectangle;

	rectangle = iter->data;
	node = json_node_new (JSON_NODE_OBJECT);
	object = json_object_new ();

	child_node = json_node_new (JSON_NODE_VALUE);
	json_node_init_int (child_node, gcv_rectangle_get_x (rectangle));
	json_object_set_member (object, 
			        "x", 
				 child_node);

	child_node = json_node_new (JSON_NODE_VALUE);
	json_node_init_int (child_node, gcv_rectangle_get_y (rectangle));
	json_object_set_member (object, 
			        "y", 
				 child_node);

	child_node = json_node_new (JSON_NODE_VALUE);
	json_node_init_int (child_node, gcv_rectangle_get_width (rectangle));
	json_object_set_member (object, 
			        "w", 
				 child_node);

	child_node = json_node_new (JSON_NODE_VALUE);
	json_node_init_int (child_node, gcv_rectangle_get_height (rectangle));
	json_object_set_member (object, 
			        "h", 
				 child_node);

	json_node_init_object (node, object);
	json_array_add_element (array, node);
    }
    
    json_node_init_array (root, array);
    json_generator_set_root (generator, root); 

    data = json_generator_to_data (generator, &len);

    g_message ("------------################json data: %s--------(%d)\n", data, len);

    total_len = len + sizeof (unsigned int) + sizeof (unsigned char); //发送的总长度
    axis = g_malloc0 (total_len); 
    axis->type = AXIS_TYPE;
    axis->len = len;
    g_strlcpy (axis->content, data, len);

    ret = zmq_send (priv->service, (unsigned char*)axis, total_len, ZMQ_DONTWAIT);

    json_array_unref (array);
    g_object_unref (generator);
    g_free (axis);
}

static gpointer
do_face_detect (gpointer data)
{
    KiranFaceManager *manager = KIRAN_FACE_MANAGER (data);
    KiranFaceManagerPrivate *priv = manager->priv;
    GList *faces;
    GList *eyes;
    
    while (priv->detect)
    {
	g_mutex_lock (&priv->mutex);
	g_cond_wait (&priv->cond, &priv->mutex);

	faces = gcv_cascade_classifier_detect (priv->face_cas, priv->detect_image);
	eyes = gcv_cascade_classifier_detect (priv->eye_cas, priv->detect_image);

	send_faces_axis (manager, faces);

	g_message ("face and eys ========== %d, %d", g_list_length (faces), g_list_length (eyes));

	if (g_list_length (faces) == 1 && 
	    g_list_length (eyes) == 2)
	{
	    //只有一张人脸时
            if (g_mutex_trylock (&priv->face_mutex))
            {
                //使用该图像进行检测人脸
	        priv->face = gcv_image_clip (priv->detect_image, faces->data);
                g_cond_signal (&priv->face_cond);
                g_mutex_unlock (&priv->face_mutex);
            }
	}
	
	g_object_unref (priv->detect_image);
	g_list_free_full (faces, g_object_unref);
	g_list_free_full (eyes, g_object_unref);

	g_mutex_unlock (&priv->mutex);

    }

    g_thread_exit (0);
}

static int
kiran_face_manager_save_face_to_file (KiranFaceManager *manager,
				      gchar *dir,
				      GCVImage *image,
				      gint index)
{
    KiranFaceManagerPrivate *priv = manager->priv;
    GError *error = NULL;
    gchar *path;

    if (!image)
	return FACE_RESULT_FAIL;

    path = g_strdup_printf ("%s/%d.png", dir, index);
    gcv_image_write (image, path, &error);
    g_free (path);

    if (error)
    {
	g_message ("kiran_face_manager_save_face_to_file: %s", error->message);
	g_error_free (error);
	return FACE_RESULT_FAIL;
    }

    return FACE_RESULT_OK;
}

static int
kiran_face_manager_save_faces (KiranFaceManager *manager,
		               gchar **md5)
{
    KiranFaceManagerPrivate *priv = manager->priv;
    gint i = 0;
    GBytes *bytes;
    const gchar *data;
    gsize len;
    gchar *dir;
    GList *iter;
    gint ret;

    iter = priv->enroll_images;

    if (!iter->data)
	return FACE_RESULT_FAIL;

    bytes = gcv_matrix_get_bytes (GCV_MATRIX(iter->data));
    if (!bytes)
        return FACE_RESULT_FAIL;

    data = g_bytes_get_data (bytes, &len);

    *md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5,
                                         data,
                                         len);
    g_bytes_unref (bytes);

    dir = g_strdup_printf ("%s", FACE_DIR);
    if (!g_file_test (dir, G_FILE_TEST_IS_DIR))
    {
        g_mkdir(dir, S_IRWXU);
    }
    g_free (dir);

    dir = g_strdup_printf ("%s/%s", FACE_DIR, *md5);
    if (!g_file_test (dir, G_FILE_TEST_IS_DIR))
    {
        g_mkdir(dir, S_IRWXU);
    }

    ret = FACE_RESULT_FAIL;
    for ( ; iter; iter = iter->next)
    {
	ret = kiran_face_manager_save_face_to_file (manager, dir, iter->data, i);
	if (ret != FACE_RESULT_OK)
	{
	    break;
	}
	i++;
    }

    g_free (dir);

    return ret;
}

static gpointer
do_face_handle (gpointer data)
{
    KiranFaceManager *manager = KIRAN_FACE_MANAGER (data);
    KiranFaceManagerPrivate *priv = manager->priv;
    int ret = 0;

    while (priv->detect)
    {
	g_mutex_lock (&priv->face_mutex);
	g_cond_wait (&priv->face_cond, &priv->face_mutex);

        if (priv->do_enroll)
	{
	    if (priv->enroll_face_count < ENROLL_FACE_NUM)
	    {
	        //采集人脸
		priv->enroll_images = g_list_append (priv->enroll_images, g_object_ref(priv->face));
    		g_signal_emit(manager,
                              signals[SIGNAL_FACE_ENROLL_STATUS], 0,
                              "", priv->enroll_face_count * 10);
	        priv->enroll_face_count++;
	    }
	    
	    if (priv->enroll_face_count == ENROLL_FACE_NUM)
	    {
		gchar *id = NULL;
	 	//完成采集
		ret = kiran_face_manager_save_faces (manager, &id);

		if (ret == FACE_RESULT_OK)
		{
		    priv->do_enroll = FALSE;
                    //发送完成录入信号
                    g_signal_emit(manager,
                                  signals[SIGNAL_FACE_ENROLL_STATUS], 0,
                                  id, 100);
		}
		else
		{
		    priv->do_enroll = TRUE;
		}

		g_free (id);

		priv->enroll_face_count = 0;
		g_list_free_full (priv->enroll_images, g_object_unref);
	    }
	}
	
	g_object_unref (priv->face);
	if (priv->do_verify)
	{
	    //认证人脸
	}

	g_mutex_unlock (&priv->face_mutex);
    }

    g_thread_exit (0);
}

static void
kiran_face_manager_init (KiranFaceManager *self)
{
    KiranFaceManagerPrivate *priv;
    GError *error;
    int ret = 0;

    priv = self->priv = KIRAN_FACE_MANAGER_GET_PRIVATE (self);
    priv->camera = NULL;
    error = NULL;
    priv->face_cas = gcv_cascade_classifier_new (FACE_CAS_FILE, &error);
    if (error)
    {
	g_message("gcv_cascade_classifier_new face ---%s\n", error->message);
	g_error_free (error);
    }

    error = NULL;
    priv->eye_cas = gcv_cascade_classifier_new (EYE_CAS_FILE, &error);
    if (error)
    {
	g_message("gcv_cascade_classifier_new eye ---%s\n", error->message);
	g_error_free (error);
    }

    priv->do_enroll = FALSE;
    priv->do_verify = FALSE;
    priv->enroll_face_count = 0;

    g_mutex_init (&priv->mutex);
    g_cond_init (&priv->cond);

    priv->detect = TRUE;
    priv->detect_thread = g_thread_new (NULL,
                                        do_face_detect,
                                        self);

    g_mutex_init (&priv->face_mutex);
    g_cond_init (&priv->face_cond);

    priv->face_thread = g_thread_new (NULL,
                                      do_face_handle,
                                      self);

    priv->addr = g_strdup (DEFAULT_ZMP_ADDR);
    priv->ctx = zmq_ctx_new();
    priv->service = zmq_socket (priv->ctx, ZMQ_PUB);
    ret = zmq_bind (priv->service,  priv->addr);
    if (ret != 0)
    {
	g_message("zmq bind  %s failed!\n", priv->addr);
    }
}

int 
kiran_face_manager_start (KiranFaceManager *kfamanager)
{
    KiranFaceManagerPrivate *priv = kfamanager->priv;
    GError *error = NULL;

    if (priv->camera)
	return FACE_RESULT_FAIL;

    priv->camera = gcv_camera_new (&error);
    
    if (error)
    {
	g_message("kiran_face_manager_start ---%s\n", error->message);
	g_error_free (error);
	return FACE_RESULT_FAIL;
    }

    g_signal_emit(kfamanager,
                  signals[SIGNAL_FACE_ENROLL_STATUS], 0,
                  "", priv->enroll_face_count * 10);
    
    return FACE_RESULT_OK;
}

static int 
send_image_data (KiranFaceManager *kfamanager,
		GCVImage *image)
{
    KiranFaceManagerPrivate *priv = kfamanager->priv;
    struct face_image *fimg;
    GBytes *bytes;
    const gchar *data;
    int width;
    int height;
    gsize len;
    gsize total_len;
    int ret;

    width = gcv_matrix_get_n_columns (GCV_MATRIX(image));
    height = gcv_matrix_get_n_rows (GCV_MATRIX(image));
    bytes = gcv_matrix_get_bytes (GCV_MATRIX(image));
    data = g_bytes_get_data (bytes, &len);

    total_len = len + 3 * sizeof (unsigned int) + sizeof (unsigned char); //发送的总长度
    fimg = g_malloc0 (total_len); 

    fimg->type = IMAGE_TYPE;
    fimg->width = width;
    fimg->height = height;
    fimg->len = len;
    memcpy (fimg->content, data, len);

    ret = zmq_send (priv->service, (unsigned char*)fimg, total_len, ZMQ_DONTWAIT);

    g_free (fimg);
    g_bytes_unref (bytes);

    return ret;
}

int 
kiran_face_manager_capture_face (KiranFaceManager *kfamanager)
{
    KiranFaceManagerPrivate *priv = kfamanager->priv;
    GCVImage *image;
    int ret = 0;

    if (!priv->camera)
	return FACE_RESULT_FAIL;

    image = gcv_video_capture_read (GCV_VIDEO_CAPTURE(priv->camera));
    if (image)
    {
	ret = send_image_data (kfamanager, image);
	if (g_mutex_trylock (&priv->mutex))
	{
	    //使用该图像进行检测人脸
	    priv->detect_image = image;
	    g_cond_signal (&priv->cond);
	    g_mutex_unlock (&priv->mutex);
	}
	else
            g_object_unref (image);
    }
    else
    {
	g_message("kiran_face_manager_capture_face can not read image");
    }

    return FACE_RESULT_OK;
}

int 
kiran_face_manager_do_enroll (KiranFaceManager *kfamanager)
{
    KiranFaceManagerPrivate *priv = kfamanager->priv;

    if (priv->do_enroll)
    {
	return FACE_RESULT_FAIL;
    }

    priv->do_enroll = TRUE;
    priv->enroll_face_count = 0;

    return FACE_RESULT_OK;
}

int 
kiran_face_manager_do_verify (KiranFaceManager *kfamanager)
{
    KiranFaceManagerPrivate *priv = kfamanager->priv;

    if (priv->do_verify)
    {
	return FACE_RESULT_FAIL;
    }

    priv->do_verify = TRUE;

    return FACE_RESULT_OK;
}

int 
kiran_face_manager_stop (KiranFaceManager *kfamanager)
{
    KiranFaceManagerPrivate *priv = kfamanager->priv; 

    if (!priv->camera)
	return FACE_RESULT_FAIL;

    priv->do_enroll = FALSE;
    priv->do_verify = FALSE;
    priv->enroll_face_count = 0;
    
    gcv_video_capture_release (GCV_VIDEO_CAPTURE(priv->camera));

    g_object_ref (priv->camera);
    priv->camera = NULL;

    return FACE_RESULT_OK;
}

char *
kiran_face_manager_get_addr (KiranFaceManager *kfamanager)
{
    KiranFaceManagerPrivate *priv = kfamanager->priv; 

    return priv->addr;
}

KiranFaceManager *
kiran_face_manager_new ()
{
    return g_object_new (KIRAN_TYPE_FACE_MANAGER, NULL);
}
