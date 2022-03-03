#include<stdlib.h> 
#include<string.h>
#include"base_openvas.h"
#include"task_openvas.h"
#include"openvas_business.h"
#include"php_openvas.h"
#include"openvas_opts.h"
#include"comm_misc.h"
#include"hydra_business.h"


#undef LOG_INFO
#define LOG_INFO(format,...) fprintf(stdout, format, ##__VA_ARGS__)


void flushStdin() {
    while (!feof(stdin) && '\n' != getchar());
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
        } else {
            LOG_INFO("%s| response=%s|\n", cmd, output.m_buf);
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
    LOG_INFO("ret=%d|\n", ret);
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

    LOG_INFO("check hosts:\n"); 

    getLine("input hosts:", msg);
    ret = chkHosts(msg);
    if (0 == ret) {
        LOG_INFO("chk hosts ok\n");
    } else {
        LOG_INFO("chk hosts error\n");
    }
}

void test07() {
    int ret = 0;
    char msg[1024] = {0};

    LOG_INFO("check extended hosts:\n"); 

    getLine("input hosts:", msg);
    ret = chkHostsExt(msg);
    if (0 == ret) {
        LOG_INFO("check extended hosts ok\n");
    } else {
        LOG_INFO("check extended hosts error\n");
    }
}

void test08() {
    int ret = 0;
    char msg[1024] = {0};

    LOG_INFO("check name:\n"); 

    getLine("input name:", msg);
    ret = chkName(msg);
    if (0 == ret) {
        LOG_INFO("check name ok\n");
    } else {
        LOG_INFO("check name error\n");
    }
}

void test09() {
    int ret = 0;
    char msg[1024] = {0};
 
    LOG_INFO("escape host:\n"); 

    getLine("input host:", msg); 

    LOG_INFO("original hosts=[%s]\n", msg);
    ret = escapeHosts(msg);
    LOG_INFO("escape hosts=[%s]\n", msg);
}

void test31() {
    int ret = 0;
    char msg[1024] = {0};
 
    LOG_INFO("check month day list:\n"); 
    getLine("input monthday:", msg); 
    
    ret = regmatch(msg, CUSTOM_WHOLE_MATCH_OR_REPEAT(CUSTOM_MONTH_DAY_PATTERN));
    LOG_INFO("ret=%d| month_days=[%s]\n", ret, msg);
}

void test32() {
    int ret = 0;
    char msg[1024] = {0};
 
    LOG_INFO("check week day list:\n"); 
    getLine("input week day:", msg); 
    
    ret = regmatch(msg, CUSTOM_WHOLE_MATCH_OR_REPEAT(CUSTOM_WEEK_DAY_PATTERN));
    LOG_INFO("ret=%d| week_days=[%s]\n", ret, msg);
} 

void test33() {
    int ret = 0;
    char msg[1024] = {0};
 
    LOG_INFO("check timestamp:\n"); 
    getLine("input timestamp:", msg); 
    
    ret = test_regmatch(msg, CUSTOM_WHOLE_MATCH_OR_NULL(CUSTOM_TIME_STAMP_PATTERN));
    LOG_INFO("ret=%d| timestamp=[%s]\n", ret, msg);
}

void test34() {
    int ret = 0;
    char msg[1024] = {0};
    char time[MAX_TIMESTAMP_SIZE] = {0};
 
    LOG_INFO("get timestamp pattern key:\n"); 
    getLine("input text:", msg); 
    
    ret = getPatternKey(msg, "timestamp", 
        CUSTOM_MATCH_OR_NULL(CUSTOM_TIME_STAMP_PATTERN),
        time, ARR_SIZE(time));
    LOG_INFO("ret=%d| timestamp=[%s]\n", ret, time);
} 

