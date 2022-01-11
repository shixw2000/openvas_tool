#include<string.h>
#include<stdarg.h>
#include<stdlib.h> 
#include<errno.h>
#include<unistd.h> 
#include<hiredis.h> 
#include<time.h>
#include"base_openvas.h"


struct kb_redis {
    struct kb kb;
    redisContext* rctx;
    const char* path;
};

#define redis_kb(__kb) ((struct kb_redis *)(__kb))


enum { 
    MAX_PATH_SIZE = 256,
};

static const char* g_redis_unix_path = "/usr/local/openvas/gvm/run/redis/redis.sock";


static int redis_lnk_reset (kb_t kb) {
    struct kb_redis *kbr;

    kbr = redis_kb (kb);
    if (kbr->rctx != NULL) {
        redisFree (kbr->rctx);
        kbr->rctx = NULL;
    }

    return 0;
}

static int redis_test_connection (struct kb_redis *kbr) {
    int rc = 0;
    redisReply *rep = NULL;

    rep = (redisReply*)redisCommand (kbr->rctx, "PING");
    if (rep == NULL) {
        rc = -ECONNREFUSED;
    } else if (rep->type != REDIS_REPLY_STATUS) {
        rc = -EINVAL;
    } else if (strcasecmp (rep->str, "PONG")) {
        rc = -EPROTO;
    }

    if (rep != NULL) {
        freeReplyObject (rep);
    }

    return rc;
}

static int get_redis_ctx (struct kb_redis *kbr) {
    int ret = 0;
    
    if (kbr->rctx != NULL) {
        return 0;
    }

    kbr->rctx = redisConnectUnix (kbr->path);
    if (kbr->rctx == NULL || kbr->rctx->err) {
        LOG_ERROR("%s: redis connection error to %s: %s", 
            __func__, kbr->path,
            kbr->rctx ? kbr->rctx->errstr : strerror (ENOMEM));

        if (NULL != kbr->rctx) {
            redisFree (kbr->rctx);
            kbr->rctx = NULL;
        }
        
        return -1;
    }

    ret = redis_test_connection(kbr);
    if (0 != ret) {
        redisFree (kbr->rctx);
        kbr->rctx = NULL;

        return ret;
    }
    
    return 0;
}

static redisReply* redis_cmd (struct kb_redis *kbr, const char *fmt, ...) {
    redisReply *rep = NULL;
    va_list ap, aq;
    int retry = 0;

    va_start (ap, fmt);
    do {
        if (0 != get_redis_ctx (kbr)) {
            va_end (ap);
            return NULL;
        }

        va_copy (aq, ap);
        rep = (redisReply*)redisvCommand (kbr->rctx, fmt, aq);
        va_end (aq);

        if (kbr->rctx->err) {
            if (rep != NULL) {
                freeReplyObject (rep);
                rep = NULL;
            }

            redis_lnk_reset((kb_t)kbr);
      
            retry = !retry;
        } else {
            retry = 0;
        }
    } while (retry);

    va_end (ap);
    return rep;
}

static int redis_delete (kb_t kb) {
    struct kb_redis *kbr = NULL;

    kbr = redis_kb (kb); 
    if (kbr->rctx != NULL) {
        redisFree (kbr->rctx);
        kbr->rctx = NULL;
    }

    free(kb);
    return 0;
}

static kb_t redis_new () {
    struct kb_redis *kbr = NULL;

    kbr = (struct kb_redis*)calloc (1, sizeof(struct kb_redis)); 
    if (NULL != kbr) { 
        kbr->path = g_redis_unix_path;
        kbr->kb.kb_ops = KBDefaultOperations;
        kbr->rctx = NULL;

        return (kb_t)kbr;
    } else { 
        return NULL;
    }
}

static kb_t redis_conn () {
    int rc = 0;
    struct kb_redis *kbr = NULL;

    kbr = (struct kb_redis*)calloc (1, sizeof(struct kb_redis)); 
    if (NULL != kbr) { 
        kbr->path = g_redis_unix_path;
        kbr->kb.kb_ops = KBDefaultOperations;
        kbr->rctx = NULL;

        rc = get_redis_ctx(kbr);
        if (0 == rc) {
            return (kb_t)kbr; 
        } else { 
            redis_delete((kb_t)kbr);
        }
    }
    
    return NULL;
}


