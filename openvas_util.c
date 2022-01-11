#include<php.h>


zend_function_entry openvas_util_functions[] = {
    PHP_FE(php_openvas_cmd_entry,  NULL)      
    {NULL, NULL, NULL} 
};


PHP_FUNCTION( php_openvas_cmd_entry ) {
    int ret = 0;
    char* cmd = NULL;
    int cmdlen = 0;
    char* param = NULL;
    int paramlen = 0;

    ret = zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ss", 
        &cmd, &cmdlen, &param, &paramlen);
    if (0 != ret) {
        RETURN_LONG(-1);
    }

    ret = openvas_cmd_entry(cmd, cmdlen, param, paramlen);
    RETURN_LONG(ret); 
}

