#define _GNU_SOURCE

#include<time.h> 
#include<sys/types.h> 
#include<signal.h> 
#include<string.h>
#include<stdarg.h>
#include<stdlib.h> 
#include<errno.h>
#include<sys/stat.h> 
#include<unistd.h>
#include<dirent.h> 
#include<sys/time.h>
#include<sys/syscall.h>
#include<ctype.h>
#include<fcntl.h> 

#include"comm_misc.h" 


/* param isFile: 0: dir, 1: file
** return : 0: exists, 1: no eixsts, -2: exists but type error, -1: error */ 
int chkExists(const char dir[], int isFile) {
    int ret = 0;
    struct stat buf;
    
    ret = stat(dir, &buf);
    if (0 == ret) {
        if (isFile && S_ISREG(buf.st_mode)) {
            ret = 0;
        } else if (!isFile && S_ISDIR(buf.st_mode)) {
            ret = 0;
        } else {
            ret = -2;
        }
    } else if (ENOENT == ERRCODE) {
        ret = 1;
    } else {
        ret = -1;
    }

    return ret;

}

/* return: 0: ok, 1: exits already, -2: exists but type err, -1: error */
int createDir(const char dir[]) {
    int ret = 0;
    struct stat buf;
    
    ret = stat(dir, &buf);
    if (0 == ret) {
        if (S_ISDIR(buf.st_mode)) {
            ret = 1;
        } else {
            LOG_ERROR("create_dir| dir=%s| msg=exists but type error|",
                dir);
            ret = -2;
        }
    } else if (ENOENT == ERRCODE) { 
        ret = mkdir(dir, 0774);
        if (0 != ret) {
            LOG_ERROR("create_dir| dir=%s| msg=mkdir error:%s|",
                dir, ERRMSG);

            ret = -1;
        }
    } else {
        LOG_ERROR("create_dir| dir=%s| msg=stat error:%s|",
            dir, ERRMSG);
        ret = -1;
    }

    return ret;
}

int deleteDir(const char dir[]) {
    int ret = 0;
    int dirlen = 0;
    int namelen = 0;
    struct stat buf;
    struct dirent ent;
    struct dirent* res;
    DIR* hd = NULL;
    char path[MAX_FILENAME_PATH_SIZE] = {0};

    /* empty dir name, or relative dir name is invalid */
    if (NULL == dir || '\0' == dir[0]) {
        LOG_ERROR("delete_dir| dir=%s| error=empty dir|", dir);
        return -1;
    } 
    
    dirlen = strnlen(dir, MAX_FILENAME_PATH_SIZE);
    while (0 < dirlen && '/' == dir[dirlen-1]) {
        /* erase the last seperator '/' */
        --dirlen;
    }

    /* absolute path only, without root '/' */
    if (1 >= dirlen || MAX_FILENAME_PATH_SIZE-2 <= dirlen || '/' != dir[0]) {
        /* invalid dir */
        LOG_ERROR("delete_dir| dir=%s| error=invalid dir|", dir);
        return -1;
    }

    strncpy(path, dir, dirlen);
    path[dirlen] = '\0';
    
    ret = lstat(path, &buf);
    if (0 == ret) {
        if (S_ISDIR(buf.st_mode)) {
            hd = opendir(path);
            if (NULL != hd) {
                /* add a path seperator */
                path[dirlen++] = '/';
                path[dirlen] = '\0';
                
                memset(&ent, 0, sizeof(ent));
                res = NULL; 
                
                while (0 == ret) {
                    ret = readdir_r(hd, &ent, &res);
                    if (0 == ret) {
                        if (NULL != res) {
                            namelen = strnlen(res->d_name, MAX_FILENAME_PATH_SIZE);
                            if (dirlen + namelen < MAX_FILENAME_PATH_SIZE) { 
                                if (DT_DIR != res->d_type) {
                                    strcpy(&path[dirlen], res->d_name);
                                    ret = unlink(path);
                                    if (0 == ret) {
                                        LOG_INFO("delete_dir| dir=%s| file=%s|"
                                            " msg=delete file ok|", 
                                            dir, res->d_name);
                                    } else {
                                        LOG_ERROR("delete_dir| dir=%s| file=%s|"
                                            " error=unlink:%s|", 
                                            dir, res->d_name, ERRMSG);
                                        ret = -1;
                                        break;
                                    }
                                } else {
                                    if (0 != strcmp(res->d_name, ".") 
                                        && 0 != strcmp(res->d_name, "..")) {
                                        strcpy(&path[dirlen], res->d_name);
                                        
                                        ret = deleteDir(path);
                                        if (0 != ret) {
                                            LOG_ERROR("delete_dir| dir=%s| subdir=%s|"
                                                " error=%d|", 
                                                dir, res->d_name, ret);
                                            break;
                                        }
                                    } 
                                }
                            } else {
                                /* too long of the whole name */
                                LOG_ERROR("delete_dir| dir=%s| name=%s|"
                                    " error=path length exceeds max[%d]|", 
                                    dir, res->d_name, MAX_FILENAME_PATH_SIZE);
                                ret = -1;
                                break;
                            }
                        } else {
                            /* end of dir */ 
                            break;
                        }
                    } else {
                        LOG_ERROR("delete_dir| dir=%s| error=readdir:%s|", 
                            dir, strerror(ret));
                        ret = -1;
                    }
                } 
                
                closedir(hd);

                if (0 == ret) {
                    path[dirlen] = '\0';
                    ret = rmdir(path);
                    if (0 == ret) {
                        LOG_INFO("delete_dir| dir=%s| msg=delete dir ok|", dir);
                    } else {
                        LOG_ERROR("delete_dir| dir=%s| error=rmdir:%s|", dir, ERRMSG);
                    }
                }
            } else {
                LOG_ERROR("delete_dir| dir=%s| error=opendir:%s|", dir, ERRMSG);
                ret = -1;
            }
        } else {
            LOG_ERROR("delete_dir| dir=%s| error=not a dir|", dir);
            ret = -2;
        }
    } else if (ENOENT == errno) {
        /* dir not exists */
        LOG_INFO("delete_dir| dir=%s| msg=dir not exists|", dir);
        ret = 1;
    } else {
        LOG_ERROR("delete_dir| dir=%s| error=stat:%s|", dir, ERRMSG);
        ret = -1;
    }

    return ret;
}

