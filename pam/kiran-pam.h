#ifndef __KIRAN_PAM_H__
#define __KIRAN_PAM_H__

#include <security/pam_modules.h>
#include <glib.h>

#define D(pamh, ...) {                                  \
    char *s;                                \
    s = g_strdup_printf (__VA_ARGS__);      \
    send_debug_msg (pamh, s);               \
    g_free (s);                             \
}


char * request_respone(pam_handle_t *pamh, 
		       int echocode, 
		       const char *prompt) ;

gboolean send_info_msg(pam_handle_t *pamh, 
		       const char *msg);

gboolean send_err_msg(pam_handle_t *pamh, 
		      const char *msg);

void send_debug_msg(pam_handle_t *pamh, 
		    const char *msg);

#endif /* __KIRAN_PAM_H__ */