static int redis_push_str (kb_t kb, const char *name, const char *value, size_t len) {
    struct kb_redis *kbr = NULL;
    redisReply *rep = NULL;
    int rc = 0;

    kbr = redis_kb (kb);
    if (0 == len) {
        rep = redis_cmd (kbr, "LPUSH %s %s", name, value);
    } else {
        rep = redis_cmd (kbr, "LPUSH %s %b", name, value, len);
    }
    if (!rep || rep->type == REDIS_REPLY_ERROR) {
        rc = -1;
    }

    if (rep) {
        freeReplyObject (rep);
    }

    return rc;
}

/* 1: ok, 0:empty, -1:error */ 
static int redis_pop_str (kb_t kb, const char *name, kb_buf_t output) {
    int ret = 0;
    struct kb_redis *kbr = NULL;
    redisReply *rep = NULL;

    kbr = redis_kb (kb);
    rep = redis_cmd (kbr, "RPOP %s", name); 
    if (NULL != rep) { 
        if (rep->type == REDIS_REPLY_STRING && 0 < rep->len) {
            if (rep->len <= output->m_capacity) {
                memcpy(output->m_buf, rep->str, rep->len);
                output->m_buf[rep->len] = '\0';
                output->m_size = rep->len;

                ret = 1;
            } else {
                LOG_ERROR("%s: too long msg size[%d]", __FUNCTION__, (int)rep->len);

                output->m_buf[0] = '\0';
                output->m_size = 0;
                ret = -1;
            }
            
        } else {
            output->m_buf[0] = '\0';
            output->m_size = 0;
            
            /* no data */
            ret = 0;
        }

        freeReplyObject (rep);
    } else {
        output->m_buf[0] = '\0';
        output->m_size = 0;

        /* error */
        ret = -1;
    }

    return ret;
}

/* 1: ok, 0:empty, -1:error */ 
static int redis_bpop_str (kb_t kb, const char *name, kb_buf_t output) {
    int ret = 0;
    struct kb_redis *kbr = NULL;
    redisReply *rep = NULL;

    kbr = redis_kb (kb);
    rep = redis_cmd (kbr, "BRPOP %s 1", name); 
    if (NULL != rep) { 
        if (rep->type == REDIS_REPLY_ARRAY && 2 == rep->elements
            && REDIS_REPLY_STRING == rep->element[1]->type
            && 0 < rep->element[1]->len) {

            if (rep->element[1]->len <= output->m_capacity) {
                memcpy(output->m_buf, rep->element[1]->str, rep->element[1]->len);
                output->m_buf[rep->element[1]->len] = '\0';
                output->m_size = rep->element[1]->len;

                ret = 1;
            } else {
                LOG_ERROR("%s: too long msg size[%d]", __FUNCTION__, 
                    (int)rep->element[1]->len);

                output->m_buf[0] = '\0';
                output->m_size = 0;
                ret = -1;
            }
        } else {
            output->m_buf[0] = '\0';
            output->m_size = 0;

            /* no data */
            ret = 0;
        }

        freeReplyObject (rep);
    } else {
        output->m_buf[0] = '\0';
        output->m_size = 0;
        
        /* error */
        ret = -1;
    }

    return ret;
}

static int redis_del_items (kb_t kb, const char *name) {
    struct kb_redis *kbr = NULL;
    redisReply *rep = NULL;
    int rc = 0;

    kbr = redis_kb (kb); 
    rep = redis_cmd (kbr, "DEL %s", name);
    if (rep == NULL || rep->type == REDIS_REPLY_ERROR) {
        rc = -1;
    }

    if (rep != NULL) {
        freeReplyObject (rep);
    }

    return rc;
}


static const struct kb_operations KBRedisOperations = {
    redis_new,
    redis_conn,
    
    redis_delete, 
    
    redis_lnk_reset,

    redis_push_str,
    redis_pop_str,
    redis_bpop_str,

    redis_del_items,
};

const struct kb_operations *KBDefaultOperations = &KBRedisOperations;


int genBuf(size_t len, kb_buf_t buffer) {    
    buffer->m_buf = (char*)calloc(1, len + 1);
    if (NULL != buffer->m_buf) {
        buffer->m_capacity = len;
        buffer->m_size = 0;
        return 0;
    } else {
        return -1;
    }
}

