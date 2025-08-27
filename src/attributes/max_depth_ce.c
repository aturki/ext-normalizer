
#include "php.h"
#include "ext/standard/php_var.h"
#include "zend_attributes.h"
#include "zend.h"
#include "max_depth_ce.h"

#define METHOD(name) PHP_METHOD(MaxDepth, name)

zend_class_entry *max_depth_attribute_class_entry;

METHOD(__construct) {
    zend_long depth;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(depth)
    ZEND_PARSE_PARAMETERS_END();

    if (depth <= 0) {
        zend_value_error("MaxDepth value must be greater than 0");
        return;
    }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_max_depth_attribute_construct, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, depth, IS_LONG, 0)
ZEND_END_ARG_INFO()

static void validate_max_depth(zend_attribute *attr, uint32_t target, zend_class_entry *scope)
{
    if (scope->ce_flags & ZEND_ACC_TRAIT) {
        zend_error_noreturn(E_ERROR, "Cannot apply #[Normalizer\\MaxDepth] to trait");
    }
    if (scope->ce_flags & ZEND_ACC_INTERFACE) {
        zend_error_noreturn(E_ERROR, "Cannot apply #[Normalizer\\MaxDepth] to interface");
    }
    if (scope->ce_flags & ZEND_ACC_READONLY_CLASS) {
        zend_error_noreturn(E_ERROR,
                            "Cannot apply #[Normalizer\\MaxDepth] to readonly class %s",
                            ZSTR_VAL(scope->name));
    }

    // Validate that the attribute has the required depth argument
    if (attr->argc != 1 || Z_TYPE(attr->args[0].value) != IS_LONG) {
        zend_error_noreturn(E_ERROR, "MaxDepth attribute requires a single integer argument");
    }

    if (Z_LVAL(attr->args[0].value) <= 0) {
        zend_error_noreturn(E_ERROR, "MaxDepth value must be greater than 0");
    }
}

void php_register_max_depth_attribute()
{
    zend_class_entry ce, *class_entry;

    zend_function_entry methods[] = {
        PHP_ME(MaxDepth, __construct, arginfo_max_depth_attribute_construct, ZEND_ACC_PUBLIC) PHP_FE_END};

    INIT_CLASS_ENTRY(ce, "Normalizer\\MaxDepth", methods);
    class_entry = zend_register_internal_class(&ce);

    zend_string *attribute_name_Attribute_class_MaxDepth_0 =
        zend_string_init_interned("Attribute", sizeof("Attribute") - 1, 1);
    zend_attribute *attribute_Attribute_class_MaxDepth_0 =
        zend_add_class_attribute(class_entry, attribute_name_Attribute_class_MaxDepth_0, 1);
    zend_string_release(attribute_name_Attribute_class_MaxDepth_0);
    zval attribute_Attribute_class_MaxDepth_0_arg0;
    ZVAL_LONG(&attribute_Attribute_class_MaxDepth_0_arg0, ZEND_ATTRIBUTE_TARGET_PROPERTY);
    ZVAL_COPY_VALUE(&attribute_Attribute_class_MaxDepth_0->args[0].value, &attribute_Attribute_class_MaxDepth_0_arg0);
    zend_internal_attribute *attr = zend_mark_internal_attribute(class_entry);
    attr->validator = validate_max_depth;
}