void test10() {
    int ret = 0;
    int cmdlen = 0;
    int paramlen = 0;
    char cmd[1024] = {0};
    char param[1024] = {0};

    LOG_INFO("test openvas_cmd_entry\n");

    getText("cmd is:", cmd); 
    getLine("param is:", param); 

    cmdlen = (int)strlen(cmd);
    paramlen = (int)strlen(param);
    LOG_INFO("cmd=%s| param=%s|\n", cmd, param);
    ret = openvas_cmd_entry(cmd, cmdlen, param, paramlen);
    LOG_INFO("ret=[%d]\n", ret);
}

void test20() {
    int ret = 0;
    char inbuf[MAX_COMM_SIZE] = {0};
    char outbuf[MAX_COMM_SIZE] = {0};

    LOG_INFO("test local2SchedTime conversion:\n");
    getLine("input time is:", inbuf); 

    ret = local2SchedTime(outbuf, ARR_SIZE(outbuf), inbuf);
    LOG_INFO("ret=[%d]| out=%s|\n", ret, outbuf);
}

void test21() {
    int ret = 0;
    char inbuf[MAX_COMM_SIZE] = {0};
    char outbuf[MAX_COMM_SIZE] = {0};

    LOG_INFO("test utc2LocalTime conversion:\n");
    getLine("input time is:", inbuf); 

    ret = utc2LocalTime(outbuf, ARR_SIZE(outbuf), inbuf);
    LOG_INFO("ret=[%d]| out=%s|\n", ret, outbuf);
}

void test22() {
    int ret = 0;
    int is_local = 0;
    long long time = 0L;
    char inbuf[MAX_COMM_SIZE] = {0};
    char outbuf[MAX_COMM_SIZE] = {0};

    LOG_INFO("test asc2time conversion:\n");
    
    getText("is_local is:", inbuf);
    is_local = atoi(inbuf);
    
    getLine("input asc is:", inbuf); 
    getLine("input format is:", outbuf); 

    ret = asc2time(&time, inbuf, outbuf, is_local);
    LOG_INFO("ret=[%d]| time=%lld| text=%s| format=%s| is_local=%d|\n", 
        ret, time, inbuf, outbuf, is_local);
}

void test23() {
    int ret = 0;
    int is_local = 0;
    long long time = 0L;
    char inbuf[MAX_COMM_SIZE] = {0};
    char outbuf[MAX_COMM_SIZE] = {0};

    LOG_INFO("test time2asc conversion:\n");

    getText("is_local is:", inbuf);
    is_local = atoi(inbuf);
    
    getText("input time is:", inbuf); 
    time = atoll(inbuf);
    
    getLine("input format is:", outbuf); 
    
    ret = time2asc(&time, outbuf, inbuf, ARR_SIZE(inbuf), is_local);
    LOG_INFO("ret=[%d]| time=%lld| text=%s| format=%s| is_local=%d|\n", 
        ret, time, inbuf, outbuf, is_local);
} 

void test24() {
    int ret = 0;
    char text[MAX_COMM_SIZE] = {0};
    char pattern[MAX_COMM_SIZE] = {0};

    LOG_INFO("===test regmatch=====");
    getLine("text is:", text); 
    getLine("pattern is:", pattern); 
    

    ret = test_regmatch(text, pattern);
    LOG_INFO("ret=[%d]| text=%s| pattern=%s|\n", ret, text, pattern);
}

void test35() {
    int ret = 0;
    struct kb_buf tmpbuf;
    char cmd[1024] = "";

    ret = genBuf(MAX_CACHE_SIZE, &tmpbuf);

    LOG_INFO("test create hydra===|\n"); 
    getLine("input cmd:\n", cmd);
    ret = php_create_hydra(cmd, 0, &tmpbuf);
    LOG_INFO("ret=%d|\n", ret);
}

void test36() {
    int ret = 0;
    struct kb_buf tmpbuf;
    char cmd[1024] = "";

    ret = genBuf(MAX_CACHE_SIZE, &tmpbuf);

    LOG_INFO("test delete hydra===|\n"); 
    getLine("input cmd:\n", cmd);
    ret = php_delete_hydra(cmd, 0, &tmpbuf);
    LOG_INFO("ret=%d|\n", ret);
}

