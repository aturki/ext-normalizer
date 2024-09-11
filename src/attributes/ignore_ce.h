#ifndef IGNORE_CE_H
#define IGNORE_CE_H

#include "php.h"


#define IGNORE_ATTRIBUTE "normalizer\\ignore"


extern zend_class_entry *ignore_attribute_class_entry;


void php_register_ignore_attribute();

#endif // IGNORE_CE_H