int readTotalFile(const char name[], kb_buf_t cache) {
    int ret = 0;
    FILE* hd = NULL;
    struct stat buf;

    memset(cache, 0, sizeof(struct kb_buf));
    
    ret = stat(name, &buf);
    if (0 != ret) {
        if (ENOENT == ERRCODE) {
            /* file not exists */
            LOG_DEBUG("readTotalFile| name=%s| msg=file not exists|", name);
            
            return 1;
        } else {
            LOG_ERROR("readTotalFile| name=%s| msg=stat file error:%s|", 
            name, ERRMSG);
            return -1;
        } 
    }
    
    if (!S_ISREG(buf.st_mode)) {
        LOG_ERROR("readTotalFile| name=%s| error=not a regular file|", name);
        return -1;
    } else if (!(S_IRUSR & buf.st_mode)) {
        LOG_ERROR("readTotalFile| name=%s| error=cannot read the file|", name);
        return -1;
    } else if (buf.st_size > MAX_TASK_FILE_SIZE) {
        LOG_ERROR("readTotalFile| name=%s| size=%ld|"
            " error=file size exceeds maxsize[%d]|", 
            name, (long)buf.st_size, MAX_TASK_FILE_SIZE);
        return -1;    
    } else if (0 < buf.st_size) {
        /* valid file size */
        ret = genBuf(buf.st_size, cache);
        if (0 != ret) {
            LOG_ERROR("readTotalFile| name=%s| size=%ld|"
                " error=no memory allocated|", 
                name, (long)buf.st_size);

            return -1; 
        } 
    } else {
        /* empty file, no need to read */
        LOG_DEBUG("readTotalFile| name=%s| msg=empty file|", name);
        return 2;
    }

    hd = fopen(name, "rb");
    if (NULL != hd) { 
        cache->m_size = fread(cache->m_buf, 1, cache->m_capacity, hd);
        fclose(hd);

        if (cache->m_size == cache->m_capacity) {
            cache->m_buf[ cache->m_size ] = '\0';

            LOG_DEBUG("readTotalFile| name=%s| size=%d| msg=read ok|",
                name, (int)cache->m_size);
            
            return 0;
        } else {
            LOG_ERROR("readTotalFile| name=%s| rdlen=%d| total=%d|"
                " error=fread error|", 
                name, (int)cache->m_size, (int)cache->m_capacity);
            
            /* free cache */
            freeBuf(cache);
            return -1;
        }
    } else {
        LOG_ERROR("readTotalFile| name=%s| msg=open file error:%s|", 
            name, ERRMSG);

        /* free cache */
        freeBuf(cache);
        return -1;
    } 
}

