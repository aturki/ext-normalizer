#include "php.h"
#include "ext/standard/php_var.h"
#include "zend_attributes.h"
#include "zend.h"
#include "object_normalizer_ce.h"
#include "../helpers.h"
#include "../attributes/normalizer_attributes.h"

zend_class_entry *object_normalizer_class_entry;

bool must_normalize(HashTable *attributes, zend_array *context);
zend_string *get_setter_method_name(zend_string *property_name);
zend_string *get_property_class_name(zend_string *property_name, zend_class_entry *parent_ce);


void normalize_object(zval *input, zval *context, zval *retval)
{
    HashTable *object_properties;

    // We have a collection of items to normalize
    if (Z_TYPE_P(input) == IS_ARRAY) {
        array_init_size(retval, Z_ARRVAL_P(input)->nNumOfElements);
        zval r, tmp;
        for (int i = 0; i < Z_ARRVAL_P(input)->nNumOfElements; i++) {
            zval t = Z_ARRVAL_P(input)->arPacked[i];
            normalize_object(&t, context, &r);
            add_index_array(retval, i, Z_ARRVAL(r));
        }
    } else {
        object_properties = zend_get_properties_for(input, ZEND_PROP_PURPOSE_DEBUG);
        array_init(retval);
        if (object_properties) {
            zend_ulong num;
            zend_string *key;
            zval *val;
            zval normalized_sub_object, tmp;

            ZEND_HASH_FOREACH_KEY_VAL(object_properties, num, key, val)
            {
                zval rv;
                bool normalize = FALSE;
                zval *property =
                    zend_read_property(Z_OBJCE_P(input), Z_OBJ_P(input), ZSTR_VAL(key), ZSTR_LEN(key), 1, &rv);
                zend_property_info *property_info = NULL;

                if (Z_TYPE_P(val) == IS_INDIRECT) {
                    val = Z_INDIRECT_P(val);
                    property_info = zend_get_typed_property_info_for_slot(Z_OBJ_P(input), val);
                }
                if (!Z_ISUNDEF_P(val) || property_info) {
                    const char *prop_name, *class_name;
                    /*int unmangle = */ zend_unmangle_property_name(key, &class_name, &prop_name);
                    const char *unmangled_name_cstr =
                        zend_get_unmangled_property_name(zend_string_init(prop_name, sizeof(prop_name) - 1, 0));
                    zend_string *unmangled_name =
                        zend_string_init(unmangled_name_cstr, strlen(unmangled_name_cstr), false);

                    if (property_info && property_info->attributes) {
                        normalize = must_normalize(property_info->attributes, Z_ARRVAL_P(context));
                    } else {
                        normalize = FALSE;
                    }

                    if (normalize) {

                    try_again:
                        if (Z_TYPE_P(val) == IS_NULL) {
                            add_assoc_null(retval, prop_name);
                        } else if (Z_TYPE_P(val) == IS_TRUE) {
                            add_assoc_bool(retval, prop_name, TRUE);
                        } else if (Z_TYPE_P(val) == IS_FALSE) {
                            add_assoc_bool(retval, prop_name, FALSE);
                        } else if (Z_TYPE_P(val) == IS_LONG) {
                            add_assoc_long(retval, prop_name, Z_LVAL_P(val));
                        } else if (Z_TYPE_P(val) == IS_DOUBLE) {
                            add_assoc_double(retval, prop_name, Z_DVAL_P(val));
                        } else if (Z_TYPE_P(val) == IS_STRING) {
                            add_assoc_string(retval, prop_name, Z_STRVAL_P(val));
                        } else if (Z_TYPE_P(val) == IS_ARRAY) {
                            if (Z_TYPE(Z_ARRVAL_P(val)->arPacked[0]) == IS_OBJECT) {
                                // Items in the array property are complex objects -> normalize
                                // each one of them
                                zval r, tmp;
                                array_init_size(&tmp, Z_ARRVAL_P(val)->nNumOfElements);
                                for (int i = 0; i < Z_ARRVAL_P(val)->nNumOfElements; i++) {
                                    normalize_object(&Z_ARRVAL_P(val)->arPacked[0], context, &r);
                                    add_index_array(&tmp, i, Z_ARRVAL(r));
                                }
                                add_assoc_array(retval, prop_name, Z_ARRVAL(tmp));
                            } else {
                                // Object holds scalar values, just copy to output
                                add_assoc_array(retval, prop_name, Z_ARRVAL_P(val));
                            }
                        } else if (Z_TYPE_P(val) == IS_OBJECT) {
                            ZVAL_COPY_VALUE(&tmp, val);
                            normalize_object(&tmp, context, &normalized_sub_object);
                            add_assoc_array(retval, prop_name, Z_ARR_P(&normalized_sub_object));
                        } else if (Z_TYPE_P(val) == IS_REFERENCE) {
                            val = Z_REFVAL_P(val);
                            goto try_again;
                        } else {
                            ZEND_UNREACHABLE();
                        }
                    }
                }
            }
            ZEND_HASH_FOREACH_END();
            zend_release_properties(object_properties);
        }
    }
}

