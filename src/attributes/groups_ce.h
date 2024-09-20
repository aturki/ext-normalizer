#ifndef GROUPS_CE_H
#define GROUPS_CE_H

#include "php.h"

#define GROUPS_ATTRIBUTE "normalizer\\groups"

extern zend_class_entry *groups_attribute_class_entry;

void php_register_groups_attribute();

#endif  // GROUPS_CE_H
