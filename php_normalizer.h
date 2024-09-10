#ifndef PHP_NORMALIZER_H
#define PHP_NORMALIZER_H

extern zend_module_entry normalizer_module_entry;
#define phpext_normalizer_ptr &normalizer_module_entry

#define PHP_NORMALIZER_VERSION "0.1.0"

#ifdef PHP_WIN32
#	define PHP_NORMALIZER_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_NORMALIZER_API __attribute__ ((visibility("default")))
#else
#	define PHP_NORMALIZER_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(normalizer);
PHP_MSHUTDOWN_FUNCTION(normalizer);
PHP_RINIT_FUNCTION(normalizer);
PHP_RSHUTDOWN_FUNCTION(normalizer);
PHP_MINFO_FUNCTION(normalizer);

PHP_FUNCTION(normalize_object);
PHP_FUNCTION(denormalize_object);

#endif	/* PHP_NORMALIZER_H */
