#ifndef __TASK_OPENVAS_H__
#define __TASK_OPENVAS_H__
#include"base_openvas.h"


#ifdef __cplusplus
extern "C" {
#endif

#define UUID_REG_PATTERN "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"
#define UUID_REG_PATTERN_OR_NULL "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}|.?"
#define GVM_RSP_STATUS_OK_PATTERN "^<%s_response status=\"([0-9]{3})\" "

#define GVM_NAME_PATTERN "[^\\#|/'~`\"&*%<>]{1,128}"
#define CUSTOM_GROUP_ID_PATTERN "[0-9]{1,10}"
#define CUSTOM_HOSTS_PATTERN "[^[\\#|'~`\"&*<>(){}]{4,128}"
#define CUSTOM_DIGIT_NUM_PATTERN "[0-9]{1,3}"
#define CUSTOM_TIME_STAMP_PATTERN "[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}"
#define CUSTOM_UTC_TIME_STAMP_PATTERN_OR_NULL "[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z|.?"
#define DEF_XML_OK_RSP_MARK "<%s status=\"20[0-9]\" status_text=\"[^\">]*\" id=\"(%s)\"/?>"

#define CUSTOM_SCHEDULE_TYPE_PATTERN "[0-3]"
#define CUSTOM_SCHEDULE_TIME_PATTERN "[0-9]{8}T[0-9]{6}"
#define CUSTOM_SCHEDULE_LIST_PATTERN "[0-9a-zA-Z,-]{0,128}"



/* file size must be not greater than 10M */ 
#define MAX_TASK_FILE_SIZE 0xA00000 

/* php input parameters */
#define PHP_KEY_TASK_PARAMS_PATT "^taskname=\"(%s)\"&taskid=\"(%s)\"&targetid=\"(%s)\"$"


struct php_key_task_param {
    char m_task_name[MAX_NAME_SIZE]; 
    char m_task_id[MAX_UUID_SIZE];
    char m_target_id[MAX_UUID_SIZE];
};

struct php_create_task_param {
    char m_task_name[MAX_NAME_SIZE];
    char m_group_name[MAX_NAME_SIZE]; 
    char m_group_id[MAX_GROUP_SIZE];
    char m_hosts[MAX_HOSTS_SIZE];
    char m_schedule_type[MAX_COMM_MIN_SIZE];
    char m_schedule_time[MAX_TIMESTAMP_SIZE];
    char m_schedule_list[MAX_SCHEDULE_LIST_SIZE];
};


typedef struct php_key_task_param* php_key_task_param_t;
typedef struct php_create_task_param* php_create_task_param_t;

typedef const char* const_char_t; 

extern int sendKbMsg(const char* key, const char* cmd, size_t len); 
extern int php_start_task(const char* input, int inputlen);
extern int php_stop_task(const char* input, int inputlen);
extern int php_delete_task(const char* input, int inputlen);
extern int php_create_task(const char* input, int inputlen);

extern int getConfigId(int type, char* buf, int maxlen);
extern int getGroupId(const char* text, int* id);
extern int chkUuid(const char* text);
extern int chkHosts(const char* text);
extern int chkName(const char* text);
extern int chkRspStatusOk(const char* cmd, const char* text);
extern int extractAttrUuid(const char* txt, const char* tag, char* uuid, int maxlen);
extern int extractTagUuid(const char* text, const char* tag, char* uuid, int maxlen);
extern int escapeXml(const char* xml, char* out, int maxlen);
extern int escapeHosts(char* hosts);
extern int trimText(char* text);

extern int getNextToken(const char* text, const char* needle,
    char* buf, int maxlen, const_char_t* saveptr);

extern int getPatternKey(const char* text, const char* key, 
    const char* pattern, char* val, int maxlen);
extern int readTotalFile(const char name[], kb_buf_t cache);

extern int getPhpKeyTaskParam(const char* text, php_key_task_param_t param);

extern int getXmlTagVal(const char* text, const char* tag, char* buf, int maxlen);


#ifdef __cplusplus
}
#endif

#endif

