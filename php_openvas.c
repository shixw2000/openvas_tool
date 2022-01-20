#include"php_openvas.h"
#include"task_openvas.h"


typedef int (*PFunc)(const char* msg, int len, kb_buf_t tmpbuf);

struct php_task_handler {
    const char* m_cmd;
    PFunc m_cb;
};

typedef struct php_task_handler* php_task_handler_t;

#define PHP_TASK_HANDLER(x) { #x, php_##x }

static struct php_task_handler g_hds[] = {
    PHP_TASK_HANDLER(start_task),
    PHP_TASK_HANDLER(stop_task),
    PHP_TASK_HANDLER(delete_task),
    PHP_TASK_HANDLER(create_task),

    {NULL, NULL}
};

int openvas_cmd_entry(const char* cmd, int cmdlen, 
    const char* param, int paramlen) {
    int ret = 0;
    const struct php_task_handler* phd = NULL;
    struct kb_buf tmpbuf;

    for (phd=g_hds; NULL != phd->m_cmd; ++phd) {
        if (0 == strcasecmp(phd->m_cmd, cmd)) {

            /* allocate tmp cache */
            ret = genBuf(MAX_CACHE_SIZE, &tmpbuf);
            if (0 == ret) {
                ret = phd->m_cb(param, paramlen, &tmpbuf);
                if (0 == ret) {
                    LOG_INFO( "php_entry| cmd=[%d]%s| param=[%d]%s| msg=deal ok|", 
                        cmdlen, cmd, paramlen, param);
                } else {
                    LOG_ERROR( "php_entry| ret=%d| cmd=[%d]%s| param=[%d]%s| msg=deal failed|",
                        ret, cmdlen, cmd, paramlen, param);
                }

                /* free cache */
                freeBuf(&tmpbuf);
            } else {
                LOG_ERROR( "php_entry| cmd=[%d]%s| param=[%d]%s| error=no memory|",
                    cmdlen, cmd, paramlen, param);
                
                ret = GVM_ERR_INTERNAL_FAIL;
            }

            return ret;
        }
    }

    LOG_ERROR( "php_entry| cmd=[%d]%s| param=[%d]%s| error=invalid params|",
        cmdlen, cmd, paramlen, param);
    return GVM_ERR_PARAM_INVALID;
}


