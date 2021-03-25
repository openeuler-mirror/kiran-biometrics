#include <opencv-glib/opencv-glib.h>
#include <glib/gstdio.h>
#include <zmq.h>
#include <zlog_ex.h>
#include <json-glib/json-glib.h>

#include "config.h"
#include "kiran-biometrics-types.h"
#include "kiran-face-manager.h"
#include "kiran-face-msg.h"

#define FACE_CAS_FILE "/usr/share/OpenCV/haarcascades/haarcascade_frontalface_default.xml"
#define EYE_CAS_FILE "/usr/share/OpenCV/haarcascades/haarcascade_eye_tree_eyeglasses.xml"
#define DEFAULT_ZMQ_ADDR "/tmp/KiranFaceService.ipc"
#define ENROLL_FACE_NUM 10

#define FACE_ZMQ_ADDR "ipc:///tmp/KiranFaceCompareService.ipc"

#define FACE_SIZE 160

enum 
{
    FACE_OK = 0,
    FACE_SMALL,
    FACE_BIG
};

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
    gpointer client;

    gboolean do_enroll;
    gboolean do_verify;
    gint enroll_face_count;

    GThread *face_thread;
    GList *enroll_images;
    GCVImage *face;

    GMutex face_mutex;
    GCond face_cond;

    gchar *id; //认证时使用的id
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
    zmq_close (priv->client);
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
                                       G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    signals[SIGNAL_FACE_ENROLL_STATUS] =
                        g_signal_new ("enroll-face-status",
                                       G_TYPE_FROM_CLASS (gobject_class),
                                       G_SIGNAL_RUN_LAST,
                                       0,
                                       NULL, NULL, NULL,
                                       G_TYPE_NONE, 3, G_TYPE_INT,G_TYPE_STRING, G_TYPE_INT);
}

static void
free_data (void *data, void *hint)
{
    g_free (data);
}

static int
zmq_msg_send_axis_with_json (KiranFaceManager *manager,
		             struct face_axis *axis)
{
    KiranFaceManagerPrivate *priv = manager->priv;
    JsonObject *object;
    JsonNode *root;
    JsonGenerator *generator;
    zmq_msg_t msg;
    gsize len;
    gchar *data;
    int ret;

    generator = json_generator_new();
    root = json_node_new (JSON_NODE_OBJECT);
    object = json_object_new ();

    json_object_set_int_member (object,
                                "type",
                                axis->type);

    json_object_set_int_member (object,
                                "len",
                                axis->len);

    json_object_set_string_member (object,
                                "content",
                                axis->content);

    json_node_init_object (root, object);
    json_object_unref (object);

    json_generator_set_root (generator, root);
    json_node_free (root);

    data = json_generator_to_data (generator, &len);
    g_object_unref (generator);

    zmq_msg_init_data (&msg, (unsigned char*)data, len, free_data, NULL);

    ret = zmq_msg_send(&msg, priv->service, 0);

    zmq_msg_close (&msg);


    return ret;
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
	JsonObject *object;
	GCVRectangle *rectangle;

	rectangle = iter->data;
	node = json_node_new (JSON_NODE_OBJECT);
	object = json_object_new ();

	json_object_set_int_member (object, 
			           "x", 
				   gcv_rectangle_get_x (rectangle));

	json_object_set_int_member (object, 
			           "y", 
				   gcv_rectangle_get_y (rectangle));

	json_object_set_int_member (object, 
			           "w", 
				    gcv_rectangle_get_width (rectangle));

	json_object_set_int_member (object, 
			           "h", 
				   gcv_rectangle_get_height (rectangle));

	json_node_init_object (node, object);
	json_object_unref (object);

