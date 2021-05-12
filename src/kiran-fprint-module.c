#include "kiran-fprint-module.h"

G_DEFINE_TYPE (KiranFprintModule, kiran_fprint_module, G_TYPE_TYPE_MODULE);

static void
kiran_fprint_module_finalize (GObject *object)
{
    KiranFprintModule *module;

    module = KIRAN_FPRINT_MODULE (object);

    g_free (module->path);

    G_OBJECT_CLASS (kiran_fprint_module_parent_class)->finalize (object);
}

static gboolean
kiran_fprint_module_load (GTypeModule *gmodule)
{
    KiranFprintModule *module;

    module = KIRAN_FPRINT_MODULE (gmodule);

    module->library = g_module_open (module->path,  G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
    if (!module->library)
    {
	g_warning ("%s", g_module_error ());
        return FALSE;
    }

    if (!g_module_symbol (module->library, 
			  "kiran_fprint_init",
			   (gpointer *)&module->fprint_init) ||
        !g_module_symbol (module->library, 
			  "kiran_fprint_finalize",
			   (gpointer *)&module->fprint_finalize) ||
        !g_module_symbol (module->library, 
			  "kiran_fprint_open_device",
			   (gpointer *)&module->fprint_open_device) ||
        !g_module_symbol (module->library, 
			  "kiran_fprint_acquire_finger_print",
			   (gpointer *)&module->fprint_acquire_finger_print) ||
        !g_module_symbol (module->library, 
			  "kiran_fprint_verify_finger_print",
			   (gpointer *)&module->fprint_verify_finger_print) ||
        !g_module_symbol (module->library, 
			  "kiran_fprint_close_device",
			   (gpointer *)&module->fprint_close_device) ||
        !g_module_symbol (module->library, 
			  "kiran_fprint_get_dev_count",
			   (gpointer *)&module->fprint_get_dev_count) ||
        !g_module_symbol (module->library, 
			  "kiran_fprint_template_merge",
			   (gpointer *)&module->fprint_template_merge) ||
        !g_module_symbol (module->library, 
			  "kiran_fprint_template_match",
			   (gpointer *)&module->fprint_template_match))
    {
	g_warning ("%s", g_module_error ());
        g_module_close (module->library);

        return FALSE;
    }

    return TRUE;
}

static void
kiran_fprint_module_unload (GTypeModule *gmodule)
{
    KiranFprintModule *module;

    module = KIRAN_FPRINT_MODULE (gmodule);

    g_module_close (module->library);
    module->fprint_init = NULL;
    module->fprint_finalize = NULL;
    module->fprint_get_dev_count = NULL;
    module->fprint_acquire_finger_print = NULL;
    module->fprint_verify_finger_print = NULL;
    module->fprint_open_device = NULL;
    module->fprint_close_device = NULL;
    module->fprint_template_merge = NULL;
    module->fprint_template_match = NULL;
    module->hDevice = NULL;
}

static void
kiran_fprint_module_class_init (KiranFprintModuleClass *class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (class);
    GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS (class);

    object_class->finalize = kiran_fprint_module_finalize;
    module_class->load = kiran_fprint_module_load;
    module_class->unload = kiran_fprint_module_unload;
}

static void
kiran_fprint_module_init (KiranFprintModule *self)
{
}

KiranFprintModule *
kiran_fprint_module_new ()
{
    return g_object_new (KIRAN_TYPE_FPRINT_MODULE, NULL);
}
