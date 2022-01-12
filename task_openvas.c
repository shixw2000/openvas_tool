#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h> 
#include<time.h> 
#include<ctype.h>
#include<string.h>
#include<stdlib.h>
#include<regex.h>
#include"base_openvas.h" 
#include"task_openvas.h"


int sendKbMsg(const char* key, const char* cmd, size_t len) {
    int ret = 0;
    kb_t kb = NULL;

    kb = kb_conn();
    if (NULL == kb) {
        return -1;
    }

    do {
        ret = kb_push_str(kb, key, cmd, len);
        if (0 != ret) {
            break;
        }

        ret = 0;
    } while (0);

    kb_delete(kb);
    
    return ret;
}

int php_start_task(const char* input, int inputlen, kb_buf_t tmpbuf) {
    int ret = 0;
    struct php_key_task_param oParam;

    do {
        ret = getPhpKeyTaskParam(input, &oParam);
        if (0 != ret) {
            LOG_ERROR("php_start_task| msg=check parameters error|" );
            break;
        }

        tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity,
            "%s%s%s%s%s%s%s%s", 
            "start_task", OPENVAS_KB_DELIM,
            oParam.m_task_name, OPENVAS_KB_DELIM,
            oParam.m_task_id, OPENVAS_KB_DELIM,
            oParam.m_target_id, OPENVAS_KB_DELIM);
        ret = sendKbMsg(OPENVAS_MSG_NAME, tmpbuf->m_buf, tmpbuf->m_size);
        if (0 != ret) { 
            LOG_INFO("php_start_task| taskname=%s| task_id=%s| target_id=%s|"
                " msg=send msg error|",
                oParam.m_task_name,
                oParam.m_task_id,
                oParam.m_target_id);
            break;
        }

        LOG_INFO("php_start_task| taskname=%s| task_id=%s| target_id=%s|"
            " msg=ok|",
            oParam.m_task_name,
            oParam.m_task_id,
            oParam.m_target_id);
    } while (0);

    return ret;
}

int php_stop_task(const char* input, int inputlen, kb_buf_t tmpbuf) {
    int ret = 0;
    struct php_key_task_param oParam;

    do {
        ret = getPhpKeyTaskParam(input, &oParam);
        if (0 != ret) {
            LOG_ERROR("php_stop_task| msg=check parameters error|" );
            break;
        }

        tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity,
            "%s%s%s%s%s%s%s%s", 
            "stop_task", OPENVAS_KB_DELIM,
            oParam.m_task_name, OPENVAS_KB_DELIM,
            oParam.m_task_id, OPENVAS_KB_DELIM,
            oParam.m_target_id, OPENVAS_KB_DELIM);
        ret = sendKbMsg(OPENVAS_MSG_NAME, tmpbuf->m_buf, tmpbuf->m_size);
        if (0 != ret) { 
            LOG_INFO("php_stop_task| taskname=%s| task_id=%s| target_id=%s|"
                " msg=send msg error|",
                oParam.m_task_name,
                oParam.m_task_id,
                oParam.m_target_id);
            break;
        }

        LOG_INFO("php_stop_task| taskname=%s| task_id=%s| target_id=%s|"
            " msg=ok|",
            oParam.m_task_name,
            oParam.m_task_id,
            oParam.m_target_id);
    } while (0);

    return ret;
}

int php_delete_task(const char* input, int inputlen, kb_buf_t tmpbuf) {
    int ret = 0;
    struct php_key_task_param oParam;

    do {
        ret = getPhpKeyTaskParam(input, &oParam);
        if (0 != ret) {
            LOG_ERROR("php_delete_task| msg=check parameters error|" );
            break;
        }

        tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity,
            "%s%s%s%s%s%s%s%s", 
            "delete_task", OPENVAS_KB_DELIM,
            oParam.m_task_name, OPENVAS_KB_DELIM,
            oParam.m_task_id, OPENVAS_KB_DELIM,
            oParam.m_target_id, OPENVAS_KB_DELIM);
        ret = sendKbMsg(OPENVAS_MSG_NAME, tmpbuf->m_buf, tmpbuf->m_size);
        if (0 != ret) { 
            LOG_ERROR("php_delete_task| taskname=%s| task_id=%s| target_id=%s|"
                " msg=send msg error|",
                oParam.m_task_name,
                oParam.m_task_id,
                oParam.m_target_id);
            break;
        }

        LOG_INFO("php_delete_task| taskname=%s| task_id=%s| target_id=%s|"
            " msg=ok|",
            oParam.m_task_name,
            oParam.m_task_id,
            oParam.m_target_id);
    } while (0);

    return ret;
}

