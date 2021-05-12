#ifndef __KIRAN_FPRINT_MANAGER_H__
#define __KIRAN_FPRINT_MANAGER_H__

#include <glib-object.h>
#include <glib.h>

#define KIRAN_TYPE_FPRINT_MANAGER		(kiran_fprint_manager_get_type())
#define KIRAN_FPRINT_MANAGER(obj) 		(G_TYPE_CHECK_INSTANCE_CAST((obj),\
							KIRAN_TYPE_FPRINT_MANAGER, KiranFprintManager))
#define KIRAN_FPRINT_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass),\
		       					KIRAN_TYPE_FPRINT_MANAGER, KiranFprintManagerClass))
#define KIRAN_IS_FPRINT_MANAGER(obj) 		(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
							KIRAN_TYPE_FPRINT_MANAGER))
#define KIRAN_IS_FPRINT_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
		       					KIRAN_TYPE_FPRINT_MANAGER))
#define KIRAN_FPRINT_MANAGER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj),\
		       					KIRAN_TYPE_FPRINT_MANAGER, KiranFprintManagerClass))

typedef struct _KiranFprintManager     	  KiranFprintManager;
typedef struct _KiranFprintManagerClass	  KiranFprintManagerClass;
typedef struct _KiranFprintManagerPrivate KiranFprintManagerPrivate;

struct _KiranFprintManager
{
    GObject parent;

    KiranFprintManagerPrivate *priv;
};

struct _KiranFprintManagerClass
{
    GObjectClass parent;
};

GType kiran_fprint_manager_get_type();
KiranFprintManager *kiran_fprint_manager_new ();
int kiran_fprint_manager_open (KiranFprintManager *kfp_manager);
int kiran_fprint_manager_close (KiranFprintManager *kfp_manager);
int kiran_fprint_manager_acquire_finger_print (KiranFprintManager *kfp_manager,
		                               unsigned char **fpTemplate,
                                               unsigned int *cbTemplate,
					       unsigned int timeout);
int kiran_fprint_manager_verify_finger_print (KiranFprintManager *kfp_manager,
		                              unsigned char *fpTemplate,
                                              unsigned int cbTemplate,
					      unsigned int timeout);
int kiran_fprint_manager_template_merge (KiranFprintManager *kfp_manager,
		                         unsigned char *fpTemplate1,
                                         unsigned char *fpTemplate2,
                                         unsigned char *fpTemplate3,
                                         unsigned char **regTemplate,
                                         unsigned int *cbRegTemplate); 
int kiran_fprint_manager_template_match (KiranFprintManager *kfp_manager,
			                 unsigned char *fpTemplate1,
				         unsigned int cbfpTemplate1,
			                 unsigned char *fpTemplate2,
				         unsigned int cbfpTemplate2);

#endif /* __KIRAN_FPRINT_MANAGER_H__ */
