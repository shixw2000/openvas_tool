#define _GNU_SOURCE

#include<sys/types.h>
#include<sys/stat.h> 
#include<fcntl.h> 
#include<unistd.h> 
#include<stdlib.h>
#include<sys/file.h> 
#include<string.h>
#include<ctype.h> 
#include<time.h> 
#include<sys/wait.h>
#include<sys/prctl.h> 

#include"comm_misc.h"
#include"task_openvas.h" 
#include"hydra_business.h"


/* hydra task config file */
#define DEF_HYDRA_TASK_FILE_NAME "hydra_task_file"

/* hydra task dir : task_%task_name% */
#define DEF_HYDRA_TASK_DIR_PATT "task_%s"
#define DEF_HYDRA_LOGIN_FILE_PATT "hydra_login"
#define DEF_HYDRA_PASSWD_FILE_PATT "hydra_passwd"
#define DEF_HYDRA_IP_LIST_FILE_PATT "hydra_ip_list" 
#define DEF_HYDRA_CRON_NORM_FILE_PATT "hydra_cron_norm_%s"
#define DEF_HYDRA_OUTPUT_FILE_PATT "hydra_output_%s"
#define DEF_HYDRA_TASK_GRP_PID_FILE_PATT "hydra_task_proc.pid"

#define DEF_SYSTEM_CRON_BASE_DIR "/etc/cron.d"
#define DEF_HYDRA_TEMPLATE_DIR_NAME_PATT "templates"
#define DEF_HYDRA_TASK_PROG_NAME_PRE "hydra_task"

#define DEF_HYDRA_PRIV_DATA_DIR "/usr/local/openvas/gvm/var/hydra"
#define DEF_HYDRA_PROGRAM_BIN "/usr/bin/hydra"


#define HYDRA_TASK_FILE_FORMAT TASK_INFO_BEG_MARK\
    "task_id=\"%s\"\n"\
    "task_name=\"%s\"\n"\
    "hosts_type=\"%d\"\n"\
    "hosts=\"%s\"\n"\
    "services=\"%s\"\n"\
    "opts=\"%s\"\n"\
    "schedule_type=\"%d\"\n"\
    "schedule_time=\"%s\"\n"\
    "schedule_list=\"%s\"\n"\
    "create_time=\"%s\"\n"\
    TASK_INFO_END_MARK 


static HydraDataList_t g_hydra_data = NULL;


int getKeyHydraParam(const char* input, ListHydraTask_t task) {
    int ret = 0;
    const char* saveptr = NULL;

    do {
        saveptr = input;

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_task_name,
            ARR_SIZE(task->m_task_name), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid task name|",
                __FUNCTION__, input);
            break;
        }

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_task_id,
            ARR_SIZE(task->m_task_id), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid task id|",
                __FUNCTION__, input);
            break;
        }

        ret = chkName(task->m_task_name);
        if (0 != ret) {
            LOG_ERROR( "%s: check parameter| task_name=%s| msg=invalid task name|",
                __FUNCTION__, task->m_task_name);
            break;
        }

        ret = chkUuid(task->m_task_id);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| task_id=%s| msg=invalid uuid|",
                __FUNCTION__, input, task->m_task_id);
            
            break;
        } 

        return 0;
    } while (0);

    return ret;
}


int daemon_start_hydra(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;
    ListHydraTask_t key = NULL;
    ListHydraTask_t task = NULL;

    key = g_hydra_data->task_ops->create_task();
    if (NULL == key) {
        LOG_ERROR("daemon_start_hydra| error=no memory|");
        return GVM_ERR_INTERNAL_FAIL;
    }

    do {
        ret = getKeyHydraParam(input, key);
        if (0 != ret) {
            ret = GVM_ERR_PARAM_INVALID;
            break;
        }

        task = g_hydra_data->task_ops->find_task(g_hydra_data, key);
        if (NULL == task) {
            LOG_ERROR("daemon_start_hydra| name=%s| task_id=%s|"
                " msg=the task donot exist|",
                key->m_task_name,
                key->m_task_id);
            
            ret = GVM_ERR_TASK_NOT_FOUND;
            break;
        }

        if (0 != task->m_pid) {
            LOG_ERROR("daemon_start_hydra| name=%s| task_id=%s| pid=%d|"
                " msg=the task is already running now|",
                task->m_task_name,
                task->m_task_id,
                task->m_pid);
            
            ret = GVM_ERR_TASK_IS_BUSY;
            break;
        }

        ret = g_hydra_data->task_ops->run_task(g_hydra_data, task, tmpbuf);
        if (0 == ret) {
            LOG_INFO("daemon_start_hydra| name=%s| task_id=%s|"
                " pid=%d| msg=start task ok|",
                task->m_task_name,
                task->m_task_id,
                task->m_pid); 
        } else {
            LOG_ERROR("daemon_start_hydra| name=%s| task_id=%s|"
                " msg=hydra process error|",
                task->m_task_name,
                task->m_task_id);

            ret = GVM_ERR_INTERNAL_FAIL;
            break;
        }
    } while (0);

    g_hydra_data->task_ops->free_task(key);

    return ret;
}

int daemon_stop_hydra(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;
    ListHydraTask_t key = NULL;
    ListHydraTask_t task = NULL;

    key = g_hydra_data->task_ops->create_task();
    if (NULL == key) {
        LOG_ERROR("daemon_stop_hydra| error=no memory|");
        return GVM_ERR_INTERNAL_FAIL;
    }

    do {
        ret = getKeyHydraParam(input, key);
        if (0 != ret) {
            ret = GVM_ERR_PARAM_INVALID;
            break;
        }

        task = g_hydra_data->task_ops->find_task(g_hydra_data, key);
        if (NULL == task) {
            LOG_ERROR("daemon_stop_hydra| name=%s| task_id=%s|"
                " msg=the task donot exist|",
                key->m_task_name,
                key->m_task_id);
            
            ret = GVM_ERR_TASK_NOT_FOUND;
            break;
        }

        if (0 == task->m_pid) {
            LOG_ERROR("daemon_stop_hydra| name=%s| task_id=%s|"
                " msg=the task is not running now|",
                task->m_task_name,
                task->m_task_id);
            
            ret = GVM_ERR_TASK_IS_NOT_RUNNING;
            break;
        }

        ret = stopProc(task->m_pid);
        if (0 == ret) { 
            LOG_INFO("daemon_stop_hydra| name=%s| task_id=%s| pid=%d| msg=stop task ok|",
                task->m_task_name,
                task->m_task_id,
                task->m_pid); 

            task->m_pid = 0;
        } else {
            LOG_ERROR("daemon_stop_hydra| name=%s| task_id=%s| pid=%d|"
                " msg=hydra process error|",
                task->m_task_name,
                task->m_task_id,
                task->m_pid);

            ret = GVM_ERR_INTERNAL_FAIL;
            break;
        }

        /* start task ok, notify to start cache */

    } while (0);

    g_hydra_data->task_ops->free_task(key);

    return ret;
}