int php_create_task(const char* input, int inputlen, kb_buf_t tmpbuf) {
    int ret = 0;
    int len = 0;
    int type = 0;
    int schedule_type = 0;
    struct php_create_task_param oParam;
    regex_t reg;
    regmatch_t matchs[8];

    tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity,
        "^group=\"(%s)\"&groupname=\"(%s)\""
        "&taskname=\"(%s)\"&hosts=\"(%s)\""
        "&schdule_type=\"(%s)\"&schedule_time=\"(%s)\"&schedule_list=\"(%s)\"$",
        CUSTOM_GROUP_ID_PATTERN,
        GVM_NAME_PATTERN,
        GVM_NAME_PATTERN,
        CUSTOM_HOSTS_PATTERN,
        CUSTOM_SCHEDULE_TYPE_PATTERN,
        CUSTOM_SCHEDULE_TIME_PATTERN_OR_NULL,
        CUSTOM_SCHEDULE_LIST_PATTERN);

    ret = regcomp(&reg, tmpbuf->m_buf , REG_EXTENDED);
    if (0 != ret) {
        LOG_ERROR("php_create_task| msg=compile error|" );
        return -1;
    }
    
    do { 
        ret = regexec(&reg, input, 8, matchs, 0);
        if (0 == ret) { 
            /* group id */
            len = matchs[1].rm_eo-matchs[1].rm_so;
            if (len < (int)ARR_SIZE(oParam.m_group_id)) {
                strncpy(oParam.m_group_id, &input[matchs[1].rm_so], len);
                oParam.m_group_id[len] = '\0';
            } else {
                LOG_ERROR("php_create_task| msg=group id size[%d] exceeds maxlen[%d]|",
                    len, (int)ARR_SIZE(oParam.m_group_id));
                ret = -1;
                break;
            }

            /* group name */
            len = matchs[2].rm_eo-matchs[2].rm_so;
            if (len < (int)ARR_SIZE(oParam.m_group_name)) {
                strncpy(oParam.m_group_name, &input[matchs[2].rm_so], len);
                oParam.m_group_name[len] = '\0';
            } else {
                LOG_ERROR("php_create_task| msg=group name size[%d] exceeds maxlen[%d]|",
                    len, (int)ARR_SIZE(oParam.m_group_name));
                ret = -1;
                break;
            }

            /* task name */
            len = matchs[3].rm_eo-matchs[3].rm_so;
            if (len < (int)ARR_SIZE(oParam.m_task_name)) {
                strncpy(oParam.m_task_name, &input[matchs[3].rm_so], len);
                oParam.m_task_name[len] = '\0';
            } else {
                LOG_ERROR("php_create_task| msg=task name size[%d] exceeds maxlen[%d]|",
                    len, (int)ARR_SIZE(oParam.m_task_name));
                ret = -1;
                break;
            }

            /* hosts */
            len = matchs[4].rm_eo-matchs[4].rm_so;
            if (len < (int)ARR_SIZE(oParam.m_hosts)) {
                strncpy(oParam.m_hosts, &input[matchs[4].rm_so], len);
                oParam.m_hosts[len] = '\0';
            } else {
                LOG_ERROR("php_create_task| msg=hosts size[%d] exceeds maxlen[%d]|",
                    len, (int)ARR_SIZE(oParam.m_hosts));
                ret = -1;
                break;
            }

            /* schedule_type */
            len = matchs[5].rm_eo-matchs[5].rm_so;
            if (len < (int)ARR_SIZE(oParam.m_schedule_type)) {
                strncpy(oParam.m_schedule_type, &input[matchs[5].rm_so], len);
                oParam.m_schedule_type[len] = '\0';
            } else {
                LOG_ERROR("php_create_task| msg=schedule_type size[%d] exceeds maxlen[%d]|",
                    len, (int)ARR_SIZE(oParam.m_schedule_type));
                ret = -1;
                break;
            }

            /* schedule_time */
            len = matchs[6].rm_eo-matchs[6].rm_so;
            if (len < (int)ARR_SIZE(oParam.m_schedule_time)) {
                strncpy(oParam.m_schedule_time, &input[matchs[6].rm_so], len);
                oParam.m_schedule_time[len] = '\0';
            } else {
                LOG_ERROR("php_create_task| msg=schedule_time size[%d] exceeds maxlen[%d]|",
                    len, (int)ARR_SIZE(oParam.m_schedule_time));
                ret = -1;
                break;
            }

            /* schedule_list */
            len = matchs[7].rm_eo-matchs[7].rm_so;
            if (len < (int)ARR_SIZE(oParam.m_schedule_list)) {
                strncpy(oParam.m_schedule_list, &input[matchs[7].rm_so], len);
                oParam.m_schedule_list[len] = '\0';
            } else {
                LOG_ERROR("php_create_task| msg=schedule_list size[%d] exceeds maxlen[%d]|",
                    len, (int)ARR_SIZE(oParam.m_schedule_list));
                ret = -1;
                break;
            }
        } else {
            LOG_ERROR("php_create_task| text=%s| msg=invalid parameters", input);
            break;
        }

        ret = getGroupId(oParam.m_group_id, &type);
        if (0 != ret) {
            LOG_ERROR("php_create_task| group_id=%s| msg=invalid group id|", 
                oParam.m_group_id);
            break;
        }

        ret = trimText(oParam.m_group_name);
        if (0 != ret) {
            LOG_ERROR("php_create_task| group_name=%s| msg=invalid group name|", 
                oParam.m_group_name);
            break;
        }

        ret = escapeHosts(oParam.m_hosts);
        if (0 != ret) {
            LOG_ERROR("php_create_task| hosts=%s| msg=invalid hosts|", 
                oParam.m_hosts);
            break;
        }
        
        ret = chkHosts(oParam.m_hosts);
        if (0 != ret) {
            LOG_ERROR("php_create_task| hosts=%s| msg=invalid hosts|", 
                oParam.m_hosts);
            break;
        }

        ret = trimText(oParam.m_task_name);
        if (0 != ret) {
            LOG_ERROR("php_create_task| hosts=%s| msg=invalid taskname|", 
                oParam.m_task_name);
            break;
        }

        schedule_type = atoi(oParam.m_schedule_type);
        ret = chkScheduleParam(schedule_type, oParam.m_schedule_time, oParam.m_schedule_list);
        if (0 != ret) {
            LOG_ERROR("php_create_task| schedul_type=%s| schedul_time=%s|"
                " schedul_list=%s| msg=invalid schedule parameters|", 
                oParam.m_schedule_type, 
                oParam.m_schedule_time,
                oParam.m_schedule_list);
            break;
        }
        
        tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity,
            "%s%s%d%s%s%s%s%s%s%s%s%s%s%s%s%s", 
            "create_task", OPENVAS_KB_DELIM,
            type, OPENVAS_KB_DELIM,
            oParam.m_group_name, OPENVAS_KB_DELIM,
            oParam.m_task_name,  OPENVAS_KB_DELIM,
            oParam.m_hosts, OPENVAS_KB_DELIM,
            oParam.m_schedule_type, OPENVAS_KB_DELIM,
            oParam.m_schedule_time, OPENVAS_KB_DELIM,
            oParam.m_schedule_list, OPENVAS_KB_DELIM);
        ret = sendKbMsg(OPENVAS_MSG_NAME, tmpbuf->m_buf, tmpbuf->m_size);
        if (0 != ret) { 
            LOG_ERROR("php_create_task| group_id=%s| group_name=%s|"
                " task_name=%s| host=%s|"
                " schedule_type=%s| schedule_time=%s| schedule_list=%s|"
                " msg=send msg error|",
                oParam.m_group_id,
                oParam.m_group_name,
                oParam.m_task_name,
                oParam.m_hosts,
                oParam.m_schedule_type,
                oParam.m_schedule_time,
                oParam.m_schedule_list);
            break;
        }

        LOG_INFO("php_create_task| group_id=%s| group_name=%s|"
            " task_name=%s| host=%s|"
            " schedule_type=%s| schedule_time=%s| schedule_list=%s|"
            " msg=ok|",
            oParam.m_group_id,
            oParam.m_group_name,
            oParam.m_task_name,
            oParam.m_hosts,
            oParam.m_schedule_type,
            oParam.m_schedule_time,
            oParam.m_schedule_list);
    } while (0);

    regfree(&reg);

    return ret;
}