	json_array_add_element (array, node);
    }
    
    json_node_init_array (root, array);
    json_array_unref (array);

    json_generator_set_root (generator, root); 
    json_node_free (root);

    data = json_generator_to_data (generator, &len);
    g_object_unref (generator);

    total_len = len + 1 +  sizeof (unsigned int) + sizeof (unsigned char); //发送的总长度
    axis = g_malloc0 (total_len); 
    axis->type = AXIS_TYPE;
    axis->len = len;
    g_strlcpy (axis->content, data, len + 1);

    ret = zmq_msg_send_axis_with_json(manager, axis);

    dzlog_debug ("send face json data: %s--------(%d)\n", data, ret);

    g_free (data);
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

	dzlog_debug ("detect face and eys number is %d, %d", g_list_length (faces), g_list_length (eyes));

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
	dzlog_debug ("kiran_face_manager_save_face_to_file: %s", error->message);
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

static int
face_compare (KiranFaceManager *manager,
	      GCVImage *image1,
	      GCVImage *image2)
{
    KiranFaceManagerPrivate *priv = manager->priv;
    gsize width1, height1, len1, width2, height2, len2;
    int total_len;
    int ret;
    char result[2] = {0};
    struct compare_source *compare;
    const gchar *data1, *data2;
    GBytes *bytes1, *bytes2;
    gint channel;

    ret = FACE_RESULT_FAIL;
    width1 = gcv_matrix_get_n_columns (GCV_MATRIX(image1));
    height1 = gcv_matrix_get_n_rows (GCV_MATRIX(image1));
    channel = gcv_matrix_get_n_channels(GCV_MATRIX(image1));

    bytes1 = gcv_matrix_get_bytes (GCV_MATRIX(image1));
    data1 = g_bytes_get_data (bytes1, &len1);

    width2 = gcv_matrix_get_n_columns (GCV_MATRIX(image2));
    height2 = gcv_matrix_get_n_rows (GCV_MATRIX(image2));
    bytes2 = gcv_matrix_get_bytes (GCV_MATRIX(image2));
    data2 = g_bytes_get_data (bytes2, &len2);

    total_len = len1 + len2 + 7 * sizeof (unsigned int) + sizeof (unsigned char); //发送的总长度
    compare = g_malloc0 (total_len);

    compare->type = COMPARE_IMAGE_TYPE;
    compare->channel = channel;
    compare->width1 = width1;
    compare->height1 = height1;
    compare->len1 = len1;
    compare->width2 = width2;
    compare->height2 = height2;
    compare->len2 = len2;

    memcpy (compare->content, data1, len1);
    memcpy (compare->content + len1, data2, len2);

    ret = zmq_send (priv->client, (unsigned char*)compare, total_len, ZMQ_DONTWAIT);
    g_message ("send to face compare service[%x, %d, %d, %d, %d] %d\n", channel, compare->type, width1, height1, len1, ret);

    if (ret > 0)
    {
        zmq_recv(priv->client, result, 2, 0);
        if(result[0] == COMPARE_RESULT_TYPE && result[1] == FACE_MATCH )
        {
   	    ret = FACE_RESULT_OK;
        }
    }

    g_bytes_unref (bytes1);
    g_bytes_unref (bytes2);
    g_free (compare);

    return ret;
}

static int
face_verify (KiranFaceManager *manager)
{
    KiranFaceManagerPrivate *priv = manager->priv;
    gchar *path;
    GError *error;
    GDir *dir;
    gchar *name;
    int ret;

    error = NULL;
    path = g_strdup_printf ("%s/%s", FACE_DIR, priv->id);
    dir = g_dir_open (path, 0, &error);

    if (error)
    {
	g_message ("open face dir %s fail:%s", path, error->message); 
	g_error_free (error);

        return FACE_RESULT_FAIL;
    }

    ret = FACE_RESULT_FAIL;
    while ((name = g_dir_read_name (dir)))
    {
	gchar *file_path;

	file_path = g_strdup_printf ("%s/%s", path, name);
	if (!g_file_test (file_path, G_FILE_TEST_IS_DIR))
	{
	    GCVImage *image;

	    image = gcv_image_read(file_path,
                                   GCV_IMAGE_READ_FLAG_UNCHANGED,
                                   NULL);
	    if (image)
	        ret = face_compare (manager, priv->face, image);

	    g_object_unref (image);
	}
	g_free (file_path);

	if (ret == FACE_RESULT_OK)
	{
	    //匹配成功
	    break;
	}
    }

    g_dir_close (dir); 
    g_free(path);

    return ret;
}