void denormalize_array(zval *input, zval *context, zval *retval, zend_class_entry *ce, bool is_array)
{
    zend_string *key;
    zend_ulong num;
    zval *val;

    if (is_array) {
        array_init_size(retval, Z_ARRVAL_P(input)->nNumOfElements);
        zval r, tmp;
        for (int i = 0; i < Z_ARRVAL_P(input)->nNumOfElements; i++) {
            zval t = Z_ARRVAL_P(input)->arPacked[i];
            denormalize_array(&t, context, &r, ce, FALSE);
            add_index_object(retval, i, Z_OBJ(r));
        }
    } else {
        object_init_ex(retval, ce);
        ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(input), key, val)
        {
            zend_function setter;
            zend_string *setter_name = get_setter_method_name(key);
            // php_printf("Setter for %s is %s()\n", ZSTR_VAL(key), setter_name ?
            // ZSTR_VAL(setter_name): "Not found");
        try_again:
            if (Z_TYPE_P(val) == IS_TRUE) {
                zend_update_property_bool(ce, Z_OBJ_P(retval), ZSTR_VAL(key), ZSTR_LEN(key), TRUE);
            } else if (Z_TYPE_P(val) == IS_FALSE) {
                zend_update_property_bool(ce, Z_OBJ_P(retval), ZSTR_VAL(key), ZSTR_LEN(key), FALSE);
            } else if (Z_TYPE_P(val) == IS_LONG) {
                zend_update_property_long(ce, Z_OBJ_P(retval), ZSTR_VAL(key), ZSTR_LEN(key), Z_LVAL_P(val));
            } else if (Z_TYPE_P(val) == IS_DOUBLE) {
                zend_update_property_double(ce, Z_OBJ_P(retval), ZSTR_VAL(key), ZSTR_LEN(key), Z_DVAL_P(val));
            } else if (Z_TYPE_P(val) == IS_STRING) {
                zend_update_property_string(ce, Z_OBJ_P(retval), ZSTR_VAL(key), ZSTR_LEN(key), Z_STRVAL_P(val));
            } else if (Z_TYPE_P(val) == IS_ARRAY) {
                zend_string *property_class_name = get_property_class_name(key, ce);
                // php_printf("type for sub array (%s) is: %s\n", ZSTR_VAL(key),
                //            property_class_name ? ZSTR_VAL(property_class_name) : "NA");

                // Property should be denormalized as object
                if (property_class_name) {
                    zend_class_entry *sub_ce = zend_lookup_class(property_class_name);
                    zval r;
                    denormalize_array(val, context, &r, sub_ce, FALSE);
                    zend_update_property(ce, Z_OBJ_P(retval), ZSTR_VAL(key), ZSTR_LEN(key), &r);
                }
            } else if (Z_TYPE_P(val) == IS_REFERENCE) {
                val = Z_REFVAL_P(val);
                goto try_again;
            } else {
                ZEND_UNREACHABLE();
            }
        }
        ZEND_HASH_FOREACH_END();
    }
}

bool must_normalize(HashTable *attributes, zend_array *context)
{
    bool normalize = FALSE;
    zend_attribute *ignore_attribute =
        zend_get_attribute_str(attributes, IGNORE_ATTRIBUTE, sizeof(IGNORE_ATTRIBUTE) - 1);

    zend_attribute *expose_attribute =
        zend_get_attribute_str(attributes, EXPOSE_ATTRIBUTE, sizeof(EXPOSE_ATTRIBUTE) - 1);

    zend_attribute *groups_attribute =
        zend_get_attribute_str(attributes, GROUPS_ATTRIBUTE, sizeof(GROUPS_ATTRIBUTE) - 1);

    if (ignore_attribute) {
        normalize = FALSE;
    } else if (expose_attribute) {
        normalize = TRUE;
    } else if (groups_attribute) {
        zend_attribute_arg p = groups_attribute->args[0];
        zval *requested_groups = zend_hash_str_find(context, "groups", sizeof("groups") - 1);
        normalize = check_array_intersection_string(&p.value, requested_groups);
    } else {
        normalize = FALSE;
    }

    return normalize;
}

zend_string *get_setter_method_name(zend_string *property_name)
{
    zend_string *setter_name, *capitalized_property_name;

    unsigned char r = zend_toupper_ascii(ZSTR_VAL(property_name)[0]);
    if (r == ZSTR_VAL(property_name)[0]) {
        capitalized_property_name = zend_string_copy(property_name);
    } else {
        capitalized_property_name = zend_string_init(ZSTR_VAL(property_name), ZSTR_LEN(property_name), 0);
        ZSTR_VAL(capitalized_property_name)
        [0] = r;
    }
    setter_name = zend_string_init("set", 3, 0);
    setter_name = zend_string_concat2(ZSTR_VAL(setter_name), ZSTR_LEN(setter_name), ZSTR_VAL(capitalized_property_name),
                                      ZSTR_LEN(capitalized_property_name));

    return setter_name;
}