int chkScheduleParam(int type, const char* schedule_time, const char* schedule_list) {
    int ret = 0;

    if (ICAL_DATE_NONE == type) {
        if ('\0' != schedule_time[0] || '\0' != schedule_list[0]) {
            ret = -1;
        }
    } else if (ICAL_DATE_ONCE == type || ICAL_DATE_DAILY == type) {
        if ('\0' == schedule_time[0] || '\0' != schedule_list[0]) {
            ret = -1;
        }
    } else if (ICAL_DATE_WEEKLY == type || ICAL_DATE_MONTHLY == type) {
        if ('\0' == schedule_time[0] || '\0' == schedule_list[0]) {
            ret = -1;
        }
    } else {
        ret = -1;
    }

    return ret;
}

int getGroupId(const char* text, int* id) {
    int type = 0;

    type = atoi(text);
    if (0 <= type && 0x10000 > type) {
        /* ok */
        *id = type;
        return 0;
    } else {
        LOG_ERROR( "%s: group=%s[%d]| msg=invalid group id", __FUNCTION__, text, type);
        return -1;
    }
}

int getConfigId(int type, char* buf, int maxlen) {
    int cnt = 0;
    char uuid[MAX_UUID_SIZE] = {0};

    if (0 < type) {
        cnt = snprintf(uuid, MAX_UUID_SIZE, DEF_OPENVAS_CONFIG_GROUP_FORMAT, type);
    } else {
        /* default full config */
        cnt = snprintf(uuid, MAX_UUID_SIZE, DEF_OPENVAS_FULL_AND_FAST_CONFIG);
    } 
    
    if (0 < cnt && cnt < maxlen) {
        memcpy(buf, uuid, cnt);
        buf[cnt] = '\0';
    } else if (cnt >= maxlen) {
        cnt = -cnt;
    } else {
        cnt = -1;
    }

    return cnt;
}