static int
face_quality (GCVImage *face)
{
    int width;
    int big_size = FACE_SIZE + 10;
    int small_size = FACE_SIZE - 10;

    width = gcv_matrix_get_n_columns (GCV_MATRIX(face));

    if ( width > big_size)
	return FACE_BIG;

    if (width < small_size)
	return FACE_SMALL;

    return FACE_OK;
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
	    ret = face_quality (priv->face);
	    if (ret == FACE_BIG)
	    {
    		g_signal_emit(manager,
                              signals[SIGNAL_FACE_ENROLL_STATUS], 0,
                              1, "", priv->enroll_face_count * 10);
	    }
	    else if (ret == FACE_SMALL)
	    {
    		g_signal_emit(manager,
                              signals[SIGNAL_FACE_ENROLL_STATUS], 0,
                              -1, "", priv->enroll_face_count * 10);
	    }

	    if (priv->enroll_face_count < ENROLL_FACE_NUM && ret == FACE_OK)
	    {
	        //采集人脸
		priv->enroll_images = g_list_append (priv->enroll_images, g_object_ref(priv->face));
    		g_signal_emit(manager,
                              signals[SIGNAL_FACE_ENROLL_STATUS], 0,
                              0, "", priv->enroll_face_count * 10);
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
                                  0, id, 100);
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
	
	if (priv->do_verify)
	{
	    //认证人脸
	    ret = face_verify (manager);
	    if (ret == FACE_RESULT_OK)
	    {
		//认证成功
		priv->do_verify = FALSE;
                g_signal_emit(manager,
                              signals[SIGNAL_FACE_VERIFY_STATUS], 0,
                              TRUE);
	    }
	    else
	    {
		//认证失败
                g_signal_emit(manager,
                              signals[SIGNAL_FACE_VERIFY_STATUS], 0,
                              FALSE);
	    }
	}

	g_object_unref (priv->face);
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
    int timeout = 30000;

    priv = self->priv = KIRAN_FACE_MANAGER_GET_PRIVATE (self);
    priv->camera = NULL;
    error = NULL;
    priv->face_cas = gcv_cascade_classifier_new (FACE_CAS_FILE, &error);
    if (error)
    {
	dzlog_debug("gcv_cascade_classifier_new face fail: %s\n", error->message);
	g_error_free (error);
    }

    error = NULL;
    priv->eye_cas = gcv_cascade_classifier_new (EYE_CAS_FILE, &error);
    if (error)
    {
	dzlog_debug("gcv_cascade_classifier_new eye fail: %s\n", error->message);
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

    priv->addr = g_strdup_printf ("ipc://%s", DEFAULT_ZMQ_ADDR);
    priv->ctx = zmq_ctx_new();
    priv->service = zmq_socket (priv->ctx, ZMQ_PUB);
    ret = zmq_bind (priv->service,  priv->addr);
    if (ret != 0)
    {
	dzlog_debug("zmq bind  %s failed!\n", priv->addr);
    }

    chmod (DEFAULT_ZMQ_ADDR, 0666); //修改权限使得普通用户可以读

    priv->client = zmq_socket (priv->ctx, ZMQ_REQ);
    ret = zmq_connect (priv->client, FACE_ZMQ_ADDR);
    if (ret != 0)
    {
	dzlog_debug("zmq coennt %s failed!\n", FACE_ZMQ_ADDR);
	g_message ("zmq connect  %s failed!\n", FACE_ZMQ_ADDR);
    }
    zmq_setsockopt (priv->client, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));

    priv->id = NULL;
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
	dzlog_debug("kiran_face_manager_start fail: %s\n", error->message);
	g_error_free (error);
	return FACE_RESULT_FAIL;
    }
    
    return FACE_RESULT_OK;
}

static int
zmq_msg_send_face_image_with_json (KiranFaceManager *kfamanager,
		                   struct face_image *fimg)
{
    KiranFaceManagerPrivate *priv = kfamanager->priv;
    JsonObject *object;
    JsonNode *root;
    JsonGenerator *generator;
    zmq_msg_t msg;
    gsize len;
    gchar *data;
    gchar *img_data; //图片数据
    int ret;

