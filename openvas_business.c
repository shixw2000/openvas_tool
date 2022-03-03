#include<sys/types.h>
#include<sys/stat.h> 
#include<fcntl.h> 
#include<unistd.h> 
#include<stdlib.h>
#include<signal.h> 
#include<sys/file.h> 
#include<string.h>

#include"comm_misc.h"
#include"task_openvas.h"
#include"openvas_business.h"
#include"openvas_opts.h"
#include"hydra_business.h" 


#define REQ_GVM_START_TASK_MARK "<start_task task_id=\"%s\"/>"
#define REQ_GVM_STOP_TASK_MARK "<stop_task task_id=\"%s\"/>"
#define REQ_GVM_DELETE_TASK_MARK "<delete_task task_id=\"%s\"/>"
#define REQ_GVM_DELETE_TARGET_MARK "<delete_target target_id=\"%s\"/>" 
#define REQ_GVM_CREATE_TASK_MARK "<create_task>"\
    "<name>wEb_%s</name><target id=\"%s\"/>"\
    "<config id=\"%s\"/>"\
    "<preferences>"\
    "<preference>"\
    "<scanner_name>auto_delete</scanner_name>"\
    "<value>keep</value>"\
    "</preference>"\
    "<preference>"\
    "<scanner_name>auto_delete_data</scanner_name>"\
    "<value>5</value>"\
    "</preference>"\
    "</preferences>"\
    "</create_task>"
#define REQ_GVM_CREATE_TASK_WITH_SCHEDULE_MARK "<create_task><name>wEb_%s</name><target id=\"%s\"/>"\
    "<config id=\"%s\"/><schedule id=\"%s\"/>"\
    "<preferences>"\
    "<preference>"\
    "<scanner_name>auto_delete</scanner_name>"\
    "<value>keep</value>"\
    "</preference>"\
    "<preference>"\
    "<scanner_name>auto_delete_data</scanner_name>"\
    "<value>5</value>"\
    "</preference>"\
    "</preferences>"\
    "</create_task>"
#define REQ_GVM_CREATE_TARGET_MARK "<create_target><name>wEb_%s</name><hosts>%s</hosts><port_list id=\"%s\"/></create_target>"
#define REQ_GVM_QUERY_REPORT_MARK "<get_reports report_id=\"%s\" details=\"0\"/>"
#define REQ_GVM_QUERY_TASK_MARK "<get_tasks task_id=\"%s\" details=\"0\" usage_type=\"scan\"/>"
#define REQ_GVM_QUERY_RESULT_MARK "<get_results task_id=\"%s\" filter=\"report_id=%s levels=hml first=%d rows=%d\"/>"


#define TASK_FILE_FORMAT TASK_INFO_BEG_MARK\
    "task_id=\"%s\"\n"\
    "task_name=\"%s\"\n"\
    "group_id=\"%s\"\n"\
    "group_name=\"%s\"\n"\
    "config_id=\"%s\"\n"\
    "config_created=\"%d\"\n"\
    "target_id=\"%s\"\n"\
    "portlist_id=\"%s\"\n"\
    "hosts=\"%s\"\n"\
    "schedule_type=\"%d\"\n"\
    "schedule_id=\"%s\"\n"\
    "schedule_created=\"%d\"\n"\
    "schedule_time=\"%s\"\n"\
    "schedule_list=\"%s\"\n"\
    "create_time=\"%s\"\n"\
    TASK_INFO_END_MARK

#define TASK_STATUS_FILE_FORMAT TASK_INFO_BEG_MARK\
    "task_id=\"%s\"\n"\
    "start_time=\"%s\"\n"\
    "stop_time=\"%s\"\n"\
    "status=\"%d\"\n"\
    "chk_type=\"%d\"\n"\
    "progress=\"%d\"\n"\
    "report_id=\"%s\"\n"\
    "last_report_id=\"%s\"\n"\
    "results=\"%s\"\n"\
    TASK_INFO_END_MARK
    
typedef int (*PFunc)(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf);

struct gvm_task_handler {
    const char* m_cmd;
    PFunc m_cb;
};

typedef struct gvm_task_handler* gvm_task_handler_t;


#define GVM_TASK_HANDLER(x) { #x, daemon_##x }

static struct gvm_task_handler g_hds[] = {
    GVM_TASK_HANDLER(start_task),
    GVM_TASK_HANDLER(stop_task),
    GVM_TASK_HANDLER(delete_task),
    GVM_TASK_HANDLER(create_task),

    GVM_TASK_HANDLER(start_hydra),
    GVM_TASK_HANDLER(stop_hydra),
    GVM_TASK_HANDLER(delete_hydra),
    GVM_TASK_HANDLER(create_hydra),

    {NULL, NULL}
};

#if 1
static const char GVM_CMD_MARK[] = "/usr/local/openvas/python/bin/gvm-cli"
    " --timeout 5"
    " --gmp-username admin --gmp-password 123456"
    " tls --xml \"%s\" 2>&1 </dev/null";
#else
static const char GVM_CMD_MARK[] = "/usr/local/openvas/python/bin/gvm-cli"
    " --timeout 5"
    " --gmp-username shixw --gmp-password shixw"
    " tls --hostname 192.168.1.102 --xml \"%s\" 2>&1";

#endif

static volatile int g_isRun = 1;
static GvmDataList_t g_gvm_data = NULL;
static char g_prog_path[MAX_FILENAME_PATH_SIZE] = {0};
static int g_prog_hd = -1;


/* keep alive: 1:ok, 0:empty, -1:error*/
static int recvKbMsg(kb_t kb, const char* key, kb_buf_t output) {
    int ret = 0; 
    
    ret = kb_pop_str(kb, key, output); 
    return ret;
}

static int clearKb(kb_t kb, const char* msg_queue_name) {
    int ret = 0; 
    
    LOG_INFO("clearKb| msg_queue_name=%s| msg=clear the msg queue|", 
        msg_queue_name); 
    
    ret = kb_del_items(kb, msg_queue_name);

    return ret;
}

/* return: 0: ok, -1: error */
static int exeCmd(const kb_buf_t cmdbuf, kb_buf_t output) {
    int ret = 0;
    int left = 0;
    int cnt = 0;
    FILE* file = NULL;

    output->m_buf[0] = '\0';
    output->m_size = 0; 
    
    file = popen(cmdbuf->m_buf, "re");
    if (NULL != file) { 
        left = (int)output->m_capacity;
        
        while (!feof(file) && !ferror(file) && 0 < left) {
            cnt = fread(&output->m_buf[output->m_size], 1, left, file);
            if (0 < cnt) {
                output->m_size += cnt;
                left -= cnt;
            }
        }

        pclose(file);
        output->m_buf[output->m_size] = '\0'; 

        LOG_DEBUG("exeCmd| cmd=[%d][%s]| rsp=[%d][%s]| msg=execute ok|", 
            (int)cmdbuf->m_size, cmdbuf->m_buf,
            (int)output->m_size, output->m_buf);
        
        ret = 0;
    } else {
        LOG_ERROR("exeCmd| cmd=[%d][%s]| msg=popen error[%d]:%s|", 
            (int)cmdbuf->m_size, cmdbuf->m_buf, ERRCODE, ERRMSG); 
        ret = -1;
    } 
    
    return ret;
}

/* return 0:ok, 1: response format err, 2: response not 2**,  -1: error */
int procGvmTask(const char* cmd, kb_buf_t cmdbuf, kb_buf_t output) {
    int ret = 0;
    int left = 0;
    int cnt = 0;

    /* temporary escape xml \" as \\\" */
    left = (int)output->m_capacity;
    output->m_size = escapeXml(cmdbuf->m_buf, output->m_buf, left);
    if (0 == output->m_size) {
        LOG_ERROR("proc_gvm| maxlen=%d| req=[%s]|"
            " error=escapeXml error|", 
            left, cmdbuf->m_buf);

        return -1;
    } 

    left = (int)cmdbuf->m_capacity;
    cnt = snprintf(cmdbuf->m_buf, left, GVM_CMD_MARK, output->m_buf);
    if (0 < cnt && cnt < left) {
        cmdbuf->m_size = cnt; 

        ret = exeCmd(cmdbuf, output);
        if (0 == ret) { 
            /* check valid cmd response */
            ret = chkRspStatusOk(cmd, output->m_buf);
            if (0 == ret) { 
                /* response ok */
            } else {
                LOG_ERROR("proc_gvm| ret=%d| req=[%d]%s| rsp_msg=[%d]%s|"
                    " error=response isnot ok|", 
                    ret,
                    (int)cmdbuf->m_size, cmdbuf->m_buf,
                    (int)output->m_size, output->m_buf);
            }
        } 
    } else { 
        LOG_ERROR("proc_gvm| req_len=%d| maxlen=%d| req=[%s]|"
            " error=xml length exceeds max|", 
            cnt, left, output->m_buf);

        output->m_size = 0;
        output->m_buf[0] = '\0';
        
        /* clear input buffer */
        cmdbuf->m_size = 0;
        cmdbuf->m_buf[0] = '\0';
        ret = -1;
    } 
    
    return ret;
}