int daemon_delete_hydra(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;
    ListHydraTask_t key = NULL;
    ListHydraTask_t task = NULL;

    key = g_hydra_data->task_ops->create_task();
    if (NULL == key) {
        LOG_ERROR("daemon_delete_hydra| error=no memory|");
        return GVM_ERR_INTERNAL_FAIL;
    }

    do {
        ret = getKeyHydraParam(input, key);
        if (0 != ret) {
            ret = GVM_ERR_PARAM_INVALID;
            break;
        }

        task = g_hydra_data->task_ops->find_task(g_hydra_data, key);
        if (NULL == task) {
            LOG_ERROR("daemon_delete_hydra| name=%s| task_id=%s|"
                " msg=the task donot exist|",
                key->m_task_name,
                key->m_task_id);
            
            ret = GVM_ERR_TASK_NOT_FOUND;
            break;
        }

        if (0 != task->m_pid) {
            LOG_ERROR("daemon_delete_hydra| name=%s| task_id=%s| pid=%d|"
                " msg=the task is busy now, please stop it first|",
                task->m_task_name,
                task->m_task_id,
                task->m_pid);
            
            ret = GVM_ERR_TASK_IS_BUSY;
            break;
        }

        ret = g_hydra_data->task_ops->del_hydra_relation_files(g_hydra_data, task, tmpbuf);
        if (0 != ret) {
            LOG_ERROR("daemon_delete_hydra| name=%s| task_id=%s|"
                " msg=delete relation files error|",
                task->m_task_name,
                task->m_task_id);
            
            ret = GVM_ERR_INTERNAL_FAIL;
            break;
        }

        LOG_INFO("daemon_delete_hydra| name=%s| task_id=%s|"
            " msg=delete task ok|",
            task->m_task_name,
            task->m_task_id); 

        /* free memory */
        g_hydra_data->task_ops->free_task(task); 
    } while (0);

    g_hydra_data->task_ops->free_task(key);

    return ret;
}

int chkHydraHostsIntern(int hosts_type, const char* hosts) {
    int ret = 0;

    if (0 == hosts_type) {
        ret = regmatch(hosts, CUSTOM_WHOLE_MATCH_IP_LIST_INTERN);
    } else if (1 == hosts_type) {
        ret = regmatch(hosts, CUSTOM_WHOLE_MATCH_IP_BLOCK);
    } else {
        ret = -1;
    }
    
    return ret;
}

static int getCreateHydraParam(const char* input, ListHydraTask_t task) {
    int ret = 0;
    int n = 0;
    int hosts_type = 0;
    const char* saveptr = NULL;
    
    do {
        saveptr = input;

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_task_name,
            ARR_SIZE(task->m_task_name), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid task name|",
                __FUNCTION__, input);
            break;
        }
        
        ret = getNextTokenInt(saveptr, OPENVAS_KB_DELIM, &hosts_type, &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid hosts_type|",
                __FUNCTION__, input);
            break;
        }

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_hosts,
            ARR_SIZE(task->m_hosts), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid hosts|",
                __FUNCTION__, input);
            break;
        }

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_services, 
            ARR_SIZE(task->m_services), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid services|",
                __FUNCTION__, input);
            break;
        }

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_opts, 
            ARR_SIZE(task->m_opts), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid opts|",
                __FUNCTION__, input);
            break;
        }

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_login_list, 
            ARR_SIZE(task->m_login_list), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid login_list|",
                __FUNCTION__, input);
            break;
        }

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, task->m_passwd_list, 
            ARR_SIZE(task->m_passwd_list), &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid passwd_list|",
                __FUNCTION__, input);
            break;
        }

        ret = getNextTokenInt(saveptr, OPENVAS_KB_DELIM, &n, &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid _schedule_type|",
                __FUNCTION__, input);
            break;
        }

        task->m_schedule_type = (enum ICAL_DATE_REP_TYPE)n; 

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, 
            task->m_first_schedule_time, 
            ARR_SIZE(task->m_first_schedule_time), 
            &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid first_schedule_time|",
                __FUNCTION__, input);
            break;
        }

        ret = getNextToken(saveptr, OPENVAS_KB_DELIM, 
            task->m_schedule_list, 
            ARR_SIZE(task->m_schedule_list), 
            &saveptr);
        if (0 != ret) {
            LOG_ERROR("%s: parse parameter| text=%s| msg=invalid schedule_list|",
                __FUNCTION__, input);
            break;
        } 
        
        /* check parameters */
        ret = chkName(task->m_task_name);
        if (0 != ret) {
            LOG_ERROR( "%s: check parameter| task_name=%s| msg=invalid task name|",
                __FUNCTION__, task->m_task_name);
            break;
        }

        ret = chkHydraHostsIntern(hosts_type, task->m_hosts);
        if (0 != ret) {
            LOG_ERROR("%s: check parameter| task_name=%s| hosts_type=%d|"
                " hosts=%s|msg=invalid hosts|", 
                __FUNCTION__,
                task->m_task_name,
                task->m_hosts_type,
                task->m_hosts);
            break;
        }

        task->m_hosts_type = (enum HOSTS_TYPE)hosts_type;

        ret = chkLoginList(task->m_login_list);
        if (0 != ret) {
            LOG_ERROR("%s: check parameter| task_name=%s| login_list=%s|"
                " msg=invalid login_list|", 
                __FUNCTION__,
                task->m_task_name, task->m_login_list);
            break;
        }

        ret = chkPasswdList(task->m_passwd_list);
        if (0 != ret) {
            LOG_ERROR("%s: check parameter| task_name=%s| passwd_list=%s|"
                " msg=invalid passwd_list|", 
                __FUNCTION__,
                task->m_task_name, task->m_passwd_list);
            break;
        } 

        ret = chkServices(task->m_services);
        if (0 != ret) {
            LOG_ERROR("%s: check parameter| task_name=%s| services=%s|"
                " hosts=%s|msg=invalid services|", 
                __FUNCTION__,
                task->m_task_name,
                task->m_services,
                task->m_hosts);
            break;
        }

        ret = chkScheduleParam(task->m_schedule_type, 
            task->m_first_schedule_time, 
            task->m_schedule_list);
        if (0 != ret) {
            LOG_ERROR( "%s: check parameter| schedul_type=%d| schedul_time=%s|"
                " schedul_list=%s| msg=invalid schedule parameters|", 
                __FUNCTION__,
                task->m_schedule_type,
                task->m_first_schedule_time, 
                task->m_schedule_list);
            break;
        }

        ret = genUUID(task->m_task_id, ARR_SIZE(task->m_task_id));
        if (0 != ret) {
            LOG_ERROR( "%s: check parameter| task_name=%d|"
                " msg=genuuid error|", 
                __FUNCTION__,
                task->m_task_name);
            break;
        }
        
        return 0;
    } while (0);

    return ret;
}


int daemon_create_hydra(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;
    ListHydraTask_t oldtask = NULL;
    ListHydraTask_t task = NULL;

    task = g_hydra_data->task_ops->create_task();
    if (NULL == task) {
        LOG_ERROR("daemon_create_hydra| error=no memory|");
        return GVM_ERR_INTERNAL_FAIL;
    }

    do {
        ret = getCreateHydraParam(input, task);
        if (0 != ret) {
            ret = GVM_ERR_PARAM_INVALID;
            break;
        }

        oldtask = g_hydra_data->task_ops->find_task(g_hydra_data, task);
        if (NULL != oldtask) {
            LOG_ERROR("daemon_create_hydra| name=%s| task_id=%s|"
                " msg=the task already exists|",
                oldtask->m_task_name,
                oldtask->m_task_id);
            
            ret = GVM_ERR_TASK_ALREADY_EXISTS;
            break;
        } 
        
        nowTimeStamp(task->m_create_time, ARR_SIZE(task->m_create_time)); 
        strncpy(task->m_modify_time, task->m_create_time,
            ARR_SIZE(task->m_modify_time));

        ret = g_hydra_data->task_ops->write_hydra_relation_files(
            g_hydra_data, task, tmpbuf, outbuf);
        if (0 != ret) {
            LOG_ERROR("daemon_create_hydra| name=%s| task_id=%s|"
                " msg=write relation files error|",
                task->m_task_name,
                task->m_task_id);
            
            ret = GVM_ERR_INTERNAL_FAIL;
            break;
        }

        /* create task ok */ 
        LOG_INFO("daemon_create_hydra| name=%s| task_id=%s| msg=create task ok|",
            task->m_task_name,
            task->m_task_id); 
        
        return 0;
    } while (0);

    /* if failed */
    g_hydra_data->task_ops->free_task(task);

    return ret;
}

