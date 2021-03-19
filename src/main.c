#include "config.h"

#include <stdlib.h>
#include <dbus/dbus-glib-bindings.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gmodule.h>
#include <locale.h>
#include "kiran-biometrics.h"


int main (int argc, char **argv)
{
    GMainLoop *loop;
    GError *error = NULL;
    KiranBiometrics *kirBiometrics;
    DBusGConnection *kiran_biometrics_dbus_conn;
    DBusGProxy *driver_proxy;
    guint request_name_ret;

    setlocale(LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

#if !GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init();
#endif

    kiran_biometrics_dbus_conn = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
    if (kiran_biometrics_dbus_conn == NULL)
    {
	g_warning("Failed to open connection to bus: %s", error->message);
	return 1;
    }

    driver_proxy = dbus_g_proxy_new_for_name (kiran_biometrics_dbus_conn,
		    DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);

    loop = g_main_loop_new(NULL, FALSE);

    if (!org_freedesktop_DBus_request_name (driver_proxy, SERVICE_NAME,
			    0, &request_name_ret, &error))
    {
	g_warning("Failed to get name: %s", error->message);
    }

    if (request_name_ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        g_warning ("Got result code %u from requesting name", request_name_ret);
        return 1;
    }

    kirBiometrics = kiran_biometrics_new ();
    dbus_g_connection_register_g_object(kiran_biometrics_dbus_conn,
                SERVICE_PATH, G_OBJECT(kirBiometrics));

    g_main_loop_run (loop);

    g_object_unref (kirBiometrics);

    return 0;
}
