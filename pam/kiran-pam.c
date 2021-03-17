#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <syslog.h>

#include "kiran-pam.h"

static int 
converse(pam_handle_t *pamh, int nargs,
         const struct pam_message **message,
         struct pam_response **response) {
      struct pam_conv *conv;
      int retval = pam_get_item(pamh, PAM_CONV, (void *)&conv);
      if (retval != PAM_SUCCESS) {
          return retval;
      }
      return conv->conv(nargs, message, response, conv->appdata_ptr);
}

char *
request_respone(pam_handle_t *pamh, int echocode, const char *prompt) 
{
    char *ret = NULL;
    const struct pam_message msg = { 
	         .msg_style = echocode,
                 .msg = prompt, };
    const struct pam_message *msgs = &msg;
    struct pam_response *resp = NULL;
  
    int retval = converse(pamh, 1, &msgs, &resp);

    if (retval != PAM_SUCCESS || resp == NULL || resp->resp == NULL ||
        resp->resp[0] == '\0') {
        if (retval == PAM_SUCCESS && resp && resp->resp) {
            ret = resp->resp;
        }
    } else 
    {
        ret = resp->resp;
    }

    if (resp) {
        if (!ret) {
            free(resp->resp);
    }
        free(resp);
    }
  
    return ret;
}

gboolean 
send_info_msg(pam_handle_t *pamh, const char *msg)
{
    const struct pam_message mymsg = {
            .msg_style = PAM_TEXT_INFO,
            .msg = msg,
    };
    const struct pam_message *msgp = &mymsg;
    const struct pam_conv *pc;
    struct pam_response *resp;
    int r;

    r = pam_get_item(pamh, PAM_CONV, (const void **) &pc);
    if (r != PAM_SUCCESS)
            return FALSE;

    if (!pc || !pc->conv)
            return FALSE;

    return (pc->conv(1, &msgp, &resp, pc->appdata_ptr) == PAM_SUCCESS);
}

gboolean 
send_err_msg(pam_handle_t *pamh, const char *msg)
{
    const struct pam_message mymsg = {
            .msg_style = PAM_ERROR_MSG,
            .msg = msg,
    };
    const struct pam_message *msgp = &mymsg;
    const struct pam_conv *pc;
    struct pam_response *resp;
    int r;

    r = pam_get_item(pamh, PAM_CONV, (const void **) &pc);
    if (r != PAM_SUCCESS)
            return FALSE;

    if (!pc || !pc->conv)
            return FALSE;

    return (pc->conv(1, &msgp, &resp, pc->appdata_ptr) == PAM_SUCCESS);
}

void 
send_debug_msg(pam_handle_t *pamh, const char *msg)
{
    gconstpointer item;
    const char *service;

    if (pam_get_item(pamh, PAM_SERVICE, &item) != PAM_SUCCESS || !item)
            service = "<unknown>";
    else
            service = item;

    openlog (service, LOG_CONS | LOG_PID, LOG_AUTHPRIV);

    syslog (LOG_AUTHPRIV|LOG_WARNING, "%s(%s): %s", "pam_fprintd", service, msg);

    closelog ();

}