static int processMsg(kb_t kb, const kb_buf_t inbuf, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;
    int size = 0;
    int isFound = 0;
    const struct gvm_task_handler* phd = NULL;
    const char* saveptr = NULL;
    char cmd[MAX_COMM_SIZE] = {0};
    char uuid[MAX_UUID_SIZE] = {0}; 

    saveptr = inbuf->m_buf; 
    ret = getNextToken(saveptr, OPENVAS_KB_DELIM, uuid,
        ARR_SIZE(uuid), &saveptr);
    if (0 != ret) {
        LOG_ERROR("%s| input=[%d]%s| msg=parse uuid error|",
            __FUNCTION__, (int)inbuf->m_size, inbuf->m_buf);

        return GVM_ERR_PARAM_INVALID;
    }
    
    ret = getNextToken(saveptr, OPENVAS_KB_DELIM, cmd,
        ARR_SIZE(cmd), &saveptr);
    if (0 != ret) {
        LOG_ERROR("%s| input=[%d]%s| msg=parse cmd error|",
            __FUNCTION__, (int)inbuf->m_size, inbuf->m_buf);

        return GVM_ERR_PARAM_INVALID;
    } 

    ret = chkUuid(uuid);
    if (0 != ret) {
        LOG_ERROR("%s| input=[%d]%s| msg=chk uuid error|",
            __FUNCTION__, (int)inbuf->m_size, inbuf->m_buf);

        return GVM_ERR_PARAM_INVALID;
    }

    for (phd=g_hds; NULL != phd->m_cmd; ++phd) {
        
        if (0 == strncmp(phd->m_cmd, cmd, ARR_SIZE(cmd))) {
            isFound = 1;
            
            size = strnlen(saveptr, inbuf->m_capacity); 
            ret = phd->m_cb((char*)saveptr, size, tmpbuf, outbuf); 
            break;
        }
    }

    if (isFound) {
        if (0 == ret) {
            LOG_DEBUG("%s| cmd=%s| msg=ok| input=[%d]%s", __FUNCTION__, 
                phd->m_cmd, (int)inbuf->m_size, inbuf->m_buf);
        } else {
            LOG_ERROR("%s| cmd=%s| error=%d| input=[%d]%s", __FUNCTION__, 
                phd->m_cmd, ret,
                (int)inbuf->m_size, inbuf->m_buf);
        }
    } else { 
        LOG_ERROR("%s| invalid cmd| input=[%d]%s|", __FUNCTION__, 
            (int)inbuf->m_size, inbuf->m_buf);
        
        ret = GVM_ERR_PARAM_INVALID;
    } 
    
    /* send dealed result to requet task */
    kb_push_result_ttl(kb, uuid, ret, MAX_REDIS_WAIT_TIMEOUT);
    return ret;
}

/* 1: busy, 0: idle, -1: error */
static int processIdle(kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int hasMore = 0;
    
    hasMore = runGvmChecks(g_gvm_data, tmpbuf, outbuf);
    
    return hasMore;
}

static int clearLogFile(int nday) {
    int ret = 0;
    long long time = 0;
    char path[MAX_FILENAME_PATH_SIZE] = {0};

    time = getTimeLastDays(nday);
    ret = getLogPath(&time, path, ARR_SIZE(path));
    if (0 == ret) {
        deleteFile(path);
    }

    return ret;
}

/* this is not an accurate timer,
    but is like a rough timeout timer triggered every minute.
*/ 
static void timerPerMinute(kb_buf_t tmpbuf, kb_buf_t outbuf) {
    static long long g_min_cnt = 0;

    ++g_min_cnt;

    /* you can define your own timer work here by g_min_cnt % n */

    /* every 512(0x200) minutes(~8h), to check and clear the last 7 day log file */
    if (0 == (g_min_cnt & 0x1FF)) {
        clearLogFile(7);
    } 

    validateGvmConn(g_gvm_data, tmpbuf, outbuf);
}
 
static void triggerTimerPerMinute(kb_buf_t tmpbuf, kb_buf_t outbuf) {
    static const long long DEF_MINUTE_INTERAL_SEC = 60;
    static long long g_last_time = 0;
    long long now = getClkTime();

    if (g_last_time + DEF_MINUTE_INTERAL_SEC <= now) {
        timerPerMinute(tmpbuf, outbuf);
        
        g_last_time = getClkTime();
    }
}

static void doEveryLoop(kb_buf_t tmpbuf, kb_buf_t outbuf) {
    monitorHydraTask();
    
    triggerTimerPerMinute(tmpbuf, outbuf);
}

int gvm_start_task(const char* uuid, char report_id[], int maxlen, 
    kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;

    do {
        ret = chkUuid(uuid);
        if (0 != ret) {
            LOG_ERROR("start_task| task_id=%s| msg=invalid taskid|", uuid);
            break;
        }

        tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity, 
            REQ_GVM_START_TASK_MARK, uuid);
        ret = procGvmTask("start_task", tmpbuf, outbuf); 
        if (0 == ret) {
            ret = extractTagUuid(outbuf->m_buf, "report_id", report_id, maxlen);
            if (0 == ret) {
                LOG_INFO("start_task| task_id=%s| report_id=%s| msg=ok|",
                    uuid, report_id);
            } else {
                LOG_ERROR("start_task| task_id=%s| msg=cannot found a report id|", uuid);
                break;
            }
        } else {
            LOG_ERROR("start_task| task_id=%s| msg=gvm process failed|", uuid);
            break;
        }

        return 0;
    } while (0);

    return -1;
}

int gvm_stop_task(const char* uuid, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;

    do {
        ret = chkUuid(uuid);
        if (0 != ret) {
            LOG_ERROR("stop_task| task_id=%s| msg=invalid taskid|", uuid);
            break;
        }

        tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity, 
            REQ_GVM_STOP_TASK_MARK, uuid);
        ret = procGvmTask("stop_task", tmpbuf, outbuf); 
        if (0 == ret) {
            LOG_INFO("stop_task| task_id=%s| msg=ok|", uuid);
        } else {
            LOG_ERROR("stop_task| task_id=%s| msg=gvm process failed|", uuid);
            break;
        }

        return 0;
    } while (0);

    return -1;
}

int gvm_query_report(const char* uuid, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;

    do {
        ret = chkUuid(uuid);
        if (0 != ret) {
            LOG_ERROR("query_report| report_id=%s| msg=invalid reportid|", uuid);
            break;
        }

        tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity,
            REQ_GVM_QUERY_REPORT_MARK, uuid);
        ret = procGvmTask("get_reports", tmpbuf, outbuf);  
        if (0 == ret) {
            LOG_DEBUG("query_report| report_id=%s| msg=ok|", uuid);
        } else {
            LOG_ERROR("query_report| report_id=%s| msg=gvm process failed|", uuid);
            break;
        }

        return 0;
    } while (0);

    return -1;
}

int gvm_query_task(const char* uuid, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;

    do {
        ret = chkUuid(uuid);
        if (0 != ret) {
            LOG_ERROR("query_task| task_id=%s| msg=invalid taskid|", uuid);
            break;
        }

        tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity, 
            REQ_GVM_QUERY_TASK_MARK, uuid);
        ret = procGvmTask("get_tasks", tmpbuf, outbuf); 
        if (0 == ret) {
            LOG_DEBUG("query_task| task_id=%s| msg=ok|", uuid);
        } else {
            LOG_ERROR("query_task| task_id=%s| msg=gvm process failed|", uuid);
            break;
        }

        return 0;
    } while (0);

    return -1;
}

int gvm_query_result(const char* taskid, const char* reportid, 
    int first, int rows,
    kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;

    do {
        ret = chkUuid(taskid);
        if (0 != ret) {
            LOG_ERROR("query_result| task_id=%s| msg=invalid taskid|", taskid);
            break;
        }

        ret = chkUuid(reportid);
        if (0 != ret) {
            LOG_ERROR("query_result| report_id=%s| msg=invalid reportid|", reportid);
            break;
        }

        tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity, 
            REQ_GVM_QUERY_RESULT_MARK, 
            taskid, reportid, first, rows);
        ret = procGvmTask("get_results", tmpbuf, outbuf); 
        if (0 == ret) {
            LOG_DEBUG("query_result| task_id=%s| report_id=%s| msg=ok|", 
                taskid, reportid);
        } else {
            LOG_ERROR("query_result| task_id=%s| report_id=%s|"
                " msg=gvm process failed|", 
                taskid, reportid);
            break;
        }

        return 0;
    } while (0);

    return -1;
}


int gvm_option_delete_task(const char* uuid, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;

    if (NULL == uuid || '\0' == uuid[0]) {
        return 0;
    }

    do {
        ret = chkUuid(uuid);
        if (0 != ret) {
            LOG_ERROR("delete_task| task_id=%s| msg=invalid taskid|", uuid);
            break;
        }

        tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity, 
            REQ_GVM_DELETE_TASK_MARK, uuid);
        ret = procGvmTask("delete_task", tmpbuf, outbuf);  
        if (0 == ret) {
            LOG_INFO("delete_task| task_id=%s| msg=ok|", uuid);
        } else {
            LOG_ERROR("delete_task| task_id=%s| msg=gvm process failed|", uuid);
            break;
        }

        return 0;
    } while (0);

    return -1;
}

int gvm_option_delete_target(const char* uuid, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;

    if (NULL == uuid || '\0' == uuid[0]) {
        return 0;
    }

    do {
        ret = chkUuid(uuid);
        if (0 != ret) {
            LOG_ERROR("delete_target| target_id=%s| msg=invalid target_id|", uuid);
            break;
        }

        tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity,
            REQ_GVM_DELETE_TARGET_MARK, uuid);
        ret = procGvmTask("delete_target", tmpbuf, outbuf); 
        if (0 == ret) {
            LOG_INFO("delete_target| target_id=%s| msg=ok|", uuid);
        } else {
            LOG_ERROR("delete_target| target_id=%s| msg=gvm process failed|", uuid);
            break;
        }

        return 0;
    } while (0);

    return -1;
}

int gvm_create_target(const char* name, const char* hosts, 
    const char* portlist, char target_id[], int maxlen,
    kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;

    do {
        ret = chkUuid(portlist);
        if (0 != ret) {
            LOG_ERROR("create_target| name=%s| port_list=%s| hosts=%s|"
                " msg=invalid portlist|", 
                name, portlist, hosts);
            break;
        }

        ret = chkHosts(hosts);
        if (0 != ret) {
            LOG_ERROR("create_target| name=%s| port_list=%s| hosts=%s|"
                " msg=invalid hosts|", 
                name, portlist, hosts);
            break;
        }

        ret = chkName(name);
        if (0 != ret) {
            LOG_ERROR("create_target| name=%s| port_list=%s| hosts=%s|"
                " msg=invalid name|", 
                name, portlist, hosts);
            break;
        }

        tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity, 
            REQ_GVM_CREATE_TARGET_MARK, 
            name, hosts, portlist);
        ret = procGvmTask("create_target", tmpbuf, outbuf); 
        if (0 == ret) {
            ret = extractAttrUuid(outbuf->m_buf, "create_target_response", 
                target_id, maxlen);
            if (0 == ret) {
                LOG_INFO("create_target| target_id=%s| name=%s|"
                    " port_list=%s| hosts=%s| msg=ok|",
                    target_id, name, portlist, hosts);
            } else {
                LOG_ERROR("create_target| name=%s| port_list=%s| hosts=%s|"
                    " msg=cannot found a target id|", 
                    name, portlist, hosts);
                break;
            }
        } else {
            LOG_ERROR("create_target| name=%s| port_list=%s| hosts=%s|"
                " msg=gvm process failed|", 
                name, portlist, hosts);
            break;
        }

        return 0;
    } while (0);

    return -1;
}

