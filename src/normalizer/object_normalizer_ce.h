#ifndef OBJECT_NORMALIZER_H
#define OBJECT_NORMALIZER_H

#include "php.h"

#define GROUPS_CONST_NAME "GROUPS"
#define GROUPS_CONST_VALUE "GROUPS"

#define OBJECT_TO_POPULATE "OBJECT_TO_POPULATE"

#define SKIP_NULL_VALUES_NAME "SKIP_NULL_VALUES"
#define SKIP_NULL_VALUES_VALUE "SKIP_NULL_VALUES"

#define SKIP_UNINITIALIZED_VALUES_NAME "SKIP_UNINITIALIZED_VALUES"
#define SKIP_UNINITIALIZED_VALUES_VALUE "SKIP_UNINITIALIZED_VALUES"

#define CIRCULAR_REFERENCE_LIMIT_NAME "CIRCULAR_REFERENCE_LIMIT"
#define DEFAULT_CIRCULAR_REFERENCE_VALUE "CIRCULAR_REFERENCE_LIMIT"


#define CIRCULAR_REFERENCE_COUNTERS_NAME "CIRCULAR_REFERENCE_COUNTERS"


#define DATE_FORMAT_RFC3339_EXTENDED "Y-m-d\\TH:i:sP"

#define DEFAULT_CIRCULAR_REFERENCE_LIMIT 1

#define DECLARE_CLASS_STRING_CONSTANT(ce, name, value, access_type) \
do { \
    zval const_##name##_value; \
    zend_string *const_##name##_value_str = zend_string_init(#value, strlen(#value), 1); \
    ZVAL_STR(&const_##name##_value, const_##name##_value_str); \
    zend_string *const_##name##_name = zend_string_init(#name, strlen(#name), 1); \
    zend_declare_typed_class_constant(ce, const_##name##_name, &const_##name##_value, access_type, NULL, (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_STRING)); \
    zend_string_release(const_##name##_name); \
} while (0)


extern zend_class_entry *object_normalizer_class_entry;

void normalize_object(zval *obj, zend_array *context, zval *retval);
void denormalize_array(zval *input, zend_array *context, zval *obj, zend_class_entry *ce, bool is_array, bool do_init);

void register_object_normalizer_class();

#endif
