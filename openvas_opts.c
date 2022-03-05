#include<libxml/parser.h>
#include<libxml/tree.h> 
#include<libxml/xpath.h> 

#include"comm_misc.h" 
#include"openvas_opts.h"
#include"task_openvas.h"


#define RESULT_NVT_INFO_FORMAT "|%s|%s|%s|%s|%s|%s|%s|\n" 


#define REQ_GVM_CREAT_SCHEDULE_MARK "<create_schedule><name>wEb_%s</name>"\
"<timezone>UTC</timezone><icalendar>"\
"BEGIN:VCALENDAR\r\n"\
"VERSION:2.0\r\n"\
"PRODID:-//Greenbone.net//NONSGML Greenbone Security Manager 21.4.3//EN\r\n"\
"BEGIN:VEVENT\r\n"\
"DTSTART;TZID=UTC:%s\r\n"\
"DURATION:PT0S\r\n"\
"%s%s%s"\
"END:VEVENT\r\n"\
"END:VCALENDAR\r\n"\
"</icalendar></create_schedule>"

#define REQ_GVM_DELETE_SCHEDULE_MARK "<delete_schedule schedule_id=\"%s\"/>" 

#define REQ_GVM_CREAT_CONFIG_EX_MARK "<create_config><name>wEb_%s</name>"\
"<comment>%s</comment>"\
"<usage_type>scan</usage_type>"\
"<multi_copy>%s</multi_copy></create_config>"

#define REQ_GVM_DELETE_CONFIG_MARK "<delete_config config_id=\"%s\"/>"


//#define NODE_TEXT(doc, node) xmlNodeGetContent(node)
#define NODE_TEXT(doc, node) xmlNodeListGetString(doc, node->xmlChildrenNode, 0)

  
int getNodeText(xmlDocPtr doc, xmlNodePtr node, char* buf, int maxlen);

int initLibXml() {
    xmlInitParser();
    LIBXML_TEST_VERSION
    xmlKeepBlanksDefault(0);

    return 0;
}

void finishLibXml() {
    xmlCleanupParser();
} 

static void printNode(xmlNode *root_element) {
    xmlNode *cur_node = NULL;
    
    for (cur_node = root_element; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            LOG_DEBUG("====node=%s| text=%s|", 
                cur_node->name,
                (char*)xmlNodeGetContent(cur_node));
        }

        printNode(cur_node->children);
    }
}

static void printDoc(const kb_buf_t buffer) {
    xmlDocPtr doc = NULL;
    xmlNode *root_element = NULL;

    doc = xmlReadMemory(buffer->m_buf, buffer->m_size, "noname.xml", NULL, 0);
    if (NULL != doc) { 

        root_element = xmlDocGetRootElement(doc); 
        printNode(root_element); 
        
        xmlFreeDoc(doc);
    } else {
        LOG_ERROR("print_xml| ctx=[%d][%s]| msg=parse xml error|",
            (int)buffer->m_size, buffer->m_buf);
    }
} 

xmlDocPtr parseXmlDoc(const kb_buf_t buffer) {
    xmlDocPtr doc = NULL;

    doc = xmlReadMemory(buffer->m_buf, buffer->m_size, "noname.xml", NULL, 0);
    if (NULL != doc) { 
        if (NULL != xmlDocGetRootElement(doc)) {
            return doc;
        } else {
            xmlFreeDoc(doc);

            LOG_ERROR("parse_xml| ctx=[%d][%s]| msg=empty xml error|",
                (int)buffer->m_size, buffer->m_buf);

            return NULL;
        } 
    } else { 
        LOG_ERROR("parse_xml| ctx=[%d][%s]| msg=parse xml error|",
            (int)buffer->m_size, buffer->m_buf);

        return NULL;
    }
} 

int getNodeAttr(xmlNodePtr node, const char* name, char* buf, int maxlen) {
    int ret = 0;
    int len = 0;
    char* psz = NULL;

    psz = (char*)xmlGetProp(node, (const xmlChar*)name);
    if (NULL != psz) {
        len = (int)strnlen(psz, maxlen);
        if (len < maxlen) {
            strncpy(buf, psz, len);
            buf[len] = '\0';
            ret = 0;
        } else {
            buf[0] = '\0';
            ret = -2;
        }

        xmlFree(BAD_CAST psz);
    } else {
        buf[0] = '\0';
        ret = -1;
    }

    return ret;
} 

xmlXPathObjectPtr findPathNode(xmlXPathContextPtr ctx, const char* path) {
    xmlXPathObjectPtr obj = NULL;
    xmlNodeSetPtr node = NULL;

    obj = xmlXPathEvalExpression((const xmlChar*)path, ctx);
    if (NULL == obj) {
        return NULL;
    }

    node = obj->nodesetval;
    if (0 < node->nodeNr && NULL != node->nodeTab) {
        return obj;
    } else {
        xmlXPathFreeObject(obj); 
        return NULL;
    }
} 

xmlNodePtr findChildNode(xmlNodePtr node, const char* name) {
    xmlNodePtr child = NULL;

    if (NULL != node) {
        for (child = node->children; child; child = child->next) {
            if (XML_ELEMENT_NODE == child->type
                && 0 == strcmp(name, (const char*)child->name)) { 
                return child;
            }
        }
    }

    return NULL;
}

/* recursively find the node */
xmlNodePtr findSimpleNode(xmlNodePtr node, const char* path) {
    int ret = 0;
    xmlNodePtr child = NULL;
    const_char_t saveptr = NULL;
    char text[MAX_COMM_SIZE] = {0}; 
    
    /* must begin with absolute path '/' */
    if (NULL == path || '/' != path[0]) {
        return NULL;
    } 
    
    saveptr = &path[1];
    child = node;
    while (NULL != child) {
        node = child;

        ret = getNextToken(saveptr, "/", text, ARR_SIZE(text), &saveptr);
        if (0 == ret) {
            /* get a subpath */
            child = findChildNode(node, text);
        } else if (1 == ret) {
            /* end path, found ok */
            break;
        } else {
            /* path exceeds maxlen */
            child = NULL;
            break;
        } 
    }

    return child;
}