static ListHydraTask_t newHydraTask() {
    ListHydraTask_t task = NULL;

    task = (ListHydraTask_t)calloc(1, sizeof(struct ListHydraTask));
    if (NULL != task) {
        reset(&task->m_mainlist);
    }
    
    return task;
}

static void freeHydraTask(ListHydraTask_t task) { 
    if (NULL != task) { 
        free(task);
    }
}
  
static int addHydraTask(HydraDataList_t data, ListHydraTask_t task) {
    int ret = 0; 
    
    ret = addToSet(&data->m_nameset, (LList_t)task); 
    if (0 == ret) { 
        push_back(&data->m_createque, &task->m_mainlist); 
    } else {
        ret = -1;
    }
    
    return ret;
}

static int delHydraTask(HydraDataList_t data, const ListHydraTask_t key) {
    int ret = 0;
    LList_t val = NULL;
    ListHydraTask_t task = NULL;

    val = delFromSet(&data->m_nameset, (const LList_t)key);
    if (NULL != val) {
        task = (ListHydraTask_t)val; 
        
        /* delete from main queue */
        del_item(&data->m_createque, &task->m_mainlist); 
        
        ret = 0;
    } else { 
        /* not found */
        ret = -1;
    }
    
    return ret;
}

static ListHydraTask_t findHydraTask(HydraDataList_t data, const ListHydraTask_t key) {
    LList_t val = NULL;
    ListHydraTask_t task = NULL; 

    val = searchListSet(&data->m_nameset, (const LList_t)key);
    if (NULL != val) {
        task = (ListHydraTask_t)val;
    }

    return task;
} 

static int writeHydraRecord(void* ctx, LList_t _list) {
    int left = 0;
    int cnt = 0;
    ListHydraTask_t task = NULL;
    kb_buf_t buffer = NULL;

    buffer = (kb_buf_t)ctx;
    task = (ListHydraTask_t)_list;

    left = buffer->m_capacity - buffer->m_size;
    cnt = snprintf(&buffer->m_buf[ buffer->m_size ], left, HYDRA_TASK_FILE_FORMAT,
        task->m_task_id,
        task->m_task_name,
        task->m_hosts_type,
        task->m_hosts,
        task->m_services, 
        task->m_opts,

        task->m_schedule_type, 
        task->m_first_schedule_time,
        task->m_schedule_list,
        
        task->m_create_time);
    if (0 < cnt && cnt <= left) {
        buffer->m_size += cnt;
        return 0;
    } else {
        LOG_ERROR("write_hydra_record| total=%d| cnt=%d| msg=write error|",
            left, cnt);
        return -1;
    }
}

static int writeHydraTaskFile(HydraDataList_t data, kb_buf_t buffer) {
    int ret = 0;
    char tmpFile[MAX_FILENAME_PATH_SIZE] = {0};
    char normalFile[MAX_FILENAME_PATH_SIZE] = {0};

    snprintf(tmpFile, MAX_FILENAME_PATH_SIZE, "%s/.%s",
            data->m_task_priv_dir, data->m_task_file_name);

    snprintf(normalFile, MAX_FILENAME_PATH_SIZE, "%s/%s",
        data->m_task_priv_dir, data->m_task_file_name);

    buffer->m_size = 0;
    ret = for_each(&data->m_createque, writeHydraRecord, (void*)buffer);
    if (0 == ret) {
        ret = writeFileSafe(buffer, normalFile, tmpFile); 
    }
    
    return ret;
}

