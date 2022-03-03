#include<stdlib.h> 
#include<string.h>
#include"base_openvas.h"
#include"task_openvas.h"
#include"openvas_business.h"
#include"php_openvas.h"
#include"openvas_opts.h"
#include"comm_misc.h"
#include"hydra_business.h"


extern int test(int option);

int serveGvm() {
    int ret = 0;

    ret = isRun();
    if (0 != ret) {
        return ret;
    }

    do {
        ret = initDaemon();
        if (0 != ret) {
            break;
        }

        ret = initHydra();
        if (0 != ret) {
            break;
        }
        
        ret = setBackgroud();
        if (0 != ret) {
            break;
        }

        ret = startKbMsg(OPENVAS_MSG_NAME);
        if (0 != ret) {
            break;
        }
    } while (0);

    finishHydra();
    finishDaemon();
    return ret;
}

static int cliHydra(const char cmd[], const char taskname[], 
    const char taskid[]) {
    int ret = 0;
    int cmdlen = 0;
    struct kb_buf tmpbuf;

    ret = genBuf(MAX_CACHE_SIZE, &tmpbuf); 
    if (0 == ret) {
        tmpbuf.m_size = snprintf(tmpbuf.m_buf, tmpbuf.m_capacity, 
            "taskname=\"%s\"&taskid=\"%s\"", 
            taskname, taskid);

        cmdlen = strlen(cmd);
        ret = openvas_cmd_entry(cmd, cmdlen, tmpbuf.m_buf, tmpbuf.m_size);

        /* free cache */
        freeBuf(&tmpbuf);
    } else { 
        ret = -1;
    }
    
    return ret;
} 

int main(int argc, char* argv[]) {
    int ret = 0;
    int log_level = 0;
    int option = 0;

    if (2 > argc) {
        fprintf(stdout, "invalid usage\n");
        return -1;
    } 

    option = atoi(argv[1]); 

    if (1 == option) {
        if (3 == argc) {
            log_level = atoi(argv[2]);
            setMyLogLevel(log_level);
        }
        
        setArgs(argc, argv);
        
        ret = serveGvm();
    } else if (2 == option) {
        if (5 == argc) {
            ret = cliHydra(argv[2], argv[3], argv[4]);
        } else {
            ret = -1;
        }
    } else {
        if (3 == argc) {
            log_level = atoi(argv[2]);
            setMyLogLevel(log_level);
        }

        if (1000 < option) {
            ret = test(option-1000);
        }
    }
    
    return ret;
}