/* 0: ok,  -1: error, -2: exceeds maxlen */ 
int getNodeText(xmlDocPtr doc, xmlNodePtr node, char* buf, int maxlen) {
    int ret = 0;
    int len = 0;
    char* psz = NULL;

    if (XML_ELEMENT_NODE == node->type) {
        psz = (char*)NODE_TEXT(doc, node);
        if (NULL != psz) {
            len = (int)strnlen(psz, maxlen);
            if (len < maxlen) {
                strncpy(buf, psz, len);
                buf[len] = '\0';
                ret = 0;
            } else {
                buf[0] = '\0';
                ret = -2;
            }

            xmlFree(BAD_CAST psz);
        } else {
            buf[0] = '\0';
            ret = 0;
        }
    } else {
        buf[0] = '\0';
        ret = -1;
    }

    return ret;
} 

/* 0: ok, 1: not found, -1: error */ 
int findPathObjText(xmlDocPtr doc, xmlXPathContextPtr ctx, 
    const char* path, char* buf, int maxlen) {
    int ret = 0;
    xmlXPathObjectPtr obj = NULL;
    xmlNodeSetPtr node = NULL;

    obj = findPathNode(ctx, path);
    if (NULL == obj) {
        /* not found */
        buf[0] = '\0';
        return 1;
    }

    do {
        node = obj->nodesetval;
        if (1 != node->nodeNr) {
            buf[0] = '\0';
            ret = -1;
            break;
        }

        ret = getNodeText(doc, node->nodeTab[0], buf, maxlen);
        if (0 != ret) {
            break;
        }
    } while (0);

    xmlXPathFreeObject(obj); 
    return ret;
}


/* 0: ok, 1: not found, -1: error */
int findChildNodeText(xmlDocPtr doc, xmlNodePtr node,
    const char* name, char* buf, int maxlen) {
    int ret = 0;
    xmlNodePtr child = NULL;

    child = findChildNode(node, name);
    if (NULL != child) {
        ret = getNodeText(doc, child, buf, maxlen);
    } else {
        buf[0] = '\0';
        ret = 1;
    }

    return ret;
}

/* 0: ok, 1: not found, -1: error */ 
int findSimpleNodeText(xmlDocPtr doc, xmlNodePtr node, 
    const char* path, char* buf, int maxlen) {
    int ret = 0;
    xmlNodePtr subnode = NULL;

    subnode = findSimpleNode(node, path);
    if (NULL != subnode) {
        ret = getNodeText(doc, subnode, buf, maxlen);
    } else {
        buf[0] = '\0';
        ret = 1;
    }

    return ret;
}

/* 0: ok, 1: not found, -1: error */ 
int findSimpleNodeAttr(xmlDocPtr doc, xmlNodePtr node, const char* path, 
    const char* attrName, char* buf, int maxlen) {
    int ret = 0;
    xmlNodePtr subnode = NULL;

    subnode = findSimpleNode(node, path);
    if (NULL != subnode) {
        ret = getNodeAttr(subnode, attrName, buf, maxlen);
        if (0 != ret) {
            ret = -1;
        }
    } else {
        buf[0] = '\0';
        ret = 1;
    }

    return ret;
} 

enum GVM_TASK_STATUS convRunStatusName(const char* val) {
    enum GVM_TASK_STATUS status = GVM_TASK_ERROR;
    
    if (0 == strcmp(val, "Running")) {
        status = GVM_TASK_RUNNING;
    } else if (0 == strcmp(val, "Queued") || 0 == strcmp(val, "Requested")
        || 0 == strcmp(val, "Delete Requested")
        || 0 == strcmp(val, "Ultimate Delete Requested")) {
        status = GVM_TASK_WAITING;
    } else if (0 == strcmp(val, "New")) {
        status = GVM_TASK_READY;
    } else if (0 == strcmp(val, "Done")) {
        status = GVM_TASK_DONE;
    } else if (0 == strcmp(val, "Stopped")
        || 0 == strcmp(val, "Stop Requested")) {
        status = GVM_TASK_STOP;
    } else {
        status = GVM_TASK_INTERRUPT;
    }

    return status;
}

int gvm_get_version(char ver[], int maxlen,
    kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;

    tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity, 
        "<get_version/>");
    ret = procGvmTask("get_version", tmpbuf, outbuf); 
    if (0 == ret) {
        if (NULL != ver) {
            getXmlTagVal(outbuf->m_buf, "version", ver, maxlen);
        }
    }

    return ret;
}

int gvm_create_schedule(const char* name, enum ICAL_DATE_REP_TYPE type,
    const char* firstRun, const char* _list,
    char uuid[], int maxlen,
    kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;

    do {  
        uuid[0] = '\0';
        
        if (ICAL_DATE_ONCE == type) {
            tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity, 
                REQ_GVM_CREAT_SCHEDULE_MARK,
                name, firstRun, 
                DEF_EMPTY_STR, DEF_EMPTY_STR, DEF_EMPTY_STR); 
        } else if (ICAL_DATE_DAILY == type) {
            tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity, 
                REQ_GVM_CREAT_SCHEDULE_MARK,
                name, firstRun, 
                "RRULE:FREQ=DAILY", DEF_EMPTY_STR, DEF_WIN_CR_LF); 
        } else if (ICAL_DATE_WEEKLY == type) {
        
            tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity, 
                REQ_GVM_CREAT_SCHEDULE_MARK,
                name, firstRun, 
                "RRULE:FREQ=WEEKLY;BYDAY=", _list, DEF_WIN_CR_LF); 
        } else if (ICAL_DATE_MONTHLY == type) {
            tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity, 
                REQ_GVM_CREAT_SCHEDULE_MARK,
                name, firstRun, 
                "RRULE:FREQ=MONTHLY;BYMONTHDAY=", _list, DEF_WIN_CR_LF); 
        } else {
            LOG_ERROR("gvm_create_schedule| name=%s| type=%d| first_time=%s| list=%s|"
                " msg=invalid type|",
                name, type, firstRun, _list);
            
            ret = -1;
            break;
        }
    
        ret = procGvmTask("create_schedule", tmpbuf, outbuf); 
        if (0 != ret) {
            LOG_ERROR("create_schedule| name=%s|"
                " first_time=%s| type=%d| list=%s| msg=gvm process failed|", 
                name, firstRun, type, _list);
                
            break;
        }
        
        ret = extractAttrUuid(outbuf->m_buf, "create_schedule_response", uuid, maxlen);
        if (0 != ret) {
            LOG_ERROR("create_schedule| name=%s|"
                " first_time=%s| type=%d| list=%s| msg=cannot found a uuid|", 
                name, firstRun, type, _list);
            break;
        }

        LOG_INFO("create_schedule| uuid=%s| name=%s|"
            " first_time=%s| type=%d| list=%s| msg=ok|",
            uuid, name, firstRun, type, _list);
    } while (0);

    return ret;
}