zend_string *get_property_class_name(zend_string *property_name, zend_class_entry *parent_ce)
{
    zend_property_info *property_info;
    zend_string *class_name = NULL;

    property_info = zend_hash_find_ptr(&parent_ce->properties_info, property_name);

    php_printf("Property Info for (%s) \n\tName: %s\n", ZSTR_VAL(property_name),
               property_info->type.type_mask == IS_ARRAY ? "Array" : "Not Array");
    if (property_info != NULL && ZEND_TYPE_IS_SET(property_info->type) && ZEND_TYPE_HAS_NAME(property_info->type)) {
        // php_printf("here for %s\n", ZSTR_VAL(property_name));
        class_name = ZEND_TYPE_NAME(property_info->type);
    } else {

        // php_printf("No type found for %s\n", ZSTR_VAL(property_info->name));
    }

    return class_name;
}

ZEND_METHOD(ObjectNormalizer, __construct)
{
    ZEND_PARSE_PARAMETERS_NONE();
}

// Function to normalize an object to an array
ZEND_METHOD(ObjectNormalizer, normalize)
{
    zval *obj;
    zval *context;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_ZVAL(obj)
    Z_PARAM_OPTIONAL
    Z_PARAM_ZVAL_OR_NULL(context)
    ZEND_PARSE_PARAMETERS_END();

    normalize_object(obj, context,
                     return_value); // Pass return_value to store result
}

// Function to denormalize an array back to an object
ZEND_METHOD(ObjectNormalizer, denormalize)
{
    char *className;
    zval *arr;
    zval *context;
    size_t className_len;
    zend_string *cname;
    zend_class_entry *ce;
    bool is_array = FALSE;

    ZEND_PARSE_PARAMETERS_START(2, 3)
    Z_PARAM_ZVAL(arr)
    Z_PARAM_STRING(className, className_len)
    Z_PARAM_OPTIONAL
    Z_PARAM_ARRAY_OR_NULL(context)
    ZEND_PARSE_PARAMETERS_END();

    if (className[strlen(className) - 1] == ']' && className[strlen(className) - 2] == '[') {
        strcpy(className + strlen(className) - 2, "\0");
        is_array = TRUE;
    }

    cname = zend_string_init(className, strlen(className), 0);
    ce = zend_lookup_class(cname);

    denormalize_array(arr, context, return_value, ce, is_array);
}

ZEND_BEGIN_ARG_INFO(arginfo_object_normalizer___construct, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_object_normalizer_normalize, 0, 0, 2)
ZEND_ARG_TYPE_INFO(0, obj, IS_MIXED, 0)
ZEND_ARG_TYPE_INFO(0, obj, IS_ARRAY, 1)
ZEND_END_ARG_INFO()

// Arginfo for denormalize_array()
ZEND_BEGIN_ARG_INFO_EX(arginfo_object_normalizer_denormalize, 0, 0, 2)
ZEND_ARG_TYPE_INFO(0, arr, IS_MIXED, 0)
ZEND_ARG_TYPE_INFO(0, className, IS_STRING, 0)
ZEND_END_ARG_INFO()

void register_object_normalizer_class()
{
    zend_class_entry object_normalizer_ce;

    static const zend_function_entry object_normalizer_methods[] = {
        PHP_ME(ObjectNormalizer, __construct, arginfo_object_normalizer___construct, ZEND_ACC_PUBLIC)
            PHP_ME(ObjectNormalizer, normalize, arginfo_object_normalizer_normalize, ZEND_ACC_PUBLIC)
                PHP_ME(ObjectNormalizer, denormalize, arginfo_object_normalizer_denormalize, ZEND_ACC_PUBLIC)
                    PHP_FE_END};

    INIT_CLASS_ENTRY(object_normalizer_ce, "Normalizer\\ObjectNormalizer", object_normalizer_methods);
    object_normalizer_class_entry = zend_register_internal_class(&object_normalizer_ce);

    zval const_GROUPS_value;
    zend_string *const_GROUPS_value_str = zend_string_init("groups", strlen("groups"), 1);
    ZVAL_STR(&const_GROUPS_value, const_GROUPS_value_str);
    zend_string *const_GROUPS_name = zend_string_init("GROUPS", strlen("GROUPS"), 1);

    zend_declare_typed_class_constant(object_normalizer_class_entry, const_GROUPS_name, &const_GROUPS_value,
                                      ZEND_ACC_PUBLIC, NULL, (zend_type)ZEND_TYPE_INIT_MASK(MAY_BE_STRING));

    zend_string_release(const_GROUPS_name);
}
