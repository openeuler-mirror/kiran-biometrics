#ifndef __KIRAN_FPRINT_MODULE_H__
#define __KIRAN_FPRINT_MODULE_H__

#include <glib-object.h>
#include <glib.h>
#include <gmodule.h>

#define KIRAN_TYPE_FPRINT_MODULE		(kiran_fprint_module_get_type())
#define KIRAN_FPRINT_MODULE(obj) 		(G_TYPE_CHECK_INSTANCE_CAST((obj),\
							KIRAN_TYPE_FPRINT_MODULE, KiranFprintModule))
#define KIRAN_FPRINT_MODULE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass),\
		       					KIRAN_TYPE_FPRINT_MODULE, KiranFprintModuleClass))
#define KIRAN_IS_FPRINT_MODULE(obj) 		(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
							KIRAN_TYPE_FPRINT_MODULE))
#define KIRAN_IS_FPRINT_MODULE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
		       					KIRAN_TYPE_FPRINT_MODULE))
#define KIRAN_FPRINT_MODULE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj),\
		       					KIRAN_TYPE_FPRINT_MODULE, KiranFprintModuleClass))

typedef struct _KiranFprintModule     	KiranFprintModule;
typedef struct _KiranFprintModuleClass	KiranFprintModuleClass;

struct _KiranFprintModule
{
    GTypeModule parent;

    GModule *library;
    gchar *path;


    gpointer hDevice;
    int (*fprint_init) ();
    int (*fprint_finalize) ();
    int (*fprint_get_dev_count) ();
    gpointer (*fprint_open_device ) (int index);
    int (*fprint_acquire_finger_print) (gpointer hDevice,
			    	       unsigned char **fpTemplate,
				       unsigned int *cbTemplate,
				       unsigned int timeout);
    int (*fprint_verify_finger_print) (gpointer hDevice,
			    	       unsigned char *fpTemplate,
				       unsigned int cbTemplate,
				       unsigned int timeout);
    int (*fprint_close_device ) (gpointer hDevice);
    int (*fprint_template_merge) (gpointer hDevice,
		    	          unsigned char *fpTemplate1,
                                  unsigned char *fpTemplate2,
                                  unsigned char *fpTemplate3,
                                  unsigned char **regTemplate,
                                  unsigned int *cbRegTemplate);
    int (*fprint_template_match) (gpointer hDevice,
                                  unsigned char *fpTemplate1,
                                  unsigned int cbfpTemplate1,
                                  unsigned char *fpTemplate2,
                                  unsigned int cbfpTemplate2);

};

struct _KiranFprintModuleClass
{
    GTypeModuleClass parent;
};

GType kiran_fprint_module_get_type();
KiranFprintModule *kiran_fprint_module_new ();

#endif /* __KIRAN_FPRINT_MODULE_H__ */