int gvm_option_create_schedule(gvm_task_info_t info,
    kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;
    int is_once = 0;
    char utc_schedule_time[MAX_TIMESTAMP_SIZE] = {0};

    do {
        info->m_schedule_id[0] = '\0';
        info->m_schedule_created = (unsigned char)0;
        
        if (ICAL_DATE_NONE != info->m_schedule_type) {
            is_once = (ICAL_DATE_ONCE == info->m_schedule_type);
            
            ret = local2SchedTime(utc_schedule_time, 
                ARR_SIZE(utc_schedule_time),
                info->m_first_schedule_time, is_once);
            if (0 != ret) {
                LOG_ERROR("option_create_schedule| is_once=%d|"
                    " schedule_time=%s| msg=invalid schedule_time|",
                    is_once,
                    info->m_first_schedule_time);

                ret = GVM_ERR_PARAM_INVALID; 
                break;
            }
            
            ret = gvm_create_schedule(info->m_task_name, info->m_schedule_type,
                utc_schedule_time, info->m_schedule_list,
                info->m_schedule_id, ARR_SIZE(info->m_schedule_id),
                tmpbuf, outbuf);
            if (0 == ret) {
                info->m_schedule_created = (unsigned char)1;
            } else { 
                ret = GVM_ERR_OPENVAS_PROC_FAIL;
                break;
            }
        } 

    } while (0);

    return ret;
}


int gvm_delete_schedule(const char* uuid, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;

    do {
        ret = chkUuid(uuid);
        if (0 != ret) {
            LOG_ERROR("delete_schedule| schedule_id=%s| msg=invalid schedule_id|", uuid);
            break;
        }

        tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity,
            REQ_GVM_DELETE_SCHEDULE_MARK, uuid);
        ret = procGvmTask("delete_schedule", tmpbuf, outbuf); 
        if (0 == ret) {
            LOG_INFO("delete_schedule| schedule_id=%s| msg=ok|", uuid);
        } else {
            LOG_ERROR("delete_schedule| schedule_id=%s| msg=gvm process failed|", uuid);
            break;
        }

        ret = 0;
    } while (0);

    return ret;
}

int gvm_option_delete_schedule(gvm_task_info_t info,
    kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;

    if (info->m_schedule_created) {
        ret = gvm_delete_schedule(info->m_schedule_id, tmpbuf, outbuf);
        if (0 == ret) {
            info->m_schedule_created = (unsigned char)0;
        }
    }

    return ret;
}


int gvm_create_config_ex(const char* name,
    const char* group_id, const char* group_name,
    char* config_id, int maxlen, 
    kb_buf_t tmpbuf, kb_buf_t outbuf) { 
    int ret = 0;

    do { 
        tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity,
            REQ_GVM_CREAT_CONFIG_EX_MARK, 
            name, group_name, group_id);
        ret = procGvmTask("create_config", tmpbuf, outbuf); 
        if (0 != ret) {
            LOG_ERROR("create_config_ex| name=%s| group_id=%s|"
                " group_name=%s| msg=gvm process failed|", 
                name, group_id, group_name);
            
            break;
        }

        ret = extractAttrUuid(outbuf->m_buf, "create_config_response", 
            config_id, maxlen);
        if (0 != ret) {
            LOG_ERROR("create_config_ex| name=%s| group_id=%s|"
                " group_name=%s| msg=cannot found a uuid|", 
                name, group_id, group_name);
            
            break;
        }

        LOG_INFO("create_config_ex| name=%s| config_id=%s| group_id=%s|"
            " group_name=%s| msg=ok|", 
            name, config_id, group_id, group_name);
    } while (0);

    return ret;
}

int gvm_delete_config(const char* uuid, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;

    do {
        ret = chkUuid(uuid);
        if (0 != ret) {
            LOG_ERROR("delete_config| config_id=%s| msg=invalid config_id|", uuid);
            break;
        }

        tmpbuf->m_size = snprintf(tmpbuf->m_buf, tmpbuf->m_capacity,
            REQ_GVM_DELETE_CONFIG_MARK, uuid);
        ret = procGvmTask("delete_config", tmpbuf, outbuf); 
        if (0 == ret) {
            LOG_INFO("delete_config| config_id=%s| msg=ok|", uuid);
        } else {
            LOG_ERROR("delete_config| config_id=%s| msg=gvm process failed|", uuid);
            break;
        }

        ret = 0;
    } while (0);

    return ret;
} 

int gvm_option_delete_config(gvm_task_info_t info, 
    kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;

    if (info->m_config_created) {
        ret = gvm_delete_config(info->m_config_id, tmpbuf, outbuf);
        if (0 == ret) {
            info->m_config_created = (unsigned char)0;
        }
    }

    return ret;
}