int gvm_create_task(const char* name, const char* target,
    const char* config, const char* schedule,
    char task_id[], int maxlen,
    kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;

    do {
        ret = chkUuid(target);
        if (0 != ret) {
            LOG_ERROR("create_task| name=%s| target_id=%s| config_id=%s|"
                " msg=invalid target|", 
                name, target, config);
            break;
        }

        ret = chkUuid(config);
        if (0 != ret) {
            LOG_ERROR("create_task| name=%s| target_id=%s| config_id=%s|"
                " msg=invalid config|", 
                name, target, config);
            break;
        }

        ret = chkName(name);
        if (0 != ret) {
            LOG_ERROR("create_task| name=%s| target_id=%s| config_id=%s|"
                " msg=invalid name|", 
                name, target, config);
            break;
        }

        if ('\0' == schedule[0]) {
            tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity, 
                REQ_GVM_CREATE_TASK_MARK, 
                name, target, config);
        } else {
            tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity, 
                REQ_GVM_CREATE_TASK_WITH_SCHEDULE_MARK, 
                name, target, config, schedule);
        }
        
        ret = procGvmTask("create_task", tmpbuf, outbuf);
        if (0 == ret) {
            ret = extractAttrUuid(outbuf->m_buf, "create_task_response", 
                task_id, maxlen);
            if (0 == ret) {
                LOG_INFO("create_task| task_id=%s|"
                    " name=%s| target_id=%s| config_id=%s| msg=ok|",
                    task_id, name, target, config);
            } else {
                LOG_ERROR("create_task| name=%s| target_id=%s| config_id=%s|"
                    " msg=cannot found a target id|", 
                    name, target, config);
                break;
            }
        } else {
            LOG_ERROR("create_task| name=%s| target_id=%s| config_id=%s|"
                " msg=gvm process failed|", 
                name, target, config);
            break;
        }
        
        return 0;
    } while (0);

    return -1;
}

int stopKbMsg() {
    g_isRun = 0;
    return 0;
}

/* keep alive */
int startKbMsg(const char* msg_queue_name) {
    int ret = 0;
    int hasMore = 0;
    int timeout = 0;
    kb_t kb = NULL; // used by recv for keep alive 
    struct kb_buf inbuf = {0, 0, NULL};
    struct kb_buf tmpbuf = {0, 0, NULL};
    struct kb_buf outbuf = {0, 0, NULL};
    static int g_id = 0;
    static const int INTERVAL_TIME[] = {10, 20, 30, 60};
    static const int MAX_INTERVAL_CNT = ARR_SIZE(INTERVAL_TIME) -1;

    genBuf(MAX_CACHE_SIZE, &inbuf);
    genBuf(MAX_CACHE_SIZE, &tmpbuf);
    genBuf(MAX_CACHE_SIZE, &outbuf);

    kb = kb_create();
    if (NULL == kb) {
        return -1;
    }
    
    /* first clear old data */
    clearKb(kb, msg_queue_name); 
    
    while (g_isRun) {
        if (0 >= timeout) {
            /* the msg queue has the first priority to deal with */
            ret = recvKbMsg(kb, msg_queue_name, &inbuf);

            /* empty queue */
            if (0 == ret) { 
                if (0 != g_id) { 
                    g_id = 0;
                } 
                
                hasMore = processIdle(&tmpbuf, &outbuf);

                /*if idle, then sleep 1sec */
                timeout = !hasMore;
            } else if (0 < ret) {
                if (0 == g_id) {
                } else if (MAX_INTERVAL_CNT > g_id) {
                    g_id = 0;
                } else {
                    /* more than 60s kb dead, delete old recv queue */
                    clearKb(kb, msg_queue_name);
                    g_id = 0;

                    continue;
                }
                
                /* work here */
                LOG_DEBUG("msg_queue_name=%s| msg=[%d]%s|", msg_queue_name, 
                    (int)inbuf.m_size, inbuf.m_buf);
                
                (void)processMsg(kb, &inbuf, &tmpbuf, &outbuf);
            } else {
                /* conn error, then sleep n seconds */
                timeout = INTERVAL_TIME[g_id];
                if (g_id < MAX_INTERVAL_CNT) {
                    ++g_id;
                }
            }
        } else {
            sleep(1);
            --timeout;
        }

        if (g_isRun) {
            doEveryLoop(&tmpbuf, &outbuf);
        }
    }

    kb_delete(kb);
    
    freeBuf(&inbuf);
    freeBuf(&tmpbuf);
    freeBuf(&outbuf);
    return 0;
}


/* compare two gvm task info by task name */
static int cmpGvmName(const ListGvmTask_t task1, const ListGvmTask_t task2) {
    int ret = 0;

    ret = strncmp(task1->m_task_info.m_task_name, 
        task2->m_task_info.m_task_name, 
        ARR_SIZE(task1->m_task_info.m_task_name));
    return ret;
}

/* compare two gvm task info by task name */
static int cmpGvmCreateTime(const ListGvmTask_t task1, const ListGvmTask_t task2) {
    int ret = 0;

    ret = strncmp(task1->m_task_info.m_create_time, 
        task2->m_task_info.m_create_time, 
        ARR_SIZE(task1->m_task_info.m_create_time));
    return ret;
}

/* compare two gvm task info by task name */
static int cmpGvmRunChkTime(const LList_t o1, const LList_t o2) {
    int ret = 0;
    const ListGvmTask_t task1 = runlist_gvm_task(o1);
    const ListGvmTask_t task2 = runlist_gvm_task(o2);

    ret = task1->m_chk_time - task2->m_chk_time;
    return ret;
} 


int daemon_start_task(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;
    ListGvmTask_t key = NULL;
    ListGvmTask_t task = NULL;
    char report_id[MAX_UUID_SIZE] = {0};

    key = g_gvm_data->task_ops->create_task();
    if (NULL == key) {
        LOG_ERROR("daemon_start_task| error=no memory|");
        return GVM_ERR_INTERNAL_FAIL;
    }

    do {
        ret = getKeyTaskParam(input, key);
        if (0 != ret) {
            ret = GVM_ERR_PARAM_INVALID;
            break;
        }

        if (!g_gvm_data->task_ops->is_gvm_conn_ok(g_gvm_data)) {
            ret = GVM_ERR_GVMD_CONN;
            break;
        }

        task = g_gvm_data->task_ops->find_task(g_gvm_data, key);
        if (NULL == task) {
            LOG_ERROR("daemon_start_task| name=%s| task_id=%s| target_id=%s|"
                " msg=the task donot exist|",
                key->m_task_info.m_task_name,
                key->m_task_info.m_task_id,
                key->m_target_info.m_target_id);
            
            ret = GVM_ERR_TASK_NOT_FOUND;
            break;
        }

        /* can only start the not-running task */
        if (g_gvm_data->task_ops->chk_task_busy(g_gvm_data, task)) {
            LOG_ERROR("daemon_start_task| name=%s| task_id=%s| target_id=%s|"
                " msg=the task is busy now, please wait to end|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id,
                task->m_target_info.m_target_id);
            
            ret = GVM_ERR_TASK_IS_BUSY;
            break;
        }

        ret = gvm_start_task(task->m_task_info.m_task_id, 
            report_id, MAX_UUID_SIZE, tmpbuf, outbuf);
        if (0 == ret) {
            LOG_INFO("daemon_start_task| name=%s| task_id=%s| target_id=%s|"
                " new_report_id=%s| last_report_id=%s| msg=start task ok|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id,
                task->m_target_info.m_target_id,
                report_id, 
                task->m_report_info.m_cur_report_id); 
        } else {
            LOG_ERROR("daemon_start_task| name=%s| task_id=%s| target_id=%s|"
                " msg=gvm process error|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id,
                task->m_target_info.m_target_id);

            ret = GVM_ERR_OPENVAS_PROC_FAIL;
            break;
        }

        /* start task ok, notify to start cache */
        setTaskNextChkTime(task, 0);
        g_gvm_data->task_ops->reque_run_task(g_gvm_data, task);

    } while (0);

    g_gvm_data->task_ops->free_task(key);

    return ret;
}

int daemon_stop_task(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;
    ListGvmTask_t key = NULL;
    ListGvmTask_t task = NULL;

    key = g_gvm_data->task_ops->create_task();
    if (NULL == key) {
        LOG_ERROR("daemon_start_task| error=no memory|");
        return GVM_ERR_INTERNAL_FAIL;
    }

    do {
        ret = getKeyTaskParam(input, key);
        if (0 != ret) {
            ret = GVM_ERR_PARAM_INVALID;
            break;
        }

        if (!g_gvm_data->task_ops->is_gvm_conn_ok(g_gvm_data)) {
            ret = GVM_ERR_GVMD_CONN;
            break;
        }

        task = g_gvm_data->task_ops->find_task(g_gvm_data, key);
        if (NULL == task) {
            LOG_ERROR("daemon_stop_task| name=%s| task_id=%s| target_id=%s|"
                " msg=the task donot exist|",
                key->m_task_info.m_task_name,
                key->m_task_info.m_task_id,
                key->m_target_info.m_target_id);
            
            ret = GVM_ERR_TASK_NOT_FOUND;
            break;
        }

        /* can only stop running task */
        if (!g_gvm_data->task_ops->chk_task_running(g_gvm_data, task)) {
            LOG_ERROR("daemon_stop_task| name=%s| task_id=%s| target_id=%s|"
                " msg=the task is not running now|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id,
                task->m_target_info.m_target_id);
            
            ret = GVM_ERR_TASK_IS_NOT_RUNNING;
            break;
        }
        
        ret = gvm_stop_task(task->m_task_info.m_task_id, tmpbuf, outbuf);
        if (0 == ret) {
            LOG_INFO("daemon_stop_task| name=%s| task_id=%s| target_id=%s|"
                " report_id=%s| msg=stop task ok|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id,
                task->m_target_info.m_target_id,
                task->m_report_info.m_cur_report_id);
        } else {
            LOG_ERROR("daemon_stop_task| name=%s| task_id=%s| target_id=%s|"
                " msg=gvm process error|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id,
                task->m_target_info.m_target_id);
            
            ret = GVM_ERR_OPENVAS_PROC_FAIL;
            break;
        }

        setTaskNextChkTime(task, 0);
        g_gvm_data->task_ops->reque_run_task(g_gvm_data, task);
    } while (0);

    g_gvm_data->task_ops->free_task(key); 
    return ret;
}

