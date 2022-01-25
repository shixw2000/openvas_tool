#ifndef __OPENVAS_BUSINESS_H__
#define __OPENVAS_BUSINESS_H__
#include"base_openvas.h"
#include"llist.h"


#ifdef __cplusplus
extern "C" {
#endif

enum GVM_TASK_STATUS {
    /* must be the begin */
    GVM_TASK_CREATE = 0,
    
    GVM_TASK_READY,
    GVM_TASK_WAITING,
    GVM_TASK_RUNNING,
    GVM_TASK_DONE, 
    GVM_TASK_STOP,
    GVM_TASK_INTERRUPT,

    /* must be the end */
    GVM_TASK_ERROR
};

enum GVM_TASK_CHK_TYPE {
    GVM_TASK_CHK_TASK,
    GVM_TASK_CHK_REPORT,
    GVM_TASK_CHK_RESULT,

    GVM_TASK_CHK_DELETED,

    
    GVM_TASK_CHK_END
}; 


struct gvm_target_info {
    char m_target_name[MAX_NAME_SIZE];
    char m_target_id[MAX_UUID_SIZE];
    char m_portlist_id[MAX_UUID_SIZE];
    char m_hosts[MAX_MEM_LIST_SIZE];
};

struct gvm_task_info {
    enum ICAL_DATE_REP_TYPE m_schedule_type;
    unsigned char m_config_created;
    unsigned char m_schedule_created;
    unsigned char m_res[2];
    char m_create_time[MAX_TIMESTAMP_SIZE];
    char m_modify_time[MAX_TIMESTAMP_SIZE];
    char m_task_id[MAX_UUID_SIZE];
    char m_task_name[MAX_NAME_SIZE];
    char m_group_id[MAX_MEM_LIST_SIZE];
    char m_group_name[MAX_MEM_LIST_SIZE];
    char m_config_id[MAX_UUID_SIZE];
    char m_schedule_id[MAX_UUID_SIZE];
    char m_first_schedule_time[MAX_TIMESTAMP_SIZE];
    char m_schedule_list[MAX_SCHEDULE_LIST_SIZE];
};

typedef struct gvm_task_info* gvm_task_info_t;

struct gvm_result_info {
    int m_totalCnt;
    int m_first;
    int m_rows;
    int m_done; // check if is end
    struct ListQueue m_results; /* nvt results queue */ 
};

typedef struct gvm_result_info* gvm_result_info_t;

struct gvm_report_info {
    int m_report_cnt;
    int m_finish_cnt;
    int m_progress; /* [0, 100]*/
    enum GVM_TASK_STATUS m_status;
    char m_start_time[MAX_TIMESTAMP_SIZE];
    char m_stop_time[MAX_TIMESTAMP_SIZE]; 
    char m_cur_report_id[MAX_UUID_SIZE];
    char m_last_report_id[MAX_UUID_SIZE];
};

typedef struct gvm_report_info* gvm_report_info_t;

struct ListGvmTask {
    struct LList m_mainlist; /* used by create queue, ***Must be the first item */
    struct LList m_runlist; /* used by running queue */
    struct gvm_task_info m_task_info;
    struct gvm_target_info m_target_info;
    struct gvm_report_info m_report_info;
    struct gvm_result_info m_result_info;
    
    enum GVM_TASK_CHK_TYPE m_chk_type;
    long long m_chk_time; 
};

typedef struct ListGvmTask* ListGvmTask_t; 

struct ResultNvtInfo {
    char res_id[MAX_UUID_SIZE];
    char res_name[MAX_NAME_SIZE];
    char res_host[MAX_HOSTS_SIZE];
    char res_threat[MAX_COMM_MIN_SIZE];
    char res_severity[MAX_COMM_MIN_SIZE];
    char res_create_time[MAX_TIMESTAMP_SIZE];
    char res_port[MAX_COMM_MIN_SIZE];
    char res_cve[MAX_COMM_MIN_SIZE];
};

#define RESULT_NVT_ITEM_CNT 8

typedef struct ResultNvtInfo* ResultNvtInfo_t;

struct ListNvtInfo {
    struct LList m_list;
    struct ResultNvtInfo m_nvtinfo;
};

typedef struct ListNvtInfo* ListNvtInfo_t;


#define offsetof(type,mem) ((size_t)(&((type*)0)->mem))
#define containof(ptr,type,mem) ((type*)((char*)(ptr) - offsetof(type,mem)))
#define runlist_gvm_task(_list) containof(_list, struct ListGvmTask, m_runlist)
#define gvm_task_runlist(task)  (&(task)->m_runlist)
#define DEF_CHK_PROGRESS_INTERVAL 30 /* 120 sec */


/* task config file */
#define DEF_GVM_TASK_FILE_NAME "gvm_task_file"

/* task status file */
#define DEF_GVM_TASK_STATUS_FILE_PATT "task_%s"

/* result file */
#define DEF_GVM_TASK_RESULT_FILE_PATT "result_%s"


#define DEF_GVM_PRIV_DATA_DIR "/usr/local/openvas/gvm/var/private"
#define ENV_LD_NAME "LD_LIBRARY_PATH"
#define ENV_LD_VAL "/usr/local/openvas/depends/lib"
#define DEF_GVM_LOG_DIR "/usr/local/openvas/gvm/var/log/gvm/"
#define DEF_LOG_FILE_NAME "daemon_openvas.log"
#define DEF_RUNLOG_FILE_NAME "run_openvas.log"


struct GvmTaskOperation;

struct GvmDataList {
    struct ListQueue m_createque; /* order by create time */
    struct ListQueue m_runque;   /* order by last running time */
    struct ListSet m_nameset;    /* order by name uniqely */
    const struct GvmTaskOperation* task_ops; 
    char m_task_file_name[MAX_FILENAME_PATH_SIZE];
    char m_task_priv_dir[MAX_FILENAME_PATH_SIZE];
    int m_is_gvm_conn_ok;
};

typedef struct GvmDataList* GvmDataList_t;

struct GvmTaskOperation { 
    ListGvmTask_t (*create_task)();
    void (*free_task)(ListGvmTask_t);
        
    int (*add_task)(GvmDataList_t, ListGvmTask_t task);
    int (*del_task)(GvmDataList_t, const ListGvmTask_t key);

    int (*add_run_task)(GvmDataList_t, ListGvmTask_t task);
    int (*del_run_task)(GvmDataList_t, ListGvmTask_t task);
    int (*reque_run_task)(GvmDataList_t, ListGvmTask_t task);
    
    ListGvmTask_t (*find_task)(GvmDataList_t, const ListGvmTask_t key);

    int (*write_task_file)(GvmDataList_t, kb_buf_t buffer);
    int (*read_task_file)(GvmDataList_t);

    int (*write_task_status_file)(GvmDataList_t, ListGvmTask_t task, kb_buf_t buffer);
    int (*read_task_status_file)(GvmDataList_t, ListGvmTask_t task);
    int (*read_all_task_status_recs)(GvmDataList_t);

    int (*write_task_result_file)(GvmDataList_t, ListGvmTask_t task, kb_buf_t outbuf);

    int (*del_task_relation_files)(GvmDataList_t, ListGvmTask_t task);

    /* return: 1-running, 0: not running */
    int (*chk_task_running)(GvmDataList_t, ListGvmTask_t task);

    /* return : 1-completed, 0-not completed */
    int (*chk_task_done)(GvmDataList_t, ListGvmTask_t task);

    /* return: 1-busy, 0: idle */
    int (*chk_task_busy)(GvmDataList_t, ListGvmTask_t task);

    void (*print_task)(const ListGvmTask_t task);
    void (*print_all_tasks)(GvmDataList_t, int);

    int (*is_gvm_conn_ok)(GvmDataList_t);
};

extern void setTaskStatus(ListGvmTask_t task, enum GVM_TASK_STATUS status);
extern void setTaskProgress(ListGvmTask_t task, int progress); 
extern enum GVM_TASK_STATUS getTaskStatus(const ListGvmTask_t task);

extern void setTaskChkType(ListGvmTask_t task, enum GVM_TASK_CHK_TYPE type);
extern enum GVM_TASK_CHK_TYPE getTaskChkType(const ListGvmTask_t task);

extern int setTaskStartTime(ListGvmTask_t task, const char* time);
extern int setTaskStopTime(ListGvmTask_t task, const char* time); 

extern GvmDataList_t createData();
extern int finishData(GvmDataList_t);

extern int initDaemon();
extern int finishDaemon();
extern int setBackgroud();
extern int isRun();

extern int startKbMsg();
extern int stopKbMsg();

/* return 0:ok, 1:status not ok, -1:fail */ 
extern int procGvmTask(const char* cmd, kb_buf_t cmdbuf, kb_buf_t output);

extern int daemon_start_task(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf);
extern int daemon_stop_task(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf);
extern int daemon_delete_task(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf);
extern int daemon_create_task(char* input, int inputlen, kb_buf_t tmpbuf, kb_buf_t outbuf);

extern int gvm_query_task(const char* uuid, kb_buf_t tmpbuf, kb_buf_t outbuf);

extern int gvm_query_result(const char* taskid, const char* reportid, 
    int first, int rows, kb_buf_t tmpbuf, kb_buf_t outbuf);

extern int gvm_query_report(const char* uuid, kb_buf_t tmpbuf, kb_buf_t outbuf);

extern void printResult(kb_buf_t buffer);

extern int getKeyTaskParam(const char* input, ListGvmTask_t task);

#ifdef __cplusplus
}
#endif 


#endif