int gvm_option_create_config(gvm_task_info_t info,
    kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;

    /* if is a single config id, then reuse it */
    if (!isMultiUuid(info->m_group_id)) {
        strncpy(info->m_config_id, info->m_group_id, ARR_SIZE(info->m_config_id));
        info->m_config_created = (unsigned char)0;
    } else {
        ret = gvm_create_config_ex(info->m_task_name,
            info->m_group_id,
            info->m_group_name,
            info->m_config_id,
            ARR_SIZE(info->m_config_id),
            tmpbuf, outbuf);
        if (0 == ret) {
            info->m_config_created = (unsigned char)1;
        } else {
            info->m_config_created = (unsigned char)0;
        }
    }
    
    return ret;
} 

static ListNvtInfo_t createNvtInfo() {
    ListNvtInfo_t nvt = NULL;

    nvt = calloc(1, sizeof(struct ListNvtInfo));
    if (NULL != nvt) {
        reset(&nvt->m_list);
    }
    
    return nvt;
}

static void freeNvtInfo(ListNvtInfo_t nvt) {
    if (NULL != nvt) {
        free(nvt);
    }
}

void resetGvmResultInfo(gvm_result_info_t result) {
    if (NULL != result) {
        result->m_totalCnt = -1;
        result->m_rows = 10;
        result->m_first = 1; 

        freeListQue(&result->m_results, (PFree)freeNvtInfo); 
        result->m_done = 0;
    }
}

/* compare two gvm task info by task name */
static int cmpNvtHost(const ListNvtInfo_t nvt1, const ListNvtInfo_t nvt2) {
    int ret = 0;

    ret = strncmp(nvt1->m_nvtinfo.res_host, nvt2->m_nvtinfo.res_host,
        ARR_SIZE(nvt1->m_nvtinfo.res_host));
    return ret;
}

void initGvmResultInfo(gvm_result_info_t info) {
    if (NULL != info) {
        info->m_totalCnt = -1;
        info->m_rows = 10;
        info->m_first = 1;

        info->m_done = 0;

        initListQue(&info->m_results, (PComp)cmpNvtHost);
    }
}

void initGvmReportInfo(gvm_report_info_t info) {
    if (NULL != info) {
        memset(info, 0, sizeof(struct gvm_report_info));

        info->m_status = GVM_TASK_CREATE; 
    }
}

void initGvmTaskInfo(gvm_task_info_t info) {
    if (NULL != info) {
        memset(info, 0, sizeof(struct gvm_task_info)); 
    }
}

int parseResult(const kb_buf_t buffer, gvm_result_info_t info) {
    int ret = 0;
    int cnt = 0;
    int size = 0;
    int i = 0;
    const char DEF_RESULT_PATH[] = "/get_results_response/result";
    xmlDocPtr doc = NULL;
    xmlXPathContextPtr ctx = NULL; 
    xmlXPathObjectPtr obj = NULL;
    xmlNodeSetPtr node = NULL;
    xmlNodePtr cur = NULL;
    xmlNodePtr child = NULL;
    ListNvtInfo_t nvt = NULL;
    char val[MAX_COMM_SIZE] = {0};
  
    doc = parseXmlDoc(buffer);
    if (NULL == doc) {
        return -1;
    }

    ctx = xmlXPathNewContext(doc);
    if (NULL == ctx) {
        xmlFreeDoc(doc); 
        return -1;
    } 

    do { 
        /* the first time to query results, get total cnt */
        if (0 > info->m_totalCnt) {
            ret = findPathObjText(doc, ctx, 
                "/get_results_response/result_count/filtered",
                val, ARR_SIZE(val));
            if (0 != ret) {
                ret = -2;
                break;
            }

            info->m_totalCnt = atoi(val);
            if (0 > info->m_totalCnt) {
                ret = -3;
                break;
            }
        } 

        /* if there has no result, it's ok and return empty */
        if (0 == info->m_totalCnt) {
            info->m_done = 1;
            ret = 0;
            break;
        }

        obj = findPathNode(ctx, DEF_RESULT_PATH);
        if (NULL == obj) { 
            ret = -4;
            break;
        }
        
        node = obj->nodesetval;
        size = node->nodeNr;
        for (i=0; i<size; ++i) {
            cur = node->nodeTab[i];
            if (XML_ELEMENT_NODE != cur->type) {
                continue;
            } 

            nvt = createNvtInfo();
            if (NULL == nvt) {
                /* no memory */
                ret = -5;
                break;
            }

            ret = getNodeAttr(cur, "id", nvt->m_nvtinfo.res_id,
                ARR_SIZE(nvt->m_nvtinfo.res_id));
            if (0 != ret) {
                ret = -6;
                break;
            }

            ret = findChildNodeText(doc, cur, "name", nvt->m_nvtinfo.res_name,
                ARR_SIZE(nvt->m_nvtinfo.res_name));
            if (0 != ret) {
                ret = -7;
                break;
            }

            ret = findChildNodeText(doc, cur, "threat", nvt->m_nvtinfo.res_threat,
                ARR_SIZE(nvt->m_nvtinfo.res_threat));
            if (0 != ret) {
                ret = -8;
                break;
            }

            ret = findChildNodeText(doc, cur, "severity", nvt->m_nvtinfo.res_severity,
                ARR_SIZE(nvt->m_nvtinfo.res_severity));
            if (0 != ret) {
                ret = -9;
                break;
            }

            ret = findChildNodeText(doc, cur, "port", nvt->m_nvtinfo.res_port,
                ARR_SIZE(nvt->m_nvtinfo.res_port));
            if (0 != ret) {
                ret = -10;
                break;
            }

            ret = findChildNodeText(doc, cur, "host", nvt->m_nvtinfo.res_host,
                ARR_SIZE(nvt->m_nvtinfo.res_host));
            if (0 != ret) {
                ret = -11;
                break;
            }

            ret = findChildNodeText(doc, cur, "creation_time", val, ARR_SIZE(val));
            if (0 != ret) {
                ret = -12;
                break;
            } 
            
            /* convert time format */
            ret = utc2LocalTime(nvt->m_nvtinfo.res_create_time,
                ARR_SIZE(nvt->m_nvtinfo.res_create_time),
                val);
            if (0 != ret) {
                ret = -13;
                break;
            }

            child = findChildNode(cur, "nvt");
            if (NULL == child) {
                ret = -14;
                break;
            }
            
            ret = findChildNodeText(doc, child, "cvss_base", 
                nvt->m_nvtinfo.res_cve,
                ARR_SIZE(nvt->m_nvtinfo.res_cve));
            if (0 != ret) {
                ret = -15;
                break;
            } 

            push_back(&info->m_results, (LList_t)nvt); 
            ++cnt;
            nvt = NULL;
        } 

        /* check if get all nvt info */
        if (0 != ret) {
            if (NULL != nvt) {
                freeNvtInfo(nvt);
            }

            break;
        } 

        /* if get all result, set status to done */
        if (info->m_totalCnt == info->m_results.m_cnt) {
            info->m_done = 1;
        }

        ret = 0;
    } while (0);

    if (NULL != obj) {
        xmlXPathFreeObject(obj); 
    }
    
    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);

    if (0 == ret) {
        LOG_DEBUG("parse_result| first=%d| rows=%d| act_cnt=%d|"
            " total_cnt=%d| current_size=%d| done=%d| msg=parse nvt ok|", 
            info->m_first,
            info->m_rows,
            cnt,
            info->m_totalCnt,
            info->m_results.m_cnt,
            info->m_done);

        info->m_first = info->m_results.m_cnt + 1;
        
        return ret;
    } else { 
        LOG_ERROR("parse_result| first=%d| rows=%d| act_cnt=%d|"
            " total_cnt=%d| current_size=%d| done=%d|"
            " ret=%d| size=%d| msg=parse nvt error|", 
            info->m_first,
            info->m_rows,
            cnt,
            info->m_totalCnt,
            info->m_results.m_cnt,
            info->m_done,
            ret, size);

        info->m_first = info->m_results.m_cnt + 1;

        return -1;
    }
}

