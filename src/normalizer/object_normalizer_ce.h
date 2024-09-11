#ifndef OBJECT_NORMALIZER_H
#define OBJECT_NORMALIZER_H

#include "php.h"

#define GROUPS_CONST_NAME "GROUPS"
#define GROUPS_CONST_VALUE "groups"


extern zend_class_entry *object_normalizer_class_entry;

void normalize_object(zval *obj, zval *context, zval *retval);
void denormalize_array(zval *input, zval *context, zval *obj, zend_class_entry *ce, bool is_array);

void register_object_normalizer_class();

#endif