int chkUuid(const char* text) {
    int ret = 0;
    regex_t reg;
    regmatch_t match;
    char pattern[MAX_BUFFER_SIZE + 1] = {0};

    snprintf(pattern, MAX_BUFFER_SIZE, "^%s$", UUID_REG_PATTERN);
    ret = regcomp(&reg, pattern, REG_EXTENDED);
    if (0 != ret) {
        LOG_ERROR("%s: regcompile error|", __FUNCTION__);
        return -1;
    }

    do {
        ret = regexec(&reg, text, 1, &match, 0);
        if (0 != ret) {
            LOG_ERROR( "%s: uuid=%s| msg=invalid uuid", __FUNCTION__, text);
            ret = -1;
            break;
        }
    } while (0);

    regfree(&reg);
    
    return ret;
}

int chkHosts(const char* text) {
    int ret = 0;
    regex_t reg;
    regmatch_t match;
    char pattern[MAX_BUFFER_SIZE + 1] = {0};

    snprintf(pattern, MAX_BUFFER_SIZE, "^([0-9., /-]){1,128}$");
    ret = regcomp(&reg, pattern, REG_EXTENDED);
    if (0 != ret) {
        LOG_ERROR("%s: patter=%s| regcompile error|", __FUNCTION__, pattern);
        return -1;
    }

    do {
        ret = regexec(&reg, text, 1, &match, 0);
        if (0 != ret) {
            LOG_ERROR( "%s: hosts=%s| msg=invalid hosts", __FUNCTION__, text);
            ret = -1;
            break;
        }
    } while (0);

    regfree(&reg);
    return ret;
}