int parseReport(const kb_buf_t buffer, gvm_report_info_t info) {
    int ret = 0;
    const char DEF_REPORT_PATH[] = "/get_reports_response/report";
    xmlDocPtr doc = NULL;
    xmlXPathContextPtr ctx = NULL; 
    xmlXPathObjectPtr obj = NULL;
    xmlNodeSetPtr node = NULL;
    xmlNodePtr cur = NULL;
    xmlNodePtr child = NULL;
    char val[MAX_COMM_SIZE] = {0};
    
    doc = parseXmlDoc(buffer);
    if (NULL == doc) {
        return -1;
    }

    ctx = xmlXPathNewContext(doc);
    if (NULL == ctx) {
        xmlFreeDoc(doc); 
        return -1;
    }

    obj = findPathNode(ctx, DEF_REPORT_PATH);
    if (NULL == obj) {
        /* if there has no report,  return fail */
        xmlXPathFreeContext(ctx);
        xmlFreeDoc(doc);
        
        return -1;
    } 
    
    do {
        node = obj->nodesetval;
        if (1 != node->nodeNr || XML_ELEMENT_NODE != node->nodeTab[0]->type) {
            ret = -1;
            break;
        }
        
        /* root report */
        cur = node->nodeTab[0];

        /* detail report */
        child = findChildNode(cur, "report");
        if (NULL == child) {
            ret = -1;
            break;
        }

        ret = getNodeAttr(child, "id", info->m_cur_report_id, 
            ARR_SIZE(info->m_cur_report_id));
        if (0 != ret) {
            break;
        }

        ret = findChildNodeText(doc, child, "scan_start", val, ARR_SIZE(val));
        if (0 != ret) {
            break;
        }

        /* convert time format */
        ret = utc2LocalTime(info->m_start_time, ARR_SIZE(info->m_start_time), val);
        if (0 != ret) {
            break;
        }

        ret = findChildNodeText(doc, child, "scan_end", val, ARR_SIZE(val));
        if (0 != ret) {
            break;
        }

        /* convert time format */
        ret = utc2LocalTime(info->m_stop_time, ARR_SIZE(info->m_stop_time), val);
        if (0 != ret) {
            break;
        }
        
        ret = findChildNodeText(doc, child, "scan_run_status", 
            val, ARR_SIZE(val));
        if (0 != ret) {
            break;
        }

        info->m_status = convRunStatusName(val);

        ret = findSimpleNodeText(doc, child, "/task/progress", val, ARR_SIZE(val));
        if (0 != ret) {
            break;
        }

        info->m_progress = atoi(val);
        if (0 > info->m_progress || 100 < info->m_progress) {
            info->m_progress = 0;
        } 
    } while (0); 
    
    xmlXPathFreeObject(obj); 
    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);

    if (0 == ret) {
        LOG_DEBUG("parse_report| report_id=%s| run_status=%d|"
            " progress=%d| msg=ok|",
            info->m_cur_report_id,
            info->m_status,
            info->m_progress);
        
        return ret;
    } else {
        LOG_ERROR("parse_report| report_id=%s| msg=parse error|",
            info->m_cur_report_id);

        return -1;
    }
}

