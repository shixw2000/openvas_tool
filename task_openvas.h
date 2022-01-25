#ifndef __TASK_OPENVAS_H__
#define __TASK_OPENVAS_H__
#include"base_openvas.h"


#ifdef __cplusplus
extern "C" {
#endif

#define CUSTOM_MATCH_OR_NULL(x) "("x")?"
#define CUSTOM_MATCH_OR_REPEAT(x) "("x")(,("x"))*" 
#define CUSTOM_MATCH_OR_REPEAT_EX(x) "("x")([,[:space:]]+("x"))*" 

#define CUSTOM_WHOLE_MATCH(x) "^"x"$"
#define CUSTOM_WHOLE_MATCH_OR_NULL(x) CUSTOM_WHOLE_MATCH(CUSTOM_MATCH_OR_NULL(x))
#define CUSTOM_WHOLE_MATCH_OR_REPEAT(x) CUSTOM_WHOLE_MATCH(CUSTOM_MATCH_OR_REPEAT(x)) 
#define CUSTOM_WHOLE_MATCH_OR_REPEAT_EX(x) CUSTOM_WHOLE_MATCH(CUSTOM_MATCH_OR_REPEAT_EX(x))


#define UUID_REG_PATTERN "[[:xdigit:]]{8}-[[:xdigit:]]{4}-[[:xdigit:]]{4}-[[:xdigit:]]{4}-[[:xdigit:]]{12}"
#define GVM_RSP_STATUS_OK_PATTERN "^<%s_response status=\"([0-9]{3})\" "

#define GVM_NAME_PATTERN "[^\\/#|'~`!^$,:;?\"&*%<>[:space:]]{1,128}"
#define CUSTOM_DIGIT_NUM_PATTERN "[0-9]{1,3}"
#define CUSTOM_TIME_STAMP_PATTERN "[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}"
#define DEF_XML_OK_RSP_MARK "<%s status=\"20[0-9]\" status_text=\"[^\">]*\" id=\"(%s)\"/?>" 

#define CUSTOM_WEEK_DAY_PATTERN "MO|TU|WE|TH|FR|SA|SU"
#define CUSTOM_MONTH_DAY_PATTERN "[12]?[0-9]|30|31|-1"
#define CUSTOM_IP_PATTERN "([12]?[0-9]{1,2})(\\.([12]?[0-9]{1,2})){3}"
#define CUSTOM_IP_RANGE_PATTERN CUSTOM_IP_PATTERN \
    "|" CUSTOM_IP_PATTERN "/[0-9]{1,2}" \
    "|" CUSTOM_IP_PATTERN"-"CUSTOM_IP_PATTERN \
    "|" CUSTOM_IP_PATTERN"-[0-9]{1,3}"

#define CUSTOM_WHOLE_MATCH_IPS_EX CUSTOM_WHOLE_MATCH_OR_REPEAT_EX(CUSTOM_IP_RANGE_PATTERN)
#define CUSTOM_WHOLE_MATCH_IPS_NORMAL CUSTOM_WHOLE_MATCH_OR_REPEAT(CUSTOM_IP_RANGE_PATTERN)


#define TOKEN_END_CHAR '\0'
#define DEF_WIN_CR_LF "\r\n"
#define DEF_EMPTY_STR "" 


/* file size must be not greater than 10M */ 
#define MAX_TASK_FILE_SIZE 0xA00000 


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

typedef const char* const_char_t; 

extern int sendKbMsg(const char* key, const char* cmd, size_t len); 
extern int php_start_task(const char* input, int inputlen, kb_buf_t tmpbuf);
extern int php_stop_task(const char* input, int inputlen, kb_buf_t tmpbuf);
extern int php_delete_task(const char* input, int inputlen, kb_buf_t tmpbuf);
extern int php_create_task(const char* input, int inputlen, kb_buf_t tmpbuf);

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
extern int escapeXml(const char* xml, char* out, int maxlen);
extern int escapeHosts(char* hosts);
extern int trimText(char* text);
extern int chkScheduleParam(int type, const char* schedule_time, 
    const char* schedule_list);

extern int getNextToken(const char* text, const char* needle,
    char* buf, int maxlen, const_char_t* saveptr);

/* return: 0: ok, 1: empty, -1: exceed max size, -2: not digit  */
extern int getNextTokenInt(const char* text, const char* needle,
    int* val, const_char_t* saveptr);

extern int getPatternKey(const char* text, const char* key, 
    const char* pattern, char* val, int maxlen);

extern int getPatternKeyInt(const char* text, const char* key, 
    const char* pattern, int* val);

extern int readTotalFile(const char name[], kb_buf_t cache);

extern int getPhpKeyTaskParam(const char* text, php_key_task_param_t param,
    kb_buf_t tmpbuf);

extern int getPhpCreateTaskParam(const char* text, php_create_task_param_t param,
    kb_buf_t tmpbuf);

extern int getXmlTagVal(const char* text, const char* tag, char* buf, int maxlen);

extern int regmatch(const char text[], const char pattern[]);

extern int test_regmatch(const char text[], const char pattern[]);

#ifdef __cplusplus
}
#endif

#endif