int chkName(const char* text) {
    int ret = 0;
    regex_t reg;
    regmatch_t match;
    char pattern[MAX_BUFFER_SIZE + 1] = {0};

    snprintf(pattern, MAX_BUFFER_SIZE, "^%s$", GVM_NAME_PATTERN);
    ret = regcomp(&reg, pattern, REG_EXTENDED);
    if (0 != ret) {
        LOG_ERROR("%s: regcompile error|", __FUNCTION__);
        return -1;
    }

    do {
        ret = regexec(&reg, text, 1, &match, 0);
        if (0 != ret) {
            LOG_ERROR( "%s: name=%s| msg=invalid name", __FUNCTION__, text);
            ret = -1;
            break;
        }
    } while(0);

    regfree(&reg);
    return ret;
}


/* 0: ok, -1: error, -2: response error */
int chkRspStatusOk(const char* cmd, const char* text) {
    int ret = 0;
    int len = 0;
    regex_t reg;
    regmatch_t matchs[2];
    char val[MAX_COMM_MIN_SIZE] = {0};
    char pattern[MAX_BUFFER_SIZE + 1] = {0};

    snprintf(pattern, MAX_BUFFER_SIZE, GVM_RSP_STATUS_OK_PATTERN, cmd);
    ret = regcomp(&reg, pattern, REG_EXTENDED);
    if (0 != ret) {
        LOG_ERROR("%s: regcompile error|", __FUNCTION__);
        return -1;
    }

    do {
        ret = regexec(&reg, text, 2, matchs, 0);
        if (0 != ret) {
            LOG_ERROR("%s: invalid response|", __FUNCTION__);
            ret = -1;
            break;
        }

        len = (int)(matchs[1].rm_eo-matchs[1].rm_so);
        strncpy(val, &text[ matchs[1].rm_so ], len);
        val[len] = '\0';
        
        if ('2' == val[0]) {
            /* response is ok as 20* */
            ret = 0;
        } else {
            LOG_ERROR("%s: status=%s| msg=response error|", 
                __FUNCTION__, val);
            ret = -2;
        }
    } while (0);
    
    regfree(&reg);
    return ret;
}


int extractTagUuid(const char* text, const char* tag, char* uuid, int maxlen) {
    int ret = 0;
    int len = 0;
    regex_t reg;
    regmatch_t matchs[2];
    char pattern[MAX_BUFFER_SIZE + 1] = {0};

    snprintf(pattern, MAX_BUFFER_SIZE, "<%1$s>(%2$s)</%1$s>", tag, UUID_REG_PATTERN);
    ret = regcomp(&reg, pattern, REG_EXTENDED);
    if (0 != ret) {
        LOG_ERROR("%s: regcompile error|", __FUNCTION__);

        uuid[0] = '\0';
        return -1;
    }

    do {
        ret = regexec(&reg, text, 2, matchs, 0);
        if (0 == ret) {
            len = (int)(matchs[1].rm_eo-matchs[1].rm_so);
            if (len < maxlen) {
                strncpy(uuid, &text[ matchs[1].rm_so ], len);
                uuid[len] = '\0';
            } else {
                LOG_ERROR("%s: tag=%s| uuid size[%d] exceeds max[%d]",
                    __FUNCTION__, tag, len, maxlen);

                uuid[0] = '\0';
                ret = -1;
                break;
            }
        } else {
            LOG_DEBUG("%s: tag=%s| msg=not found a uuid|", __FUNCTION__, tag);

            uuid[0] = '\0';
            ret = -1;
            break;
        }
    } while (0);

    regfree(&reg);
    
    return ret;
}

