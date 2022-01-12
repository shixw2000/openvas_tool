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

#define LOG_PATH "/usr/local/openvas/gvm/var/log/gvm/gvm_util.log"

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
    
    int (*kb_push_str)(kb_t, const char *, const char *, size_t len);

    /* 1: ok, 0:empty, -1:error */
    int (*kb_pop_str)(kb_t, const char *, kb_buf_t); 
    int (*kb_bpop_str)(kb_t, const char *, kb_buf_t); 

    int (*kb_del_items)(kb_t, const char *);
};

enum { 
    MAX_CACHE_SIZE = 0x1000000, // 16M
    MAX_XML_SIZE = 2048,
    MAX_MSG_SIZE = 4096,
    MAX_UUID_SIZE = 64,
    MAX_GROUP_SIZE = 32,
    MAX_TASK_NAME_SIZE = 256,
    MAX_NAME_SIZE = MAX_TASK_NAME_SIZE,
    MAX_HOSTS_SIZE = 256,
    MAX_BUFFER_SIZE = 1024,
    MAX_COMM_SIZE = 256,
    MAX_COMM_MIN_SIZE = 32,
    MAX_TIMESTAMP_SIZE = 32,
    MAX_FILENAME_PATH_SIZE = 256,
    MAX_SCHEDULE_LIST_SIZE = 256,
};

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

static inline int kb_push_str(kb_t kb, const char *name, const char *value, size_t len) {
    return kb->kb_ops->kb_push_str(kb, name, value, len);
}

/* 1: ok, 0:empty, -1:error */ 
static inline int kb_bpop_str(kb_t kb, const char *name, kb_buf_t output) {
    return kb->kb_ops->kb_bpop_str(kb, name, output);
}

/* 1: ok, 0:empty, -1:error */ 
static inline int kb_pop_str(kb_t kb, const char *name, kb_buf_t output) {
    return kb->kb_ops->kb_pop_str(kb, name, output);
} 

static inline int kb_del_items(kb_t kb, const char *name) {
    return kb->kb_ops->kb_del_items(kb, name);
}


extern int genBuf(size_t len, kb_buf_t buffer);
extern void freeBuf(kb_buf_t buffer);
extern int MyLog(int level, const char format[], ...);
extern int FileLog(int level, const char format[], ...);
extern void setMyLogLevel(int level);

extern long long getTime();
extern long long getClkTime();

extern int time2asc(long long time, char psz[], int maxlen);
extern int getTimeStamp(char psz[], int maxlen);

#ifdef __cplusplus
}
#endif 

#endif

