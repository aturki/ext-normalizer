
#include "php.h"
#include "ext/standard/php_var.h"
#include "zend_attributes.h"
#include "zend.h"
#include "serialized_name_ce.h"

#define METHOD(name) PHP_METHOD(SerializedName, name)

zend_class_entry *serialized_name_attribute_class_entry;

METHOD(__construct)
{
    zend_string *name;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STR(name)
    ZEND_PARSE_PARAMETERS_END();

    zend_update_property_str(serialized_name_attribute_class_entry,
                             Z_OBJ_P(ZEND_THIS),
                             "name",
                             sizeof("name") - 1,
                             name);
}

ZEND_BEGIN_ARG_INFO(arginfo_serialized_name_attribute___construct, 0)
ZEND_END_ARG_INFO()

static void validate_serialized_name(zend_attribute *attr, uint32_t target, zend_class_entry *scope)
{
    if (scope->ce_flags & ZEND_ACC_TRAIT) {
        zend_error_noreturn(E_ERROR, "Cannot apply #[Normalizer\\SerializedName] to trait");
    }
    if (scope->ce_flags & ZEND_ACC_INTERFACE) {
        zend_error_noreturn(E_ERROR, "Cannot apply #[Normalizer\\SerializedName] to interface");
    }
    if (scope->ce_flags & ZEND_ACC_READONLY_CLASS) {
        zend_error_noreturn(E_ERROR,
                            "Cannot apply #[Normalizer\\SerializedName] to readonly class %s",
                            ZSTR_VAL(scope->name));
    }
}

void php_register_serialized_name_attribute()
{
    zend_class_entry ce, *class_entry;

    zend_function_entry methods[] = {PHP_ME(SerializedName,
                                            __construct,
                                            arginfo_serialized_name_attribute___construct,
                                            ZEND_ACC_PUBLIC | ZEND_ACC_CTOR) PHP_FE_END};

    INIT_CLASS_ENTRY(ce, "Normalizer\\SerializedName", methods);
    class_entry = zend_register_internal_class(&ce);

    class_entry->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES;

    zend_string *attribute_name_Attribute_class_SerializedName_0 =
        zend_string_init_interned("Attribute", sizeof("Attribute") - 1, 1);
    zend_attribute *attribute_Attribute_class_SerializedName_0 =
        zend_add_class_attribute(class_entry, attribute_name_Attribute_class_SerializedName_0, 1);
    zend_string_release(attribute_name_Attribute_class_SerializedName_0);
    zval attribute_Attribute_class_SerializedName_0_arg0;
    ZVAL_LONG(&attribute_Attribute_class_SerializedName_0_arg0, ZEND_ATTRIBUTE_TARGET_PROPERTY);
    ZVAL_COPY_VALUE(&attribute_Attribute_class_SerializedName_0->args[0].value,
                    &attribute_Attribute_class_SerializedName_0_arg0);
    zend_internal_attribute *attr = zend_mark_internal_attribute(class_entry);
    attr->validator = validate_serialized_name;
}