    generator = json_generator_new();
    root = json_node_new (JSON_NODE_OBJECT);

    object = json_object_new ();

    json_object_set_int_member (object,
                            "type",
                             fimg->type);

    json_object_set_int_member (object,
                            "channel",
                             fimg->channel);

    json_object_set_int_member (object,
		             "width",
                              fimg->width);

    json_object_set_int_member (object,
		            "height",
                             fimg->height);

    img_data = g_base64_encode (fimg->content, fimg->len);
    json_object_set_string_member (object,
		            "content",
                             img_data);
    g_free (img_data);

    json_node_init_object (root, object);
    json_object_unref (object);

    json_generator_set_root (generator, root); 
    json_node_free (root);

    data = json_generator_to_data (generator, &len);
    g_object_unref (generator);

    zmq_msg_init_data (&msg, (unsigned char*)data, len, free_data, NULL);

    ret = zmq_msg_send(&msg, priv->service, 0);

    zmq_msg_close (&msg);

    return ret;
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
    int channel;
    int ret;

    width = gcv_matrix_get_n_columns (GCV_MATRIX(image));
    height = gcv_matrix_get_n_rows (GCV_MATRIX(image));
    channel = gcv_matrix_get_n_channels(GCV_MATRIX(image));
    bytes = gcv_matrix_get_bytes (GCV_MATRIX(image));
    data = g_bytes_get_data (bytes, &len);

    total_len = len + 3 * sizeof (unsigned int) + sizeof (unsigned char); //发送的总长度
    fimg = g_malloc0 (total_len); 

    fimg->type = IMAGE_TYPE;
    fimg->channel = channel;
    fimg->width = width;
    fimg->height = height;
    fimg->len = len;
    memcpy (fimg->content, data, len);

   ret = zmq_msg_send_face_image_with_json (kfamanager, fimg);

    g_bytes_unref (bytes);
    g_free (fimg);

    return ret;
}

static GCVImage *
face_area_image (GCVImage *image)
{
    GCVRectangle *rect;
    GCVImage *img;
    gsize x, y, width, height;
    gsize len;
   
    width = gcv_matrix_get_n_columns (GCV_MATRIX(image));
    height = gcv_matrix_get_n_rows (GCV_MATRIX(image));

    len = width < height ? width : height; //取最小的边

    x = (width - len) / 2;
    y = (height - len) / 2;

    rect = gcv_rectangle_new(x, y, len, len);
    img = gcv_image_clip (image, rect);

    g_object_unref (rect);

    return img;
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
	    priv->detect_image = face_area_image (image);
	    g_cond_signal (&priv->cond);
	    g_mutex_unlock (&priv->mutex);
	}
        
	g_object_unref (image);
    }
    else
    {
	dzlog_debug("kiran_face_manager_capture_face can not read image");
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

    g_signal_emit(kfamanager,
                  signals[SIGNAL_FACE_ENROLL_STATUS], 0,
                  0, "", priv->enroll_face_count * 10);

    return FACE_RESULT_OK;
}

int 
kiran_face_manager_do_verify (KiranFaceManager *kfamanager,
		              const gchar *id)
{
    KiranFaceManagerPrivate *priv = kfamanager->priv;

    if (priv->do_verify)
    {
	return FACE_RESULT_FAIL;
    }

    priv->do_verify = TRUE;

    g_free (priv->id);
    priv->id = g_strdup (id);

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

int 
kiran_face_manager_delete (const gchar *id)
{
    gchar *path;
    int ret;

    path = g_strdup_printf ("%s/%s", FACE_DIR, id);
    if (!g_file_test (path, G_FILE_TEST_EXISTS))
    {
        g_free (path);
        return -1;
    }

    ret = g_rmdir (path);
    g_free(path);

    return ret;
}

KiranFaceManager *
kiran_face_manager_new ()
{
    return g_object_new (KIRAN_TYPE_FACE_MANAGER, NULL);
}