static int parseHydraRecord(HydraDataList_t data, kb_buf_t cache) {
    int ret = 0;
    int schedule_type = 0;
    int hosts_type = 0;
    char* psz = NULL;
    char* pend = NULL;
    char* start = NULL;
    char* stop = NULL;
    ListHydraTask_t task = NULL;

    psz = cache->m_buf;
    pend = cache->m_buf + cache->m_size;

    while (psz < pend && 0 == ret) {
        start = strstr(psz, TASK_INFO_BEG_MARK);
        if (NULL != start) {
            start += strlen(TASK_INFO_BEG_MARK);
            
            stop = strstr(start, TASK_INFO_END_MARK);
            if (NULL != stop) {
                /* temporary end the text */
                *stop = TOKEN_END_CHAR;

                task = g_hydra_data->task_ops->create_task();
                if (NULL == task) {
                    LOG_ERROR("parse_hydra_record| error=no memory to allocate task|");
                    return -1;
                }

                do { 
                    ret = getPatternKey(start, "task_id", UUID_REG_PATTERN, 
                        task->m_task_id, 
                        ARR_SIZE(task->m_task_id));
                    if (0 != ret) {
                        break;
                    }
                    
                    ret = getPatternKey(start, "task_name", GVM_NAME_PATTERN, 
                        task->m_task_name, 
                        ARR_SIZE(task->m_task_name));
                    if (0 != ret) {
                        break;
                    }

                    ret = getPatternKeyInt(start, "hosts_type", CUSTOM_ON_OFF_PATTERN, 
                        &hosts_type);
                    if (0 != ret) {
                        break;
                    }
                    
                    ret = getPatternKey(start, "hosts", CUSTOM_COMM_PATTERN, 
                        task->m_hosts, 
                        ARR_SIZE(task->m_hosts));
                    if (0 != ret) {
                        break;
                    }
                    
                    ret = getPatternKey(start, "services", CUSTOM_SERVICES_PATTERN, 
                        task->m_services, 
                        ARR_SIZE(task->m_services));
                    if (0 != ret) {
                        break;
                    }
                    
                    ret = getPatternKey(start, "opts", CUSTOM_COMM_PATTERN, 
                        task->m_opts, 
                        ARR_SIZE(task->m_opts));
                    if (0 != ret) {
                        break;
                    }

                    ret = getPatternKeyInt(start, "schedule_type", 
                        CUSTOM_SCHEDULE_TYPE_PATTERN, 
                        &schedule_type);
                    if (0 != ret) {
                        break;
                    }
                    
                    ret = getPatternKey(start, "schedule_time", 
                        CUSTOM_MATCH_OR_NULL(CUSTOM_TIME_STAMP_PATTERN), 
                        task->m_first_schedule_time, 
                        ARR_SIZE(task->m_first_schedule_time));
                    if (0 != ret) {
                        break;
                    }

                    ret = getPatternKey(start, "schedule_list", CUSTOM_COMM_PATTERN, 
                        task->m_schedule_list, 
                        ARR_SIZE(task->m_schedule_list));
                    if (0 != ret) {
                        break;
                    }

                    ret = getPatternKey(start, "create_time", CUSTOM_TIME_STAMP_PATTERN, 
                        task->m_create_time, 
                        ARR_SIZE(task->m_create_time));
                    if (0 != ret) {
                        break;
                    }

                    ret = chkName(task->m_task_name);
                    if (0 != ret) {
                        LOG_ERROR("parse_hydra_record| task_name=%d|"
                            " msg=invalid task_name|", 
                            task->m_task_name);
                                    
                        break;
                    }

                    ret = chkHydraHostsIntern(hosts_type, task->m_hosts);
                    if (0 != ret) {
                        LOG_ERROR("parse_hydra_record| task_name=%s| hosts_type=%d|"
                            " hosts=%s|msg=invalid hosts|", 
                            task->m_task_name,
                            hosts_type,
                            task->m_hosts);
                        break;
                    }

                    task->m_hosts_type = (enum HOSTS_TYPE)hosts_type; 

                    ret = chkScheduleParam(schedule_type,
                        task->m_first_schedule_time,
                        task->m_schedule_list);
                    if (0 != ret) {
                        LOG_ERROR("parse_hydra_record| task_name=%s|"
                            " schedul_type=%d| schedul_time=%s|"
                            " schedul_list=%s| msg=invalid schedule parameters|", 
                            task->m_task_name,
                            schedule_type,
                            task->m_first_schedule_time, 
                            task->m_schedule_list);
                                    
                        break;
                    }

                    task->m_schedule_type = (enum ICAL_DATE_REP_TYPE)schedule_type;

                    /* modify time is consistent as create time */
                    strncpy(task->m_modify_time, task->m_create_time,
                        ARR_SIZE(task->m_modify_time));

                    ret = g_hydra_data->task_ops->add_task(g_hydra_data, task);
                    if (0 == ret) { 
                        task = NULL;
                    } else {
                        break;
                    }
                } while (0);

                if (NULL != task) {
                    g_hydra_data->task_ops->free_task(task); 
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

int readHydraTaskFile(HydraDataList_t data) {
    int ret = 0;
    char normalFile[MAX_FILENAME_PATH_SIZE] = {0};
    struct kb_buf buffer;
    
    snprintf(normalFile, MAX_FILENAME_PATH_SIZE, "%s/%s",
        data->m_task_priv_dir, data->m_task_file_name);

    ret = readTotalFile(normalFile, &buffer);
    if (0 == ret) {
        /* read data ok */
        ret = parseHydraRecord(data, &buffer);
        
        freeBuf(&buffer);

        return ret;
    } else if (0 < ret) {
        /* no file or empty file */
        return 0;
    } else {
        /* read error */
        return -1;
    } 
}

static int prepareHydraPaths(HydraDataList_t data, ListHydraTask_t task) {
    int ret = 0;
    int len = 0;

    do {
        len = snprintf(task->m_paths[HYDRA_TASK_DIR], MAX_FILENAME_PATH_SIZE, 
            "%s/"DEF_HYDRA_TASK_DIR_PATT,
            data->m_task_priv_dir, task->m_task_id);
        if (0 > len || len >= MAX_FILENAME_PATH_SIZE) {
            /* path length exceeds max */
            ret = -1;
            break;
        } 

        len = snprintf(task->m_paths[HYDRA_LOGIN_FILE], MAX_FILENAME_PATH_SIZE, 
            "%s/%s",
            task->m_paths[HYDRA_TASK_DIR], 
            DEF_HYDRA_LOGIN_FILE_PATT);
        if (0 > len || len >= MAX_FILENAME_PATH_SIZE) {
            /* path length exceeds max */
            ret = -1;
            break;
        } 

        len = snprintf(task->m_paths[HYDRA_LOGIN_FILE_TMP], MAX_FILENAME_PATH_SIZE, 
            "%s/.%s",
            task->m_paths[HYDRA_TASK_DIR], 
            DEF_HYDRA_LOGIN_FILE_PATT);
        if (0 > len || len >= MAX_FILENAME_PATH_SIZE) {
            /* path length exceeds max */
            ret = -1;
            break;
        }

        len = snprintf(task->m_paths[HYDRA_PASSWD_FILE], MAX_FILENAME_PATH_SIZE, 
            "%s/%s",
            task->m_paths[HYDRA_TASK_DIR], 
            DEF_HYDRA_PASSWD_FILE_PATT);
        if (0 > len || len >= MAX_FILENAME_PATH_SIZE) {
            /* path length exceeds max */
            ret = -1;
            break;
        }

        len = snprintf(task->m_paths[HYDRA_PASSWD_FILE_TMP], MAX_FILENAME_PATH_SIZE, 
            "%s/.%s",
            task->m_paths[HYDRA_TASK_DIR], 
            DEF_HYDRA_PASSWD_FILE_PATT);
        if (0 > len || len >= MAX_FILENAME_PATH_SIZE) {
            /* path length exceeds max */
            ret = -1;
            break;
        }

        len = snprintf(task->m_paths[HYDRA_IP_LIST_FILE], MAX_FILENAME_PATH_SIZE, 
            "%s/%s",
            task->m_paths[HYDRA_TASK_DIR], 
            DEF_HYDRA_IP_LIST_FILE_PATT);
        if (0 > len || len >= MAX_FILENAME_PATH_SIZE) {
            /* path length exceeds max */
            ret = -1;
            break;
        }

        len = snprintf(task->m_paths[HYDRA_IP_LIST_FILE_TMP], MAX_FILENAME_PATH_SIZE, 
            "%s/.%s",
            task->m_paths[HYDRA_TASK_DIR], 
            DEF_HYDRA_IP_LIST_FILE_PATT);
        if (0 > len || len >= MAX_FILENAME_PATH_SIZE) {
            /* path length exceeds max */
            ret = -1;
            break;
        }

        len = snprintf(task->m_paths[HYDRA_CRON_FILE_NORM], MAX_FILENAME_PATH_SIZE, 
            "%s/"DEF_HYDRA_CRON_NORM_FILE_PATT,
            DEF_SYSTEM_CRON_BASE_DIR, 
            task->m_task_id);
        if (0 > len || len >= MAX_FILENAME_PATH_SIZE) {
            /* path length exceeds max */
            ret = -1;
            break;
        }

        /* task pid file */
        len = snprintf(task->m_paths[HYDRA_TASK_GRP_PID_FILE], MAX_FILENAME_PATH_SIZE, 
            "%s/%s",
            task->m_paths[HYDRA_TASK_DIR], 
            DEF_HYDRA_TASK_GRP_PID_FILE_PATT);
        if (0 > len || len >= MAX_FILENAME_PATH_SIZE) {
            /* path length exceeds max */
            ret = -1;
            break;
        }

        ret = 0;
    } while (0);
    
    return ret;
}

/* return: 0: ok, -1: err,  convert such as "SU" to "SUN" */
int convWeekDay(char dst[], const char src[], int maxlen) {
    int ret = 0;
    int cnt = 0;
    int n = 0;
    const char* WEEK_DAYS[] = {"SUN", "MON", "TUS", "WED", "THU", "FRI", "SAT"};
    const char* psz = NULL;
    
    psz = src;
    while (0 == ret && '\0' != *psz) {
        
        ret = -1;
        
        for (n=0; n<(int)ARR_SIZE(WEEK_DAYS); ++n) {
            /* found tag, then replace it */
            if (0 == strncmp(WEEK_DAYS[n], psz, 2)) {
                if (cnt + 4 < maxlen) {
                    strncpy(&dst[cnt], WEEK_DAYS[n], 3);

                    cnt += 3;
                    psz +=2;

                    if (',' == *psz) {
                        dst[cnt] = ',';

                        ++cnt;
                        ++psz;
                        
                        ret = 0;
                    } else if ('\0' == *psz) {
                        /* end list */
                        
                        ret = 0;
                    } else {
                        /* invalid char */ 
                    }
                } else {
                    /* exceeds maxlen */
                }

                break;
            }
        }
    }

    if (0 < cnt && cnt < maxlen && 0 == ret) {
        dst[cnt] = '\0';
        return 0;
    } else {
        LOG_ERROR("convert_weekday| maxlen=%d| list=%s| msg=invalid weed day|", 
            maxlen, src);

        dst[0] = '\0';
        return -1;
    }
}

int convMonthDay(int* hasLastDay, char dst[], const char src[], int maxlen) {
    int ret = 0;
    int len = 0;
    const char* psz = NULL;

    *hasLastDay = 0;

    /* -1 is the last day of month, must be in the end of list */
    psz = strstr(src, "-1");
    if (NULL != psz) {
        len = (int)(psz - src);
        if (0 <= len && len < maxlen) {
            *hasLastDay = 1;

            if (0 < len && ',' == src[len-1]) {
                len -= 1;
            }

            if (0 < len) {
                strncpy(dst, src, len);
            } 
        } else {
            ret = -1;
        }
    } else {        
        len = (int)strnlen(src, maxlen);
        /* here must not be empty */
        if (0 < len && len < maxlen) {
            strncpy(dst, src, len);
        } else {
            ret = -1;
        }
    }

    if (0 == ret) {
        dst[len] = '\0'; 
        
        return 0;
    } else {
        LOG_ERROR("convert_monthday| maxlen=%d| list=%s| msg=invalid month day|", 
            maxlen, src);

        dst[0] = '\0';
        return -1;
    }
}

int formatCronTask(kb_buf_t buffer,
    enum ICAL_DATE_REP_TYPE schedule_type, 
    const char timestamp[],
    const char schedule_list[],
    const char user[],
    const char cmd[]) {
    int ret = 0;
    int len = 0;
    int left = 0;
    int hasLastDay = 0;
    struct tm tm;
    char* psz = NULL;
    char list[MAX_COMM_SIZE] = {0};

    memset(&tm, 0, sizeof(struct tm));
    psz = strptime(timestamp, "%Y-%m-%d %H:%M:%S", &tm);
    if (NULL == psz || '\0' != psz[0]) {
        return -1;
    } 

    buffer->m_size = 0;
    left = (int)buffer->m_capacity;
    
    switch (schedule_type) {
    case ICAL_DATE_ONCE:
        len = snprintf(buffer->m_buf, left, "%d %d %d %d * %s %s >/dev/null&\n",
            tm.tm_min, tm.tm_hour, tm.tm_mday, tm.tm_mon+1, 
            user, cmd);
        if (0 < len && len < left) {
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }
        
        break;

    case ICAL_DATE_DAILY:
        len = snprintf(buffer->m_buf, left, "%d %d * * * %s %s >/dev/null&\n",
            tm.tm_min, tm.tm_hour, 
            user, cmd);
        if (0 < len && len < left) {
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }
        
        break;

    case ICAL_DATE_WEEKLY:
        ret = convWeekDay(list, schedule_list, ARR_SIZE(list));
        if (0 != ret) {
            break;
        }
        
        len = snprintf(buffer->m_buf, left, "%d %d * * %s %s %s >/dev/null&\n",
            tm.tm_min, tm.tm_hour, list, 
            user, cmd);
        if (0 < len && len < left) {
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }
        
        break;

    case ICAL_DATE_MONTHLY:
        ret = convMonthDay(&hasLastDay, list, schedule_list, ARR_SIZE(list));
        if (0 != ret) {
            break;
        }

        if ('\0' != list[0]) {
            len = snprintf(buffer->m_buf, left, "%d %d %s * * %s %s >/dev/null&\n",
                tm.tm_min, tm.tm_hour, list, 
                user, cmd);
            if (0 < len && len < left) {
                buffer->m_size += len; 
                left -= len;
            } else {
                ret = -1;
                break;
            }
        }

        if (hasLastDay) {
            len = snprintf(&buffer->m_buf[len], left, 
                "%d %d 28-31 * * %s %s && (%s >/dev/null&)\n",
                tm.tm_min, tm.tm_hour,
                user, 
                CRON_LAST_MONTHDAY_CMD_PATT, cmd);
            if (0 < len && len < left) {
                buffer->m_size += len; 
                left -= len;
            } else {
                ret = -1;
                break;
            }
        }
        
        break;

    default:
        ret = -1;
        break;
    }

    if (0 == ret) {
        return 0;
    } else {
        buffer->m_size = 0;
        return -1;
    } 
}

extern const char* progPath();

static int writeScheduleFiles(HydraDataList_t data, ListHydraTask_t task,
    kb_buf_t buffer1, kb_buf_t buffer2) {
    int ret = 0; 
    int len = 0;
    
    if (ICAL_DATE_NONE != task->m_schedule_type) {
        len = snprintf(buffer2->m_buf, buffer2->m_capacity,
            "%s 2 \"start_hydra\" \"%s\" \"%s\"",
            progPath(), task->m_task_name, task->m_task_id);
        if (0 < len && len < (int)buffer2->m_capacity) {
            buffer2->m_size = len;
        
            ret = formatCronTask(buffer1, task->m_schedule_type,
                task->m_first_schedule_time,
                task->m_schedule_list,
                CRON_USER_NAME, buffer2->m_buf);
            if (0 == ret) {
                /**** attention: cron file must be write directly ******/
                ret = writeFile(buffer1, task->m_paths[HYDRA_CRON_FILE_NORM]);
            }
        } else {
            ret = -1;
        }
    }
    
    return ret;
}

static int deleteScheduleFiles(HydraDataList_t data, ListHydraTask_t task) {
    int ret = 0;

    deleteFile(task->m_paths[HYDRA_CRON_FILE_NORM]);

    return ret;
} 

static int delHydraRelationFiles(HydraDataList_t data, 
    ListHydraTask_t task, kb_buf_t buffer) {
    int ret = 0;

    ret = prepareHydraPaths(data, task);
    if (0 != ret) {
        return -1;
    }

    ret = data->task_ops->del_task(data, task);
    if (0 == ret) { 
        ret = data->task_ops->write_task_file(data, buffer);
        if (0 == ret) {
            deleteScheduleFiles(data, task); 

            /* delete the whole dir */
            deleteDir(task->m_paths[HYDRA_TASK_DIR]);
        } else {
            /* roll back */
            data->task_ops->add_task(data, task);
        }
    } else {
        ret = -1;
    }

    return ret;
} 

/* return: >0: ok, 0: err */
static int convIPsByLine(char cache[], const char ips[], int maxlen) {
    int cnt = 0;
    
    while (cnt < maxlen-2 && '\0' != ips[cnt]) {
        if (isdigit(ips[cnt]) || '.' == ips[cnt] || ':' == ips[cnt]) {
            cache[cnt] = ips[cnt];
        } else if (',' == ips[cnt]) {
            cache[cnt] = '\n';
        } else {
            break;
        }

        ++cnt;
    } 

    if (0 < cnt && '\0' == ips[cnt]) { 
        cache[cnt++] = '\n';
        cache[cnt] = '\0';
        
        return cnt;
    } else {
        /* exceed max length */
        cache[0] = '\0'; 
        
        return 0;
    }
}

static int writeHydraRelationFile(HydraDataList_t data, 
    ListHydraTask_t task, kb_buf_t buffer1, kb_buf_t buffer2) {
    int ret = 0;

    ret = prepareHydraPaths(data, task);
    if (0 != ret) {
        return -1;
    }

    ret = data->task_ops->add_task(data, task);
    if (0 != ret) {
        return -1;
    } 
    
    do { 
        /* create task base dir */
        ret = createDir(task->m_paths[HYDRA_TASK_DIR]);
        if (0 > ret) {
            ret = -1;
            break;
        } 

        /* write login ctx */
        ret = copyFileListSafe(data->m_task_templ_dir, task->m_login_list, 
            task->m_paths[HYDRA_LOGIN_FILE],
            task->m_paths[HYDRA_LOGIN_FILE_TMP]);
        if (0 != ret) {
            break;
        }

        /* write passwd ctx */
        ret = copyFileListSafe(data->m_task_templ_dir, task->m_passwd_list, 
            task->m_paths[HYDRA_PASSWD_FILE],
            task->m_paths[HYDRA_PASSWD_FILE_TMP]);
        if (0 != ret) {
            break;
        }

        /* write ip list to the file */
        if (HOSTS_IP_LIST == task->m_hosts_type) {
            buffer1->m_size = convIPsByLine(buffer1->m_buf, task->m_hosts, 
                (int)buffer1->m_capacity);
            if (0 == buffer1->m_size) {
                LOG_ERROR("write_hydra_file| task_name=%s| hosts_type=%d|"
                    " ip_list=%s| msg=get ip list error|",
                    task->m_task_name, task->m_hosts_type, task->m_hosts);

                ret = -1;
                break;
            }

            ret = writeFileSafe(buffer1, task->m_paths[HYDRA_IP_LIST_FILE],
                task->m_paths[HYDRA_IP_LIST_FILE_TMP]);
            if (0 != ret) {
                break;
            }
        }

        ret = writeScheduleFiles(data, task, buffer1, buffer2);
        if (0 != ret) {
            break;
        }

        /* update task list */
        ret = data->task_ops->write_task_file(data, buffer1);
        if (0 != ret) {
            break;
        }

        /* ok */
        return 0;
    } while (0);
    
    /* if create fail, then roll back */
    data->task_ops->del_task(data, task);

    deleteScheduleFiles(data, task); 

    /* delete the whole dir */
    deleteDir(task->m_paths[HYDRA_TASK_DIR]);

    return -1;
}

int genCmdParams(char* params[], HydraDataList_t data, 
    ListHydraTask_t task, kb_buf_t buffer,
    char service[], char outfile[]) {
    int ret = 0;
    int cnt = 0;
    int len = 0;
    int left = 0;
    char* psz = NULL;

    buffer->m_size = 0;
    left = buffer->m_capacity;

    do { 
        /* cmd path */
        len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
            DEF_HYDRA_PROGRAM_BIN);
        if (0 < len && len < left) {
            params[cnt++] = &buffer->m_buf[buffer->m_size];
            
            /* include the '\0' trailing */
            ++len;
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }
        
        /* -I */
        len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
            "-I");
        if (0 < len && len < left) {
            params[cnt++] = &buffer->m_buf[buffer->m_size];
            
            /* include the '\0' trailing */
            ++len;
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }

        /* -t */
        len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
            "-t");
        if (0 < len && len < left) {
            params[cnt++] = &buffer->m_buf[buffer->m_size];
            
            /* include the '\0' trailing */
            ++len;
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }

        /* 4 tasks of connect in parallel per target */
        len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
            "4");
        if (0 < len && len < left) {
            params[cnt++] = &buffer->m_buf[buffer->m_size];
            
            /* include the '\0' trailing */
            ++len;
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }

        /* -T */
        len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
            "-T");
        if (0 < len && len < left) {
            params[cnt++] = &buffer->m_buf[buffer->m_size];
            
            /* include the '\0' trailing */
            ++len;
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }

        /* 8 tasks of connect in parallel overall */
        len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
            "8");
        if (0 < len && len < left) {
            params[cnt++] = &buffer->m_buf[buffer->m_size];
            
            /* include the '\0' trailing */
            ++len;
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }

        /* -w */
        len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
            "-w");
        if (0 < len && len < left) {
            params[cnt++] = &buffer->m_buf[buffer->m_size];
            
            /* include the '\0' trailing */
            ++len;
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }

        /* wait time for response betweent connects */
        len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
            "10");
        if (0 < len && len < left) {
            params[cnt++] = &buffer->m_buf[buffer->m_size];
            
            /* include the '\0' trailing */
            ++len;
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }

        /* -o */
        len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
            "-o");
        if (0 < len && len < left) {
            params[cnt++] = &buffer->m_buf[buffer->m_size];
            
            /* include the '\0' trailing */
            ++len;
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }

        /* output file path, according different services */
        params[cnt++] = outfile; 

        /* -b */
        len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
            "-b");
        if (0 < len && len < left) {
            params[cnt++] = &buffer->m_buf[buffer->m_size];
            
            /* include the '\0' trailing */
            ++len;
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }

        /* output format json */
        len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
            "json");
        if (0 < len && len < left) {
            params[cnt++] = &buffer->m_buf[buffer->m_size];
            
            /* include the '\0' trailing */
            ++len;
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }

        /* -L */
        len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
            "-L");
        if (0 < len && len < left) {
            params[cnt++] = &buffer->m_buf[buffer->m_size];
            
            /* include the '\0' trailing */
            ++len;
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }

        /* login file path */
        len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
            task->m_paths[HYDRA_LOGIN_FILE]);
        if (0 < len && len < left) {
            params[cnt++] = &buffer->m_buf[buffer->m_size];
            
            /* include the '\0' trailing */
            ++len;
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }

        /* -P */
        len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
            "-P");
        if (0 < len && len < left) {
            params[cnt++] = &buffer->m_buf[buffer->m_size];
            
            /* include the '\0' trailing */
            ++len;
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }

        /* passwd file path */
        len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
            task->m_paths[HYDRA_PASSWD_FILE]);
        if (0 < len && len < left) {
            params[cnt++] = &buffer->m_buf[buffer->m_size];
            
            /* include the '\0' trailing */
            ++len;
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }

        /* -f: exit when a login/pass is found per host */
        len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
            "-f");
        if (0 < len && len < left) {
            params[cnt++] = &buffer->m_buf[buffer->m_size];
            
            /* include the '\0' trailing */
            ++len;
            buffer->m_size += len;
            left -= len;
        } else {
            ret = -1;
            break;
        }

        /* host */
        if (HOSTS_IP_LIST == task->m_hosts_type) { 
            /* -M */
            len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
                "-M");
            if (0 < len && len < left) {
                params[cnt++] = &buffer->m_buf[buffer->m_size];
                
                /* include the '\0' trailing */
                ++len;
                buffer->m_size += len;
                left -= len;
            } else {
                ret = -1;
                break;
            }
            
            /* ip list file path */
            len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
                task->m_paths[HYDRA_IP_LIST_FILE]);
            if (0 < len && len < left) {
                params[cnt++] = &buffer->m_buf[buffer->m_size];
                
                /* include the '\0' trailing */
                ++len;
                buffer->m_size += len;
                left -= len;
            } else {
                ret = -1;
                break;
            } 
        } else if (HOSTS_IP_BLOCK == task->m_hosts_type) {
            /* -s parameter, may not exists if port hasnot specified */
            len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
                "-s");
            if (0 < len && len < left) { 
                params[++cnt] = &buffer->m_buf[buffer->m_size];
                
                /* include the '\0' trailing */
                ++len;
                buffer->m_size += len;
                left -= len;
            } else {
                ret = -1;
                break;
            }
            
            /* ip block */
            len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s", 
                task->m_hosts);
            if (0 < len && len < left) {
                psz = strchr(&buffer->m_buf[buffer->m_size], ':');
                if (NULL != psz) {
                    /* has port specified, seperate ip block from port */
                    *psz = '\0';

                    /* port parameter */
                    params[++cnt] = psz + 1;

                    /* ip block */
                    params[++cnt] = &buffer->m_buf[buffer->m_size];
                } else {
                    /* no port specified by only ip block, override -s param */
                    params[cnt-1] = &buffer->m_buf[buffer->m_size];
                }
                                
                /* include the '\0' trailing */
                ++len;
                buffer->m_size += len;
                left -= len;
            } else {
                ret = -1;
                break;
            } 
        } else {
            /* invalid host type */
            ret = -1;
            break;
        }

        /* services, loop outside this function while service is changed */
        params[cnt++] = service; 

        /* opts if not empty */
        if ('\0' != task->m_opts[0]) {
            len = snprintf(&buffer->m_buf[buffer->m_size], left, "%s",
                task->m_opts);
            if (0 < len && len < left) {
                params[cnt++] = &buffer->m_buf[buffer->m_size];
                
                /* include the '\0' trailing */
                ++len;
                buffer->m_size += len;
                left -= len;
            } else {
                ret = -1;
                break;
            }
        }

        /* must be null ended */
        params[cnt++] = NULL;
    } while (0);
    
    return ret;
}