int getKeyTaskParam(const char* input, ListGvmTask_t task) {
    int ret = 0;
    const char* saveptr = NULL;

    do {
        saveptr = input;

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_task_info.m_task_name,
            ARR_SIZE(task->m_task_info.m_task_name), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid task name|",
                __FUNCTION__, input);
            break;
        }

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_task_info.m_task_id,
            ARR_SIZE(task->m_task_info.m_task_id), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid task id|",
                __FUNCTION__, input);
            break;
        }

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_target_info.m_target_id,
            ARR_SIZE(task->m_target_info.m_target_id), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid target id|",
                __FUNCTION__, input);
            break;
        } 

        ret = chkName(task->m_task_info.m_task_name);
        if (0 != ret) {
            LOG_ERROR( "%s: check parameter| task_name=%s| msg=invalid task name|",
                __FUNCTION__, task->m_task_info.m_task_name);
            break;
        }

        /* target name is the same as task name */
        strncpy(task->m_target_info.m_target_name,
            task->m_task_info.m_task_name,
            ARR_SIZE(task->m_target_info.m_target_name)); 

        ret = chkUuid(task->m_task_info.m_task_id);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| task_id=%s| msg=invalid uuid|",
                __FUNCTION__, input, task->m_task_info.m_task_id);
            
            break;
        } 

        ret = chkUuid(task->m_target_info.m_target_id);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| target_id=%s| msg=invalid uuid|",
                __FUNCTION__, input, task->m_target_info.m_target_id);
            
            break;
        }

        return 0;
    } while (0);

    return ret;
}

int daemon_delete_task(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;
    ListGvmTask_t key = NULL;
    ListGvmTask_t task = NULL;

    key = g_gvm_data->task_ops->create_task();
    if (NULL == key) {
        LOG_ERROR("daemon_start_task| error=no memory|");
        return GVM_ERR_INTERNAL_FAIL;
    }

    do {
        ret = getKeyTaskParam(input, key);
        if (0 != ret) {
            ret = GVM_ERR_PARAM_INVALID;
            break;
        }

        if (!g_gvm_data->task_ops->is_gvm_conn_ok(g_gvm_data)) {
            ret = GVM_ERR_GVMD_CONN;
            break;
        }

        task = g_gvm_data->task_ops->find_task(g_gvm_data, key);
        if (NULL == task) {
            LOG_ERROR("daemon_del_task| name=%s| task_id=%s| target_id=%s|"
                " msg=the task donot exist|",
                key->m_task_info.m_task_name,
                key->m_task_info.m_task_id,
                key->m_target_info.m_target_id);
            
            ret = GVM_ERR_TASK_NOT_FOUND;
            break;
        }

        /* can only delete the not-busy task */
        if (g_gvm_data->task_ops->chk_task_busy(g_gvm_data, task)) {
            LOG_ERROR("daemon_del_task| name=%s| task_id=%s| target_id=%s|"
                " task_status=%d| chk_type=%d|"
                " error=the task is busy now, please stop it first|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id,
                task->m_target_info.m_target_id,
                getTaskStatus(task),
                getTaskChkType(task));
            
            ret = GVM_ERR_TASK_IS_BUSY;
            break;
        }

        ret = gvm_option_delete_task(task->m_task_info.m_task_id, tmpbuf, outbuf);
        if (0 != ret) {
            LOG_ERROR("daemon_del_task| name=%s| task_id=%s| target_id=%s|"
                " msg=del gvm task error|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id,
                task->m_target_info.m_target_id);

            ret = GVM_ERR_OPENVAS_PROC_FAIL; 
            break;
        } 

        ret = gvm_option_delete_target(task->m_target_info.m_target_id, tmpbuf, outbuf);
        if (0 == ret) {
            LOG_INFO("daemon_del_task| name=%s| task_id=%s| target_id=%s|"
                " msg=del gvm target ok|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id,
                task->m_target_info.m_target_id);
        } else {
            LOG_ERROR("daemon_del_task| name=%s| task_id=%s| target_id=%s|"
                " msg=del gvm task ok, but del target error|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id,
                task->m_target_info.m_target_id);

            ret = 0;
        } 

        /* if this schedule is created, then delete it */ 
        ret = gvm_option_delete_schedule(&task->m_task_info, tmpbuf, outbuf);
        if (0 == ret) {
            LOG_INFO("daemon_del_task| name=%s| task_id=%s| target_id=%s|"
                " msg=del gvm schedule ok|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id,
                task->m_target_info.m_target_id);
        } else {
            LOG_ERROR("daemon_del_task| name=%s| task_id=%s| target_id=%s|"
                " msg=del gvm task and target ok, but del schedule error|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id,
                task->m_target_info.m_target_id);

            ret = 0;
        } 

        /* if this config is created, then delete it */ 
        ret = gvm_option_delete_config(&task->m_task_info, tmpbuf, outbuf);
        if (0 == ret) {
            LOG_INFO("daemon_del_task| name=%s| task_id=%s| target_id=%s|"
                " msg=del gvm config ok|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id,
                task->m_target_info.m_target_id);
        } else {
            LOG_ERROR("daemon_del_task| name=%s| task_id=%s| target_id=%s|"
                " msg=del gvm task and target ok, but del config error|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id,
                task->m_target_info.m_target_id);

            ret = 0;
        } 

        /* delete cache and files */ 
        ret = g_gvm_data->task_ops->del_task_relation_files(g_gvm_data, task, tmpbuf);
        if (0 == ret) { 
            LOG_INFO("daemon_del_task| name=%s| task_id=%s| target_id=%s|"
                " msg=del task relation files ok|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id,
                task->m_target_info.m_target_id);
            
            /* memory free */
            g_gvm_data->task_ops->free_task(task); 
        } else {
            LOG_ERROR("daemon_del_task| name=%s| task_id=%s| target_id=%s|"
                " msg=delete task relation files error here, and tries again background|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id,
                task->m_target_info.m_target_id);
                
            setTaskChkType(task, GVM_TASK_CHK_DELETED);
            
            ret = 0;
        } 
    } while (0);

    g_gvm_data->task_ops->free_task(key);

    return ret;
}

static int getCreateTaskParam(const char* input, ListGvmTask_t task) {
    int ret = 0;
    int n = 0;
    const char* saveptr = NULL;
    
    do {
        saveptr = input;

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_task_info.m_task_name,
            ARR_SIZE(task->m_task_info.m_task_name), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid task name|",
                __FUNCTION__, input);
            break;
        }
        
        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_task_info.m_group_id,
            ARR_SIZE(task->m_task_info.m_group_id), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid group id|",
                __FUNCTION__, input);
            break;
        }

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_task_info.m_group_name,
            ARR_SIZE(task->m_task_info.m_group_name), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid group name|",
                __FUNCTION__, input);
            break;
        }

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_target_info.m_hosts, 
            ARR_SIZE(task->m_target_info.m_hosts), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid hosts|",
                __FUNCTION__, input);
            break;
        }

        ret = getNextTokenInt(saveptr, OPENVAS_KB_DELIM, &n, &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid _schedule_type|",
                __FUNCTION__, input);
            break;
        }

        task->m_task_info.m_schedule_type = (enum ICAL_DATE_REP_TYPE)n; 

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, 
            task->m_task_info.m_first_schedule_time, 
            ARR_SIZE(task->m_task_info.m_first_schedule_time), 
            &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid first_schedule_time|",
                __FUNCTION__, input);
            break;
        }

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, 
            task->m_task_info.m_schedule_list, 
            ARR_SIZE(task->m_task_info.m_schedule_list), 
            &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid schedule_list|",
                __FUNCTION__, input);
            break;
        }

        ret = chkName(task->m_task_info.m_task_name);
        if (0 != ret) {
            LOG_ERROR( "%s: check parameter| task_name=%s| msg=invalid task name|",
                __FUNCTION__, task->m_task_info.m_task_name);
            break;
        }

        /* target name is the same as task name */
        strncpy(task->m_target_info.m_target_name,
            task->m_task_info.m_task_name,
            ARR_SIZE(task->m_target_info.m_target_name));

        ret = chkConfigInfo(task->m_task_info.m_group_id,
            task->m_task_info.m_group_name);
        if (0 != ret) {
            LOG_ERROR("%s: check parameter| text=%s| group_id=%s|"
                " group_name=%s| msg=invalid groups|",
                __FUNCTION__, input, 
                task->m_task_info.m_group_id,
                task->m_task_info.m_group_name);
            
            break;
        } 

        ret = chkHosts(task->m_target_info.m_hosts);
        if (0 != ret) {
            LOG_ERROR( "%s: check parameter| hosts=%s| msg=invalid hosts",
                __FUNCTION__, task->m_target_info.m_hosts);
            break;
        } 

        ret = chkScheduleParam(task->m_task_info.m_schedule_type, 
            task->m_task_info.m_first_schedule_time, 
            task->m_task_info.m_schedule_list);
        if (0 != ret) {
            LOG_ERROR( "php_create_task| schedul_type=%d| schedul_time=%s|"
                " schedul_list=%s| msg=invalid schedule parameters|", 
                task->m_task_info.m_schedule_type,
                task->m_task_info.m_first_schedule_time, 
                task->m_task_info.m_schedule_list);
            break;
        }

        /* use the default port list */
        strncpy(task->m_target_info.m_portlist_id, 
            DEF_OPENVAS_PORT_LIST_ID, 
            ARR_SIZE(task->m_target_info.m_portlist_id));

        return 0;
    } while (0);

    return ret;
}

