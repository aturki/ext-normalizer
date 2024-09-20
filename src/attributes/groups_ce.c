#include "php.h"
#include "ext/standard/php_var.h"
#include "zend_attributes.h"
#include "zend.h"
#include "../helpers.h"
#include "groups_ce.h"

#define METHOD(name) PHP_METHOD(Groups, name)

zend_class_entry *groups_attribute_class_entry;

METHOD(__construct) { ZEND_PARSE_PARAMETERS_NONE(); }

ZEND_BEGIN_ARG_INFO(arginfo_groups_attribute_construct, 0)
ZEND_END_ARG_INFO()

static void validate_groups(zend_attribute *attr, uint32_t target, zend_class_entry *scope)
{
    if (scope->ce_flags & ZEND_ACC_TRAIT) {
        zend_error_noreturn(E_ERROR, "Cannot apply #[Normalizer\\Groups] to trait");
    }
    if (scope->ce_flags & ZEND_ACC_INTERFACE) {
        zend_error_noreturn(E_ERROR, "Cannot apply #[Normalizer\\Groups] to interface");
    }
    if (scope->ce_flags & ZEND_ACC_READONLY_CLASS) {
        zend_error_noreturn(E_ERROR, "Cannot apply #[Normalizer\\Groups] to readonly class %s", ZSTR_VAL(scope->name));
    }
    if (scope->ce_flags & ZEND_ACC_CTOR) {
        zend_error_noreturn(E_ERROR, "Cannot apply #[Normalizer\\Groups] to readonly class %s", ZSTR_VAL(scope->name));
    }
}

void php_register_groups_attribute()
{
    zend_class_entry ce, *class_entry;

    zend_function_entry methods[] = {PHP_ME(Groups, __construct, arginfo_groups_attribute_construct, ZEND_ACC_PUBLIC)
                                         PHP_FE_END};

    INIT_CLASS_ENTRY(ce, "Normalizer\\Groups", methods);
    class_entry = zend_register_internal_class(&ce);
    class_entry->ce_flags |= ZEND_ACC_FINAL;

    zend_string *attribute_name_Attribute_class_Groups_0 =
        zend_string_init_interned("Attribute", sizeof("Attribute") - 1, 1);
    zend_attribute *attribute_Attribute_class_Groups_0 =
        zend_add_class_attribute(class_entry, attribute_name_Attribute_class_Groups_0, 1);
    zend_string_release(attribute_name_Attribute_class_Groups_0);
    zval attribute_Attribute_class_Groups_0_arg0;
    ZVAL_LONG(&attribute_Attribute_class_Groups_0_arg0, ZEND_ATTRIBUTE_TARGET_METHOD | ZEND_ATTRIBUTE_TARGET_PROPERTY);
    ZVAL_COPY_VALUE(&attribute_Attribute_class_Groups_0->args[0].value, &attribute_Attribute_class_Groups_0_arg0);
    zend_internal_attribute *attr = zend_mark_internal_attribute(class_entry);
    attr->validator = validate_groups;
}
