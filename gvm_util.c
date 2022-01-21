/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2016 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_gvm_util.h"

/* If you declare any globals in php_gvm_util.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(gvm_util)
*/

/* True global resources - no need for thread safety here */
static int le_gvm_util;

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("gvm_util.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_gvm_util_globals, gvm_util_globals)
    STD_PHP_INI_ENTRY("gvm_util.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_gvm_util_globals, gvm_util_globals)
PHP_INI_END()
*/
/* }}} */

/* Remove the following function when you have successfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/

/* {{{ proto int php_gvm_cmd_entry(string cmd, string param)
    */
PHP_FUNCTION(php_gvm_cmd_entry)
{
    int ret = 0;
	char *cmd = NULL;
	char *param = NULL;
	int argc = ZEND_NUM_ARGS();
	int cmd_len;
	int param_len;

	if (zend_parse_parameters(argc TSRMLS_CC, "ss", &cmd, &cmd_len, &param, &param_len) == FAILURE) 
		RETURN_LONG(-1);

    ret = openvas_cmd_entry(cmd, cmd_len, param, param_len);
    RETURN_LONG(ret);
}
/* }}} */


/* {{{ php_gvm_util_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_gvm_util_init_globals(zend_gvm_util_globals *gvm_util_globals)
{
	gvm_util_globals->global_value = 0;
	gvm_util_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(gvm_util)
{
	/* If you have INI entries, uncomment these lines 
	REGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(gvm_util)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(gvm_util)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(gvm_util)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(gvm_util)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "gvm_util support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */

/* {{{ gvm_util_functions[]
 *
 * Every user visible function must have an entry in gvm_util_functions[].
 */
const zend_function_entry gvm_util_functions[] = {
	PHP_FE(php_gvm_cmd_entry,	NULL)
	PHP_FE_END	/* Must be the last line in gvm_util_functions[] */
};
/* }}} */

/* {{{ gvm_util_module_entry
 */
zend_module_entry gvm_util_module_entry = {
	STANDARD_MODULE_HEADER,
	"gvm_util",
	gvm_util_functions,
	PHP_MINIT(gvm_util),
	PHP_MSHUTDOWN(gvm_util),
	PHP_RINIT(gvm_util),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(gvm_util),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(gvm_util),
	PHP_GVM_UTIL_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_GVM_UTIL
ZEND_GET_MODULE(gvm_util)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
