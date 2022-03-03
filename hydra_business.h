#ifndef __HYDRA_BUSINESS_H__
#define __HYDRA_BUSINESS_H__
#include"base_openvas.h"
#include"llist.h"


#ifdef __cplusplus
extern "C" {
#endif

enum HOSTS_TYPE {
    HOSTS_IP_LIST,
    HOSTS_IP_BLOCK
};

enum HYDRA_PATH_TYPE {
    HYDRA_TASK_DIR,
    HYDRA_LOGIN_FILE,
    HYDRA_LOGIN_FILE_TMP,
    HYDRA_PASSWD_FILE,
    HYDRA_PASSWD_FILE_TMP,
    HYDRA_IP_LIST_FILE,
    HYDRA_IP_LIST_FILE_TMP,

    HYDRA_CRON_FILE_NORM,

    HYDRA_TASK_GRP_PID_FILE,

    HYDRA_PATH_END
};

struct ListHydraTask {
    struct LList m_mainlist; /* used by create queue, ***Must be the first item */
    char m_create_time[MAX_TIMESTAMP_SIZE];
    char m_modify_time[MAX_TIMESTAMP_SIZE];
    char m_task_id[MAX_UUID_SIZE];
    char m_task_name[MAX_NAME_SIZE];
    char m_hosts[MAX_MEM_LIST_SIZE]; 
    char m_services[MAX_MEM_LIST_SIZE];
    char m_opts[MAX_MEM_LIST_SIZE]; 
    char m_login_list[MAX_MEM_LIST_SIZE]; 
    char m_passwd_list[MAX_MEM_LIST_SIZE];
    char m_first_schedule_time[MAX_TIMESTAMP_SIZE];
    char m_schedule_list[MAX_SCHEDULE_LIST_SIZE];

    /* work dir and files, for temporary used */
    char m_paths[HYDRA_PATH_END][MAX_FILENAME_PATH_SIZE];
    
    enum ICAL_DATE_REP_TYPE m_schedule_type;
    enum HOSTS_TYPE m_hosts_type;
    int m_pid; // running process group pid of this task
};

typedef struct ListHydraTask* ListHydraTask_t; 


struct HydraTaskOperation;

struct HydraDataList {
    struct ListQueue m_createque; /* order by create time */
    struct ListSet m_nameset;    /* order by name uniqely */
    const struct HydraTaskOperation* task_ops; 
    char m_task_file_name[MAX_FILENAME_PATH_SIZE];
    char m_task_priv_dir[MAX_FILENAME_PATH_SIZE];
    char m_task_templ_dir[MAX_FILENAME_PATH_SIZE];
};

typedef struct HydraDataList* HydraDataList_t;

struct HydraTaskOperation { 
    ListHydraTask_t (*create_task)();
    void (*free_task)(ListHydraTask_t);
        
    int (*add_task)(HydraDataList_t, ListHydraTask_t task);
    int (*del_task)(HydraDataList_t, const ListHydraTask_t key); 
    
    ListHydraTask_t (*find_task)(HydraDataList_t, const ListHydraTask_t key);

    int (*write_task_file)(HydraDataList_t, kb_buf_t buffer);
    int (*read_task_file)(HydraDataList_t);

    int (*write_hydra_relation_files)(HydraDataList_t, ListHydraTask_t, kb_buf_t, kb_buf_t);
    int (*del_hydra_relation_files)(HydraDataList_t, ListHydraTask_t, kb_buf_t);

    int (*prepare_paths)(HydraDataList_t data, ListHydraTask_t task);
    int (*run_task)(HydraDataList_t, ListHydraTask_t, kb_buf_t);
};


#define CRON_LAST_MONTHDAY_CMD_PATT "[ `date -d tomorrow +\\%e` -eq 1 ]"
#define CRON_USER_NAME "root"


extern int daemon_start_hydra(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf);
extern int daemon_stop_hydra(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf);
extern int daemon_delete_hydra(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf);
extern int daemon_create_hydra(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf);

extern int getKeyHydraParam(const char* input, ListHydraTask_t task);
extern int chkHydraHostsIntern(int hosts_type, const char* hosts);

extern int monitorHydraTask();

extern void setArgs(int argc, char* argv[]);

extern int initHydra();
extern int finishHydra();


#ifdef __cplusplus
}
#endif 


#endif