int parseTask(const kb_buf_t buffer, gvm_report_info_t info) {
    int ret = 0;
    const char DEF_TASK_PATH[] = "/get_tasks_response/task";
    xmlDocPtr doc = NULL;
    xmlXPathContextPtr ctx = NULL; 
    xmlXPathObjectPtr obj = NULL;
    xmlNodeSetPtr node = NULL;
    xmlNodePtr cur = NULL;
    char uuid[MAX_UUID_SIZE] = {0};
    char val[MAX_COMM_SIZE] = {0};
    
    doc = parseXmlDoc(buffer);
    if (NULL == doc) {
        return -1;
    }

    ctx = xmlXPathNewContext(doc);
    if (NULL == ctx) {
        xmlFreeDoc(doc); 
        return -1;
    }

    obj = findPathNode(ctx, DEF_TASK_PATH);
    if (NULL == obj) {
        /* if there has no task, return empty */
        xmlXPathFreeContext(ctx);
        xmlFreeDoc(doc);
        
        return 1;
    } 
    
    do {
        node = obj->nodesetval;
        if (1 != node->nodeNr || XML_ELEMENT_NODE != node->nodeTab[0]->type) {
            ret = -1;
            break;
        }
        
        /* root task */
        cur = node->nodeTab[0];

        /* current task id */
        ret = getNodeAttr(cur, "id", uuid, ARR_SIZE(uuid));
        if (-1 == ret) {
            break;
        }

        /* report count */
        ret = findSimpleNodeText(doc, cur, "/report_count", val, ARR_SIZE(val));
        if (0 != ret) {
            break;
        }

        info->m_report_cnt = atoi(val);

        /* finished report count */
        ret = findSimpleNodeText(doc, cur, "/report_count/finished", val, ARR_SIZE(val));
        if (0 != ret) {
            break;
        }

        info->m_finish_cnt = atoi(val);

        /* current report id, may be null */
        ret = findSimpleNodeAttr(doc, cur, "/current_report/report", "id",
            info->m_cur_report_id, ARR_SIZE(info->m_cur_report_id));
        if (-1 == ret) {
            break;
        }

        /* last report id, may be null */
        ret = findSimpleNodeAttr(doc, cur, "/last_report/report", "id",
            info->m_last_report_id, ARR_SIZE(info->m_last_report_id));
        if (-1 == ret) {
            break;
        } 
        
        ret = 0;
    } while (0); 
    
    xmlXPathFreeObject(obj); 
    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);

    if (0 == ret) {
        LOG_DEBUG("parse_task| task_id=%s| report_cnt=%d| finish_report_cnt=%d|"
            " current_report=%s| last_report=%s| msg=ok|",
            uuid,
            info->m_report_cnt, info->m_finish_cnt,
            info->m_cur_report_id,
            info->m_last_report_id);
        
        return ret;
    } else {
        LOG_ERROR("parse_task| task_id=%s| msg=parse error|", uuid);

        return -1;
    }
}

static int printNvts(ListQueue_t queue, kb_buf_t buffer, PWork func) {
    int ret = 0;

    buffer->m_size = 0;
    buffer->m_buf[ buffer->m_size ] = '\0';
    
    ret = for_each(queue, func, buffer);
    return ret;
} 

static int printNvtInfo(void* data, ListNvtInfo_t nvt) {
    int ret = 0;
    int size = 0;
    kb_buf_t buffer = (kb_buf_t)data;

    size = (int)(buffer->m_capacity - buffer->m_size);
    if (MAX_COMM_SIZE < size) {
        ret = snprintf(&buffer->m_buf[ buffer->m_size ], size,
            RESULT_NVT_INFO_FORMAT,
            nvt->m_nvtinfo.res_host,
            nvt->m_nvtinfo.res_name,
            nvt->m_nvtinfo.res_threat,
            nvt->m_nvtinfo.res_severity,
            nvt->m_nvtinfo.res_port,
            nvt->m_nvtinfo.res_cve,
            nvt->m_nvtinfo.res_create_time);

        if (0 < ret && ret < size) {
            buffer->m_size += ret;
            buffer->m_buf[ buffer->m_size ] = '\0';
            return 0;
        } else {
            buffer->m_buf[ buffer->m_size ] = '\0';
            return -1;
        }
    } else {
        return -1;
    }
}

void printResult(kb_buf_t buffer) {
    int ret = 0;
    struct gvm_result_info info;

    initGvmResultInfo(&info);

    ret = parseResult(buffer, &info);
    if (0 == ret) {
        
        ret = printNvts(&info.m_results, buffer, (PWork)printNvtInfo);
        if (0 == ret) {
            LOG_DEBUG("====print_result| size=%d|ctx=[%s]", 
                info.m_results.m_cnt, buffer->m_buf);
        } else {
            LOG_ERROR("====print_result_error| size=%d|", 
                info.m_results.m_cnt);
        }
    } else {
        LOG_ERROR("====print_result_error===");
    }

    resetGvmResultInfo(&info);
    return;
}


int backend_query_result(GvmDataList_t data, 
    ListGvmTask_t task, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0; 
    long long interval = DEF_TASK_CHK_TIME_INTERVAL[GVM_TASK_CHK_RESULT];

    do {
        if (!task->m_result_info.m_done) {
            
            ret = gvm_query_result(task->m_task_info.m_task_id, 
                task->m_report_info.m_cur_report_id, 
                task->m_result_info.m_first,
                task->m_result_info.m_rows,
                tmpbuf, outbuf);
            if (0 != ret) {
                break;
            }

            ret = parseResult(outbuf, &task->m_result_info);
            if (0 != ret) {
                break;
            }
        } 

        if (task->m_result_info.m_done) {
            ret = printNvts(&task->m_result_info.m_results, outbuf, (PWork)printNvtInfo);
            if (0 != ret) {
                break; 
            }
            
            /* write output result to file */
            data->task_ops->write_task_result_file(data, task, outbuf); 

            setTaskChkType(task, GVM_TASK_CHK_TASK); 
            interval = DEF_TASK_CHK_TIME_INTERVAL[GVM_TASK_CHK_TASK];

            /* write to file while check_type changes */
            data->task_ops->write_task_status_file(data, task, tmpbuf);

            /* end and clear old info */
            resetGvmResultInfo(&task->m_result_info);
        } 

        ret = 0;
    } while (0);

    setTaskNextChkTime(task, interval);
    data->task_ops->reque_run_task(data, task);
    return ret;
} 

