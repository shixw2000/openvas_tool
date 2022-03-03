#ifndef __TASK_OPENVAS_H__
#define __TASK_OPENVAS_H__
#include"base_openvas.h"


#ifdef __cplusplus
extern "C" {
#endif


struct php_key_task_param {
    char m_task_name[MAX_NAME_SIZE]; 
    char m_task_id[MAX_UUID_SIZE];
    char m_target_id[MAX_UUID_SIZE];
};

struct php_create_task_param {
    char m_task_name[MAX_NAME_SIZE];
    char m_group_name[MAX_MEM_LIST_SIZE]; 
    char m_group_id[MAX_MEM_LIST_SIZE];
    char m_hosts[MAX_MEM_LIST_SIZE];
    char m_schedule_type[MAX_COMM_MIN_SIZE];
    char m_schedule_time[MAX_TIMESTAMP_SIZE];
    char m_schedule_list[MAX_SCHEDULE_LIST_SIZE];
};

enum GVM_ERR_CODE {
    GVM_ERR_FAIL = -1,
    GVM_ERR_SUCCESS = 0,

    GVM_ERR_PARAM_INVALID,

    GVM_ERR_INTERNAL_FAIL,
    GVM_ERR_WAIT_TIMEOUT,

    GVM_ERR_TASK_NOT_FOUND,
    GVM_ERR_TASK_ALREADY_EXISTS,
    GVM_ERR_TASK_IS_BUSY,
    GVM_ERR_TASK_IS_NOT_RUNNING,

    GVM_ERR_OPENVAS_PROC_FAIL,

    GVM_ERR_REDIS_CONN,

    GVM_ERR_GVMD_CONN,
};


typedef struct php_key_task_param* php_key_task_param_t;
typedef struct php_create_task_param* php_create_task_param_t;


/*  send and wait for response in 15s, return 0: get response ok */  
extern int sendKbMsg(const char* key, char uuid[], const char* cmd, size_t len); 
extern int php_start_task(const char* input, int inputlen, kb_buf_t tmpbuf);
extern int php_stop_task(const char* input, int inputlen, kb_buf_t tmpbuf);
extern int php_delete_task(const char* input, int inputlen, kb_buf_t tmpbuf);
extern int php_create_task(const char* input, int inputlen, kb_buf_t tmpbuf);

extern int php_start_hydra(const char* input, int inputlen, kb_buf_t tmpbuf);
extern int php_stop_hydra(const char* input, int inputlen, kb_buf_t tmpbuf);
extern int php_delete_hydra(const char* input, int inputlen, kb_buf_t tmpbuf);
extern int php_create_hydra(const char* input, int inputlen, kb_buf_t tmpbuf);


/* param: ',' seperated uuids,
    return: 1: yes multi, 0:no
*/ 
extern int isMultiUuid(const char* ids);

extern int chkUuid(const char* text);
extern int chkHosts(const char* text);
extern int chkHostsExt(const char* text);
extern int chkName(const char* text);
extern int chkTimeStamp(const char* text);
extern int chkConfigInfo(const char* ids, const char* names);

extern int chkRspStatusOk(const char* cmd, const char* text);
extern int extractAttrUuid(const char* txt, const char* tag, char* uuid, int maxlen);
extern int extractTagUuid(const char* text, const char* tag, char* uuid, int maxlen);

/* return: >0: ok total cnt escaped, 0: error */ 
extern int escapeXml(const char* xml, char* out, int maxlen);

/* return: 0: ok, -1: err */ 
extern int escapeHosts(char* hosts);

extern int chkScheduleParam(int type, const char* schedule_time, 
    const char* schedule_list); 

extern int getPatternKey(const char* text, const char* key, 
    const char* pattern, char* val, int maxlen);

extern int getPatternKeyInt(const char* text, const char* key, 
    const char* pattern, int* val);

extern int getPhpKeyTaskParam(const char* text, php_key_task_param_t param,
    kb_buf_t tmpbuf);

extern int getPhpCreateTaskParam(const char* text, php_create_task_param_t param,
    kb_buf_t tmpbuf);

extern int getXmlTagVal(const char* text, const char* tag, char* buf, int maxlen);

extern int regmatch(const char text[], const char pattern[]);

extern int test_regmatch(const char text[], const char pattern[]);


/********** hydra operations ********/
struct php_key_hydra_param {
    char m_task_name[MAX_NAME_SIZE]; 
    char m_task_id[MAX_UUID_SIZE];
}; 

typedef struct php_key_hydra_param* php_key_hydra_param_t;

struct php_create_hydra_param {
    int m_hosts_type;
    char m_task_name[MAX_NAME_SIZE];
    char m_hosts[MAX_MEM_LIST_SIZE]; 
    char m_services[MAX_MEM_LIST_SIZE];
    char m_opts[MAX_MEM_LIST_SIZE]; 
    
    char m_login_list[MAX_MEM_LIST_SIZE]; 
    char m_passwd_list[MAX_MEM_LIST_SIZE];
    char m_schedule_type[MAX_COMM_MIN_SIZE];
    char m_schedule_time[MAX_TIMESTAMP_SIZE];
    char m_schedule_list[MAX_SCHEDULE_LIST_SIZE];
};

typedef struct php_create_hydra_param* php_create_hydra_param_t;


extern int chkLoginList(const char* text);
extern int chkPasswdList(const char* text);
extern int chkHydraHosts(int hosts_type, const char* hosts);
extern int chkServices(const char* text);

#ifdef __cplusplus
}
#endif

#endif