int daemon_create_task(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;
    ListGvmTask_t oldtask = NULL;
    ListGvmTask_t task = NULL;

    task = g_gvm_data->task_ops->create_task();
    if (NULL == task) {
        LOG_ERROR("daemon_start_task| error=no memory|");
        return GVM_ERR_INTERNAL_FAIL;
    }

    do {
        ret = getCreateTaskParam(input, task);
        if (0 != ret) {
            ret = GVM_ERR_PARAM_INVALID;
            break;
        }

        if (!g_gvm_data->task_ops->is_gvm_conn_ok(g_gvm_data)) {
            ret = GVM_ERR_GVMD_CONN;
            break;
        }
        
        /* check if task exists */
        oldtask = g_gvm_data->task_ops->find_task(g_gvm_data, task);
        if (NULL != oldtask) {
            LOG_ERROR("daemon_create_task| name=%s| group_name=%s| hosts=%s|"
                " msg=the task already exists|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_group_name,
                task->m_target_info.m_hosts);
  
            ret = GVM_ERR_TASK_ALREADY_EXISTS;
            break;
        }

        ret = gvm_create_target(task->m_target_info.m_target_name, 
            task->m_target_info.m_hosts, 
            task->m_target_info.m_portlist_id, 
            task->m_target_info.m_target_id, 
            ARR_SIZE(task->m_target_info.m_target_id), 
            tmpbuf, outbuf);
        if (0 != ret) {
            LOG_ERROR("daemon_create_task| name=%s| hosts=%s| portlist=%s| config_id=%s|"
                " target_id=%s| schedule_id=%s|"
                " msg=create target fail|",
                task->m_task_info.m_task_name,
                task->m_target_info.m_hosts, 
                task->m_target_info.m_portlist_id,
                task->m_task_info.m_config_id, 
                task->m_target_info.m_target_id,
                task->m_task_info.m_schedule_id);

            ret = GVM_ERR_OPENVAS_PROC_FAIL;
            break;
        }

        ret = gvm_option_create_schedule(&task->m_task_info, tmpbuf, outbuf); 
        if (0 != ret) {
            LOG_ERROR("daemon_create_task| name=%s| hosts=%s| portlist=%s| config_id=%s|"
                " target_id=%s| schedule_id=%s|"
                " msg=create schedule fail|",
                task->m_task_info.m_task_name,
                task->m_target_info.m_hosts, 
                task->m_target_info.m_portlist_id,
                task->m_task_info.m_config_id, 
                task->m_target_info.m_target_id,
                task->m_task_info.m_schedule_id);

            ret = GVM_ERR_OPENVAS_PROC_FAIL;
            break;
        } 

        ret = gvm_option_create_config(&task->m_task_info, tmpbuf, outbuf);
        if (0 != ret) {
            LOG_ERROR("daemon_create_task| name=%s| hosts=%s| portlist=%s| config_id=%s|"
                " target_id=%s| schedule_id=%s|"
                " msg=create config fail|",
                task->m_task_info.m_task_name,
                task->m_target_info.m_hosts, 
                task->m_target_info.m_portlist_id,
                task->m_task_info.m_config_id, 
                task->m_target_info.m_target_id,
                task->m_task_info.m_schedule_id);

            ret = GVM_ERR_OPENVAS_PROC_FAIL;
            break;
        }
        
        ret = gvm_create_task(task->m_task_info.m_task_name,
            task->m_target_info.m_target_id,
            task->m_task_info.m_config_id,
            task->m_task_info.m_schedule_id,
            task->m_task_info.m_task_id, 
            ARR_SIZE(task->m_task_info.m_task_id), 
            tmpbuf, outbuf);
        if (0 == ret) {
            LOG_INFO("daemon_create_task| task_id=%s| name=%s| hosts=%s| portlist=%s|"
                " config_id=%s| target_id=%s| schedule_id=%s|"
                " msg=create task ok|",
                task->m_task_info.m_task_id,
                task->m_task_info.m_task_name,
                task->m_target_info.m_hosts, 
                task->m_target_info.m_portlist_id,
                task->m_task_info.m_config_id, 
                task->m_target_info.m_target_id,
                task->m_task_info.m_schedule_id);
        } else {
            LOG_ERROR("daemon_create_task| name=%s| hosts=%s| portlist=%s| config_id=%s|"
                " target_id=%s| schedule_id=%s|"
                " msg=create task fail|",
                task->m_task_info.m_task_name,
                task->m_target_info.m_hosts, 
                task->m_target_info.m_portlist_id,
                task->m_task_info.m_config_id, 
                task->m_target_info.m_target_id,
                task->m_task_info.m_schedule_id);

            ret = GVM_ERR_OPENVAS_PROC_FAIL;
            break;
        }

        /* create task ok, record to file */
        nowTimeStamp(task->m_task_info.m_create_time, 
            ARR_SIZE(task->m_task_info.m_create_time));

        strncpy(task->m_task_info.m_modify_time, 
            task->m_task_info.m_create_time,
            ARR_SIZE(task->m_task_info.m_modify_time));
       
        setTaskStatus(task, GVM_TASK_CREATE);
        setTaskChkType(task, GVM_TASK_CHK_TASK);
        setTaskNextChkTime(task, DEF_TASK_CHK_TIME_INTERVAL[GVM_TASK_CHK_TASK]);
         
        ret = g_gvm_data->task_ops->write_task_relation_files(g_gvm_data, task, tmpbuf);
        if (0 != ret) {
            LOG_ERROR("daemon_create_task| name=%s| task_id=%s| msg=add task error|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id);

            ret = GVM_ERR_INTERNAL_FAIL; 
            break;
        }

        LOG_INFO("daemon_create_task| name=%s| task_id=%s| msg=add task ok|", 
            task->m_task_info.m_task_name, 
            task->m_task_info.m_task_id); 

        /* success here */
        return 0;
    } while (0);

    gvm_option_delete_task(task->m_task_info.m_task_id, tmpbuf, outbuf);
    gvm_option_delete_target(task->m_target_info.m_target_id, tmpbuf, outbuf);
    
    gvm_option_delete_schedule(&task->m_task_info, tmpbuf, outbuf);
    gvm_option_delete_config(&task->m_task_info, tmpbuf, outbuf);

    /* free unused data */
    g_gvm_data->task_ops->free_task(task);    
    
    return ret;
}

static ListGvmTask_t newGvmTask() {
    ListGvmTask_t task = NULL;

    task = (ListGvmTask_t)calloc(1, sizeof(struct ListGvmTask));
    if (NULL != task) {
        reset(&task->m_mainlist);
        reset(&task->m_runlist);
        
        initGvmTaskInfo(&task->m_task_info);
        initGvmReportInfo(&task->m_report_info);
        initGvmResultInfo(&task->m_result_info); 

        task->m_chk_type = GVM_TASK_CHK_TASK; 
    }
    
    return task;
}

static void freeGvmTask(ListGvmTask_t task) { 
    if (NULL != task) {
        resetGvmResultInfo(&task->m_result_info);
        
        free(task);
    }
}
  
static int addGvmTask(GvmDataList_t data, ListGvmTask_t task) {
    int ret = 0; 
    
    ret = addToSet(&data->m_nameset, (LList_t)task); 
    if (0 == ret) { 
        push_back(&data->m_createque, &task->m_mainlist); 
    } else {
        ret = -1;
    }
    
    return ret;
}

static int addGvmRunTask(GvmDataList_t data, ListGvmTask_t task) {
    int ret = 0; 

    if (isEmpty(&task->m_runlist)) {        
        ret = push_back(&data->m_runque, &task->m_runlist); 
    }
    
    return ret;
}

static int delGvmRunTask(GvmDataList_t data, ListGvmTask_t task) {
    int ret = 0; 

    if (!isEmpty(&task->m_runlist)) { 
        ret = del_item(&data->m_runque, &task->m_runlist); 
    }
    
    return ret;
}

static int queGvmRunTask(GvmDataList_t data, ListGvmTask_t task) {
    int ret = 0; 

    reque_list(&data->m_runque, &task->m_runlist);
    
    return ret;
}

static int delGvmTask(GvmDataList_t data, const ListGvmTask_t key) {
    int ret = 0;
    LList_t val = NULL;
    ListGvmTask_t task = NULL;

    val = delFromSet(&data->m_nameset, (const LList_t)key);
    if (NULL != val) {
        task = (ListGvmTask_t)val; 
        
        /* delete from main queue */
        del_item(&data->m_createque, &task->m_mainlist); 
        
        ret = 0;
    } else { 
        /* not found */
        ret = -1;
    }
    
    return ret;
}

static ListGvmTask_t findGvmTask(GvmDataList_t data, const ListGvmTask_t key) {
    LList_t val = NULL;
    ListGvmTask_t task = NULL; 

    val = searchListSet(&data->m_nameset, (const LList_t)key);
    if (NULL != val) {
        task = (ListGvmTask_t)val;
    }

    return task;
}

static void printGvmTask(const ListGvmTask_t task) {
    LOG_INFO("task_id=\"%s\"\n"
        "task_name=\"%s\"\n"
        "group_id=\"%s\"\n"
        "group_name=\"%s\"\n"
        "config_id=\"%s\"\n"
        
        "target_id=\"%s\"\n"
        "portlist_id=\"%s\"\n"
        "hosts=\"%s\"\n"
        "schedule_type=\"%d\"\n"\
        "schedule_id=\"%s\"\n"\
        "schedule_time=\"%s\"\n"\
        "schedule_list=\"%s\"\n"\
        "create_time=\"%s\"\n"

        "status=%d\n"
        "progress=%d\n"
        "report_id=\"%s\"\n"
        "last_report_id=\"%s\"\n"
        
        "start_time=\"%s\"\n"\
        "stop_time=\"%s\"\n"\
        "chk_time=\"%lld\"\n",
        task->m_task_info.m_task_id,
        task->m_task_info.m_task_name,
        task->m_task_info.m_group_id,
        task->m_task_info.m_group_name,
        task->m_task_info.m_config_id, 
        
        task->m_target_info.m_target_id,
        task->m_target_info.m_portlist_id,
        task->m_target_info.m_hosts,

        (int)task->m_task_info.m_schedule_type,
        task->m_task_info.m_schedule_id, 
        task->m_task_info.m_first_schedule_time,
        task->m_task_info.m_schedule_list,
        task->m_task_info.m_create_time,

        task->m_report_info.m_status,
        task->m_report_info.m_progress,
        task->m_report_info.m_cur_report_id,
        task->m_report_info.m_last_report_id,

        task->m_report_info.m_start_time,
        task->m_report_info.m_stop_time,
        task->m_chk_time);
}

static int printGvmCreate(void* ctx, LList_t _list) {
    ListGvmTask_t task = (ListGvmTask_t)_list;

    printGvmTask(task);
    return 0;
}

static int printGvmRun(void* ctx, LList_t _list) {
    ListGvmTask_t task = NULL;

    task = runlist_gvm_task(_list);
    printGvmTask(task);
    return 0;
} 

static void printAllTaskRecs(GvmDataList_t data, int type) {
    int ret = 0; 

    if (0 == type) {
        LOG_INFO("print_all_task_create| total=%d|", data->m_createque.m_cnt);
        
        ret = for_each(&data->m_createque, printGvmCreate, (void*)data);
    } else if (1 == type) {
        LOG_INFO("print_all_task_run| total=%d|", data->m_runque.m_cnt);
    
        ret = for_each(&data->m_runque, printGvmRun, (void*)data);
    }
    return;
}

static int parseTaskRecord(GvmDataList_t data, kb_buf_t cache) {
    int ret = 0;
    int on_off = 0;
    int type = 0;
    char* psz = NULL;
    char* start = NULL;
    char* stop = NULL;
    ListGvmTask_t task = NULL;

    psz = cache->m_buf;

    while (psz < cache->m_buf + cache->m_size && 0 == ret) {
        start = strstr(psz, TASK_INFO_BEG_MARK);
        if (NULL != start) {
            start += strlen(TASK_INFO_BEG_MARK);
            
            stop = strstr(start, TASK_INFO_END_MARK);
            if (NULL != stop) {
                /* temporary end the text */
                *stop = TOKEN_END_CHAR;

                task = g_gvm_data->task_ops->create_task();
                if (NULL == task) {
                    LOG_ERROR("parse_record| error=no memory to allocate task|");
                    return -1;
                }

                do { 
                    ret = getPatternKey(start, "task_id", UUID_REG_PATTERN, 
                        task->m_task_info.m_task_id, 
                        ARR_SIZE(task->m_task_info.m_task_id));
                    if (0 != ret) {
                        break;
                    }
                    
                    ret = getPatternKey(start, "task_name", GVM_NAME_PATTERN, 
                        task->m_task_info.m_task_name, 
                        ARR_SIZE(task->m_task_info.m_task_name));
                    if (0 != ret) {
                        break;
                    }
                    
                    ret = getPatternKey(start, "group_id", CUSTOM_COMM_PATTERN, 
                        task->m_task_info.m_group_id, 
                        ARR_SIZE(task->m_task_info.m_group_id));
                    if (0 != ret) {
                        break;
                    }
                    
                    ret = getPatternKey(start, "group_name", CUSTOM_COMM_PATTERN, 
                        task->m_task_info.m_group_name, 
                        ARR_SIZE(task->m_task_info.m_group_name));
                    if (0 != ret) {
                        break;
                    }
                    
                    ret = getPatternKey(start, "config_id", UUID_REG_PATTERN, 
                        task->m_task_info.m_config_id, 
                        ARR_SIZE(task->m_task_info.m_config_id));
                    if (0 != ret) {
                        break;
                    }

                    ret = getPatternKeyInt(start, "config_created", 
                        CUSTOM_ON_OFF_PATTERN, &on_off);
                    if (0 != ret) {
                        break;
                    }

                    task->m_task_info.m_config_created = (unsigned char)on_off;
                    
                    ret = getPatternKey(start, "target_id", UUID_REG_PATTERN, 
                        task->m_target_info.m_target_id, 
                        ARR_SIZE(task->m_target_info.m_target_id));
                    if (0 != ret) {
                        break;
                    }
                    
                    ret = getPatternKey(start, "portlist_id", UUID_REG_PATTERN, 
                        task->m_target_info.m_portlist_id, 
                        ARR_SIZE(task->m_target_info.m_portlist_id));
                    if (0 != ret) {
                        break;
                    }
                    
                    ret = getPatternKey(start, "hosts", CUSTOM_COMM_PATTERN, 
                        task->m_target_info.m_hosts, 
                        ARR_SIZE(task->m_target_info.m_hosts));
                    if (0 != ret) {
                        break;
                    }

                    ret = getPatternKeyInt(start, "schedule_type", 
                        CUSTOM_SCHEDULE_TYPE_PATTERN, 
                        &type);
                    if (0 != ret) {
                        break;
                    }

                    ret = getPatternKey(start, "schedule_id", 
                        CUSTOM_MATCH_OR_NULL(UUID_REG_PATTERN), 
                        task->m_task_info.m_schedule_id, 
                        ARR_SIZE(task->m_task_info.m_schedule_id));
                    if (0 != ret) {
                        break;
                    } 

                    ret = getPatternKeyInt(start, "schedule_created", 
                        CUSTOM_ON_OFF_PATTERN, &on_off);
                    if (0 != ret) {
                        break;
                    }

                    task->m_task_info.m_schedule_created = (unsigned char)on_off;
                    
                    ret = getPatternKey(start, "schedule_time", 
                        CUSTOM_MATCH_OR_NULL(CUSTOM_TIME_STAMP_PATTERN), 
                        task->m_task_info.m_first_schedule_time, 
                        ARR_SIZE(task->m_task_info.m_first_schedule_time));
                    if (0 != ret) {
                        break;
                    }

                    ret = getPatternKey(start, "schedule_list", CUSTOM_COMM_PATTERN, 
                        task->m_task_info.m_schedule_list, 
                        ARR_SIZE(task->m_task_info.m_schedule_list));
                    if (0 != ret) {
                        break;
                    }

                    ret = getPatternKey(start, "create_time", CUSTOM_TIME_STAMP_PATTERN, 
                        task->m_task_info.m_create_time, 
                        ARR_SIZE(task->m_task_info.m_create_time));
                    if (0 != ret) {
                        break;
                    }

                    ret = chkHosts(task->m_target_info.m_hosts);
                    if (0 != ret) {
                        LOG_ERROR("parse_record| hosts=%s| msg=invalid hosts|", 
                            task->m_target_info.m_hosts);
                        break;
                    }

                    ret = chkScheduleParam(type,
                        task->m_task_info.m_first_schedule_time,
                        task->m_task_info.m_schedule_list);
                    if (0 != ret) {
                        LOG_ERROR( "parse_record| schedul_type=%d| schedul_time=%s|"
                            " schedul_list=%s| msg=invalid schedule parameters|", 
                            type,
                            task->m_task_info.m_first_schedule_time, 
                            task->m_task_info.m_schedule_list);
                                    
                        break;
                    }

                    task->m_task_info.m_schedule_type = (enum ICAL_DATE_REP_TYPE)type;

                    /* target name is consistent as task name */
                    strncpy(task->m_target_info.m_target_name,
                        task->m_task_info.m_task_name,
                        ARR_SIZE(task->m_target_info.m_target_name));

                    /* modify time is consistent as create time */
                    strncpy(task->m_task_info.m_modify_time,
                        task->m_task_info.m_create_time,
                        ARR_SIZE(task->m_task_info.m_modify_time));

                    ret = g_gvm_data->task_ops->add_task(g_gvm_data, task);
                    if (0 == ret) { 
                        task = NULL;
                    } else {
                        break;
                    }
                } while (0);

                if (NULL != task) {
                    g_gvm_data->task_ops->free_task(task); 
                }
                
                /* forward the psz */
                psz = stop + strlen(TASK_INFO_END_MARK); 
            } else {
                /* no end, ignore */
                break;
            }
        } else {
            /* no begin, ignore */
            break;
        }
    }
    
    return ret;
}

int readTaskFile(GvmDataList_t data) {
    int ret = 0;
    char normalFile[MAX_FILENAME_PATH_SIZE] = {0};
    struct kb_buf buffer;
    
    snprintf(normalFile, MAX_FILENAME_PATH_SIZE, "%s/%s",
        data->m_task_priv_dir, data->m_task_file_name);

    ret = readTotalFile(normalFile, &buffer);
    if (0 == ret) {
        /* read data ok */
        ret = parseTaskRecord(data, &buffer);
        
        freeBuf(&buffer);
    } else if (0 < ret) {
        /* no file or empty file */
        return 0;
    } else {
        /* read error */
        return -1;
    } 
    
    return ret;
}

static int writeTaskRecord(void* ctx, LList_t _list) {
    int left = 0;
    int cnt = 0;
    ListGvmTask_t task = NULL;
    kb_buf_t buffer = NULL;

    buffer = (kb_buf_t)ctx;
    task = (ListGvmTask_t)_list;

    left = buffer->m_capacity - buffer->m_size;
    cnt = snprintf(&buffer->m_buf[ buffer->m_size ], left, TASK_FILE_FORMAT,
        task->m_task_info.m_task_id,
        task->m_task_info.m_task_name,
        task->m_task_info.m_group_id,
        task->m_task_info.m_group_name, 
        task->m_task_info.m_config_id,
        (int)task->m_task_info.m_config_created,
        
        task->m_target_info.m_target_id,
        task->m_target_info.m_portlist_id,
        task->m_target_info.m_hosts,

        task->m_task_info.m_schedule_type,
        task->m_task_info.m_schedule_id, 
        (int)task->m_task_info.m_schedule_created,
        
        task->m_task_info.m_first_schedule_time,
        task->m_task_info.m_schedule_list,
        
        task->m_task_info.m_create_time);
    if (0 < cnt && cnt <= left) {
        buffer->m_size += cnt;
        return 0;
    } else {
        LOG_ERROR("write_task_record| total=%d| cnt=%d| msg=write error|",
            left, cnt);
        return -1;
    }
}

static int writeTaskFile(GvmDataList_t data, kb_buf_t buffer) {
    int ret = 0;
    char tmpFile[MAX_FILENAME_PATH_SIZE] = {0};
    char normalFile[MAX_FILENAME_PATH_SIZE] = {0};

    snprintf(tmpFile, MAX_FILENAME_PATH_SIZE, "%s/.%s",
            data->m_task_priv_dir, data->m_task_file_name);

    snprintf(normalFile, MAX_FILENAME_PATH_SIZE, "%s/%s",
        data->m_task_priv_dir, data->m_task_file_name);

    buffer->m_size = 0;
    ret = for_each(&data->m_createque, writeTaskRecord, (void*)buffer);
    if (0 == ret) {
        ret = writeFileSafe(buffer, normalFile, tmpFile); 
    }
    
    return ret;
}

static int parseTaskStatusRecord(GvmDataList_t data,
    ListGvmTask_t task, kb_buf_t cache) {
    int ret = 0;
    int n = 0;
    char uuid[MAX_UUID_SIZE] = {0};
    char* start = NULL;
    char* stop = NULL;

    if (0 < cache->m_size) {
        start = strstr(cache->m_buf, TASK_INFO_BEG_MARK);
        if (NULL != start) {
            start += strlen(TASK_INFO_BEG_MARK);
            
            stop = strstr(start, TASK_INFO_END_MARK);
            if (NULL != stop) {
                /* temporary end the text */
                *stop = TOKEN_END_CHAR;

                do {
                    ret = getPatternKey(start, "task_id", UUID_REG_PATTERN, 
                        uuid, ARR_SIZE(uuid));
                    if (0 != ret) {
                        break;
                    }

                    /* check if task id is consistent */
                    ret = strncmp(uuid, task->m_task_info.m_task_id, ARR_SIZE(uuid));
                    if (0 != ret) {
                        LOG_ERROR("parse_status| task_name=%s| task_id=%s|"
                            " status_task_id=%s| error=task_id is inconsistent|",
                            task->m_task_info.m_task_name,
                            task->m_task_info.m_task_id,
                            uuid);
                        
                        ret = -1;
                        break;
                    } 
                    
                    ret = getPatternKey(start, "start_time", 
                        CUSTOM_MATCH_OR_NULL(CUSTOM_TIME_STAMP_PATTERN),
                        task->m_report_info.m_start_time, 
                        ARR_SIZE(task->m_report_info.m_start_time));
                    if (0 != ret) {
                        break;
                    }


                    ret = getPatternKey(start, "stop_time", 
                        CUSTOM_MATCH_OR_NULL(CUSTOM_TIME_STAMP_PATTERN), 
                        task->m_report_info.m_stop_time, 
                        ARR_SIZE(task->m_report_info.m_stop_time));
                    if (0 != ret) {
                        break;
                    }

                    ret = getPatternKeyInt(start, "status", CUSTOM_DIGIT_NUM_PATTERN, &n);
                    if (0 != ret) {
                        break;
                    }

                    if (GVM_TASK_CREATE <= n && GVM_TASK_ERROR > n) {
                        setTaskStatus(task, (enum GVM_TASK_STATUS)n);
                    } else {
                        LOG_ERROR("parse_status| task_name=%s| task_id=%s|"
                            " start_time=%s| stop_time=%s|"
                            " status=%d| msg=invalid status|",
                            task->m_task_info.m_task_name,
                            task->m_task_info.m_task_id,
                            task->m_report_info.m_start_time,
                            task->m_report_info.m_stop_time,
                            n);
                        
                        ret = -1;
                        break;
                    }

                    ret = getPatternKeyInt(start, "chk_type", CUSTOM_DIGIT_NUM_PATTERN, &n);
                    if (0 != ret) {
                        break;
                    }

                    if (GVM_TASK_CHK_TASK <= n && GVM_TASK_CHK_END > n) {
                        setTaskChkType(task, (enum GVM_TASK_CHK_TYPE)n);
                    } else {
                        LOG_ERROR("parse_chk_type| task_name=%s| task_id=%s|"
                            " start_time=%s| stop_time=%s|"
                            " chk_type=%d| msg=invalid chk_type|",
                            task->m_task_info.m_task_name,
                            task->m_task_info.m_task_id,
                            task->m_report_info.m_start_time,
                            task->m_report_info.m_stop_time,
                            n);
                        
                        ret = -1;
                        break;
                    }

                    ret = getPatternKeyInt(start, "progress", CUSTOM_DIGIT_NUM_PATTERN, &n);
                    if (0 != ret) {
                        break;
                    }

                    if (0 <= n && 100 >= n) {
                        setTaskProgress(task, n);
                    } else {
                        LOG_ERROR("parse_status| task_name=%s| task_id=%s|"
                            " progress=%d| msg=invalid progress|",
                            task->m_task_info.m_task_name,
                            task->m_task_info.m_task_id,
                            n);
                        
                        ret = -1;
                        break;
                    }

                    /* get current report id */
                    ret = getPatternKey(start, "report_id", UUID_REG_PATTERN, 
                        task->m_report_info.m_cur_report_id, 
                        ARR_SIZE(task->m_report_info.m_cur_report_id));
                    if (0 != ret) {
                        break;
                    }

                    /* get last report id */
                    ret = getPatternKey(start, "last_report_id", 
                        CUSTOM_MATCH_OR_NULL(UUID_REG_PATTERN),
                        task->m_report_info.m_last_report_id, 
                        ARR_SIZE(task->m_report_info.m_last_report_id));
                    if (0 != ret) {
                        break;
                    }

                    ret = 0;
                } while (0);
            } 
        } 
    }
    
    return ret;
}

static int readTaskStatusFile(GvmDataList_t data, ListGvmTask_t task) {
    int ret = 0;
    struct kb_buf buffer;

    ret = data->task_ops->prepare_paths(data, task);
    if (0 != ret) {
        return -1;
    } 

    ret = readTotalFile(task->m_paths[OPENVAS_STATUS_FILE], &buffer);
    if (0 == ret) {
        /* read data ok */
        ret = parseTaskStatusRecord(data, task, &buffer);
        
        freeBuf(&buffer);
    } else if (0 < ret) {
        /* no file or empty file */
        return 0;
    } else {
        /* read error */
        return -1;
    } 
    
    return ret;
}

static int readTaskStatusRec(void* ctx, LList_t _list) {
    int ret = 0;

    GvmDataList_t data = (GvmDataList_t)ctx;
    ListGvmTask_t task = (ListGvmTask_t)_list;

    ret = readTaskStatusFile(data, task);    
    return ret;
}


static int readAllTaskStatusRecs(GvmDataList_t data) {
    int ret = 0;
    
    ret = for_each(&data->m_createque, readTaskStatusRec, (void*)data);
    return ret;
}

static int writeTaskStatusFile(GvmDataList_t data, 
    ListGvmTask_t task, kb_buf_t buffer) {
    int ret = 0;

    ret = data->task_ops->prepare_paths(data, task);
    if (0 != ret) {
        return -1;
    }

    buffer->m_size = snprintf(buffer->m_buf, buffer->m_capacity, TASK_STATUS_FILE_FORMAT,
        task->m_task_info.m_task_id,
        task->m_report_info.m_start_time,
        task->m_report_info.m_stop_time,
        (int)getTaskStatus(task),
        (int)getTaskChkType(task),
        task->m_report_info.m_progress,
        task->m_report_info.m_cur_report_id, 
        task->m_report_info.m_last_report_id, 
        "");

    ret = writeFileSafe(buffer, task->m_paths[OPENVAS_STATUS_FILE], 
        task->m_paths[OPENVAS_STATUS_FILE_TMP]);
    return ret;
}

int createTaskRelationFiles(GvmDataList_t data, ListGvmTask_t task,
    kb_buf_t buffer) {
    int ret = 0;

    ret = data->task_ops->prepare_paths(data, task);
    if (0 != ret) {
        return -1;
    }
  
    /* create task dir */
    ret = createDir(task->m_paths[OPENVAS_TASK_DIR]);
    if (0 <= ret) {
        /* if created, or exists already */
        ret = 0;
    } else {
        return -1;
    } 
  
    ret = data->task_ops->add_task(data, task);
    if (0 == ret) {
        ret = data->task_ops->write_task_file(data, buffer);
        if (0 == ret) {
            /* start run check */
            data->task_ops->add_run_task(data, task);
        } else {
            /* roll back */
            data->task_ops->del_task(data, task);

            deleteDir(task->m_paths[OPENVAS_TASK_DIR]);
        }
    } else {
        deleteDir(task->m_paths[OPENVAS_TASK_DIR]);
    }
    
    return ret;
}

int delTaskRelationFiles(GvmDataList_t data, ListGvmTask_t task, kb_buf_t buffer) {
    int ret = 0;

    ret = data->task_ops->prepare_paths(data, task);
    if (0 != ret) {
        return -1;
    }

    /* delete task ok, notify to delete cache */
    ret = data->task_ops->del_task(data, task);
    if (0 == ret) {
        /* update task list */
        ret = data->task_ops->write_task_file(data, buffer); 
        if (0 == ret) {
            data->task_ops->del_run_task(data, task);

            deleteDir(task->m_paths[OPENVAS_TASK_DIR]); 
        } else {
            /* roll back */
            data->task_ops->add_task(data, task);
        }
    } else {
        ret = -1;
    }
    
    return ret;
}

void setTaskStatus(ListGvmTask_t task, enum GVM_TASK_STATUS status) {
    task->m_report_info.m_status = status;
}

void setTaskProgress(ListGvmTask_t task, int progress) {
    task->m_report_info.m_progress = progress;
}

enum GVM_TASK_STATUS getTaskStatus(const ListGvmTask_t task) {
    return task->m_report_info.m_status;
}

void setTaskChkType(ListGvmTask_t task, enum GVM_TASK_CHK_TYPE type) {
    task->m_chk_type = type;
}

enum GVM_TASK_CHK_TYPE getTaskChkType(const ListGvmTask_t task) {
    return task->m_chk_type;
}

int setTaskStartTime(ListGvmTask_t task, const char* time) {
    int cnt = 0;
    int maxlen = 0;

    maxlen = (int)ARR_SIZE(task->m_report_info.m_start_time);
    cnt = strnlen(time, maxlen); 
    if (cnt < maxlen) {
        strncpy(task->m_report_info.m_start_time, time, maxlen);
        return 0;
    } else {
        task->m_report_info.m_start_time[0] = '\0';
        return -1;
    }
}

int setTaskStopTime(ListGvmTask_t task, const char* time) {
    int cnt = 0;
    int maxlen = 0;

    maxlen = (int)ARR_SIZE(task->m_report_info.m_stop_time);
    cnt = strnlen(time, maxlen); 
    if (cnt < maxlen) {
        strncpy(task->m_report_info.m_stop_time, time, maxlen);
        return 0;
    } else {
        task->m_report_info.m_stop_time[0] = '\0';
        return -1;
    }
} 

/* return : 1-running, 0-not running */
int chkTaskRunning(GvmDataList_t data, ListGvmTask_t task) {
    if (GVM_TASK_WAITING == getTaskStatus(task)
        || GVM_TASK_RUNNING == getTaskStatus(task)) {
        return 1;
    } else {
        return 0;
    }
}

/* return : 1-completed, 0-not completed */ 
int chkTaskDone(GvmDataList_t data, ListGvmTask_t task) {
    if (GVM_TASK_DONE == getTaskStatus(task)
        || GVM_TASK_STOP == getTaskStatus(task)
        || GVM_TASK_INTERRUPT == getTaskStatus(task)) {
        return 1;
    } else {
        return 0;
    }
} 

/* return: 1-busy, 0: idle */ 
int chkTaskBusy(GvmDataList_t data, ListGvmTask_t task) {
    if (chkTaskRunning(data, task) || GVM_TASK_CHK_TASK != getTaskChkType(task)) {
        return 1;
    } else {
        return 0;
    }
}

static int writeTaskResult(GvmDataList_t data, ListGvmTask_t task, kb_buf_t outbuf) {
    int ret = 0;

    ret = data->task_ops->prepare_paths(data, task);
    if (0 != ret) {
        return -1;
    }

    ret = writeFileSafe(outbuf, task->m_paths[OPENVAS_RESULT_FILE], 
        task->m_paths[OPENVAS_RESULT_FILE_TMP]);    
    return ret;
} 

static int getGvmConnStatus(GvmDataList_t data) {
    return data->m_is_gvm_conn_ok;
}

static int prepareGvmPaths(GvmDataList_t data, ListGvmTask_t task) {
    int ret = 0;
    int len = 0;

    do {
        /* task root dir */
        len = snprintf(task->m_paths[OPENVAS_TASK_DIR], MAX_FILENAME_PATH_SIZE, 
            "%s/"DEF_GVM_TASK_DIR_PATT,
            data->m_task_priv_dir, task->m_task_info.m_task_id);
        if (0 > len || len >= MAX_FILENAME_PATH_SIZE) {
            /* path length exceeds max */
            ret = -1;
            break;
        } 

        /* status file */
        len = snprintf(task->m_paths[OPENVAS_STATUS_FILE], MAX_FILENAME_PATH_SIZE, 
            "%s/%s",
            task->m_paths[OPENVAS_TASK_DIR], 
            DEF_GVM_TASK_STATUS_FILE_PATT);
        if (0 > len || len >= MAX_FILENAME_PATH_SIZE) {
            /* path length exceeds max */
            ret = -1;
            break;
        } 

        /* status tmp file */
        len = snprintf(task->m_paths[OPENVAS_STATUS_FILE_TMP], MAX_FILENAME_PATH_SIZE, 
            "%s/.%s",
            task->m_paths[OPENVAS_TASK_DIR], 
            DEF_GVM_TASK_STATUS_FILE_PATT);
        if (0 > len || len >= MAX_FILENAME_PATH_SIZE) {
            /* path length exceeds max */
            ret = -1;
            break;
        }

        /* result file */
        len = snprintf(task->m_paths[OPENVAS_RESULT_FILE], MAX_FILENAME_PATH_SIZE, 
            "%s/%s",
            task->m_paths[OPENVAS_TASK_DIR], 
            DEF_GVM_TASK_RESULT_FILE_PATT);
        if (0 > len || len >= MAX_FILENAME_PATH_SIZE) {
            /* path length exceeds max */
            ret = -1;
            break;
        }

        /* result tmp file */
        len = snprintf(task->m_paths[OPENVAS_RESULT_FILE_TMP], MAX_FILENAME_PATH_SIZE, 
            "%s/.%s",
            task->m_paths[OPENVAS_TASK_DIR], 
            DEF_GVM_TASK_RESULT_FILE_PATT);
        if (0 > len || len >= MAX_FILENAME_PATH_SIZE) {
            /* path length exceeds max */
            ret = -1;
            break;
        }

        ret = 0;
    } while (0);
    
    return ret;
}


static const struct GvmTaskOperation DEFAULT_TASK_OPS = {
    newGvmTask,
    freeGvmTask,
    
    addGvmTask,
    delGvmTask,

    addGvmRunTask,
    delGvmRunTask,
    queGvmRunTask,
    
    findGvmTask,

    writeTaskFile,
    readTaskFile, 

    writeTaskStatusFile,
    readTaskStatusFile,
    readAllTaskStatusRecs,

    writeTaskResult,

    createTaskRelationFiles,
    delTaskRelationFiles,
        
    chkTaskRunning,
    chkTaskDone, 
    chkTaskBusy,

    printGvmTask,
    printAllTaskRecs,

    getGvmConnStatus,

    prepareGvmPaths,
};

GvmDataList_t createData() {
    int ret = 0;
    GvmDataList_t data = NULL;

    data = (GvmDataList_t)malloc(sizeof(struct GvmDataList));
    
    memset(data, 0, sizeof(struct GvmDataList));

    strncpy(data->m_task_file_name, DEF_GVM_TASK_FILE_NAME,
        ARR_SIZE(data->m_task_file_name));
    
    strncpy(data->m_task_priv_dir, DEF_GVM_PRIV_DATA_DIR,
        ARR_SIZE(data->m_task_priv_dir)); 

    data->task_ops = &DEFAULT_TASK_OPS;

    do { 
        ret = initListQue(&data->m_createque, (PComp)cmpGvmCreateTime);
        if (0 != ret) {
            break;
        }

        ret = initListQue(&data->m_runque, cmpGvmRunChkTime);
        if (0 != ret) {
            break;
        }

        ret = initListSet(&data->m_nameset, (PComp)cmpGvmName);
        if (0 != ret) {
            break;
        }

        return data;
    } while (0);

    finishData(data);
    return NULL;
}

int finishData(GvmDataList_t data) {
    int ret = 0;

    if (NULL != data) {
        ret = resetListQue(&data->m_runque);
        ret = resetListQue(&data->m_createque);
        
        ret = freeListSet(&data->m_nameset, (PFree)(data->task_ops->free_task));
        
        free(data);
    }

    return ret;
} 

static void daemonSignalHandler(int sig) {
    LOG_INFO("=====recv_sig| sig=%d| msg=exit the daemon|", sig);
    stopKbMsg();

    /* set default now */
    signal(sig, SIG_DFL);
} 
  
static int initLog() { 
    return 0;
}

static int handleSignal() {
    int ret = 0;
    struct sigaction act;
    
    memset(&act, 0, sizeof(struct sigaction));
    
    act.sa_flags = 0;
    act.sa_handler = daemonSignalHandler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGQUIT, &act, NULL); 

    act.sa_flags = SA_RESTART;
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);

    /* no child here */
    //sigaction(SIGCHLD, &act, NULL);

    return ret;
}