int backend_query_report(GvmDataList_t data, 
    ListGvmTask_t task, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;
    long long interval = DEF_TASK_CHK_TIME_INTERVAL[GVM_TASK_CHK_REPORT];
    struct gvm_report_info info; 
        
    do {
        initGvmReportInfo(&info);
        
        ret = gvm_query_report(task->m_report_info.m_cur_report_id, tmpbuf, outbuf);
        if (0 != ret) {
            break;
        } 
        
        ret = parseReport(outbuf, &info);
        if (0 != ret) {
            break;
        } 

        if (info.m_status != task->m_report_info.m_status
            || info.m_progress != task->m_report_info.m_progress) { 

            /* set time */ 
            setTaskStartTime(task, info.m_start_time);
            setTaskStopTime(task, info.m_stop_time);

            /* update task status */
            setTaskStatus(task, info.m_status);
            setTaskProgress(task, info.m_progress);

            if (data->task_ops->chk_task_running(data, task)) {
                /* continue, conno op */
            } else if (data->task_ops->chk_task_done(data, task)) {
                /* if task is end, then query the results */            
                setTaskChkType(task, GVM_TASK_CHK_RESULT);
                interval = 0;
            } else {
                /* other status, reset to type task  */
                setTaskChkType(task, GVM_TASK_CHK_TASK);
                interval = DEF_TASK_CHK_TIME_INTERVAL[GVM_TASK_CHK_TASK];
            }

            /* write to file */
            data->task_ops->write_task_status_file(data, task, tmpbuf);
        }

        ret = 0;
    } while (0);

    setTaskNextChkTime(task, interval);
    data->task_ops->reque_run_task(data, task);
    return ret;
}

int backend_query_task(GvmDataList_t data, 
    ListGvmTask_t task, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;
    int need_update = 0;
    long long interval = DEF_TASK_CHK_TIME_INTERVAL[GVM_TASK_CHK_TASK];
    struct gvm_report_info info; 
        
    do {
        initGvmReportInfo(&info);
        
        ret = gvm_query_task(task->m_task_info.m_task_id, tmpbuf, outbuf);
        if (0 != ret) {
            break;
        } 
        
        ret = parseTask(outbuf, &info);
        if (0 != ret) {
            break;
        }

        need_update = updateTaskReport(task, &info);
        if (need_update) {
            task->m_report_info.m_report_cnt = info.m_report_cnt;
            task->m_report_info.m_finish_cnt = info.m_finish_cnt; 

            /* reset time */
            setTaskStartTime(task, info.m_start_time);
            setTaskStopTime(task, info.m_stop_time); 

            /* change status to prepare running */
            setTaskStatus(task, GVM_TASK_READY); 
            setTaskProgress(task, 0); 

            /* change chk type */
            setTaskChkType(task, GVM_TASK_CHK_REPORT); 

            data->task_ops->write_task_status_file(data, task, tmpbuf);
            
            /* go to query the report details */
            interval = 0;
        }
        
        ret = 0;
    } while (0);

    setTaskNextChkTime(task, interval);
    data->task_ops->reque_run_task(data, task);
    
    return ret;
}

int updateTaskReport(ListGvmTask_t task, const gvm_report_info_t info) {
    int need_update = 0;
    
    if ('\0' != info->m_cur_report_id[0]) {
        if (0 != strcmp(task->m_report_info.m_cur_report_id, info->m_cur_report_id)) {
            need_update = 1;
            
            strncpy(task->m_report_info.m_last_report_id, 
                task->m_report_info.m_cur_report_id,
                ARR_SIZE(task->m_report_info.m_last_report_id));

            strncpy(task->m_report_info.m_cur_report_id, 
                info->m_cur_report_id,
                ARR_SIZE(task->m_report_info.m_cur_report_id));
        }
    } else if ('\0' != info->m_last_report_id[0]) {
        if (0 != strcmp(task->m_report_info.m_cur_report_id, info->m_last_report_id)) {
            need_update = 1;

            strncpy(task->m_report_info.m_last_report_id, 
                task->m_report_info.m_cur_report_id,
                ARR_SIZE(task->m_report_info.m_last_report_id));

            strncpy(task->m_report_info.m_cur_report_id, 
                info->m_last_report_id,
                ARR_SIZE(task->m_report_info.m_cur_report_id));
        }
    } else {
        /* no change */
    } 

    return need_update;
} 

void setTaskNextChkTime(ListGvmTask_t task, long long time) {
    if (0 < time) {
        task->m_chk_time = getClkTime() + time;
    } else {
        task->m_chk_time = 0;
    }
}

int isTaskChkTimeExpired(ListGvmTask_t task) {
    long long time = getClkTime();

    if (time >= task->m_chk_time) {
        return 1;
    } else {
        return 0;
    }
} 

int addTaskChecks(GvmDataList_t data, ListGvmTask_t task) {
    LOG_DEBUG("add_task_check| name=%s| status=%d| chk_type=%d|"
        " task_id=%s| target_id=%s|"
        " msg=ok|",
        task->m_task_info.m_task_name,
        getTaskStatus(task),
        getTaskChkType(task),
        task->m_task_info.m_task_id,
        task->m_target_info.m_target_id);

    /* add new task for the first time, set the largest wait time */
    setTaskNextChkTime(task, DEF_TASK_CHK_TIME_INTERVAL[GVM_TASK_CHK_TASK]);
    data->task_ops->add_run_task(data, task); 
    
    return 0;
}

int addGvmChecks(GvmDataList_t data) {
    int ret = 0;
    
    LOG_INFO("add_gvm_checks| total=%d|", data->m_nameset.m_size);
    ret = for_each(&data->m_createque, (PWork)addTaskChecks, (void*)data);
    return ret;
} 

