#include<sys/types.h>
#include<sys/stat.h> 
#include<fcntl.h> 
#include<unistd.h> 
#include<stdlib.h>
#include<signal.h> 
#include<sys/file.h> 
#include<string.h>

#include"task_openvas.h"
#include"openvas_business.h"
#include"openvas_opts.h"



#define REQ_GVM_START_TASK_MARK "<start_task task_id=\"%s\"/>"
#define REQ_GVM_STOP_TASK_MARK "<stop_task task_id=\"%s\"/>"
#define REQ_GVM_DELETE_TASK_MARK "<delete_task task_id=\"%s\"/>"
#define REQ_GVM_DELETE_TARGET_MARK "<delete_target target_id=\"%s\"/>" 
#define REQ_GVM_CREATE_TASK_MARK "<create_task><name>wEb_%s</name><target id=\"%s\"/>"\
    "<config id=\"%s\"/></create_task>"
#define REQ_GVM_CREATE_TASK_WITH_SCHEDULE_MARK "<create_task><name>wEb_%s</name><target id=\"%s\"/>"\
    "<config id=\"%s\"/><schedule id=\"%s\"/></create_task>"
#define REQ_GVM_CREATE_TARGET_MARK "<create_target><name>wEb_%s</name><hosts>%s</hosts><port_list id=\"%s\"/></create_target>"
#define REQ_GVM_QUERY_REPORT_MARK "<get_reports report_id=\"%s\" details=\"0\"/>"
#define REQ_GVM_QUERY_TASK_MARK "<get_tasks task_id=\"%s\" details=\"0\" usage_type=\"scan\"/>"
#define REQ_GVM_QUERY_RESULT_MARK "<get_results task_id=\"%s\" filter=\"report_id=%s levels=hml first=%d rows=%d\"/>"

#define TOKEN_END_CHAR '\0'

/* attensiont: must not be changed for anything */
#define TASK_INFO_BEG_MARK "\n==========begin========\n"

/* attensiont: must not be changed for anything */ 
#define TASK_INFO_END_MARK "\n==========end==========\n"

#define TASK_FILE_FORMAT TASK_INFO_BEG_MARK\
    "task_id=\"%s\"\n"\
    "task_name=\"%s\"\n"\
    "group_id=\"%s\"\n"\
    "group_name=\"%s\"\n"\
    "config_id=\"%s\"\n"\
    "target_id=\"%s\"\n"\
    "portlist_id=\"%s\"\n"\
    "hosts=\"%s\"\n"\
    "schedule_type=\"%d\"\n"\
    "schedule_id=\"%s\"\n"\
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


#define GVM_TASK_HANDLER(x) { #x OPENVAS_KB_DELIM, daemon_##x }

static struct gvm_task_handler g_hds[] = {
    GVM_TASK_HANDLER(start_task),
    GVM_TASK_HANDLER(stop_task),
    GVM_TASK_HANDLER(delete_task),
    GVM_TASK_HANDLER(create_task),

    {NULL, NULL}
};

#if 1
static const char GVM_CMD_MARK[] = "/usr/local/openvas/python/bin/gvm-cli"
    " --timeout 5"
    " --gmp-username admin --gmp-password 123456"
    " tls --xml \"%s\" 2>&1";
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

static int clearKb(kb_t kb) {
    int ret = 0; 

    ret = kb_del_items(kb, OPENVAS_MSG_NAME);

    return ret;
}

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

