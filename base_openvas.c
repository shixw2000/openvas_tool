#include<string.h>
#include<stdarg.h>
#include<stdlib.h> 
#include<errno.h>
#include<sys/types.h>
#include<sys/stat.h> 
#include<unistd.h> 
#include<hiredis.h>

#define _XOPEN_SOURCE 
#define __USE_XOPEN
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

        free(buffer->m_buf);
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

static int writeLog(const char path[], const kb_buf_t buff) {
    int cnt = 0;
    FILE* file = NULL; 

    if (NULL != path && '\0' != path[0]) {
        file = fopen(path, "ab");
        if (NULL != file) { 
            cnt = fwrite(buff->m_buf, 1, buff->m_size, file); 
            fclose(file);
        } 
    }

    return cnt;
}

static int formatLog(const long long* time, kb_buf_t buff,
    int level, const char format[], va_list ap) {
    int total = 0;
    int cnt = 0;
    int left = 0;
    char timestamp[MAX_TIMESTAMP_SIZE] = {0};

    total = 0;
    left = (int)(buff->m_capacity - 2);
    
    getTimeStamp(time, timestamp, ARR_SIZE(timestamp)); 
    
    cnt = (int)snprintf(&buff->m_buf[total], left, "<%s>[%s]:",
        DEF_LOG_NAME[level+1], timestamp);

    total += cnt;
    left -= cnt;
    
    cnt = (int)vsnprintf(&buff->m_buf[total], left, format, ap); 
    if (0 < cnt && cnt < left) {
        total += cnt;
        left -= cnt;

        buff->m_buf[ total++ ] = '\n'; 
    } else if (cnt >= left) {
        /* truncate too long text */
        total += left;
        left = 0;

        buff->m_buf[ total++ ] = '\n'; 
    } else {
        /* ignore empty text */
        total = 0; 
    }

    buff->m_buf[total] = '\0'; 
    buff->m_size = total;
    return total;
}

int getLogPath(const long long* time, char path[], int maxlen) {
    int ret = 0;
    char date[MAX_TIMESTAMP_SIZE] = {0}; 
    
    time2asc(time, "%Y%m%d", date, ARR_SIZE(date), 1);

    ret = snprintf(path, maxlen, LOG_PATH_MARK, date);
    if (ret < maxlen) {
        return 0;
    } else {
        path[0] = '\0';
        return -1;
    }
}

int FileLog(int level, const char format[], ...) { 
    int ret = 0;
    int cnt = 0;
    long long time = 0;
    va_list ap;
    struct kb_buf buff;
    char path[MAX_FILENAME_PATH_SIZE] = {0};

    if (level <= g_log_level) {
        ret = genBuf(MAX_CACHE_SIZE, &buff);
        if (0 == ret) {
            time = getTime();
            
            va_start(ap, format);
            cnt = formatLog(&time, &buff, level, format, ap);
            va_end(ap); 

            if (0 < cnt) {
                ret = getLogPath(&time, path, ARR_SIZE(path));
                if (0 == ret) {
                    cnt = writeLog(path, &buff);
                } else {
                    cnt = 0;
                }
            } 

            freeBuf(&buff);
        }
    }

    return cnt;
}

int MyLog(int level, const char format[], ...) {
    int ret = 0;
    int cnt = 0;
    long long time = 0;
    va_list ap;
    struct kb_buf buff;

    if (level <= g_log_level) {
        ret = genBuf(MAX_CACHE_SIZE, &buff);
        if (0 == ret) {
            time = getTime();
            
            va_start(ap, format);
            cnt = formatLog(&time, &buff, level, format, ap);
            va_end(ap); 

            if (0 < cnt) {
                cnt = fwrite(buff.m_buf, 1, buff.m_size, stdout);
            } 

            freeBuf(&buff);
        }
    }

    return cnt;
}


long long getTime() {
    return time(NULL);    
}

long long getTimeLastDays(int nday) { 
    long long time = 0;

    time = getTime() - DEF_DAY_SECONDS * nday;
    if (0 < time) {
        return time;
    } else {
        return 0;
    }    
} 

long long getClkTime() {
    struct timespec res = {0, 0};

    clock_gettime(CLOCK_MONOTONIC, &res);
    return res.tv_sec;
} 

int time2asc(const long long* ptime, const char format[], 
    char psz[], int maxlen, int is_local) {
    int cnt = 0;
    time_t tmpTime = 0;
    struct tm tm;
    struct tm* p = NULL;

    if (NULL != ptime && NULL != psz) {
        memset(&tm, 0, sizeof(struct tm));
        
        tmpTime = (time_t)*ptime; 
        if (is_local) {
            p = localtime_r(&tmpTime, &tm);
        } else {
            p = gmtime_r(&tmpTime, &tm);
        }
        
        if (NULL != p) {
            cnt = strftime(psz, maxlen, format, p);
            if (0 < cnt && cnt < maxlen) {
                psz[cnt] = '\0';
                return 0;
            } else {
                LOG_ERROR("%s: time=%lld| maxlen=%d| msg=buf size[%d] exceeds maxlen|",
                    __FUNCTION__, *ptime, maxlen, cnt);

                psz[0] = '\0';
                return -1;
            }
        } else {
            LOG_ERROR("%s: time=%lld| maxlen=%d| msg=time to ascii error:%s|",
                __FUNCTION__, *ptime, maxlen, ERRMSG);

            psz[0] = '\0';
            return -1;
        }
    } else {
        LOG_ERROR("%s: error=NULL_PTR|", __FUNCTION__);

        psz[0] = '\0';
        return -1;
    }
}

