#include "php.h"
#include "ext/standard/php_var.h"
#include "zend_attributes.h"
#include "zend.h"
#include "../helpers.h"
#include "groups_ce.h"

#define METHOD(name) PHP_METHOD(Groups, name)

zend_class_entry *groups_attribute_class_entry;

METHOD(__construct)
{
    ZEND_PARSE_PARAMETERS_NONE();
}

ZEND_BEGIN_ARG_INFO(arginfo_groups_attribute_construct, 0)
ZEND_END_ARG_INFO()


void php_register_groups_attribute()
{
    zend_class_entry ce, *class_entry;

    zend_function_entry methods[] = {
        PHP_ME(Groups, __construct, arginfo_groups_attribute_construct, ZEND_ACC_PUBLIC)
        PHP_FE_END
    };

    INIT_CLASS_ENTRY(ce, "Normalizer\\Groups", methods);
    class_entry = zend_register_internal_class(&ce);

    // zend_string *attribute_name_Attribute_class_groups =
    //     zend_string_init_interned("Attribute", sizeof("Attribute") - 1, 1);
    // zend_attribute *attribute_Attribute_class_groups =
    //     zend_add_class_attribute(class_entry, attribute_name_Attribute_class_groups, 1);
    // zend_string_release(attribute_name_Attribute_class_groups);
    // zval attribute_Attribute_class_groups_arg0;
    // ZVAL_LONG(&attribute_Attribute_class_groups_arg0, ZEND_ATTRIBUTE_TARGET_METHOD | ZEND_ATTRIBUTE_TARGET_PROPERTY);
    // ZVAL_COPY_VALUE(&attribute_Attribute_class_groups->args[0].value, &attribute_Attribute_class_groups_arg0);

    class_entry->ce_flags |= ZEND_ACC_FINAL|ZEND_ACC_NO_DYNAMIC_PROPERTIES;

    // zend_declare_class_constant_long(php_ds_vector_ce, STR_AND_LEN("MIN_CAPACITY"), DS_VECTOR_MIN_CAPACITY);

    // zend_class_implements(php_ds_vector_ce, 1, sequence_ce);
    // php_register_vector_handlers();
}
