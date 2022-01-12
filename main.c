#include<stdlib.h> 
#include<string.h>
#include"base_openvas.h"
#include"task_openvas.h"
#include"openvas_business.h"
#include"php_openvas.h"
#include"openvas_opts.h"


#undef LOG_INFO
#define LOG_INFO(format,...) fprintf(stdout, format, ##__VA_ARGS__)

void serveGvm() {
    int ret = 0;

    ret = isRun();
    if (0 != ret) {
        return;
    }

    do {
        ret = initDaemon();
        if (0 != ret) {
            break;
        }
        
        ret = setBackgroud();
        if (0 != ret) {
            break;
        }

        ret = startKbMsg();
        if (0 != ret) {
            break;
        }
    } while (0);

    finishDaemon();
    return;
}

void flushStdin() {
    while ('\n' != getchar() && !feof(stdin));
}

int getLine(const char* promt, char line[]) { 
    flushStdin();
    LOG_INFO("%s", promt);
    if (!ferror(stdin) && !feof(stdin)) {
        gets(line);

        return 0;
    } else {
        line[0] = '\0';
        
        return -1;
    }
}

int getText(const char* promt, char text[]) {    
    LOG_INFO("%s", promt);
    
    if (!ferror(stdin) && !feof(stdin)) {
        scanf("%s", text);

        return 0;
    } else {
        text[0] = '\0';

        return -1;
    }
}

void test02() {
    int ret = 0;
    struct kb_buf input;
    struct kb_buf output;
    char cmd[MAX_COMM_SIZE] = {0};
    
    ret = genBuf(MAX_CACHE_SIZE, &input);
    ret = genBuf(MAX_CACHE_SIZE, &output);

    LOG_INFO("====test gvm-cli====\n");
    
    while (0 == getText("cmd is:", cmd)
        && 0 == getLine("gvm-cli xml:\n", input.m_buf)) {

        input.m_size = strlen(input.m_buf); 
        ret = procGvmTask(cmd, &input, &output);
        if (0 == ret && 0 == strcmp(cmd, "get_results")) {
            printResult(&output);    
        }
    }
}

void test03() {
    int ret = 0;
    struct kb_buf input;
    struct kb_buf output;
    char cmd[MAX_COMM_SIZE] = {0};
    char path[MAX_COMM_SIZE] = {0};
    
    ret = genBuf(MAX_CACHE_SIZE, &input);
    ret = genBuf(MAX_CACHE_SIZE, &output);

    getText("cmd is:", cmd);

    if (0 == getLine("gvm-cli xml:\n", input.m_buf)) {
        input.m_size = strlen(input.m_buf); 
        ret = procGvmTask(cmd, &input, &output);
        while (0 <= ret && 0 == getText("path is:", path)) {            
            ret = testParseXmlPath(&output, path);
        }
    }
}

void test13() {
    int ret = 0;
    struct kb_buf input;
    struct kb_buf output;
    char cmd[MAX_COMM_SIZE] = {0};
    char base[MAX_COMM_SIZE] = {0};
    char path[MAX_COMM_SIZE] = {0};
    
    ret = genBuf(MAX_CACHE_SIZE, &input);
    ret = genBuf(MAX_CACHE_SIZE, &output);

    getText("cmd is:", cmd);
    getText("base is:", base); 

    if (0 == getLine("gvm-cli xml:\n", input.m_buf)) { 
        input.m_size = strlen(input.m_buf); 
        
        ret = procGvmTask(cmd, &input, &output);
        while (0 <= ret && 0 == getText("path is:", path)) { 
            ret = testParseXmlSimple(&output, base, path);
        }
    }
} 

void test14() {
    int ret = 0;
    struct kb_buf input;
    struct kb_buf output;
    char cmd[MAX_COMM_SIZE] = {0};
    char base[MAX_COMM_SIZE] = {0};
    char path[MAX_COMM_SIZE] = {0};
    char attr[MAX_COMM_SIZE] = {0};
    
    ret = genBuf(MAX_CACHE_SIZE, &input);
    ret = genBuf(MAX_CACHE_SIZE, &output);

    getText("cmd is:", cmd);
    getText("base is:", base); 

    if (0 == getLine("gvm-cli xml:\n", input.m_buf)) { 
        input.m_size = strlen(input.m_buf); 
        ret = procGvmTask(cmd, &input, &output);
        
        while (0 <= ret && 0 == getText("path is:", path)
            && 0 == getText("attr is:", attr)) { 
            ret = testParseXmlSimpleAttr(&output, base, path, attr);
        }
    }
} 