void test37() {
    int ret = 0;
    struct kb_buf tmpbuf;
    char cmd[1024] = "";

    ret = genBuf(MAX_CACHE_SIZE, &tmpbuf);

    LOG_INFO("test start hydra===|\n"); 
    getLine("input cmd:\n", cmd);
    ret = php_start_hydra(cmd, 0, &tmpbuf);
    LOG_INFO("ret=%d|\n", ret);
}

void test38() {
    int ret = 0;
    struct kb_buf tmpbuf;
    char cmd[1024] = "";

    ret = genBuf(MAX_CACHE_SIZE, &tmpbuf);

    LOG_INFO("test stop hydra===|\n"); 
    getLine("input cmd:\n", cmd);
    ret = php_stop_hydra(cmd, 0, &tmpbuf);
    LOG_INFO("ret=%d|\n", ret);
} 


void test42() {
    int ret = 0;
    char dir[MAX_FILENAME_PATH_SIZE] = {0};
 
    LOG_INFO("delete dir:\n"); 
    getText("input dir:", dir); 
    
    ret = deleteDir(dir);
    LOG_INFO("ret=[%d]| dir=%s|\n", ret, dir);
} 

void test43() {
    int ret = 0;
    int cnt = 100;
    char uuid[MAX_UUID_SIZE] = "";


    LOG_INFO("test genUUID===|\n"); 
    while (0 <= --cnt) {
        ret = genUUID(uuid, sizeof(uuid));
        LOG_INFO("ret=%d| uuid=%s|\n", ret, uuid);
    }
}

void test44() {
    int ret = 0;
    char msg[MAX_MSG_SIZE] = "";

    LOG_INFO("test chkHydraHosts===|\n"); 
    getLine("input hosts:", msg);
    ret = chkHydraHosts(0, msg);
    LOG_INFO("ret=%d| hosts=%s|\n", ret, msg);
}

void test45() {
    int ret = 0;
    int port = 0;
    char buf[MAX_COMM_MIN_SIZE] = {0};

    LOG_INFO("test port===|\n"); 

    do {
        snprintf(buf, ARR_SIZE(buf), "%d", port);
        
        ret = regmatch(buf, CUSTOM_WHOLE_MATCH(CUSTOM_PORT_PATTERN));
    } while (0 == ret && ++port < 0x10000);
    
    LOG_INFO("ret=%d| port=%s|\n", ret, buf);
}


void test46() {
    int ret = 0;
    int port = 0;
    int len = 0;
    char msg[MAX_MSG_SIZE] = "";

    LOG_INFO("test ip_or_port===|\n"); 

    len = snprintf(msg, ARR_SIZE(msg), "127.0.0.1:");

    do {
        snprintf(&msg[len], ARR_SIZE(msg), "%d", port);
        
        ret = regmatch(msg, CUSTOM_WHOLE_MATCH(CUSTORM_IP_OR_PORT));
    } while (0 == ret && ++port < 0x10000);
    
    LOG_INFO("ret=%d| ip_or_port=%s|\n", ret, msg);
}


int test(int option) {
    int ret = 0;
    
    ret = initDaemon();
    if (0 != ret) {
        return ret;
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

    case 20:
        test20();
        break;

    case 21:
        test21();
        break;

    case 22:
        test22();
        break;

    case 23:
        test23();
        break;

    case 24:
        test24();
        break;

    case 31:
        test31();
        break;

    case 32:
        test32();
        break;

    case 33:
        test33();
        break;

    case 34:
        test34();
        break;

    case 35:
        test35();
        break;

    case 36:
        test36();
        break;

    case 37:
        test37();
        break;

    case 38:
        test38();
        break;;

    case 42:
        test42();
        break;

    case 43:
        test43();
        break;

    case 44:
        test44();
        break;

    case 45:
        test45();
        break;

    case 46:
        test46();
        break;
        
    default:
        fprintf(stderr, "invalid option\n");
        break;
    }

    finishDaemon();
    return ret;
}