/* return 0:ok, 1:status not ok, -1:fail */
int procGvmTask(const char* cmd, kb_buf_t cmdbuf, kb_buf_t output) {
    int ret = 0;
    int left = 0;
    int cnt = 0;

    /* temporary escape xml \" as \\\" */
    left = (int)output->m_capacity;
    output->m_size = escapeXml(cmdbuf->m_buf, output->m_buf, left);

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

static int processMsg(const kb_buf_t inbuf, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;
    int size = 0;
    const struct gvm_task_handler* phd = NULL;

    for (phd=g_hds; NULL != phd->m_cmd; ++phd) {
        
        size = (int)strlen(phd->m_cmd);
        if (0 == strncmp(phd->m_cmd, inbuf->m_buf, size)) {
            ret = phd->m_cb(&inbuf->m_buf[size], inbuf->m_size - size, tmpbuf, outbuf);
            if (0 == ret) {
                LOG_DEBUG("%s| cmd=%s| msg=ok| input=[%d]%s", __FUNCTION__, 
                    phd->m_cmd, (int)inbuf->m_size, inbuf->m_buf);
            } else {
                LOG_ERROR("%s| cmd=%s| error=%d| input=[%d]%s", __FUNCTION__, 
                    phd->m_cmd, ret,
                    (int)inbuf->m_size, inbuf->m_buf);
            }
            
            return ret;
        }
    }

    LOG_ERROR("%s| invalid cmd| input=[%d]%s|", __FUNCTION__, 
        (int)inbuf->m_size, inbuf->m_buf);
    return -1;
}

/* 1: busy, 0: idle, -1: error */
static int processIdle(kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int hasMore = 0;
    
    hasMore = runGvmChecks(g_gvm_data, tmpbuf, outbuf);
    
    return hasMore;
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
            LOG_INFO("query_report| report_id=%s| msg=ok|", uuid);
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
            LOG_INFO("query_task| task_id=%s| msg=ok|", uuid);
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
            LOG_INFO("query_result| task_id=%s| report_id=%s| msg=ok|", 
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


int gvm_delete_task(const char* uuid, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;

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

int gvm_delete_target(const char* uuid, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;

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
int startKbMsg() {
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
    clearKb(kb); 
    
    while (g_isRun) {
        if (0 >= timeout) {
            /* the msg queue has the first priority to deal with */
            ret = recvKbMsg(kb, OPENVAS_MSG_NAME, &inbuf);

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
                    clearKb(kb);
                    g_id = 0;

                    continue;
                }
                
                /* work here */
                LOG_DEBUG("key=%s| msg=[%d]%s|", OPENVAS_MSG_NAME, 
                    (int)inbuf.m_size, inbuf.m_buf);
                
                (void)processMsg(&inbuf, &tmpbuf, &outbuf);
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

static int getKeyTaskParam(const char* input, ListGvmTask_t task);

int daemon_start_task(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;
    struct ListGvmTask oKey;
    ListGvmTask_t task = NULL;
    char report_id[MAX_UUID_SIZE] = {0};

    do {
        ret = getKeyTaskParam(input, &oKey);
        if (0 != ret) {
            break;
        }

        task = g_gvm_data->task_ops->find_task(g_gvm_data, &oKey);
        if (NULL == task) {
            LOG_ERROR("daemon_start_task| name=%s| task_id=%s| target_id=%s|"
                " msg=the task donot exist|",
                oKey.m_task_info.m_task_name,
                oKey.m_task_info.m_task_id,
                oKey.m_target_info.m_target_id);
            
            ret = -1;
            break;
        }

        /* can only start the not-running task */
        if (g_gvm_data->task_ops->chk_task_busy(g_gvm_data, task)) {
            LOG_ERROR("daemon_start_task| name=%s| task_id=%s| target_id=%s|"
                " msg=the task is busy now, please wait to end|",
                oKey.m_task_info.m_task_name,
                oKey.m_task_info.m_task_id,
                oKey.m_target_info.m_target_id);
            
            ret = -1;
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
            
            break;
        }

        /* start task ok, notify to start cache */
        setTaskNextChkTime(task, 0);
        g_gvm_data->task_ops->reque_run_task(g_gvm_data, task);

        return 0;
    } while (0);

    return ret;
}

int daemon_stop_task(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;
    struct ListGvmTask oKey;
    ListGvmTask_t task = NULL;

    do {
        ret = getKeyTaskParam(input, &oKey);
        if (0 != ret) {
            break;
        }

        task = g_gvm_data->task_ops->find_task(g_gvm_data, &oKey);
        if (NULL == task) {
            LOG_ERROR("daemon_stop_task| name=%s| task_id=%s| target_id=%s|"
                " msg=the task donot exist|",
                oKey.m_task_info.m_task_name,
                oKey.m_task_info.m_task_id,
                oKey.m_target_info.m_target_id);
            
            ret = -1;
            break;
        }

        /* can only stop running task */
        if (!g_gvm_data->task_ops->chk_task_running(g_gvm_data, task)) {
            LOG_ERROR("daemon_stop_task| name=%s| task_id=%s| target_id=%s|"
                " msg=the task is not running now|",
                oKey.m_task_info.m_task_name,
                oKey.m_task_info.m_task_id,
                oKey.m_target_info.m_target_id);
            
            ret = -1;
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
            
            break;
        }

        setTaskNextChkTime(task, 0);
        g_gvm_data->task_ops->reque_run_task(g_gvm_data, task);
    } while (0);

    return ret;
}

static int getKeyTaskParam(const char* input, ListGvmTask_t task) {
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

        ret = trimText(task->m_task_info.m_task_name);
        if (0 != ret) {
            LOG_ERROR( "%s: trim parameter| text=%s| msg=invalid task name|",
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

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_task_info.m_task_id,
            ARR_SIZE(task->m_task_info.m_task_id), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid task id|",
                __FUNCTION__, input);
            break;
        }

        ret = chkUuid(task->m_task_info.m_task_id);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| task_id=%s| msg=invalid uuid|",
                __FUNCTION__, input, task->m_task_info.m_task_id);
            
            break;
        }

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_target_info.m_target_id,
            ARR_SIZE(task->m_target_info.m_target_id), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid target id|",
                __FUNCTION__, input);
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
    struct ListGvmTask oKey;
    ListGvmTask_t task = NULL;

    do {
        ret = getKeyTaskParam(input, &oKey);
        if (0 != ret) {
            break;
        }

        task = g_gvm_data->task_ops->find_task(g_gvm_data, &oKey);
        if (NULL == task) {
            LOG_ERROR("daemon_del_task| name=%s| task_id=%s| target_id=%s|"
                " msg=the task donot exist|",
                oKey.m_task_info.m_task_name,
                oKey.m_task_info.m_task_id,
                oKey.m_target_info.m_target_id);
            
            ret = -1;
            break;
        }

        /* can only delete the not-busy task */
        if (g_gvm_data->task_ops->chk_task_busy(g_gvm_data, task)) {
            LOG_ERROR("daemon_del_task| name=%s| task_id=%s| target_id=%s|"
                " task_status=%d| chk_type=%d|"
                " error=the task is busy now, please stop it first|",
                oKey.m_task_info.m_task_name,
                oKey.m_task_info.m_task_id,
                oKey.m_target_info.m_target_id,
                getTaskStatus(task),
                getTaskChkType(task));
            
            ret = -1;
            break;
        }

        ret = gvm_delete_task(task->m_task_info.m_task_id, tmpbuf, outbuf);
        if (0 != ret) {
            LOG_ERROR("daemon_del_task| name=%s| task_id=%s| target_id=%s|"
                " msg=del gvm task error|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id,
                task->m_target_info.m_target_id);
            
            break;
        } 

        ret = gvm_delete_target(task->m_target_info.m_target_id, tmpbuf, outbuf);
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
        } 

        /* delete schedule if has */
        if ('\0' != task->m_task_info.m_schedule_id[0]) {
            ret = gvm_delete_schedule(task->m_task_info.m_schedule_id, tmpbuf, outbuf);
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
            } 
        } 

        /* delete cache and files */ 
        ret = deleteTaskWhole(g_gvm_data, task, tmpbuf);
        if (0 != ret) { 
            setTaskChkType(task, GVM_TASK_CHK_DELETED);
            
            break;
        }
        
        ret = 0;
    } while (0);

    return ret;
}

static int getCreateTaskParam(const char* input, ListGvmTask_t task) {
    int ret = 0;
    int type = 0;
    const char* saveptr = NULL;
    char val[MAX_COMM_MIN_SIZE] = {0};
    
    do {
        saveptr = input;
        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_task_info.m_group_id,
            ARR_SIZE(task->m_task_info.m_group_id), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid group|",
                __FUNCTION__, input);
            break;
        }

        ret = getGroupId(task->m_task_info.m_group_id, &type);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| group=%s| msg=invalid group|",
                __FUNCTION__, input, task->m_task_info.m_group_id);
            
            break;
        }

        ret = getConfigId(type, task->m_task_info.m_config_id, 
            ARR_SIZE(task->m_task_info.m_config_id));
        if (0 > ret) {
            LOG_ERROR("%s: parse parameter| text=%s| type=%d| msg=invalid group type|",
                __FUNCTION__, input, type);
            
            ret = -1;
            break;
        }
        
        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_task_info.m_group_name,
            ARR_SIZE(task->m_task_info.m_group_name), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid group name|",
                __FUNCTION__, input);
            break;
        }

        ret = trimText(task->m_task_info.m_group_name);
        if (0 != ret) {
            LOG_ERROR( "%s: trim parameter| text=%s| msg=invalid group name|",
                __FUNCTION__, input);
            break;
        }

        ret = chkName(task->m_task_info.m_group_name);
        if (0 != ret) {
            LOG_ERROR( "%s: check parameter| group_name=%s| msg=invalid group name|",
                __FUNCTION__, task->m_task_info.m_group_name);
            break;
        }

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_task_info.m_task_name,
            ARR_SIZE(task->m_task_info.m_task_name), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid task name|",
                __FUNCTION__, input);
            break;
        }

        ret = trimText(task->m_task_info.m_task_name);
        if (0 != ret) {
            LOG_ERROR( "%s: trim parameter| text=%s| msg=invalid task name|",
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

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_target_info.m_hosts, 
            ARR_SIZE(task->m_target_info.m_hosts), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid hosts|",
                __FUNCTION__, input);
            break;
        }

        ret = escapeHosts(task->m_target_info.m_hosts);
        if (0 != ret) {
            LOG_ERROR( "%s: escape parameter| text=%s| msg=invalid hosts|",
                __FUNCTION__, input);
            break;
        }

        ret = chkHosts(task->m_target_info.m_hosts);
        if (0 != ret) {
            LOG_ERROR( "%s: check parameter| hosts=%s| msg=invalid hosts",
                __FUNCTION__, task->m_target_info.m_hosts);
            break;
        }

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, val, 
            ARR_SIZE(val), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid _schedule_type|",
                __FUNCTION__, input);
            break;
        }

        task->m_task_info.m_schedule_type = (enum ICAL_DATE_REP_TYPE)atoi(val); 

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
    char utc_schedule_time[MAX_TIMESTAMP_SIZE] = {0};

    task = g_gvm_data->task_ops->create_task();
    if (NULL == task) {
        LOG_ERROR("%s| inputlen=%d| input=%s| error=no memory for task|",
            __FUNCTION__, inputlen, input);
        return -1;
    }

    do {
        ret = getCreateTaskParam(input, task);
        if (0 != ret) {
            break;
        }

        ret = local2SchedTime(utc_schedule_time, ARR_SIZE(utc_schedule_time),
            task->m_task_info.m_first_schedule_time);
        if (0 != ret) {
            LOG_ERROR("daemon_create_task| name=%s| group_name=%s| hosts=%s|"
                " schedule_time=%s| msg=invalid schedule_time|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_group_name,
                task->m_target_info.m_hosts,
                task->m_task_info.m_first_schedule_time);
            
            break;
        }
        
        /* check if task exists */
        oldtask = g_gvm_data->task_ops->find_task(g_gvm_data, task);
        if (NULL != oldtask) {
            LOG_ERROR("daemon_create_task| name=%s| group_name=%s| hosts=%s|"
                " msg=the task already exist|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_group_name,
                task->m_target_info.m_hosts);
            
            /* need free task at end */
            ret = -1;
            break;
        }

        ret = gvm_create_target(task->m_target_info.m_target_name, 
            task->m_target_info.m_hosts, 
            task->m_target_info.m_portlist_id, 
            task->m_target_info.m_target_id, 
            ARR_SIZE(task->m_target_info.m_target_id), 
            tmpbuf, outbuf);
        if (0 == ret) {
            LOG_INFO("daemon_create_task| target_id=%s| name=%s| hosts=%s| portlist=%s|"
                " msg=create target ok|",
                task->m_target_info.m_target_id,
                task->m_target_info.m_target_name, 
                task->m_target_info.m_hosts, 
                task->m_target_info.m_portlist_id);
        } else {
            LOG_ERROR("daemon_create_task| name=%s| hosts=%s| portlist=%s| config_id=%s|"
                " target_id=%s| schedule_id=%s|"
                " msg=create target fail|",
                task->m_task_info.m_task_name,
                task->m_target_info.m_hosts, 
                task->m_target_info.m_portlist_id,
                task->m_task_info.m_config_id, 
                task->m_target_info.m_target_id,
                task->m_task_info.m_schedule_id);
            break;
        }

        ret = gvm_create_schedule(task->m_task_info.m_task_name,
            task->m_task_info.m_schedule_type,
            utc_schedule_time, 
            task->m_task_info.m_schedule_list,
            task->m_task_info.m_schedule_id,
            ARR_SIZE(task->m_task_info.m_schedule_id),
            tmpbuf, outbuf);
        if (0 == ret) {
            LOG_INFO("daemon_create_task| task_id=%s| name=%s| hosts=%s| portlist=%s|"
                " config_id=%s| target_id=%s| schedule_id=%s|"
                " msg=create schedule ok|",
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
                " msg=create schedule fail, so delete new target|",
                task->m_task_info.m_task_name,
                task->m_target_info.m_hosts, 
                task->m_target_info.m_portlist_id,
                task->m_task_info.m_config_id, 
                task->m_target_info.m_target_id,
                task->m_task_info.m_schedule_id);

            /* delete the created target */
            gvm_delete_target(task->m_target_info.m_target_id, tmpbuf, outbuf);
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
                " msg=create task fail, so delete new target and schedule|",
                task->m_task_info.m_task_name,
                task->m_target_info.m_hosts, 
                task->m_target_info.m_portlist_id,
                task->m_task_info.m_config_id, 
                task->m_target_info.m_target_id,
                task->m_task_info.m_schedule_id);

            /* delete the created target and schedule */
            gvm_delete_target(task->m_target_info.m_target_id, tmpbuf, outbuf);

            if ('\0' != task->m_task_info.m_schedule_id[0]) {
                gvm_delete_schedule(task->m_task_info.m_schedule_id, tmpbuf, outbuf);
            }
            break;
        }

        /* create task ok, record to file */
        getTimeStamp(task->m_task_info.m_create_time, 
            ARR_SIZE(task->m_task_info.m_create_time));

        strncpy(task->m_task_info.m_modify_time, 
            task->m_task_info.m_create_time,
            ARR_SIZE(task->m_task_info.m_modify_time));
       
        setTaskStatus(task, GVM_TASK_CREATE);
        setTaskChkType(task, GVM_TASK_CHK_TASK);
         
        ret = g_gvm_data->task_ops->add_task(g_gvm_data, task);
        if (0 == ret) { 
            LOG_INFO("daemon_create_task| name=%s| task_id=%s| msg=add task ok|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id);
            
            g_gvm_data->task_ops->write_task_file(g_gvm_data, tmpbuf); 

            addTaskChecks(g_gvm_data, task);

            /* avoid to free ptr */
            task = NULL;
        } else {
            LOG_ERROR("daemon_create_task| name=%s| task_id=%s| msg=add task error|",
                task->m_task_info.m_task_name,
                task->m_task_info.m_task_id);
            
            break;
        }
    } while (0);

    if (NULL != task) {
        /* free unused data */
        g_gvm_data->task_ops->free_task(task);    
    }
    
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

        /* delete from run queue */
        del_item(&data->m_runque, &task->m_runlist); 
        
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
    char* psz = NULL;
    char* start = NULL;
    char* stop = NULL;
    ListGvmTask_t task = NULL;
    char val[MAX_COMM_SIZE] = {0};

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
                    
                    ret = getPatternKey(start, "group_id", CUSTOM_GROUP_ID_PATTERN, 
                        task->m_task_info.m_group_id, 
                        ARR_SIZE(task->m_task_info.m_group_id));
                    if (0 != ret) {
                        break;
                    }
                    
                    ret = getPatternKey(start, "group_name", GVM_NAME_PATTERN, 
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
                    
                    ret = getPatternKey(start, "hosts", CUSTOM_HOSTS_PATTERN, 
                        task->m_target_info.m_hosts, 
                        ARR_SIZE(task->m_target_info.m_hosts));
                    if (0 != ret) {
                        break;
                    }

                    ret = getPatternKey(start, "schedule_type", 
                        CUSTOM_SCHEDULE_TYPE_PATTERN, 
                        val, ARR_SIZE(val));
                    if (0 != ret) {
                        break;
                    }

                    ret = getPatternKey(start, "schedule_id", UUID_REG_PATTERN_OR_NULL, 
                        task->m_task_info.m_schedule_id, 
                        ARR_SIZE(task->m_task_info.m_schedule_id));
                    if (0 != ret) {
                        break;
                    } 

                    task->m_task_info.m_schedule_type = (enum ICAL_DATE_REP_TYPE)atoi(val);
                    
                    ret = getPatternKey(start, "schedule_time", 
                        CUSTOM_TIME_STAMP_PATTERN_OR_NULL, 
                        task->m_task_info.m_first_schedule_time, 
                        ARR_SIZE(task->m_task_info.m_first_schedule_time));
                    if (0 != ret) {
                        break;
                    }

                    ret = getPatternKey(start, "schedule_list", CUSTOM_SCHEDULE_LIST_PATTERN, 
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
        
        task->m_target_info.m_target_id,
        task->m_target_info.m_portlist_id,
        task->m_target_info.m_hosts,

        task->m_task_info.m_schedule_type,
        task->m_task_info.m_schedule_id, 
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
    FILE* hd = NULL;

    snprintf(tmpFile, MAX_FILENAME_PATH_SIZE, "%s/.%s",
            data->m_task_priv_dir, data->m_task_file_name);

    snprintf(normalFile, MAX_FILENAME_PATH_SIZE, "%s/%s",
        data->m_task_priv_dir, data->m_task_file_name);

    buffer->m_size = 0;
    ret = for_each(&data->m_createque, writeTaskRecord, (void*)buffer);
    if (0 == ret) {
        hd = fopen(tmpFile, "wb");
        if (NULL != hd) { 
            ret = fwrite(buffer->m_buf, 1, buffer->m_size, hd); 
            fclose(hd);

            if (buffer->m_size == ret) {
                ret = rename(tmpFile, normalFile);
                if (0 != ret) {
                    LOG_ERROR("writeTaskFile| old=%s| new=%s| msg=rename file error:%s|",
                        tmpFile, normalFile, ERRMSG);
                }
            } else {
                LOG_ERROR("writeTaskFile| name=%s| size=%d| wr_len=%d| msg=write error|", 
                    tmpFile, (int)buffer->m_size, ret);
                ret = -1;
            }
        } else {
            LOG_ERROR("writeTaskFile| name=%s| msg=open file error:%s|", 
                tmpFile, ERRMSG);
            ret = -1;
        } 
    }
    
    return ret;
}

static int parseTaskStatusRecord(GvmDataList_t data,
    ListGvmTask_t task, kb_buf_t cache) {
    int ret = 0;
    int n = 0;
    char uuid[MAX_UUID_SIZE] = {0};
    char val[MAX_COMM_SIZE] = {0};
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
                    
                    ret = getPatternKey(start, "start_time", CUSTOM_TIME_STAMP_PATTERN_OR_NULL, 
                        task->m_report_info.m_start_time, 
                        ARR_SIZE(task->m_report_info.m_start_time));
                    if (0 != ret) {
                        break;
                    }


                    ret = getPatternKey(start, "stop_time", CUSTOM_TIME_STAMP_PATTERN_OR_NULL, 
                        task->m_report_info.m_stop_time, 
                        ARR_SIZE(task->m_report_info.m_stop_time));
                    if (0 != ret) {
                        break;
                    }

                    ret = getPatternKey(start, "status", CUSTOM_DIGIT_NUM_PATTERN, 
                        val, ARR_SIZE(val));
                    if (0 != ret) {
                        break;
                    }

                    n = atoi(val);
                    if (GVM_TASK_CREATE <= n && GVM_TASK_ERROR > n) {
                        setTaskStatus(task, (enum GVM_TASK_STATUS)n);
                    } else {
                        LOG_ERROR("parse_status| task_name=%s| task_id=%s|"
                            " start_time=%s| stop_time=%s|"
                            " status=%s| msg=invalid status|",
                            task->m_task_info.m_task_name,
                            task->m_task_info.m_task_id,
                            task->m_report_info.m_start_time,
                            task->m_report_info.m_stop_time,
                            val);
                        
                        ret = -1;
                        break;
                    }

                    ret = getPatternKey(start, "chk_type", CUSTOM_DIGIT_NUM_PATTERN, 
                        val, ARR_SIZE(val));
                    if (0 != ret) {
                        break;
                    }

                    n = atoi(val);
                    if (GVM_TASK_CHK_TASK <= n && GVM_TASK_CHK_END > n) {
                        setTaskChkType(task, (enum GVM_TASK_CHK_TYPE)n);
                    } else {
                        LOG_ERROR("parse_chk_type| task_name=%s| task_id=%s|"
                            " start_time=%s| stop_time=%s|"
                            " chk_type=%s| msg=invalid chk_type|",
                            task->m_task_info.m_task_name,
                            task->m_task_info.m_task_id,
                            task->m_report_info.m_start_time,
                            task->m_report_info.m_stop_time,
                            val);
                        
                        ret = -1;
                        break;
                    }

                    memset(val, 0, ARR_SIZE(val));
                    ret = getPatternKey(start, "progress", CUSTOM_DIGIT_NUM_PATTERN, 
                        val, ARR_SIZE(val));
                    if (0 != ret) {
                        break;
                    }

                    n = atoi(val);
                    if (0 <= n && 100 >= n) {
                        setTaskProgress(task, n);
                    } else {
                        LOG_ERROR("parse_status| task_name=%s| task_id=%s|"
                            " progress=%s| msg=invalid progress|",
                            task->m_task_info.m_task_name,
                            task->m_task_info.m_task_id,
                            val);
                        
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
                    ret = getPatternKey(start, "last_report_id", UUID_REG_PATTERN_OR_NULL, 
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
    char normalFile[MAX_FILENAME_PATH_SIZE] = {0};
    struct kb_buf buffer;
    
    snprintf(normalFile, MAX_FILENAME_PATH_SIZE, "%s/"DEF_GVM_TASK_STATUS_FILE_PATT,
        data->m_task_priv_dir, task->m_task_info.m_task_name);

    ret = readTotalFile(normalFile, &buffer);
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
    int cnt = 0;
    char tmpFile[MAX_FILENAME_PATH_SIZE] = {0};
    char normalFile[MAX_FILENAME_PATH_SIZE] = {0};
    FILE* hd = NULL;

    snprintf(tmpFile, MAX_FILENAME_PATH_SIZE, "%s/."DEF_GVM_TASK_STATUS_FILE_PATT,
            data->m_task_priv_dir, task->m_task_info.m_task_name);

    snprintf(normalFile, MAX_FILENAME_PATH_SIZE, "%s/"DEF_GVM_TASK_STATUS_FILE_PATT,
        data->m_task_priv_dir, task->m_task_info.m_task_name);

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
    
    hd = fopen(tmpFile, "wb");
    if (NULL != hd) { 
        cnt = fwrite(buffer->m_buf, 1, buffer->m_size, hd);
        fclose(hd);

        if (cnt == (int)buffer->m_size) {
            ret = rename(tmpFile, normalFile);
            if (0 != ret) {
                LOG_ERROR("write_task_status_file| old=%s| new=%s|"
                    " size=%d| msg=rename file error:%s|",
                    tmpFile, normalFile, (int)buffer->m_size, ERRMSG);
            }
        } else {
            LOG_ERROR("write_task_status_file| name=%s|"
                " total=%d| wr_cnt=%d| msg=write error|",
                tmpFile, (int)buffer->m_size, cnt);

            ret = -1;
        }
    } else {
        LOG_ERROR("write_task_status_file| name=%s| msg=open file error:%s|", 
            tmpFile, ERRMSG);
        ret = -1;
    } 
    
    return ret;
}

int delTaskRelationFiles(GvmDataList_t data, ListGvmTask_t task) {
    int ret = 0;
    char statusFile[MAX_FILENAME_PATH_SIZE] = {0};
    char resultFile[MAX_FILENAME_PATH_SIZE] = {0};

    snprintf(statusFile, MAX_FILENAME_PATH_SIZE, "%s/"DEF_GVM_TASK_STATUS_FILE_PATT,
        data->m_task_priv_dir, task->m_task_info.m_task_name);
    
    snprintf(resultFile, MAX_FILENAME_PATH_SIZE, "%s/"DEF_GVM_TASK_RESULT_FILE_PATT,
        data->m_task_priv_dir, task->m_task_info.m_task_name);

    /* delete status file and result file if exists, */
    ret = unlink(statusFile);
    ret = unlink(resultFile);
    return 0;
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

int writeTaskResult(GvmDataList_t data, ListGvmTask_t task, kb_buf_t outbuf) {
    int ret = 0;
    int cnt = 0;
    int total = 0;
    char tmpFile[MAX_FILENAME_PATH_SIZE] = {0};
    char normalFile[MAX_FILENAME_PATH_SIZE] = {0};
    FILE* hd = NULL;
    const char* psz = NULL;
    static const char DEF_EMPTY_RESULT[] = "empty"; 

    snprintf(tmpFile, MAX_FILENAME_PATH_SIZE, "%s/."DEF_GVM_TASK_RESULT_FILE_PATT,
            data->m_task_priv_dir, task->m_task_info.m_task_name);

    snprintf(normalFile, MAX_FILENAME_PATH_SIZE, "%s/"DEF_GVM_TASK_RESULT_FILE_PATT,
        data->m_task_priv_dir, task->m_task_info.m_task_name);

    if (0 < outbuf->m_size) {
        psz = outbuf->m_buf;
        total = (int)outbuf->m_size;
    } else {
        psz = DEF_EMPTY_RESULT;
        total  = (int)strlen(DEF_EMPTY_RESULT);
    }
    
    hd = fopen(tmpFile, "wb");
    if (NULL != hd) {   
        cnt = fwrite(psz, 1, total, hd);
        fclose(hd);

        if (cnt == total) {
            ret = rename(tmpFile, normalFile);
            if (0 != ret) {
                LOG_ERROR("write_task_result_file| old=%s| new=%s|"
                    " size=%d| msg=rename file error:%s|",
                    tmpFile, normalFile, total, ERRMSG);
            }
        } else {
            LOG_ERROR("write_task_result_file| name=%s|"
                " total=%d| wr_cnt=%d| msg=write error|",
                tmpFile, total, cnt);

            ret = -1;
        }
    } else {
        LOG_ERROR("write_task_status_file| name=%s| msg=open file error:%s|", 
            tmpFile, ERRMSG);
        ret = -1;
    } 
    
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

    delTaskRelationFiles,
        
    chkTaskRunning,
    chkTaskDone, 
    chkTaskBusy,

    printGvmTask,
    printAllTaskRecs,
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

static int createDir(const char dir[]) {
    int ret = 0;
    struct stat buf;
    
    ret = stat(dir, &buf);
    if (0 != ret) {
        ret = mkdir(dir, 0774);
        if (0 != ret) {
            LOG_ERROR("mkdir| dir=%s| msg=mkdir error:%s|",
                dir, ERRMSG);
        }
    }

    return ret;
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
    sigaction(SIGCHLD, &act, NULL);

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
        ret = createDir(g_gvm_data->m_task_priv_dir);
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