void freeBuf(kb_buf_t buffer) {
    if (NULL != buffer->m_buf) {
        buffer->m_capacity = 0;
        buffer->m_size = 0;
        buffer->m_buf = NULL; 
    }
}

/* -1: err, 0:info, 1:debug */
static int g_log_level = 0;

void setMyLogLevel(int level) {
    if (0 < level) {
        g_log_level = 1;
    } else if (0 > level) {
        g_log_level = -1;
    } else {
        g_log_level = 0;
    }
}

static const char* DEF_LOG_NAME[] = {
    "ERROR", // 0
    "INFO",
    "DEBUG"
};

static char g_buffer[MAX_CACHE_SIZE] = {0};

int MyLog(int level, const char format[], ...) {
    size_t left = 0;
    int size = 0;
    int cnt = 0;
    va_list ap;
    char timestamp[MAX_TIMESTAMP_SIZE] = {0};

    if (level <= g_log_level) {
        getTimeStamp(timestamp, MAX_TIMESTAMP_SIZE);
        
        left = ARR_SIZE(g_buffer) - 2;
        
        cnt = snprintf(&g_buffer[size], left, "<%s>[%s]:",
            DEF_LOG_NAME[level+1], timestamp);

        size += cnt;
        left -= cnt;
        
        va_start(ap, format);
        cnt = vsnprintf(&g_buffer[size], left, format, ap);
        va_end(ap); 

        if (0 < cnt && cnt < left) {
            size += cnt;
            left -= cnt;
            
            g_buffer[size++] = '\n'; 
            g_buffer[size] = '\0';

            cnt = fwrite(g_buffer, 1, size, stdout);
            return 0;
        }
    }

    return -1;
}

int FileLog(int level, const char format[], ...) {
    size_t left = 0;
    int size = 0;
    int cnt = 0;
    FILE* file = NULL;
    va_list ap;
    char timestamp[MAX_TIMESTAMP_SIZE] = {0};

    if (level <= g_log_level) {
        getTimeStamp(timestamp, MAX_TIMESTAMP_SIZE);
        
        left = ARR_SIZE(g_buffer) - 2;
        
        cnt = snprintf(&g_buffer[size], left, "<%s>[%s]:",
            DEF_LOG_NAME[level+1], timestamp);

        size += cnt;
        left -= cnt;
        
        va_start(ap, format);
        cnt = vsnprintf(&g_buffer[size], left, format, ap);
        va_end(ap); 

        if (0 < cnt && cnt < left) {
            size += cnt;
            left -= cnt;

            g_buffer[size++] = '\n'; 
            g_buffer[size] = '\0';

            file = fopen(LOG_PATH, "ab");
            if (NULL != file) { 
                cnt = fwrite(g_buffer, 1, size, file); 
            
                fclose(file);

                return 0;
            } 
        }
    }

    return -1;
}

long long getTime() {
    return time(NULL);    
}

long long getClkTime() {
    struct timespec res = {0, 0};

    clock_gettime(CLOCK_MONOTONIC, &res);
    return res.tv_sec;
} 

int time2asc(long long time, char psz[], int maxlen) {
    int cnt = 0;
    time_t tmpTime = 0;
    struct tm local;
    struct tm* p = NULL;

    tmpTime = (time_t)time;
    p = localtime_r(&tmpTime, &local);
    if (NULL != p) {
        cnt = strftime(psz, maxlen, "%Y-%m-%d %H:%M:%S", &local);
        if (0 < cnt && cnt < maxlen) {
            psz[cnt] = '\0';
            return 0;
        } 
    }

    LOG_ERROR("%s: time=%lld| maxlen=%d| msg=time to ascii error:%s|",
        __FUNCTION__, time, maxlen, ERRMSG);

    psz[0] = '\0';
    return -1;
}

int getTimeStamp(char psz[], int maxlen) {
    int ret = 0; 
    long long timestamp = 0;

    timestamp = getTime();
    if (0 < timestamp) {
        ret = time2asc(timestamp, psz, maxlen);
    } else {
        psz[0] = '\0';
        ret = -1;
    }
    
    return ret;
}