int extractAttrUuid(const char* text, const char* tag, char* uuid, int maxlen) {
    int ret = 0;
    int len = 0;
    regex_t reg;
    regmatch_t matchs[2];
    char pattern[MAX_BUFFER_SIZE + 1] = {0};

    snprintf(pattern, MAX_BUFFER_SIZE, DEF_XML_OK_RSP_MARK, tag, UUID_REG_PATTERN);
    ret = regcomp(&reg, pattern, REG_EXTENDED);
    if (0 != ret) {
        LOG_ERROR("%s: regcompile error|", __FUNCTION__);

        uuid[0] = '\0';
        return -1;
    }

    do {
        ret = regexec(&reg, text, 2, matchs, 0);
        if (0 == ret) {
            len = (int)(matchs[1].rm_eo-matchs[1].rm_so);
            if (len < maxlen) {
                strncpy(uuid, &text[ matchs[1].rm_so ], len);
                uuid[len] = '\0';
            } else {
                LOG_ERROR("%s: tag=%s| uuid size[%d] exceeds max[%d]",
                    __FUNCTION__, tag, len, maxlen);

                uuid[0] = '\0';
                ret = -1;
                break;
            }
        } else {
            LOG_DEBUG("%s: tag=%s| msg=not found a uuid|", __FUNCTION__, tag);

            uuid[0] = '\0';
            ret = -1;
            break;
        }
    } while (0);

    regfree(&reg); 
    return ret;
}

int escapeXml(const char* xml, char* out, int maxlen) {
    int cnt = 0;
    const char* psz = NULL;

    for (psz=xml; '\0' != *psz && cnt < (maxlen-1); ++psz) {
        if ('\"' == *psz) {
            if (cnt < maxlen-1) {
                out[cnt++] = '\\';
                out[cnt++] = '\"';
            }
        } else {
            out[cnt++] = *psz;
        }
    }

    out[cnt] = '\0'; 
    return cnt;
}

int escapeHosts(char* hosts) {
    int cnt = 0;
    int isDelim = 0;
    char* psz = NULL;
    
    for (psz = hosts; *psz; ++psz) {
        if (isdigit(*psz) || '/' == *psz || '.' == *psz || '-' == *psz) {
            hosts[cnt++] = *psz;

            if (isDelim) {
                isDelim = 0;
            }
        } else {
            if (!isDelim && 0 != cnt) {
                hosts[cnt++] = ',';
                isDelim = 1;
            }
        }
    }

    if (0 < cnt) {
        if (',' == hosts[cnt-1]) {
            hosts[cnt-1] = '\0';
        } else {
            hosts[cnt] = '\0';
        }

        return 0;
    } else {
        hosts[0] = '\0';
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
        return -1;
    }
}

