#ifndef __PHP_OPENVAS_H__
#define __PHP_OPENVAS_H__


#ifdef __cplusplus
extern "C" {
#endif


/* user define types used by php */
#pragma pack(push, 4)

#pragma pack(pop)


extern int openvas_cmd_entry(const char* cmd, int cmdlen, 
    const char* param, int paramlen);


#ifdef __cplusplus
}
#endif

#endif

