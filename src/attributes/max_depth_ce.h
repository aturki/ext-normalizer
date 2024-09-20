#ifndef MAX_DEPTH_CE_H
#define MAX_DEPTH_CE_H

#include "php.h"

#define MAX_DEPTH_ATTRIBUTE "normalizer\\maxdepth"

extern zend_class_entry *max_depth_attribute_class_entry;

void php_register_max_depth_attribute();

#endif  // MAX_DEPTH_CE_H
