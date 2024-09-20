#ifndef SERIALIZED_NAME_CE_H
#define SERIALIZED_NAME_CE_H

#include "php.h"

#define SERIALIZED_NAME_ATTRIBUTE "normalizer\\serializedname"

extern zend_class_entry *serialized_name_attribute_class_entry;

void php_register_serialized_name_attribute();

#endif  // SERIALIZED_NAME_CE_H
