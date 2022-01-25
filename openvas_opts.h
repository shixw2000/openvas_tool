#ifndef __OPENVAS_OPTS_H__
#define __OPENVAS_OPTS_H__
#include"llist.h" 
#include"base_openvas.h" 
#include"openvas_business.h"


#ifdef __cplusplus
extern "C" {
#endif


extern void resetGvmResultInfo(gvm_result_info_t result);
extern void initGvmResultInfo(gvm_result_info_t result);

extern void initGvmReportInfo(gvm_report_info_t info);
extern void initGvmTaskInfo(gvm_task_info_t info);

extern int backend_query_result(GvmDataList_t data, 
    ListGvmTask_t task, kb_buf_t tmpbuf, kb_buf_t outbuf);

extern int backend_query_report(GvmDataList_t data, 
    ListGvmTask_t task, kb_buf_t tmpbuf, kb_buf_t outbuf);

extern int backend_query_task(GvmDataList_t data, 
    ListGvmTask_t task, kb_buf_t tmpbuf, kb_buf_t outbuf);

extern int runGvmChecks(GvmDataList_t data, kb_buf_t tmpbuf, kb_buf_t outbuf);
extern int addGvmChecks(GvmDataList_t data);
extern int addTaskChecks(GvmDataList_t data, ListGvmTask_t task);

extern int initLibXml();
extern void finishLibXml();

extern int testParseXmlPath(const kb_buf_t buffer, const char* path);
extern int testParseXmlSimple(const kb_buf_t buffer, 
    const char* base, const char* path);
extern int testParseXmlSimpleAttr(const kb_buf_t buffer, 
    const char* base, const char* path, const char* attr);

extern int updateTaskReport(ListGvmTask_t task, const gvm_report_info_t info);

extern void setTaskNextChkTime(ListGvmTask_t task, long long time);
extern int isTaskChkTimeExpired(ListGvmTask_t task);

extern int deleteTaskWhole(GvmDataList_t data, ListGvmTask_t task, kb_buf_t tmpbuf); 


extern int gvm_get_version(char ver[], int maxlen,
    kb_buf_t tmpbuf, kb_buf_t outbuf);

extern int gvm_create_schedule(const char* name, enum ICAL_DATE_REP_TYPE type,
    const char* firstRun, const char* _list,
    char uuid[], int maxlen,
    kb_buf_t tmpbuf, kb_buf_t outbuf);

extern int gvm_option_create_schedule(gvm_task_info_t info,
    kb_buf_t tmpbuf, kb_buf_t outbuf); 

extern int gvm_delete_schedule(const char* uuid, 
    kb_buf_t tmpbuf, kb_buf_t outbuf);

extern int gvm_option_delete_schedule(gvm_task_info_t info,
    kb_buf_t tmpbuf, kb_buf_t outbuf);

extern int gvm_option_create_config(gvm_task_info_t info, 
    kb_buf_t tmpbuf, kb_buf_t outbuf);

extern int gvm_create_config_ex(const char* name,
    const char* group_id, const char* group_name,
    char* config_id, int maxlen, 
    kb_buf_t tmpbuf, kb_buf_t outbuf);

extern int gvm_delete_config(const char* uuid, kb_buf_t tmpbuf, kb_buf_t outbuf);

extern int gvm_option_delete_config(gvm_task_info_t info,
    kb_buf_t tmpbuf, kb_buf_t outbuf);

extern int validateGvmConn(GvmDataList_t data, kb_buf_t tmpbuf, kb_buf_t outbuf);

#ifdef __cplusplus
}
#endif 

#endif