int getTimeStamp(const long long* time, char psz[], int maxlen) {
    int ret = 0; 
    
    if (NULL != time && 0 < *time) {
        ret = time2asc(time, "%Y-%m-%d %H:%M:%S", psz, maxlen, 1);
    } else {
        psz[0] = '\0';
        ret = -1;
    }
    
    return ret;
}

int nowTimeStamp(char psz[], int maxlen) {
    int ret = 0; 
    long long time = 0;

    time = getTime();
    ret = time2asc(&time, "%Y-%m-%d %H:%M:%S", psz, maxlen, 1);
    
    return ret;
} 

int asc2time(long long* ptime, const char text[], 
    const char format[], int is_local) {
    int ret = 0;
    struct tm tm;
    char* psz = NULL;

    if (NULL != ptime) {
        memset(&tm, 0, sizeof(struct tm));
        
        psz = strptime(text, format, &tm);
        if (NULL != psz) {
            if (is_local) {
                *ptime = (long long)timelocal(&tm);
            } else {
                *ptime = (long long)timegm(&tm);
            }
        } else {
            LOG_ERROR("%s: text=%s| format=%s| error=convert time fail|",
                __FUNCTION__, text, format);

            ret = -1;
        }
    } else {
        LOG_ERROR("%s: error=NULL_PTR|", __FUNCTION__);
        ret = -1;
    } 

    return ret;
}

int utc2LocalTime(char local[], int maxlen, const char utc[]) {
    int ret = 0;
    long long time = 0L;

    local[0] = '\0';
    
    /* allow empty utc */
    if (NULL != utc && '\0' != utc[0]) {
        ret = asc2time(&time, utc, "%Y-%m-%dT%H:%M:%SZ", 0);
        if (0 == ret) {
            ret = time2asc(&time, "%Y-%m-%d %H:%M:%S", local, maxlen, 1);
        }
    }

    return ret;
}

int local2SchedTime(char sched[], int maxlen, const char local[]) {
    int ret = 0;
    long long time = 0L;

    sched[0] = '\0';
    
    /* allow empty local */
    if (NULL != local && '\0' != local[0]) {
        ret = asc2time(&time, local, "%Y-%m-%d %H:%M:%S", 1);
        if (0 == ret) {
            ret = time2asc(&time, CUSTOM_SCHEDULE_TIME_MARK, sched, maxlen, 0);
        }
    } 

    return ret;
}

/* delete a file(if symbolic, then the link itself), but not a directory
    return: 0-delete, 1:no file, -2: fail for directory, -1: error
*/
int deleteFile(const char path[]) {
    int ret = 0;
    struct stat buf;

    ret = lstat(path, &buf);
    if (0 == ret) {
        if (!S_ISDIR(buf.st_mode)) {
            ret = unlink(path);
            if (0 != ret) {
                ret = -1;
            }
        } else {
            ret = -2;
        }
    } else if (ENOENT == errno) {
        /* file not exists */
        ret = 1;
    } else {
        ret = -1;
    }

    return ret;
}

/* return: 0:ok, -1:open err, -2: write err, -3: rename err */
int writeFile(const kb_buf_t buffer, const char normalFile[], 
    const char tmpFile[]) {
    int ret = 0;
    int cnt = 0;
    int total = 0;
    int left = 0;
    FILE* file = NULL;
    
    left = (int)buffer->m_size; 
    
    file = fopen(tmpFile, "wb");
    if (NULL != file) { 
        while (0 < left && !ferror(file)) {
            cnt = fwrite(&buffer->m_buf[total], 1, left, file);
            if (0 < cnt) {
                total += cnt;
                left -= cnt;
            }
        }
        
        fclose(file);

        if (0 == left) {
            ret = rename(tmpFile, normalFile);
            if (0 != ret) { 
                LOG_ERROR("write_file| old=%s| new=%s|"
                    " size=%d| msg=rename file error:%s|",
                    tmpFile, normalFile, total, ERRMSG);

                ret = -3;
            }
        } else {
            LOG_ERROR("write_file| name=%s|"
                " total=%d| wr_size=%d| msg=write error:%s|",
                tmpFile, (int)buffer->m_size, total, ERRMSG);

            ret = -2;
        }
    } else {
        LOG_ERROR("write_file| name=%s| msg=open file error:%s|", 
            tmpFile, ERRMSG);
        
        ret = -1;
    } 
    
    return ret;
}

/* return: 0:ok, -1:open err, -2: write err */ 
int appendFile(const kb_buf_t buffer, const char path[]) {
    int ret = 0;
    int cnt = 0;
    int total = 0;
    int left = 0;
    FILE* file = NULL; 

    left = (int)buffer->m_size; 
    
    file = fopen(path, "ab");
    if (NULL != file) { 
        while (0 < left && !ferror(file)) {
            cnt = fwrite(&buffer->m_buf[total], 1, left, file);
            if (0 < cnt) {
                total += cnt;
                left -= cnt;
            }
        }
        
        fclose(file);

        if (0 == left) {
            ret = 0;
        } else {
            LOG_ERROR("append_file| name=%s|"
                " total=%d| wr_size=%d| msg=write error:%s|",
                path, (int)buffer->m_size, total, ERRMSG);

            ret = -2;
        }
    } else {
        LOG_ERROR("append_file| name=%s| msg=open file error:%s|", 
            path, ERRMSG);
        ret = -1;
    } 

    return ret;
}