/* these variables are used for the task process only */
static ListHydraTask_t g_curr_task = NULL;
static int g_has_stop_task = 0;
static int g_argc = 0;
static char** g_argv = NULL;
static int g_argv_len = 0;

void setArgs(int argc, char* argv[]) {
    
    g_argc = argc;
    g_argv = argv;

    g_argv_len = g_argv[g_argc-1] + strlen(g_argv[g_argc-1]) - g_argv[0]; 
}

void setProcTitle(const char name[]) {
    if (NULL != g_argv) {
        memset(g_argv[0], 0, g_argv_len);
        
        snprintf(g_argv[0], g_argv_len, "%s:%s",
            DEF_HYDRA_TASK_PROG_NAME_PRE, name);

        g_argv[1] = NULL;

        LOG_INFO("setProcTitle| name=%s| argc=%d| len=%d|",
            g_argv[0], g_argc, g_argv_len);
    }
}

static void hydraIgnHandler(int sig) {
    LOG_INFO("=====hydra_ignore_sig| sig=%d|", sig);
} 

static void hydraChildHandler(int sig) {
    LOG_INFO("=====hydra_child_sig| sig=%d|", sig);
}

static void hydraStopHandler(int sig) {
    g_has_stop_task = 1;
    
    LOG_INFO("=====hydra_stop_sig| sig=%d|", sig);
}