void test04() {
    int ret = 0;
    struct kb_buf tmpbuf;
    char cmd[1024] = "";

    ret = genBuf(MAX_CACHE_SIZE, &tmpbuf);
    
    getLine("create task:\n", cmd);
    ret = php_create_task(cmd, 0, &tmpbuf);
} 

void test05() {
    int ret = 0;
    struct kb_buf tmpbuf;
    struct kb_buf input;

    ret = genBuf(MAX_CACHE_SIZE, &tmpbuf);
    ret = genBuf(MAX_CACHE_SIZE, &input);

    getLine("start task:\n", input.m_buf);
    php_start_task(input.m_buf, 0, &tmpbuf);

    getLine("stop task:\n", input.m_buf);
    php_stop_task(input.m_buf, 0, &tmpbuf);

    getLine("delete task:\n", input.m_buf);
    php_delete_task(input.m_buf, 0, &tmpbuf);    
}

void test06() {
    int ret = 0;
    char msg[1024] = {0};

    getText("check uuid:", msg);        
    ret = chkUuid(msg);
    if (0 == ret) {
        LOG_INFO("chkUuid ok\n");
    } else {
        LOG_ERROR("chkUuid error\n");
    }
}

void test07() {
    int ret = 0;
    char msg[1024] = {0};

    getText("check hosts:", msg); 
    ret = chkHosts(msg);
    if (0 == ret) {
        LOG_INFO("check hosts ok\n");
    } else {
        LOG_ERROR("check hosts error\n");
    }
}

void test08() {
    int ret = 0;
    char msg[1024] = {0};

    getText("check name:", msg); 
    ret = chkName(msg);
    if (0 == ret) {
        LOG_INFO("check name ok\n");
    } else {
        LOG_ERROR("check name error\n");
    }
}

void test09() {
    int ret = 0;
    char msg[1024] = {0};
 
    getText("escape hosts:", msg); 

    LOG_INFO("original hosts=[%s]\n", msg);
    ret = escapeHosts(msg);
    LOG_INFO("escape hosts=[%s]\n", msg);
}

void test10() {
    int ret = 0;
    int cmdlen = 0;
    int paramlen = 0;
    char cmd[1024] = {0};
    char param[1024] = {0};

    LOG_INFO("test openvas_cmd_entry\n");

    getText("cmd is:", cmd); 
    getText("param is:", param); 

    cmdlen = (int)strlen(cmd);
    paramlen = (int)strlen(param);
    LOG_INFO("cmd=%s| param=%s|\n", cmd, param);
    ret = openvas_cmd_entry(cmd, cmdlen, param, paramlen);;
    LOG_INFO("ret=[%d]\n", ret);
}

void test(int option) {
    int ret = 0;
    
    ret = initDaemon();
    if (0 != ret) {
        return;
    }
    
    switch (option) { 
    case 2:
        test02();
        break;

    case 3:
        test03();
        break;

    case 4:
        test04();
        break;

    case 5:
        test05();
        break;

    case 6:
        test06();
        break;

    case 7:
        test07();
        break;

    case 8:
        test08();
        break;

    case 9:
        test09();
        break;

    case 10:
        test10();
        break;

    case 13:
        test13();
        break;

    case 14:
        test14();
        break;
        
    default:
        fprintf(stderr, "invalid option\n");
        break;
    }

    finishDaemon();
    return;
}

int main(int argc, char* argv[]) {
    int ret = 0;
    int log_level = 0;
    int option = 0;

    if (2 > argc) {
        fprintf(stderr, "invalid usage\n");
        return -1;
    } else {
        option = atoi(argv[1]);
        
        if (3 == argc) {
            log_level = atoi(argv[2]);
            setMyLogLevel(log_level);
        }
    }

    if (1 == option) {
        serveGvm();
    } else {
        test(option);
    }
    
    return ret;
}