int lockFile() {
    int ret = 0;

    if (0 > g_prog_hd) {
        g_prog_hd = open(g_prog_path, O_RDONLY);
        if (0 > g_prog_hd) {
            fprintf(stderr, "the program is missing:%s|\n", ERRMSG);
            return -1;
        }
    }
    
    ret = flock(g_prog_hd, LOCK_EX | LOCK_NB);
    if (0 == ret) {
        return 0;
    } else {
        fprintf(stderr, "the program is already running|\n");
        close(g_prog_hd);
        g_prog_hd= -1;
        
        return -1;
    }
}

int unlockFile() {
    int ret = 0;

    if (0 <= g_prog_hd) {
        ret = flock(g_prog_hd, LOCK_UN | LOCK_NB);
        if (0 != ret) {
            fprintf(stderr, "the program is not running|\n");
            ret = -1;
        }

        close(g_prog_hd);
        g_prog_hd= -1;
        return ret;
    } else {
        return 0;
    }
} 

int getProgPath() {
    int ret = 0;
    
    ret = (int)readlink("/proc/self/exe", g_prog_path, ARR_SIZE(g_prog_path));
    if (0 < ret && ret < (int)ARR_SIZE(g_prog_path)-1) {
        g_prog_path[ret] = '\0';
        return 0;
    } else {
        fprintf(stderr, "get program path err|\n");
        return -1;
    }
} 

