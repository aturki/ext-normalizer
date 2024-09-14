#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_var.h"
#include "zend.h"
#include "zend_attributes.h"
#include "zend_interfaces.h"
#include "php_normalizer.h"
#include "src/attributes/normalizer_attributes.h"
#include "src/normalizer/object_normalizer_ce.h"


zend_module_entry normalizer_module_entry = {STANDARD_MODULE_HEADER,
                                             "normalizer",
                                             NULL, // Register functions
                                             PHP_MINIT(normalizer),
                                             PHP_MSHUTDOWN(normalizer),
                                             PHP_RINIT(normalizer),     // Request init
                                             PHP_RSHUTDOWN(normalizer), // Request shutdown
                                             PHP_MINFO(normalizer),
                                             PHP_NORMALIZER_VERSION,
                                             STANDARD_MODULE_PROPERTIES};

#ifdef COMPILE_DL_NORMALIZER
ZEND_GET_MODULE(normalizer)
#endif


PHP_MINIT_FUNCTION(normalizer)
{
    php_register_ignore_attribute();
    php_register_expose_attribute();
    php_register_max_depth_attribute();
    php_register_groups_attribute();
    php_register_serialized_name_attribute();

    register_object_normalizer_class();

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(normalizer)
{
    return SUCCESS;
}

PHP_RINIT_FUNCTION(normalizer)
{
#if defined(COMPILE_DL_NORMALIZER) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(normalizer)
{
    return SUCCESS;
}

PHP_MINFO_FUNCTION(normalizer)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "normalizer support", "enabled");
    php_info_print_table_header(2, "normalizer version", PHP_NORMALIZER_VERSION);
    php_info_print_table_end();
}