static int handleChildSignal() {
    int ret = 0;
    struct sigaction act;
    
    memset(&act, 0, sizeof(struct sigaction));
    
    act.sa_flags = 0;

    /* ignore sigs */
    act.sa_handler = hydraIgnHandler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGQUIT, &act, NULL); 
    sigaction(SIGPIPE, &act, NULL);

    act.sa_handler = hydraChildHandler;
    sigaction(SIGCHLD, &act, NULL);

    /* child can only be kill by user sigs */
    act.sa_handler = hydraStopHandler;
    sigaction(SIGUSR1, &act, NULL);
    sigaction(SIGUSR2, &act, NULL);

    /* if parent root dies, then all of tasks must stop */
    ret = prctl(PR_SET_PDEATHSIG, SIGUSR2);
    if (0 != ret) {
        LOG_ERROR("handle_sig| ret=%d| msg=prctl err:%s|",
            ret, ERRMSG);
        ret = -1;
    }

    return ret;
}

static int onStartTask() {
    int ret = 0;

    do {
        setProcTitle(g_curr_task->m_task_id); 
        
        /* in child for a new process group */
        ret = setpgid(0, 0);
        if (0 != ret) {
            LOG_ERROR("onStartTask| name=%s| task_id=%s| ret=%d|"
                " msg=setpgid error:%s|",
                g_curr_task->m_task_name,
                g_curr_task->m_task_id,
                ret, ERRMSG);

            ret = -1;
            break;
        }
        
        handleChildSignal(); 
        
        /* create a pid file while running */
        ret = createPidFile(g_curr_task->m_paths[HYDRA_TASK_GRP_PID_FILE]);
        if (0 != ret) {
            break;
        }
    } while (0);

    return ret;
}

