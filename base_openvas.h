#ifndef __BASE_OPENVAS_H__
#define __BASE_OPENVAS_H__
#include<stdio.h>
#include<errno.h>
#include<string.h>


#ifdef __cplusplus
extern "C" {
#endif


#define ERRCODE errno
#define ERRMSG strerror(errno)

#define LOG_PATH_MARK "/usr/local/openvas/gvm/var/log/gvm/gvm_util_%s.log"

#if 1
#define LOG_DEBUG(format, ...) FileLog(1, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)  FileLog(0, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) FileLog(-1, format, ##__VA_ARGS__)
#else
#define LOG_DEBUG(format, ...) MyLog(1, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) MyLog(0, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) MyLog(-1, format, ##__VA_ARGS__) 
#endif

#define OPENVAS_MSG_NAME "openvas_msg_id"
#define OPENVAS_KB_DELIM "\x01"
#if 1
#define DEF_OPENVAS_PORT_LIST_ID "00000001-0000-0000-0000-000000000000"
#else
#define DEF_OPENVAS_PORT_LIST_ID "33d0cd82-57c6-11e1-8ed1-406186ea4fc5"
#endif
#define DEF_OPENVAS_CONFIG_GROUP_FORMAT "%08d-0000-0000-0000-000000000000"
#define DEF_OPENVAS_FULL_AND_FAST_CONFIG "daba56c8-73ec-11df-a475-002264764cea"

#define ARR_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))

typedef const char* const_char_t; 

struct kb_operations; 

struct kb {
  const struct kb_operations *kb_ops;
};

struct kb_buf {
    size_t m_capacity;
    size_t m_size;
    char* m_buf;
};

typedef struct kb *kb_t; 
typedef struct kb_buf* kb_buf_t;

struct kb_operations {
    kb_t (*kb_new)(); 
    kb_t (*kb_conn)();
    
    int (*kb_delete)(kb_t); 
    
    int (*kb_lnk_reset) (kb_t);

    int (*kb_expire)(kb_t, const char* name, int ttl);
    
    int (*kb_push_str)(kb_t, const char *, const char *, size_t len);

    /* 1: ok, 0:empty, -1:error */
    int (*kb_pop_str)(kb_t, const char *, kb_buf_t); 

    int (*kb_push_str_ttl)(kb_t, const char *, const char *, size_t len, int ttl);
    int (*kb_push_int_ttl)(kb_t, const char *, int val, int ttl);

    /* 1: ok, 0:empty, -1:error */
    int (*kb_bpop_str)(kb_t, const char *, kb_buf_t, int timeout); 
    int (*kb_bpop_int)(kb_t, const char *, int*, int timeout); 

    int (*kb_del_items)(kb_t, const char *);
};

enum { 
    MAX_CACHE_SIZE = 0x1000000, // 16M
    MAX_XML_SIZE = 2048,
    MAX_MSG_SIZE = 4096,
    MAX_UUID_SIZE = 64,
    MAX_GROUP_SIZE = 32,
    MAX_NAME_SIZE = 64,
    MAX_HOSTS_SIZE = 256,
    MAX_BUFFER_SIZE = 1024,
    MAX_COMM_SIZE = 256,
    MAX_COMM_MIN_SIZE = 32,
    MAX_TIMESTAMP_SIZE = 32,
    MAX_FILENAME_PATH_SIZE = 256,
    MAX_SCHEDULE_LIST_SIZE = 256,

    MAX_MEM_LIST_SIZE = 4096,

    DEF_DAY_SECONDS = 24 * 3600,

    MAX_CMD_PARAM_SIZE = 64,
    MAX_NVT_NAME_SIZE = 256, 
};

/* attensiont: must not be changed for anything */
#define TASK_INFO_BEG_MARK "\n==========begin========\n"

/* attensiont: must not be changed for anything */ 
#define TASK_INFO_END_MARK "\n==========end==========\n"


#define CUSTOM_COMM_PATTERN "[^\\'`\"]*"
#define CUSTOM_SCHEDULE_TYPE_PATTERN "[0-9]"
#define CUSTOM_SCHEDULE_TIME_MARK "%Y%m%dT%H%M%SZ"
#define CUSTOM_ON_OFF_PATTERN "[01]"
#define CUSTOM_PORT_PATTERN "([1-5]?[0-9]{1,4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[012][0-9]|6553[0-5])"
#define CUSTOM_SERVICES_PATTERN "[-[:alnum:],]*"


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
#define CUSTOM_MONTH_DAY_PATTERN "[1-9]|[12][0-9]|30|31|-1"
#define CUSTOM_IP_NUM "([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])"
#define CUSTOM_IP_PATTERN CUSTOM_IP_NUM"(\\."CUSTOM_IP_NUM"){3}"
#define CUSTOM_IP_RANGE_PATTERN CUSTOM_IP_PATTERN \
    "|" CUSTOM_IP_PATTERN "/[0-9]{1,2}" \
    "|" CUSTOM_IP_PATTERN"-"CUSTOM_IP_PATTERN \
    "|" CUSTOM_IP_PATTERN"-[0-9]{1,3}"

#define CUSTOM_WHOLE_MATCH_IPS_EX CUSTOM_WHOLE_MATCH_OR_REPEAT_EX(CUSTOM_IP_RANGE_PATTERN)
#define CUSTOM_WHOLE_MATCH_IPS_NORMAL CUSTOM_WHOLE_MATCH_OR_REPEAT(CUSTOM_IP_RANGE_PATTERN)

#define CUSTOM_HOST_OR_PORT(x) "("x")(:"CUSTOM_PORT_PATTERN")?"

#define CUSTORM_IP_OR_PORT CUSTOM_HOST_OR_PORT(CUSTOM_IP_PATTERN)
#define CUSTOM_WHOLE_MATCH_IP_LIST CUSTOM_WHOLE_MATCH_OR_REPEAT_EX(CUSTORM_IP_OR_PORT)
#define CUSTOM_WHOLE_MATCH_IP_LIST_INTERN CUSTOM_WHOLE_MATCH_OR_REPEAT(CUSTORM_IP_OR_PORT) 

#define CUSTOM_IP_BLOCK CUSTOM_IP_PATTERN"/(1[6-9]|2[0-9]|30|31)"
#define CUSTOM_IP_BLOCK_OR_PORT CUSTOM_HOST_OR_PORT(CUSTOM_IP_BLOCK)
#define CUSTOM_WHOLE_MATCH_IP_BLOCK CUSTOM_WHOLE_MATCH(CUSTOM_IP_BLOCK_OR_PORT)

#define TOKEN_END_CHAR '\0'
#define DEF_WIN_CR_LF "\r\n"
#define DEF_EMPTY_STR "" 


/* file size must be not greater than 10M */ 
#define MAX_TASK_FILE_SIZE 0xA00000 

#define MAX_REDIS_WAIT_TIMEOUT 15 // 15 s


enum ICAL_DATE_REP_TYPE {
    ICAL_DATE_NONE,
    ICAL_DATE_ONCE,
    ICAL_DATE_DAILY,
    ICAL_DATE_WEEKLY,
    ICAL_DATE_MONTHLY, 
    
    ICAL_DATE_END
};

extern const struct kb_operations *KBDefaultOperations;

static inline kb_t kb_create() {
    return KBDefaultOperations->kb_new();
}

static inline void kb_delete(kb_t kb) {
    KBDefaultOperations->kb_delete(kb);
}

static inline kb_t kb_conn() {
    return KBDefaultOperations->kb_conn();
}

static inline int kb_push_str(kb_t kb, const char *name, 
    const char *value, size_t len) {
    return kb->kb_ops->kb_push_str(kb, name, value, len);
}

static inline int kb_push_result_ttl(kb_t kb, const char *name,
    int result, int ttl) {
    return kb->kb_ops->kb_push_int_ttl(kb, name, result, ttl);
}

/* 1: ok, 0:empty, -1:error */ 
static inline int kb_bpop_result(kb_t kb, const char *name, int* presult, int timeout) {
    return kb->kb_ops->kb_bpop_int(kb, name, presult, timeout);
}

/* 1: ok, 0:empty, -1:error */ 
static inline int kb_pop_str(kb_t kb, const char *name, kb_buf_t output) {
    return kb->kb_ops->kb_pop_str(kb, name, output);
} 

static inline int kb_del_items(kb_t kb, const char *name) {
    return kb->kb_ops->kb_del_items(kb, name);
}


extern void initBuf(kb_buf_t buffer);
extern int genBuf(size_t len, kb_buf_t buffer);
extern void freeBuf(kb_buf_t buffer);

extern int getLogPath(const long long* time, char path[], int maxlen);
extern int MyLog(int level, const char format[], ...);
extern int FileLog(int level, const char format[], ...);
extern void setMyLogLevel(int level);

extern long long getTime();
extern long long getTimeLastDays(int nday);
extern long long getClkTime();

extern int time2asc(const long long* ptime, const char format[], 
    char psz[], int maxlen, int is_local);

extern int asc2time(long long* ptime, const char text[], 
    const char format[], int is_local);

extern int getTimeStamp(const long long* time, char psz[], int maxlen);
extern int nowTimeStamp(char psz[], int maxlen);
extern int utc2LocalTime(char local[], int maxlen, const char utc[]);
extern int local2SchedTime(char sched[], int maxlen, const char local[]);


#ifdef __cplusplus
}
#endif 

#endif

