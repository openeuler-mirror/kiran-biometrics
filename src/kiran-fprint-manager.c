#include "kiran-fprint-manager.h"
#include "kiran-fprint-module.h"
#include "config.h"
#include "kiran-biometrics-types.h"

struct _KiranFprintManagerPrivate
{ 
    KiranFprintModule *current_module; /* 当前正在使用的模块 */

    GList *modules;
};

#define KIRAN_FPRINT_MANAGER_GET_PRIVATE(O) \
	(G_TYPE_INSTANCE_GET_PRIVATE((O), KIRAN_TYPE_FPRINT_MANAGER, KiranFprintManagerPrivate))

G_DEFINE_TYPE (KiranFprintManager, kiran_fprint_manager, G_TYPE_OBJECT);

static void
kiran_fprint_manager_finalize (GObject *object)
{
    KiranFprintManager *manager;
    KiranFprintManagerPrivate *priv;
    GList *l, *next;

    manager = KIRAN_FPRINT_MANAGER (object);
    priv = manager->priv;

    for (l = priv->modules; l != NULL; l = next)
    {
        next = l->next;
        g_object_unref (l->data);
    }

    g_list_free (priv->modules);

    G_OBJECT_CLASS (kiran_fprint_manager_parent_class)->finalize (object);
}

static void
kiran_fprint_manager_class_init (KiranFprintManagerClass *class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (class);

    object_class->finalize = kiran_fprint_manager_finalize;

    g_type_class_add_private (class, sizeof (KiranFprintManagerPrivate));
}

static KiranFprintModule *
kiran_fprint_manager_load_module_file (KiranFprintManager *manager,
				       const char *filename)
{
    KiranFprintManagerPrivate *priv = manager->priv;
    KiranFprintModule *module;

    module = kiran_fprint_module_new ();
    module->path = g_strdup (filename);

    if (g_type_module_use (G_TYPE_MODULE(module)))
    {
	priv->modules = g_list_prepend (priv->modules, module);
        g_type_module_unuse (G_TYPE_MODULE (module));
        return module;
    }
    else
    {
	g_object_unref (module);
        return NULL;
    }

}

static void
kiran_fprint_manager_load_module_dir (KiranFprintManager *manager,
				      const char *dirname)
{
    GDir *dir;

    dir = g_dir_open (dirname, 0, NULL);

    if (dir)
    {
        const char *name;

        while ((name = g_dir_read_name (dir)))
        {
            if (g_str_has_suffix (name, "." G_MODULE_SUFFIX))
            {
                char *filename;

                filename = g_build_filename (dirname,
                                             name,
                                             NULL);
                kiran_fprint_manager_load_module_file (manager, filename);
            }
        }
        g_dir_close (dir);
    }
}

static void
kiran_fprint_manager_init (KiranFprintManager *self)
{
    KiranFprintManagerPrivate *priv;

    priv = self->priv = KIRAN_FPRINT_MANAGER_GET_PRIVATE (self);
    priv->modules= NULL;
    priv->current_module = NULL;

    kiran_fprint_manager_load_module_dir (self, FPRINT_MODULEDIR);
}

static int 
kiran_fprint_manager_open_with_module (KiranFprintManager *kfp_manager,
		                       KiranFprintModule *module)
{
    int ret;
    int i;
   
    module->fprint_init ();

    ret = module->fprint_get_dev_count ();
    
    if (ret <= 0)
        return FPRINT_RESULT_NO_DEVICE;
    
    for (i = 0; i < ret; i++)
    {
        //默认打开一个设备
        module->hDevice = module->fprint_open_device(i);
        
        if (module->hDevice)
    	    return FPRINT_RESULT_OK;
    }

    return FPRINT_RESULT_FAIL;
}

int 
kiran_fprint_manager_open (KiranFprintManager *kfp_manager)
{
    KiranFprintManagerPrivate *priv = kfp_manager->priv;
    KiranFprintModule *module;
    GList *l, *next;
    int ret = FPRINT_RESULT_FAIL;

    module = priv->current_module;
    if (module)
    {
	ret = kiran_fprint_manager_open_with_module (kfp_manager, module);
	if (ret == 0)
	    return 0;
    }

    for (l = priv->modules; l != NULL; l = next)
    {
	module = l->data;
	if (g_type_module_use (G_TYPE_MODULE(module)))
	{
	    ret = kiran_fprint_manager_open_with_module (kfp_manager, module);
	    if (ret == 0)
	    {
		if (priv->current_module)
		    g_type_module_unuse (G_TYPE_MODULE(priv->current_module));
		priv->current_module = module;
		return 0;
	    }
	    else
	    {
		g_type_module_unuse (G_TYPE_MODULE(module));
	    }
	}
        next = l->next;
    }

    return ret;
}

int 
kiran_fprint_manager_close (KiranFprintManager *kfp_manager)
{
    KiranFprintManagerPrivate *priv = kfp_manager->priv;
    KiranFprintModule *module;
    int ret = FPRINT_RESULT_FAIL;

    module = priv->current_module;
    if (module)
    {
	if (module->fprint_close_device && module->hDevice)
	{
	    ret = module->fprint_close_device(module->hDevice);
	    module->fprint_finalize();
	}
    }

    return ret;
}

int 
kiran_fprint_manager_acquire_finger_print (KiranFprintManager *kfp_manager,
		                           unsigned char **fpTemplate,
                                           unsigned int *cbTemplate,
					   unsigned int timeout)
{
    KiranFprintManagerPrivate *priv = kfp_manager->priv;
    KiranFprintModule *module;

    module = priv->current_module;
    if(module)
    {
	if (module->fprint_acquire_finger_print && module->hDevice)
	    return module->fprint_acquire_finger_print (module->hDevice,
			    			        fpTemplate, 
							cbTemplate,
							timeout);
    }

    return FPRINT_RESULT_FAIL;
}

int 
kiran_fprint_manager_template_merge (KiranFprintManager *kfp_manager,
		                     unsigned char *fpTemplate1,
                                     unsigned char *fpTemplate2,
                                     unsigned char *fpTemplate3,
                                     unsigned char **regTemplate,
                                     unsigned int *cbRegTemplate)
{
    KiranFprintManagerPrivate *priv = kfp_manager->priv;
    KiranFprintModule *module;

    module = priv->current_module;
    if(module)
    {
	if (module->fprint_template_merge && module->hDevice)
	    return module->fprint_template_merge (module->hDevice,
			    			  fpTemplate1,
			                          fpTemplate2,
					          fpTemplate3,
					          regTemplate,
					          cbRegTemplate);
    }

    return FPRINT_RESULT_FAIL;
}

int 
kiran_fprint_manager_template_match (KiranFprintManager *kfp_manager,
			             unsigned char *fpTemplate1,
				     unsigned int cbfpTemplate1,
			             unsigned char *fpTemplate2,
				     unsigned int cbfpTemplate2)
{
    KiranFprintManagerPrivate *priv = kfp_manager->priv;
    KiranFprintModule *module;

    module = priv->current_module;
    if(module)
    {
	if (module->fprint_template_match && module->hDevice)
	    return module->fprint_template_match (module->hDevice,
			    			  fpTemplate1,
						  cbfpTemplate1,
			    			  fpTemplate2,
						  cbfpTemplate2);
    }

    return FPRINT_RESULT_FAIL;
}

KiranFprintManager *
kiran_fprint_manager_new ()
{
    return g_object_new (KIRAN_TYPE_FPRINT_MANAGER, NULL);
}