int runGvmChecks(GvmDataList_t data, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    int ret = 0;
    int cnt = 0;
    LList_t _list = NULL;
    ListGvmTask_t task = NULL; 

    do {
        /* if gvm is connect error, no operations */
        if (!data->task_ops->is_gvm_conn_ok(data)) {
            break;
        }
        
        _list = get_head(&data->m_runque);
        if (NULL == _list) {
            /* if no task in the queue */
            break;
        }
        
        /*****attention here for the ptr conversion *****/
        task = runlist_gvm_task(_list);

        /* if not expired, stop */
        ret = isTaskChkTimeExpired(task);
        if (!ret) {
            break;
        } 

        if (GVM_TASK_CHK_TASK == task->m_chk_type) {
            /* query task status */
            ret = backend_query_task(data, task, tmpbuf, outbuf);
        } else if (GVM_TASK_CHK_REPORT == task->m_chk_type) {
            /* query report status */
            ret = backend_query_report(data, task, tmpbuf, outbuf);
        } else if (GVM_TASK_CHK_RESULT == task->m_chk_type) {
            /* query result status */
            ret = backend_query_result(data, task, tmpbuf, outbuf);
        } else { 
            /* no check but delete the task */
            ret = data->task_ops->del_task_relation_files(data, task, tmpbuf);
            if (0 == ret) { 
                /* memory free */
                data->task_ops->free_task(task);
            } else {
                /* fail to delete, then wait the next time */
                setTaskNextChkTime(task, DEF_TASK_CHK_TIME_INTERVAL[GVM_TASK_CHK_DELETED]);
                data->task_ops->reque_run_task(data, task);
            }
        } 

        /* return busy */
        cnt = 1;
    } while (0); 

    return cnt;
}

int testParseXmlPath(const kb_buf_t buffer, const char* path) {
    int ret = 0;
    xmlDocPtr doc = NULL;
    xmlXPathContextPtr ctx = NULL; 
    char val[MAX_COMM_SIZE] = {0};
    
    doc = parseXmlDoc(buffer);
    if (NULL == doc) {
        return -1;
    }

    ctx = xmlXPathNewContext(doc);
    if (NULL == ctx) {
        xmlFreeDoc(doc); 
        return -1;
    }

    ret = findPathObjText(doc, ctx, path, val, ARR_SIZE(val));    
    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);

    if (0 == ret) { 
        printf("parse_xml| path=%s| val=%s| msg=ok|\n", path, val);
    } else {
        printf("parse_xml| ret=%d| path=%s| msg=parse error|\n", ret, path);
    }

    return ret;
}

int testParseXmlSimple(const kb_buf_t buffer, 
    const char* base, const char* path) {
    int ret = 0;
    xmlDocPtr doc = NULL;
    xmlXPathContextPtr ctx = NULL; 
    xmlXPathObjectPtr obj = NULL;
    xmlNodeSetPtr node = NULL;
    xmlNodePtr cur = NULL;
    char val[MAX_MSG_SIZE] = {0};
    
    doc = parseXmlDoc(buffer);
    if (NULL == doc) {
        return -1;
    }

    ctx = xmlXPathNewContext(doc);
    if (NULL == ctx) {
        xmlFreeDoc(doc); 
        return -1;
    }

    obj = findPathNode(ctx, base);
    if (NULL == obj) {
        /* if there has no result, it's ok and return empty */
        xmlXPathFreeContext(ctx);
        xmlFreeDoc(doc);
        
        return 0;
    }

    do {
        node = obj->nodesetval;
        if (1 != node->nodeNr || XML_ELEMENT_NODE != node->nodeTab[0]->type) {
            ret = -1;
            break;
        }

        /* root report */
        cur = node->nodeTab[0];

        /* report count */
        ret = findSimpleNodeText(doc, cur, path, val, ARR_SIZE(val));
        if (0 != ret) {
            break;
        }

        ret = 0;
    } while (0); 
    
    xmlXPathFreeObject(obj); 
    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);

    if (0 == ret) { 
        printf("parse_xml| base=%s| path=%s| val=%s| msg=ok|\n", base, path, val);
    } else {
        printf("parse_xml| ret=%d| base=%s| path=%s| msg=parse error|\n", 
            ret, base, path);
    }

    return ret;
}

int testParseXmlSimpleAttr(const kb_buf_t buffer, 
    const char* base, const char* path, const char* attr) {
    int ret = 0;
    xmlDocPtr doc = NULL;
    xmlXPathContextPtr ctx = NULL; 
    xmlXPathObjectPtr obj = NULL;
    xmlNodeSetPtr node = NULL;
    xmlNodePtr cur = NULL;
    char val[MAX_COMM_SIZE] = {0};
    
    doc = parseXmlDoc(buffer);
    if (NULL == doc) {
        return -1;
    }

    ctx = xmlXPathNewContext(doc);
    if (NULL == ctx) {
        xmlFreeDoc(doc); 
        return -1;
    }

    obj = findPathNode(ctx, base);
    if (NULL == obj) {
        /* if there has no result, it's ok and return empty */
        xmlXPathFreeContext(ctx);
        xmlFreeDoc(doc);
        
        return 0;
    }

    do {
        node = obj->nodesetval;
        if (1 != node->nodeNr || XML_ELEMENT_NODE != node->nodeTab[0]->type) {
            ret = -1;
            break;
        }

        /* root report */
        cur = node->nodeTab[0];

        /* report count */
        ret = findSimpleNodeAttr(doc, cur, path, attr, val, ARR_SIZE(val));
        if (0 != ret) {
            break;
        }

        ret = 0;
    } while (0); 
    
    xmlXPathFreeObject(obj); 
    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);

    if (0 == ret) { 
        printf("parse_xml| base=%s| path=%s| attr=%s| val=%s| msg=ok|\n", 
            base, path, attr, val);
    } else {
        printf("parse_xml| ret=%d| base=%s| path=%s| attr=%s| msg=parse error|\n", 
            ret, base, path, attr);
    }

    return ret;
}


int validateGvmConn(GvmDataList_t data, kb_buf_t tmpbuf, kb_buf_t outbuf) {
    static int is_last_status_ok = 1;
    int ret = 0;

    ret = gvm_get_version(NULL, 0, tmpbuf, outbuf);
    if (0 == ret) {
        if (!is_last_status_ok) {
            is_last_status_ok = 1;
        }

        if (!data->m_is_gvm_conn_ok) {
            data->m_is_gvm_conn_ok = 1;
        }
    } else {
        /* if Two consecutive failures, then set conn status to fail */
        if (is_last_status_ok) {
            is_last_status_ok = 0;
        } else {
            if (data->m_is_gvm_conn_ok) {
                data->m_is_gvm_conn_ok = 0;
            }
        } 
    }
  
    return ret;
}