void getDevRand(unsigned int* buf) {
    int fd = -1;
    int len = 0;
    struct timespec res = {0, 0}; 

    fd = open("/dev/random", O_RDONLY|O_NONBLOCK);
    if (0 <= fd) {
        len = read(fd, buf, 4);
        close(fd);
        
        if (len == 4) {
            return;
        } 
    }

    clock_gettime(CLOCK_MONOTONIC, &res);
    *buf = (unsigned int)(res.tv_sec + res.tv_nsec);
    return;
}

int genUUID(char id[], int maxlen) {
    int len = 0;
    unsigned int tid = 0;
    unsigned int rand[2] = {0, 0};
    unsigned long long ullTmp = 0;
    struct timeval tv;
    struct tm tm;
    char buf[MAX_UUID_SIZE] = {0};

    memset(&tv, 0, sizeof(tv));
    memset(&tm, 0, sizeof(tm));
    
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm);

    tid = (unsigned int)syscall(SYS_gettid) & 0x0FFFF;

    ullTmp = (unsigned long long)buf + tv.tv_sec + tv.tv_usec;
    rand[0] = (unsigned int)((ullTmp & 0xFFFFFFFF) + (ullTmp >> 32));
    getDevRand(&rand[1]);
    
    len = snprintf(buf, sizeof(buf), "%08x-%02x%02d-%02d%02d-%02d%02d-%04x%08x",
        rand[0],
        (unsigned char)(tm.tm_year & 0x0FF), tm.tm_mon+1,
        tm.tm_mday, tm.tm_hour,
        tm.tm_min, tm.tm_sec, 
        tid, rand[1]);
    
    if (maxlen > len) {
        strncpy(id, buf, len);
        id[len] = '\0';

        return 0;
    } else {
        id[0] = '\0';
        return -1;
    } 
}

int trimText(char* text) {
    int size = 0;
    int cnt = 0;
    char* beg = NULL;
    char* end = NULL;
    
    size = (int)strnlen(text, MAX_MSG_SIZE);
    if (MAX_MSG_SIZE > size) {
        beg = text;
        end = text + size;
        
        while (beg < end && isspace(*beg)) {
            ++beg;
        }
        
        while (end > beg && isspace(*(end-1))) {
            --end;
        }

        if (end > beg) {
            cnt = (int)(end - beg);
            if (cnt < size) {
                memmove(text, beg, cnt);
                text[cnt] = '\0';
            } 
            
            return 0;
        } else {
            /* empty text */
            text[0] = '\0';
            return -1;
        } 
    } else {
        /* do nothing  */
        return -1;
    }
}

/* return: 0: ok, 1: empty, -1: exceed max size  */
int getNextToken(const char* text, const char* needle,
    char* buf, int maxlen, const_char_t* saveptr) {
    int cnt = 0;
    const char* psz = NULL;

    if (NULL == text || '\0' == text[0]) {
        return 1;
    }
    
    psz = strstr(text, needle);
    if (NULL != psz) {
        cnt = (int)(psz - text);
    } else {
        /* total text */
        cnt = (int)strnlen(text, maxlen);
    }

    if (0 <= cnt && cnt < maxlen) {
        strncpy(buf, text, cnt);
        buf[cnt] = '\0'; 

        if (NULL != saveptr) {
            if (NULL != psz) {
                *saveptr = psz + strlen(needle);
            } else {
                *saveptr = text + cnt;
            }
        }

        return 0;
    } else {
        LOG_ERROR("getNextToken| text=%s| needle=%s|"
            " maxlen=%d| cnt=%d| msg=token len error|",
            text, needle, maxlen, cnt);
        return -1;
    }
}