const char* progPath() {
    return g_prog_path;
}

int isRun() {
    int ret = 0;
    
    ret = getProgPath();
    if (0 != ret) {
        return ret;
    }

    ret = lockFile();
    if (0 != ret) {
        return ret;
    }

    return ret;
}

int setBackgroud() {
    int fd = -1;
    pid_t pid = 0;
    
    pid = fork();
    if (0 < pid) {
        /* parent */
        exit(0);
    } else if (0 == pid) {
        /* child */
        setsid();
    } else {
        exit(-1);
    } 

    fd = open("/dev/null", O_RDWR);
    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);

    if (2 < fd) {
        close(fd);
    }
    
    return 0;
}

int initDaemon() {
    int ret = 0;

    /* set env for gvm-cli */
    setenv(ENV_LD_NAME, ENV_LD_VAL, 1);
    umask(022);

    ret = initLog();
    if (0 != ret) {
        return ret;
    }

    handleSignal();

    /* xml init lib*/
    ret = initLibXml();
    if (0 != ret) {
        return ret;
    }

    g_gvm_data = createData();
    if (NULL == g_gvm_data) {
        LOG_ERROR("initDaemon| error=create data failed|");
        
        return -1;
    }

    do { 
        ret = chkExists(g_gvm_data->m_task_priv_dir, 0);
        if (0 != ret) {
            break;
        } 

        ret = g_gvm_data->task_ops->read_task_file(g_gvm_data);
        if (0 != ret) {
            break;
        }

        ret = g_gvm_data->task_ops->read_all_task_status_recs(g_gvm_data);
        if (0 != ret) {
            break;
        }

        g_gvm_data->task_ops->print_all_tasks(g_gvm_data, 0); 

        ret = addGvmChecks(g_gvm_data);
        if (0 != ret) {
            break;
        }

        /* return ok here */
        return 0;
    } while (0);

    LOG_ERROR("initDaemon| ret=%d| msg=check env parameters error|", ret);

    /* return failed here */
    finishData(g_gvm_data);
    g_gvm_data = NULL;
    return ret;
}

int finishDaemon() {
    int ret = 0;

    ret = finishData(g_gvm_data);
    g_gvm_data = NULL;

    finishLibXml();
  
    unlockFile();
    
    return ret;
}