static int onEndTask() {
    int ret = 0; 
    
    if (NULL != g_curr_task) {
        /* kill the child execv process here if alive */
        killProc(g_curr_task->m_pid);
        
        ret = deleteFile(g_curr_task->m_paths[HYDRA_TASK_GRP_PID_FILE]); 
    }
    
    return ret;
}

static int waitExecTask(char* cmds[], char* service) {
    int ret = 0;
    int childPid = 0;

    childPid = execvSafe(cmds); 
    if (0 < childPid) {
        /* record child execv pid */
        g_curr_task->m_pid = childPid;
        
        do {
            /* wait for cmds ends up*/
            ret = waitpid(childPid, NULL, 0);
        } while (-1 == ret && EINTR == ERRCODE && !g_has_stop_task);

        if (ret == childPid) {
            /* child ends up, reset child pid */
            g_curr_task->m_pid = 0;

            LOG_INFO("waitExecTask| name=%s| task_id=%s|"
                " service=%s| msg=task ends up|",
                g_curr_task->m_task_name, 
                g_curr_task->m_task_id, 
                service);
            
            ret = 0;
        } else if (g_has_stop_task) {
            LOG_INFO("waitExecTask| name=%s| task_id=%s| service=%s|"
                " msg=manual stop task|",
                g_curr_task->m_task_name, 
                g_curr_task->m_task_id,
                service);

            ret = 0;
        } else {
            LOG_ERROR("waitExecTask| name=%s| task_id=%s| service=%s|"
                " msg=wait error:%s|",
                g_curr_task->m_task_name, 
                g_curr_task->m_task_id, 
                service, ERRMSG);
            
            ret = -1;
        }
    } else {
        LOG_ERROR("waitExecTask| name=%s| task_id=%s| service=%s| msg=execv error|",
            g_curr_task->m_task_name, 
            g_curr_task->m_task_id, 
            service);
        
        ret = -1;
    }

    return ret;
}

static int runHydraTask(char* cmds[], char* service, int maxServLen,
    char* outfile, int maxFileLen) {
    int ret = 0;
    int len = 0;
    const char* saveptr = NULL;     

    /* run services one by one */
    saveptr = g_curr_task->m_services;
    while (!g_has_stop_task) {
        ret = getNextToken(saveptr, ",", service, maxServLen, &saveptr);
        if (0 == ret) {
            len = snprintf(outfile, maxFileLen, 
                "%s/"DEF_HYDRA_OUTPUT_FILE_PATT,
                g_curr_task->m_paths[HYDRA_TASK_DIR],
                service);
            if (0 < len && len < maxFileLen) {
                /* ok */
                ret = waitExecTask(cmds, service);
                if (0 != ret) {
                    break;
                } 
            } else {
                LOG_ERROR("runHydraTask| name=%s| task_id=%s| service_list=%s|"
                    " service=%s| maxlen=%d| len=%d| msg=get outfile error|",
                    g_curr_task->m_task_name, 
                    g_curr_task->m_task_id, 
                    g_curr_task->m_services, 
                    service,
                    maxFileLen, len);
                    
                ret = -1;
                break;
            } 
        } else if (1 == ret) {
            /* end of service */
            LOG_INFO("runHydraTask| name=%s| task_id=%s| service_list=%s|"
                " msg=all of tasks ends up|",
                g_curr_task->m_task_name, 
                g_curr_task->m_task_id, 
                g_curr_task->m_services);
            
            ret = 0;
            break;
        } else {
            LOG_ERROR("runHydraTask| name=%s| task_id=%s| service_list=%s| service=%s|"
                " maxlen=%d| ret=%d| msg=get next service error|",
                g_curr_task->m_task_name, 
                g_curr_task->m_task_id,
                g_curr_task->m_services, 
                service,
                maxServLen,
                ret);
            
            ret = -1;
            break;
        } 
    }
    
    return ret;
}