/* return: 0: ok, 1: empty, -1: exceed max size, -2: not digit */
int getNextTokenInt(const char* text, const char* needle,
    int* val, const_char_t* saveptr) {
    int ret = 0;
    char buf[MAX_COMM_MIN_SIZE] = {0};

    *val = -1;
    
    ret = getNextToken(text, needle, buf, ARR_SIZE(buf), saveptr);
    if (0 == ret) {
        if (isdigit(buf[0])) {
            *val = atoi(buf);
        } else {
            ret = -2;
        }
    }

    return ret;
} 

/* return: 0: ok, 1: no file,  -1: error, -2: write err */ 
int copyFileList(const char basedir[], const char filelist[], 
    const char dstFile[]) {
    int ret = 0;
    int len = 0;
    int has_write = 0;
    struct kb_buf buffer;
    const char* saveptr = NULL;
    char name[MAX_FILENAME_PATH_SIZE] = {0};
    char filepath[MAX_FILENAME_PATH_SIZE] = {0};

    initBuf(&buffer);
    saveptr = filelist;
    
    while (1) { 
        ret = getNextToken(saveptr, ",", name, ARR_SIZE(name), &saveptr);
        if (0 == ret && '\0' != name[0]) {
            /* ok */
            len = (int)snprintf(filepath, MAX_FILENAME_PATH_SIZE,
                "%s/%s", 
                basedir, name);
            if (0 > len || len >= MAX_FILENAME_PATH_SIZE) {
                /* file path exceeds max length */
                LOG_ERROR("copy_filelist| basedir=%s| filelist=%s|"
                    " name=%s| msg=path exceeds maxlen[%d]|",
                    basedir, filelist, name, MAX_FILENAME_PATH_SIZE);
                    
                ret = -1;
            }
        } else if (1 == ret) {
            /* go to the end of list */
            if (has_write) {
                /* just here return ok */
                return 0;
            } else {
                /* no file in list */
                LOG_ERROR("copy_filelist| basedir=%s| filelist=%s|"
                    " msg=empty filelist error|",
                    basedir, filelist);
                ret = 1;
            }
        } else {
            LOG_ERROR("copy_filelist| basedir=%s| filelist=%s|"
                " ret=%d| msg=get filelist next error|",
                basedir, filelist, ret);
            ret = -1;
        }

        if (0 != ret) {
            break;
        }

        ret = readTotalFile(filepath, &buffer);
        if (0 == ret) {
            /* read data ok */
            if (!has_write) {
                has_write = 1;
                ret = writeFile(&buffer, dstFile);
            } else { 
                ret = appendFile(&buffer, dstFile);
            }

            /* append a newline to separate file if not */
            if (0 < buffer.m_size && '\n' != buffer.m_buf[buffer.m_size-1] && 0 == ret) {
                buffer.m_buf[0] = '\n';
                buffer.m_size = 1;
                appendFile(&buffer, dstFile);
            }
            
            freeBuf(&buffer); 
        } else if (1 == ret) {
            /* no file exists in system */
            LOG_ERROR("copy_filelist| basedir=%s| filelist=%s|"
                " name=%s| msg=file not exists error|",
                basedir, filelist, name);
        } else if (2 == ret) {
            /* empty file,  no context needed to copy */
            ret = 0;
            continue;
        } else {
            /* read error */
            LOG_ERROR("copy_filelist| basedir=%s| filelist=%s|"
                " name=%s| ret=%d| msg=read file error|",
                basedir, filelist, name, ret);
            ret = -1;
        } 

        if (0 != ret) {
            break;
        }
    }

    return ret;
}

