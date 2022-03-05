#ifndef __COMM_MISC_H__
#define __COMM_MISC_H__
#include"base_openvas.h"
#include"llist.h"


#ifdef __cplusplus
extern "C" {
#endif


/* param isFile: 0: dir, 1: file
  * return : 0: exists, 1: no eixsts, -2: exists but type error, -1: error 
**/
extern int chkExists(const char dir[], int isFile);

/* delete a file, return: 0-delete, 1:no file, -2: fail for directory, -1: error */
extern int deleteFile(const char path[]);

/* return: 0:ok, -1:open err, -2: write err */
int writeFile(const kb_buf_t buffer, const char filename[]);

/* write buffer to a tmpfile, then overrides the normal file,
    return: 0:ok, -1:open err, -2: write err, -3: rename err */
extern int writeFileSafe(const kb_buf_t buffer, const char normalFile[],
    const char tmpFile[]);

/* append buffer to file,  return: 0:ok, -1:open err, -2: write err  */
extern int appendFile(const kb_buf_t buffer, const char filename[]);

/* return: 0: ok, 1: exits already, -2: exists but type err, -1: error */ 
extern int createDir(const char dir[]);

/* delete a file, return: 0-delete, 1:no dir, -2: fail for not directory, -1: error */
extern int deleteDir(const char dir[]);

/* return: 0: ok, 1: no file, 2: empty file, -1: error */
extern int readTotalFile(const char name[], kb_buf_t cache);

/* return: 0: ok, 1: no file,  -1: error, -2: write err */ 
int copyFileList(const char basedir[], const char filelist[], 
    const char dstFile[]);

/* return: 0: ok, 1: no file,  -1: error, -2: write err, -3: rename err */ 
extern int copyFileListSafe(const char basedir[], const char filelist[], 
    const char dstFile[], const char dstFileTmp[]);

extern void getDevRand(unsigned int* buf);
extern int genUUID(char id[], int maxlen);

/* return: 0: ok, 1: empty, -1: exceed max size  */ 
extern int getNextToken(const char* text, const char* needle,
    char* buf, int maxlen, const_char_t* saveptr);

/* return: 0: ok, 1: empty, -1: exceed max size, -2: not digit  */
extern int getNextTokenInt(const char* text, const char* needle,
    int* val, const_char_t* saveptr);

extern int trimText(char* text);

extern int killSafe(int pid, int sig);
extern int killProc(int pid);
extern int stopProc(int pid);

/* return: >0: childpid in parent, =0: in child, -1: error */ 
extern int forkSafe();

extern int createPidFile(const char path[], int pid);

/* only return from parent, cmds run in child and never return 
 * return: >0: child pid in parent,  -1: error */
extern int execvSafe(char* cmds[]);

#ifdef __cplusplus
}
#endif 


#endif