/* return: 0: ok, -1: error */
static int runHydra(HydraDataList_t data, ListHydraTask_t task, kb_buf_t buffer) {
    int ret = 0;
    int pid = 0;
    char* params[MAX_CMD_PARAM_SIZE] = {NULL};
    char service[MAX_NAME_SIZE] = {0}; 
    char outfile[MAX_FILENAME_PATH_SIZE] = {0}; 

    ret = prepareHydraPaths(data, task);
    if (0 != ret)  {
        LOG_ERROR("runHydra| name=%s| task_id=%s|"
            " msg=prepare paths error|",
            task->m_task_name,
            task->m_task_id);
        return -1;
    }
    
    ret = genCmdParams(params, data, task, buffer, service, outfile);
    if (0 != ret) {
        LOG_ERROR("runHydra| name=%s| task_id=%s| ret=%d|"
            " msg=generate cmd parameters error|",
            task->m_task_name,
            task->m_task_id,
            ret);
        return -1;
    }

    pid = forkSafe();
    if (0 < pid) {
        /* in parent, successfully start work */
        task->m_pid = pid;
        
        return 0;
    } else if (0 > pid) {
        /* error ocurrs while fork */
        LOG_ERROR("runHydra| name=%s| task_id=%s|"
            " msg=fork error|",
            task->m_task_name,
            task->m_task_id);
        
        return -1;
    } else { 
        
        /* assign the task for process group leader */
        g_curr_task = task;
        g_curr_task->m_pid = 0;
        g_has_stop_task = 0;

        do { 
            ret = onStartTask();
            if (0 != ret) {
                break;
            }
            
            ret = runHydraTask(params, service, ARR_SIZE(service),
                outfile, ARR_SIZE(outfile)); 
            if (0 != ret) {
                LOG_ERROR("runHydra| name=%s| task_id=%s| service_list=%s|"
                    " cur_service=%s| outfile=%s| ret=%d| msg=run error|",
                    g_curr_task->m_task_name,
                    g_curr_task->m_task_id,
                    g_curr_task->m_services,
                    service, outfile,
                    ret);
                
                break;
            }
        } while (0);

        (void)onEndTask(); 
        
        /* child exit here after task being ended */
        exit(ret);
    }
} 

static const struct HydraTaskOperation DEFAULT_HYDRA_OPS = {
    newHydraTask,
    freeHydraTask,
    
    addHydraTask,
    delHydraTask,
    
    findHydraTask,

    writeHydraTaskFile,
    readHydraTaskFile,

    writeHydraRelationFile,
    delHydraRelationFiles,

    prepareHydraPaths,

    runHydra,
};

/* compare two hydra task info by time */
static int cmpHydraTime(const ListHydraTask_t task1, const ListHydraTask_t task2) {
    int ret = 0;

    ret = strncmp(task1->m_create_time, task2->m_create_time, 
        ARR_SIZE(task1->m_create_time));
    return ret;
}

/* compare two hydra task info by name */
static int cmpHydraName(const ListHydraTask_t task1, const ListHydraTask_t task2) {
    int ret = 0;

    ret = strncmp(task1->m_task_name, task2->m_task_name, 
        ARR_SIZE(task1->m_task_name));
    return ret;
} 

static int finishHydraData(HydraDataList_t data) {
    int ret = 0;

    if (NULL != data) {
        ret = resetListQue(&data->m_createque);
        
        ret = freeListSet(&data->m_nameset, (PFree)(data->task_ops->free_task));
        
        free(data);
    }

    return ret;
}

static HydraDataList_t createHydraData() {
    int ret = 0;
    int len = 0;
    HydraDataList_t data = NULL;

    data = (HydraDataList_t)malloc(sizeof(struct HydraDataList));
    
    memset(data, 0, sizeof(struct HydraDataList));

    strncpy(data->m_task_file_name, DEF_HYDRA_TASK_FILE_NAME,
        ARR_SIZE(data->m_task_file_name));
    
    strncpy(data->m_task_priv_dir, DEF_HYDRA_PRIV_DATA_DIR,
        ARR_SIZE(data->m_task_priv_dir)); 

    data->task_ops = &DEFAULT_HYDRA_OPS;

    do { 
        len = snprintf(data->m_task_templ_dir, MAX_FILENAME_PATH_SIZE,
            "%s/%s",
            data->m_task_priv_dir,
            DEF_HYDRA_TEMPLATE_DIR_NAME_PATT);
        if (0 > len || MAX_FILENAME_PATH_SIZE <= len) {
            ret = -1;
            break;
        }
            
        ret = initListQue(&data->m_createque, (PComp)cmpHydraTime);
        if (0 != ret) {
            break;
        }

        ret = initListSet(&data->m_nameset, (PComp)cmpHydraName);
        if (0 != ret) {
            break;
        }

        return data;
    } while (0);

    finishHydraData(data);
    return NULL;
}

/* this var is used by the parent root process */
static int g_has_child_exit = 0;

static void hydraTaskChild(int sig) {
    g_has_child_exit = 1;
}

static int hydraSignal() {
    int ret = 0;
    struct sigaction act;
    
    memset(&act, 0, sizeof(struct sigaction));
    
    act.sa_flags = 0;

    /* deal child exits */
    act.sa_handler = hydraTaskChild;
    sigaction(SIGCHLD, &act, NULL);

    /* in root process, ignore user sigs */
    act.sa_handler = SIG_IGN;
    sigaction(SIGUSR1, &act, NULL);
    sigaction(SIGUSR2, &act, NULL);

    return ret;
}

static int chgChildTask(void* ctx, LList_t _list) {
    int pid = 0;
    ListHydraTask_t task = NULL;

    pid = (int)(long)ctx;
    task = (ListHydraTask_t)_list;

    if (task->m_pid != pid) {
        return 0;
    } else {
        LOG_INFO("chgChildTask| name=%s| task_id=%s| pid=%d|"
            " msg=task ends|",
            task->m_task_name,
            task->m_task_id,
            pid);

        task->m_pid = 0;

        /* stop for next loop */
        return 1;
    } 
}

int monitorHydraTask() {
    int ret = 0;

    /* received a SIGCHILD sig */
    if (g_has_child_exit) {
        if (NULL != g_hydra_data) {
            while (1) {
                /* wait for child tasks */
                ret = waitpid(-1, NULL, WNOHANG);
                if (0 < ret) {
                    for_each(&g_hydra_data->m_createque, chgChildTask, (void*)(long)ret);
                } else {
                    break;
                }
            }
        }

        g_has_child_exit = 0;
    }
    
    return 0;
}

int initHydra() {
    int ret = 0;

    (void)hydraSignal();
    
    g_hydra_data = createHydraData();
    if (NULL == g_hydra_data) {
        LOG_ERROR("initHydra| error=create data failed|");
        
        return -1;
    }

    do { 
        ret = chkExists(g_hydra_data->m_task_priv_dir, 0);
        if (0 != ret) {
            break;
        } 

        ret = chkExists(g_hydra_data->m_task_templ_dir, 0);
        if (0 != ret) {
            break;
        }

        ret = g_hydra_data->task_ops->read_task_file(g_hydra_data);
        if (0 != ret) {
            break;
        }

        /* return ok here */
        return 0;
    } while (0);

    LOG_ERROR("initHydra| ret=%d| msg=check env parameters error|", ret);

    /* return failed here */
    finishHydraData(g_hydra_data);
    g_hydra_data = NULL;
    return ret;
}

int finishHydra() {
    int ret = 0;

    ret = finishHydraData(g_hydra_data);
    g_hydra_data = NULL;

    return ret;
}