/* return: 0: ok, 1: no file,  -1: error, -2: write err, -3: rename err */ 
int copyFileListSafe(const char basedir[], const char filelist[], 
    const char dstFile[], const char dstFileTmp[]) {
    int ret = 0;

    ret = copyFileList(basedir, filelist, dstFileTmp);
    if (0 == ret) {
        ret = rename(dstFileTmp, dstFile);
        if (0 != ret) { 
            LOG_ERROR("copy_filelist_safe| old=%s| new=%s|"
                " msg=rename file error:%s|",
                dstFileTmp, dstFile, ERRMSG);

            ret = -3;
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
            if (0 == ret) {
                LOG_INFO("delete_file| path=%s| msg=delete file ok|", path);
            } else {
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

/* return: 0:ok, -1:open err, -2: write err */
int writeFile(const kb_buf_t buffer, const char filename[]) {
    int ret = 0;
    int cnt = 0;
    int total = 0;
    int left = 0;
    FILE* file = NULL;
    
    left = (int)buffer->m_size; 
    
    file = fopen(filename, "wb");
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
            /* write total ok */
            ret = 0;
        } else {
            LOG_ERROR("write_file| name=%s|"
                " total=%d| wr_size=%d| msg=write error:%s|",
                filename, (int)buffer->m_size, total, ERRMSG);

            ret = -2;
        }
    } else {
        LOG_ERROR("write_file| name=%s| msg=open file error:%s|", 
            filename, ERRMSG);
        
        ret = -1;
    } 
    
    return ret;
}

/* return: 0:ok, -1:open err, -2: write err, -3: rename err */
int writeFileSafe(const kb_buf_t buffer, 
    const char normalFile[], const char tmpFile[]) {
    int ret = 0;

    ret = writeFile(buffer, tmpFile);
    if (0 == ret) {
        ret = rename(tmpFile, normalFile);
        if (0 != ret) { 
            LOG_ERROR("write_file_safe| old=%s| new=%s|"
                " msg=rename file error:%s|",
                tmpFile, normalFile, ERRMSG);

            ret = -3;
        }
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

int killSafe(int pid, int sig) {
    int ret = 0;

    if (1 < pid) {
        ret = kill((pid_t)pid, sig);
        if (0 == ret) {
            LOG_ERROR("killSafe| pid=%d| sig=%d| msg=kill ok|",
                pid, sig);
        } else {
            LOG_ERROR("killSafe| pid=%d| sig=%d| msg=kill error:%s|",
                pid, sig, ERRMSG);
            ret = -1;
        }
    } else {
        LOG_ERROR("killSafe| pid=%d| sig=%d| msg=invalid pid|",
            pid, sig);
        ret = -1;
    }

    return ret;
}

int killProc(int pid) {
    int ret = 0;

    if (0 != pid) {
        ret = killSafe(pid, SIGTERM);
    }
    return ret;
}

int stopProc(int pid) {
    int ret = 0;

    if (0 != pid) {
        ret = killSafe(pid, SIGUSR1);
    }
    return ret;
}


/* return: >0: child pid in parent, =0: in child, -1: error */
int forkSafe() {
    pid_t pid = 0;

    pid = fork ();
    if (0 < pid) {
        /* in parent */
        return pid;
    } else if (pid == 0) { 
        /* in child */
        return 0;
    } else {
        LOG_ERROR("forkSafe| ret=%d| msg=fork error:%s|",
            (int)pid, ERRMSG); 
        
        return -1;
    }
}

int createPidFile(const char path[]) {
    int ret = 0;
    int pid = 0;
    struct kb_buf buffer;
    char tmp[MAX_COMM_MIN_SIZE] = {0};
    
    buffer.m_buf = tmp;
    buffer.m_capacity = ARR_SIZE(tmp);

    pid = (int)getpid();
    buffer.m_size = snprintf(buffer.m_buf, buffer.m_capacity, "%d", pid); 
    
    ret = writeFile(&buffer, path);
    return ret;
} 

/* only return from parent, cmds run in child and never return 
 * return: >0: childpid in parent,  -1: error */
int execvSafe(char* cmds[]) {
    int ret = 0;
    int pid = 0;

    pid = forkSafe();
    if (0 < pid) {
        /* in parent,  wait and block for child exits */

        return pid; 
    } else if (pid == 0) {
        /* in child */
        ret = execv(cmds[0], cmds);
        
        /* should never go to here unless error ocurrs */
        LOG_ERROR("execvSafe| prog=%s| msg=execv error:%s|",
            cmds[0], ERRMSG);
        exit(-1);
    } else {
        LOG_ERROR("execvSafe| prog=%s| msg=fork error|",
            cmds[0]);
        
        return -1;
    } 
}