int getPatternKey(const char* text, const char* key, 
    const char* pattern, char* val, int maxlen) {
    int ret = 0;
    int len = 0;
    regex_t reg;
    regmatch_t matchs[2];  
    char format[MAX_COMM_SIZE] = {0};

    snprintf(format, MAX_COMM_SIZE, "^%s=\"(%s)\"$", key, pattern);

    ret = regcomp(&reg, format, REG_EXTENDED | REG_NEWLINE);
    if (0 != ret) {
        LOG_ERROR("getPatternKey| text=%s| key=%s| pattern=%s| error=compile failed|",
            text, key, pattern);
        return -1;
    }

    do {
        ret = regexec(&reg, text, 2, matchs, 0);
        if (0 == ret) { 
            len = (int)(matchs[1].rm_eo-matchs[1].rm_so);
            if (len < maxlen) {
                strncpy(val, &text[ matchs[1].rm_so ], len);
                val[len] = '\0';
            } else {
                LOG_ERROR("getPatternKey| text=%s| key=%s| pattern=%s|"
                    " error=size[%d] exceeds maxlen[%d]|",
                    text, key, pattern, 
                    len, maxlen);
                ret = -1;
                break;
            }
        } else {
            LOG_ERROR("getPatternKey| text=%s| key=%s| pattern=%s| error=check failed|",
                text, key, pattern);
            ret = -1;
            break;
        }
    } while (0);

    regfree(&reg);
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

int getPhpKeyTaskParam(const char* text, php_key_task_param_t param) {
    int ret = 0;
    int len = 0;
    regex_t reg;
    regmatch_t matchs[4];
    char pattern[MAX_COMM_SIZE] = {0};

    snprintf(pattern, MAX_BUFFER_SIZE, PHP_KEY_TASK_PARAMS_PATT, 
        GVM_NAME_PATTERN, UUID_REG_PATTERN, UUID_REG_PATTERN); 

    ret = regcomp(&reg, pattern, REG_EXTENDED);
    if (0 != ret) {
        LOG_ERROR( "getPhpKeyTaskParam| patter=%s| msg=compile error|", pattern);
        return -1;
    }
    
    do { 
        ret = regexec(&reg, text, 4, matchs, 0);
        if (0 == ret) { 
            /* task name */
            len = matchs[1].rm_eo-matchs[1].rm_so;
            if (len < (int)ARR_SIZE(param->m_task_name)) {
                strncpy(param->m_task_name, &text[matchs[1].rm_so], len);
                param->m_task_name[len] = '\0';
            } else {
                LOG_ERROR("getPhpKeyTaskParam| msg=task name size[%d] exceeds maxlen[%d]|",
                    len, (int)ARR_SIZE(param->m_task_name));
                ret = -1;
                break;
            }

            /* task id */
            len = matchs[2].rm_eo-matchs[2].rm_so;
            if (len < (int)ARR_SIZE(param->m_task_id)) {
                strncpy(param->m_task_id, &text[matchs[2].rm_so], len);
                param->m_task_id[len] = '\0';
            } else {
                LOG_ERROR("getPhpKeyTaskParam| msg=task id size[%d] exceeds maxlen[%d]|",
                    len, (int)ARR_SIZE(param->m_task_id));
                ret = -1;
                break;
            }

            /* target id */
            len = matchs[3].rm_eo-matchs[3].rm_so;
            if (len < (int)ARR_SIZE(param->m_target_id)) {
                strncpy(param->m_target_id, &text[matchs[3].rm_so], len);
                param->m_target_id[len] = '\0';
            } else {
                LOG_ERROR("getPhpKeyTaskParam| msg=target id size[%d] exceeds maxlen[%d]|",
                    len, (int)ARR_SIZE(param->m_target_id));
                ret = -1;
                break;
            }

            ret = 0;
        } else {
            LOG_ERROR( "getPhpKeyTaskParam| text=%s| msg=invalid parameters|", text);
            ret = -1;
            break;
        }
    } while (0);

    regfree(&reg);

    return ret;
}

int getXmlTagVal(const char* text, const char* tag, char* buf, int maxlen) {
    int len = 0; 
    char* start = NULL;
    char* stop = NULL;
    char pattern[MAX_COMM_SIZE] = {0};

    /* beg tag */
    snprintf(pattern, ARR_SIZE(pattern), "<%s>", tag);
    start = strstr(text, pattern);
    if (NULL != start) {
        start += strnlen(pattern, ARR_SIZE(pattern));

        /* end tag */
        snprintf(pattern, ARR_SIZE(pattern), "</%s>", tag);
        stop = strstr(start, pattern);
        if (NULL != stop) {
            len = (int)(stop - start);
            if (len < maxlen) {
                strncpy(buf, start, maxlen);
                buf[len] = '\0';

                return 0;
            } else {
                LOG_ERROR("getXmlTagVal| text=%s| tag=%s|"
                    " error=size[%d] exceeds maxlen[%d]|",
                    text, tag, len, maxlen);
                return -1;
            }
        }
    }

    LOG_ERROR("getXmlTagVal| text=%s| tag=%s| error=check failed|", text, tag);
    return -1; 
}


