#ifndef EXPOSE_CE_H
#define EXPOSE_CE_H

#include "php.h"


#define EXPOSE_ATTRIBUTE "normalizer\\expose"


extern zend_class_entry *expose_attribute_class_entry;


void php_register_expose_attribute();

#endif // EXPOSE_CE_H
