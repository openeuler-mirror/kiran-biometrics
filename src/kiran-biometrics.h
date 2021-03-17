#ifndef __KIRAN_BIOMETRICS_H__
#define __KIRAN_BIOMETRICS_H__

#include <glib.h>
#include <dbus/dbus-glib.h>

#define KIRAN_TYPE_BIOMETRICS			(kiran_biometrics_get_type())
#define KIRAN_BIOMETRICS(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), \
							KIRAN_TYPE_BIOMETRICS, KiranBiometrics))
#define KIRAN_BIOMETRICS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), \
							KIRAN_TYPE_BIOMETRICS, KiranBiometricsClass))
#define KIRAN_IS_BIOMETRICS(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
		       					KIRAN_TYPE_BIOMETRICS))
#define KIRAN_IS_BIOMETRICS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), \
							KIRAN_TYPE_BIOMETRICS))
#define KIRAN_BIOMETRICS_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS((obj), \
							KIRAN_TYPE_BIOMETRICS, KiranBiometricsClass))


typedef struct _KiranBiometrics 	KiranBiometrics;
typedef struct _KiranBiometricsClass 	KiranBiometricsClass;
typedef struct _KiranBiometricsPrivate  KiranBiometricsPrivate;

struct _KiranBiometrics
{
    GObject parent;

    KiranBiometricsPrivate *priv;
};

struct _KiranBiometricsClass
{
    GObjectClass parent;
};

GType kiran_biometrics_get_type();

KiranBiometrics *kiran_biometrics_new();

#endif /* __KIRAN_BIOMETRICS_H__ */
