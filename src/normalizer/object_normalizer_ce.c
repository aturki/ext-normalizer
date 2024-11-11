#include "php.h"
#include "ext/standard/php_var.h"
#include "ext/date/php_date.h"
#include "ext/spl/php_spl.h"
#include "zend_attributes.h"
#include "zend_exceptions.h"
#include "zend.h"
#include "zend_enum.h"
#include "object_normalizer_arginfo.h"
#include "object_normalizer_ce.h"
#include "../helpers.h"
#include "../attributes/normalizer_attributes.h"

#define CLEAR_ZVAL(z)     \
    if (z) {              \
        zval_ptr_dtor(z); \
        z = NULL;         \
    }

zend_class_entry *object_normalizer_class_entry;

/**********************/
/* internal utilities */
/**********************/

bool must_normalize_property(HashTable *attributes, zend_array *context, zval *value);
bool must_normalize_function(HashTable *attributes, zend_array *context);
zend_string *get_setter_method_name(zend_string *property_name, zend_class_entry *parent_ce);
zend_string *get_getter_method_name(zend_string *property_name, zend_class_entry *parent_ce);
zend_string *get_property_class_name(zend_string *property_name, zend_class_entry *parent_ce);
zend_string *get_normalized_name(zend_string *property_name, HashTable *attributes, bool is_function);
zend_string *get_property_name_from_normalized_name(zend_string *normalized_name, zend_class_entry *ce);
bool is_circular_reference(zval *object, zend_array *context);
void handle_circular_reference(zval *object, zend_array *context, zval *retval);
zend_array *get_object_properties(zval *object);
zend_string *get_unmangled_property_name(zend_string *name);

/*****************************/
/* normalize value functions */
/*****************************/

/*******************************/
/* denormalize value functions */
/*******************************/
void denormalize_string_value(zend_string *property_name,
                              zend_string *property_class_name,
                              zend_class_entry *ce,
                              zval *val,
                              zval *retval);
void denormalize_long_value(zend_string *property_name,
                            zend_string *property_class_name,
                            zend_class_entry *ce,
                            zval *val,
                            zval *retval);
void denormalize_array_value(zend_string *property_name,
                             zend_string *property_class_name,
                             zend_class_entry *ce,
                             zval *val,
                             zval *retval,
                             zend_array *context);

void normalize_object(zval *input, zend_array *context, zval *retval)
{
    HashTable *object_properties = NULL;
    // We have a collection of items to normalize
    if (Z_TYPE_P(input) == IS_ARRAY) {
        array_init_size(retval, Z_ARRVAL_P(input)->nNumOfElements);
        zval r, tmp;
        for (int i = 0; i < Z_ARRVAL_P(input)->nNumOfElements; i++) {
            zval t = Z_ARRVAL_P(input)->arPacked[i];
            normalize_object(&t, context, &r);
            add_index_array(retval, i, Z_ARRVAL(r));
        }
    } else if (Z_TYPE_P(input) == IS_NULL) {
        ZVAL_NULL(retval);
    } else {
        object_properties = Z_OBJ_HANDLER_P(input, get_properties)(Z_OBJ_P(input));

        if (object_properties) {
            zend_ulong num;
            zend_string *key = NULL;
            zval *val = NULL;

            array_init(retval);

            ZEND_HASH_FOREACH_KEY_VAL(object_properties, num, key, val)
            {
                zval rv;
                bool normalize = FALSE;
                zend_property_info *property_info = NULL;

                if (Z_TYPE_P(val) == IS_INDIRECT) {
                    val = Z_INDIRECT_P(val);
                    property_info = zend_get_property_info_for_slot(Z_OBJ_P(input), val);
                }
                if (!Z_ISUNDEF_P(val) || property_info) {
                    zend_string *unmangled_name = get_unmangled_property_name(key);

                    if (property_info && property_info->attributes) {
                        normalize = must_normalize_property(property_info->attributes, context, val);
                    } else {
                        normalize = FALSE;
                    }

                    if (normalize) {
                        zend_string *normalized_name =
                            get_normalized_name(unmangled_name, property_info->attributes, FALSE);
                    try_again:
                        if (Z_TYPE_P(val) == IS_NULL || Z_TYPE_P(val) == IS_UNDEF) {
                            add_assoc_null(retval, ZSTR_VAL(normalized_name));
                        } else if (Z_TYPE_P(val) == IS_TRUE) {
                            add_assoc_bool(retval, ZSTR_VAL(normalized_name), TRUE);
                        } else if (Z_TYPE_P(val) == IS_FALSE) {
                            add_assoc_bool(retval, ZSTR_VAL(normalized_name), FALSE);
                        } else if (Z_TYPE_P(val) == IS_LONG) {
                            add_assoc_long(retval, ZSTR_VAL(normalized_name), Z_LVAL_P(val));
                        } else if (Z_TYPE_P(val) == IS_DOUBLE) {
                            add_assoc_double(retval, ZSTR_VAL(normalized_name), Z_DVAL_P(val));
                        } else if (Z_TYPE_P(val) == IS_STRING) {
                            add_assoc_string(retval, ZSTR_VAL(normalized_name), Z_STRVAL_P(val));
                        } else if (Z_TYPE_P(val) == IS_ARRAY) {
                            if ((Z_ARRVAL_P(val)->nNumOfElements > 0) && (Z_TYPE(Z_ARRVAL_P(val)->arPacked[0]) == IS_OBJECT)) {
                                // Items in the array property are complex objects -> normalize each one of them
                                zval tmp;
                                array_init_size(&tmp, Z_ARRVAL_P(val)->nNumOfElements);
                                for (int i = 0; i < Z_ARRVAL_P(val)->nNumOfElements; i++) {
                                    zval r;
                                    normalize_object(&Z_ARRVAL_P(val)->arPacked[i], context, &r);
                                    Z_TRY_ADDREF(r);
                                    add_index_array(&tmp, i, Z_ARRVAL(r));
                                    zval_ptr_dtor(&r);
                                }
                                Z_TRY_ADDREF(tmp);
                                add_assoc_array(retval, ZSTR_VAL(normalized_name), Z_ARRVAL(tmp));
                                zval_ptr_dtor(&tmp);
                            } else {
                                // Object holds scalar values, just copy to output
                                Z_TRY_ADDREF_P(val);
                                add_assoc_array(retval, ZSTR_VAL(normalized_name), Z_ARRVAL_P(val));
                            }
                        } else if (Z_TYPE_P(val) == IS_OBJECT) {
                            if (zend_class_implements_interface(Z_OBJCE_P(val), php_date_get_interface_ce())) {
                                zend_string *str_date = php_format_date(DATE_FORMAT_RFC3339_EXTENDED,
                                                                        sizeof(DATE_FORMAT_RFC3339_EXTENDED) - 1,
                                                                        Z_PHPDATE_P(val)->time->sse,
                                                                        Z_PHPDATE_P(val)->time->is_localtime);
                                add_assoc_string(retval, ZSTR_VAL(normalized_name), ZSTR_VAL(str_date));

                                zend_string_release(str_date);
                            } else if (zend_class_implements_interface(Z_OBJCE_P(val), zend_ce_backed_enum)) {
                                zval *case_value = zend_enum_fetch_case_value(Z_OBJ_P(val));
                                if (Z_OBJCE_P(val)->enum_backing_type == IS_LONG) {
                                    add_assoc_long(retval, ZSTR_VAL(normalized_name), Z_LVAL_P(case_value));
                                } else {
                                    ZEND_ASSERT(Z_OBJCE_P(val)->enum_backing_type == IS_STRING);
                                    add_assoc_string(retval, ZSTR_VAL(normalized_name), Z_STRVAL_P(case_value));
                                }
                            } else {
                                zend_string *getter_name = get_getter_method_name(key, Z_OBJCE_P(input));
                                if (getter_name) {
                                    zval getter_value, normalized_sub_object;
                                    zend_string *getter_name_lower_case = zend_string_tolower(getter_name);

                                    zend_function *func =
                                        zend_hash_find_ptr(&Z_OBJCE_P(input)->function_table, getter_name_lower_case);
                                    zend_call_known_instance_method_with_0_params(func, Z_OBJ_P(input), &getter_value);
                                    normalize_object(&getter_value, context, &normalized_sub_object);
                                    Z_TRY_ADDREF(normalized_sub_object);
                                    add_assoc_array(retval,
                                                    ZSTR_VAL(normalized_name),
                                                    Z_ARRVAL_P(&normalized_sub_object));
                                    zval_ptr_dtor(&getter_value);
                                    zval_ptr_dtor(&normalized_sub_object);
                                    zend_string_release(getter_name);
                                    zend_string_release(getter_name_lower_case);
                                } else {
                                    zval normalized_sub_object;
                                    normalize_object(val, context, &normalized_sub_object);
                                    Z_TRY_ADDREF(normalized_sub_object);
                                    add_assoc_array(retval, ZSTR_VAL(normalized_name), Z_ARR(normalized_sub_object));
                                    zval_ptr_dtor(&normalized_sub_object);
                                }
                            }

                        } else if (Z_TYPE_P(val) == IS_REFERENCE) {
                            val = Z_REFVAL_P(val);
                            goto try_again;
                        } else {
                            ZEND_UNREACHABLE();
                        }
                        zend_string_release(normalized_name);
                    }

                    zend_string_release(unmangled_name);
                }
            }
            ZEND_HASH_FOREACH_END();

            zend_function *func;
            zend_string *normalized_name;
            ZEND_HASH_MAP_FOREACH_PTR(&Z_OBJCE_P(input)->function_table, func)
            {
                if (zend_string_starts_with_cstr(func->common.function_name, "get", 3) ||
                    zend_string_starts_with_cstr(func->common.function_name, "has", 3) ||
                    zend_string_starts_with_cstr(func->common.function_name, "is", 2)) {
                    normalized_name = get_normalized_name(func->common.function_name, func->common.attributes, TRUE);

                    if ((func->common.fn_flags & ZEND_ACC_PUBLIC) && !(func->common.fn_flags & ZEND_ACC_CTOR) &&
                        must_normalize_function(func->common.attributes, context)) {
                        zval rv;
                        zend_call_method_if_exists(Z_OBJ_P(input), func->common.function_name, &rv, 0, NULL);
                        add_assoc_zval(retval, ZSTR_VAL(normalized_name), &rv);
                    }
                    zend_string_release(normalized_name);
                }
            }
            ZEND_HASH_FOREACH_END();
        }
        zend_array_release(object_properties);
    }
}

void denormalize_array(zval *input,
                       zend_array *context,
                       zval *retval,
                       zend_class_entry *ce,
                       bool is_array,
                       bool do_init)
{
    if (is_array) {
        array_init_size(retval, Z_ARRVAL_P(input)->nNumOfElements);
        zval r, tmp;
        for (int i = 0; i < Z_ARRVAL_P(input)->nNumOfElements; i++) {
            zval t = Z_ARRVAL_P(input)->arPacked[i];
            denormalize_array(&t, context, &r, ce, FALSE, TRUE);
            add_index_object(retval, i, Z_OBJ(r));
        }
    } else {
        if (Z_TYPE_P(input) == IS_NULL) {
            ZVAL_NULL(retval);
        } else {
            if (do_init) {
                object_init_ex(retval, ce);
                if (ce->constructor) {
                    zend_call_known_instance_method_with_0_params(ce->constructor, Z_OBJ_P(retval), NULL);
                }
            }
            zend_string *key;
            zend_ulong num;
            zval *val;
            ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(input), key, val)
            {
                zend_string *property_name = get_property_name_from_normalized_name(key, ce);
                zend_string *setter_name = get_setter_method_name(property_name, ce);
                zend_property_info *property_info = zend_hash_find_ptr(&ce->properties_info, property_name);
                zend_function *setter_func = zend_hash_find_ptr(&ce->function_table, setter_name);
                bool setter_exists_and_accessible =
                    (setter_func != NULL) && (setter_func->common.fn_flags & ZEND_ACC_PUBLIC);
                zend_string *property_class_name = get_property_class_name(property_name, ce);
                zend_class_entry *property_ce = ce;

                // Property is declared (not dynamic)
                if (property_info != NULL) {
                    if (property_info->flags & ZEND_ACC_PUBLIC) {
                    } else if (property_info->flags & ZEND_ACC_PROTECTED) {
                    } else if (property_info->flags & ZEND_ACC_PRIVATE) {
                        property_ce = property_info->ce;
                    }
                try_again:
                    if (Z_TYPE_P(val) == IS_TRUE) {
                        zend_update_property_bool(property_ce,
                                                  Z_OBJ_P(retval),
                                                  ZSTR_VAL(property_name),
                                                  ZSTR_LEN(property_name),
                                                  TRUE);
                    } else if (Z_TYPE_P(val) == IS_FALSE) {
                        zend_update_property_bool(property_ce,
                                                  Z_OBJ_P(retval),
                                                  ZSTR_VAL(property_name),
                                                  ZSTR_LEN(property_name),
                                                  FALSE);
                    } else if (Z_TYPE_P(val) == IS_LONG) {
                        denormalize_long_value(property_name, property_class_name, property_ce, val, retval);
                    } else if (Z_TYPE_P(val) == IS_DOUBLE) {
                //         zend_update_property_double(property_ce,
                //                                     Z_OBJ_P(retval),
                //                                     ZSTR_VAL(property_name),
                //                                     ZSTR_LEN(property_name),
                //                                     Z_DVAL_P(val));
                    } else if (Z_TYPE_P(val) == IS_STRING) {
                        denormalize_string_value(property_name, property_class_name, property_ce, val, retval);
                    } else if (Z_TYPE_P(val) == IS_ARRAY) {
                        denormalize_array_value(property_name, property_class_name, property_ce, val, retval, context);
                    } else if (Z_TYPE_P(val) == IS_REFERENCE) {
                        val = Z_REFVAL_P(val);
                        goto try_again;
                    } else {
                        ZEND_UNREACHABLE();
                    }
                }
                zend_string_release(setter_name);
                zend_string_release(property_name);
                if (property_class_name != NULL) {
                    zend_string_release(property_class_name);
                }
            }
            ZEND_HASH_FOREACH_END();
        }
    }
}

void denormalize_array_value(zend_string *property_name,
                             zend_string *property_class_name,
                             zend_class_entry *ce,
                             zval *val,
                             zval *retval,
                             zend_array *context)
{
    // Property should be denormalized as object
    if (property_class_name) {
        zend_class_entry *sub_ce = zend_lookup_class(property_class_name);
        // Property should be denormalized as object
        zval tmp;
        ZVAL_UNDEF(&tmp);
        denormalize_array(val, context, &tmp, sub_ce, FALSE, TRUE);
        zend_update_property(ce, Z_OBJ_P(retval), ZSTR_VAL(property_name), ZSTR_LEN(property_name), &tmp);
    } else {
        zend_update_property(ce, Z_OBJ_P(retval), ZSTR_VAL(property_name), ZSTR_LEN(property_name), val);
    }
}

void denormalize_string_value(zend_string *property_name,
                              zend_string *property_class_name,
                              zend_class_entry *ce,
                              zval *val,
                              zval *retval)
{
    // Property is Enum or Date
    if (property_class_name) {
        zend_class_entry *sub_ce = zend_lookup_class(property_class_name);
        if (sub_ce) {
            if (zend_class_implements_interface(sub_ce, zend_ce_backed_enum)) {
                //* Dealing with an Enum
                zval enum_value, case_value;
                ZVAL_STR(&enum_value, Z_STR_P(val));

                zend_function *func = zend_hash_str_find_ptr(&sub_ce->function_table, "tryfrom", strlen("tryFrom"));
                if (func != NULL) {
                    zend_call_known_function(func, NULL, sub_ce, &case_value, 1, &enum_value, NULL);

                    /* Check if return_value is a valid object and is an instance of the enum class */
                    if (Z_TYPE(case_value) == IS_OBJECT && instanceof_function(Z_OBJCE(case_value), sub_ce)) {
                        zend_update_property(ce,
                                             Z_OBJ_P(retval),
                                             ZSTR_VAL(property_name),
                                             ZSTR_LEN(property_name),
                                             &case_value);
                    } else {
                        zend_value_error("\"%s\" is not a valid backing value for enum \"%s\"",
                                         Z_STRVAL_P(val),
                                         ZSTR_VAL(ce->name));
                    }
                    zval_ptr_dtor(&enum_value);
                    zval_ptr_dtor(&case_value);
                }
            } else if (zend_class_implements_interface(sub_ce, php_date_get_interface_ce())) {
                //* Dealing with dates represented as strings
                zval date_value;
                php_date_instantiate(sub_ce, &date_value);
                bool success = php_date_initialize(Z_PHPDATE_P(&date_value),
                                                   Z_STRVAL_P(val),
                                                   Z_STRLEN_P(val) - 1,
                                                   DATE_FORMAT_RFC3339_EXTENDED,
                                                   NULL,
                                                   PHP_DATE_INIT_FORMAT);
                zend_update_property(ce,
                                     Z_OBJ_P(retval),
                                     ZSTR_VAL(property_name),
                                     ZSTR_LEN(property_name),
                                     &date_value);
                zval_ptr_dtor(&date_value);
            }
        }
    } else {
        // Property is literal string
        zend_update_property_string(ce,
                                    Z_OBJ_P(retval),
                                    ZSTR_VAL(property_name),
                                    ZSTR_LEN(property_name),
                                    Z_STRVAL_P(val));
    }
}

void denormalize_long_value(zend_string *property_name,
                            zend_string *property_class_name,
                            zend_class_entry *ce,
                            zval *val,
                            zval *retval)
{
    if (property_class_name) {
        zend_class_entry *sub_ce = zend_lookup_class(property_class_name);
        if (zend_class_implements_interface(sub_ce, zend_ce_backed_enum)) {
            //* Dealing with an Enum
            zval enum_value, case_value;
            ZVAL_LONG(&enum_value, Z_LVAL_P(val));

            zend_function *func = zend_hash_str_find_ptr(&sub_ce->function_table, "tryfrom", strlen("tryfrom"));
            if (func != NULL) {
                zend_call_known_function(func, NULL, sub_ce, &case_value, 1, &enum_value, NULL);

                /* Check if return_value is a valid object and is an instance of the enum class */
                if (Z_TYPE(case_value) == IS_OBJECT && instanceof_function(Z_OBJCE(case_value), sub_ce)) {
                    zend_update_property(ce,
                                         Z_OBJ_P(retval),
                                         ZSTR_VAL(property_name),
                                         ZSTR_LEN(property_name),
                                         &case_value);
                } else {
                    zend_value_error("\"%lld\" is not a valid backing value for enum \"%s\"",
                                     Z_LVAL_P(val),
                                     ZSTR_VAL(ce->name));
                }
                zval_ptr_dtor(&enum_value);
                zval_ptr_dtor(&case_value);
            }
        }
    } else {
        zend_update_property_long(ce, Z_OBJ_P(retval), ZSTR_VAL(property_name), ZSTR_LEN(property_name), Z_LVAL_P(val));
    }
}

bool must_normalize_property(HashTable *attributes, zend_array *context, zval *value)
{
    bool normalize = TRUE;
    bool use_symfony_attributes = FALSE;
    zval *skip_null_values = NULL;
    zval *skip_uninitialized_values = NULL;
    zend_attribute *ignore_attribute = NULL;
    zend_attribute *expose_attribute = NULL;
    zend_attribute *groups_attribute = NULL;

    zval *options = zend_hash_str_find(context, "options", sizeof("options") - 1);
    if (options) {
        zval *zv_usa =
            zend_hash_str_find(Z_ARRVAL_P(options), "use_symfony_attributes", sizeof("use_symfony_attributes") - 1);
        if (zv_usa && Z_TYPE_P(zv_usa) == IS_TRUE) {
            use_symfony_attributes = TRUE;
            CLEAR_ZVAL(zv_usa);
        }
    }

    if (use_symfony_attributes) {
        ignore_attribute = zend_get_attribute_str(attributes,
                                                  "symfony\\component\\serializer\\attribute\\ignore",
                                                  sizeof("symfony\\component\\serializer\\attribute\\ignore") - 1);
        if (ignore_attribute == NULL) {
            ignore_attribute = zend_get_attribute_str(attributes, "ignore", sizeof("ignore") - 1);
        }
        groups_attribute = zend_get_attribute_str(attributes,
                                                  "symfony\\component\\serializer\\attribute\\groups",
                                                  sizeof("symfony\\component\\serializer\\attribute\\groups") - 1);
        if (groups_attribute == NULL) {
            groups_attribute = zend_get_attribute_str(attributes,
                                                      "symfony\\component\\serializer\\annotation\\groups",
                                                      sizeof("symfony\\component\\serializer\\annotation\\groups") - 1);
        }
    } else {
        ignore_attribute = zend_get_attribute_str(attributes, IGNORE_ATTRIBUTE, sizeof(IGNORE_ATTRIBUTE) - 1);
        groups_attribute = zend_get_attribute_str(attributes, GROUPS_ATTRIBUTE, sizeof(GROUPS_ATTRIBUTE) - 1);
    }

    expose_attribute = zend_get_attribute_str(attributes, EXPOSE_ATTRIBUTE, sizeof(EXPOSE_ATTRIBUTE) - 1);
    skip_null_values = zend_hash_str_find(context, SKIP_NULL_VALUES_VALUE, sizeof(SKIP_NULL_VALUES_VALUE) - 1);
    skip_uninitialized_values =
        zend_hash_str_find(context, SKIP_UNINITIALIZED_VALUES_VALUE, sizeof(SKIP_UNINITIALIZED_VALUES_VALUE) - 1);

    if (skip_null_values != NULL && Z_TYPE_INFO_P(skip_null_values) == IS_TRUE && Z_TYPE_P(value) == IS_NULL) {
        normalize = FALSE;
    } else if (skip_uninitialized_values != NULL && Z_TYPE_INFO_P(skip_uninitialized_values) == IS_TRUE &&
               Z_TYPE_P(value) == IS_UNDEF) {
        normalize = FALSE;
    } else if (ignore_attribute) {
        normalize = FALSE;
    } else if (expose_attribute) {
        normalize = TRUE;
    } else if (groups_attribute) {
        zend_attribute_arg p = groups_attribute->args[0];
        zval *requested_groups = zend_hash_str_find(context, GROUPS_CONST_VALUE, sizeof(GROUPS_CONST_VALUE) - 1);
        if (requested_groups != NULL && Z_TYPE_P(requested_groups) == IS_ARRAY) {
            normalize = check_array_intersection_string(&p.value, requested_groups);
        }
    } else {
        normalize = FALSE;
    }

    CLEAR_ZVAL(skip_null_values);
    CLEAR_ZVAL(skip_uninitialized_values);
    return normalize;
}

bool must_normalize_function(HashTable *attributes, zend_array *context)
{
    bool normalize = FALSE;
    zend_attribute *expose_attribute = NULL;
    zend_attribute *groups_attribute = NULL;

    expose_attribute = zend_get_attribute_str(attributes, EXPOSE_ATTRIBUTE, sizeof(EXPOSE_ATTRIBUTE) - 1);

    groups_attribute = zend_get_attribute_str(attributes, GROUPS_ATTRIBUTE, sizeof(GROUPS_ATTRIBUTE) - 1);

    if (expose_attribute) {
        normalize = TRUE;
    } else if (groups_attribute) {
        zend_attribute_arg p = groups_attribute->args[0];
        zval *requested_groups = zend_hash_str_find(context, GROUPS_CONST_VALUE, sizeof(GROUPS_CONST_VALUE) - 1);
        normalize = check_array_intersection_string(&p.value, requested_groups);
        zval_ptr_dtor(requested_groups);
    } else {
        normalize = FALSE;
    }

    return normalize;
}

/**
 * Given a property name, return the normalized name for normalization defined by SerializedName attribute if defined
 */
zend_string *get_normalized_name(zend_string *property_name, HashTable *attributes, bool is_function)
{
    zend_string *normalized_name = NULL;

    zend_attribute *normalized_name_attribute =
        zend_get_attribute_str(attributes, SERIALIZED_NAME_ATTRIBUTE, sizeof(SERIALIZED_NAME_ATTRIBUTE) - 1);

    if (normalized_name_attribute) {
        normalized_name = zend_string_init(ZSTR_VAL(Z_STR(normalized_name_attribute->args[0].value)),
                                           ZSTR_LEN(Z_STR(normalized_name_attribute->args[0].value)),
                                           0);
    } else {
        if (is_function) {
            //* Dealing with a virtual property
            int prefix_length = 0;
            if (zend_string_starts_with_cstr(property_name, "get", 3) ||
                zend_string_starts_with_cstr(property_name, "has", 3)) {
                prefix_length = 3;
            } else if (zend_string_starts_with_cstr(property_name, "is", 2)) {
                prefix_length = 2;
            }

            zval rv;
            ZVAL_STRINGL_FAST(&rv, ZSTR_VAL(property_name) + prefix_length, ZSTR_LEN(property_name) - prefix_length);
            normalized_name = Z_STR(rv);
            char *normalized_name_cstr = ZSTR_VAL(normalized_name);
            zend_tolower_ascii(normalized_name_cstr[0]);
            normalized_name = zend_string_init_fast(normalized_name_cstr, sizeof(normalized_name_cstr));
            zval_ptr_dtor(&rv);
        }
    }

    return normalized_name == NULL ? zend_string_copy(property_name) : normalized_name;
}

/**
 * Reverse operation of get_normalized_name
 */
zend_string *get_property_name_from_normalized_name(zend_string *normalized_name, zend_class_entry *ce)
{
    zend_string *property_name = zend_string_copy(normalized_name);

    for (int i = 0; i < ce->properties_info.nNumOfElements; i++) {
        Bucket b = ce->properties_info.arData[i];
        zend_property_info *property_info = zend_hash_find_ptr(&ce->properties_info, b.key);
        if (property_info) {
            zend_attribute *normalized_name_attribute = zend_get_attribute_str(property_info->attributes,
                                                                               SERIALIZED_NAME_ATTRIBUTE,
                                                                               sizeof(SERIALIZED_NAME_ATTRIBUTE) - 1);
            if (normalized_name_attribute) {
                if (zend_string_equals(normalized_name, Z_STR(normalized_name_attribute->args[0].value))) {
                    property_name = zend_string_copy(b.key);
                    break;
                }
            }
        }
    }

    return property_name;
}

zend_string *get_setter_method_name(zend_string *property_name, zend_class_entry *parent_ce)
{
    zend_string *setter_name, *capitalized_property_name, *prefix;

    unsigned char r = zend_toupper_ascii(ZSTR_VAL(property_name)[0]);
    if (r == ZSTR_VAL(property_name)[0]) {
        capitalized_property_name = zend_string_copy(property_name);
    } else {
        capitalized_property_name = zend_string_init_fast(ZSTR_VAL(property_name), ZSTR_LEN(property_name));
        ZSTR_VAL(capitalized_property_name)
        [0] = r;
    }
    prefix = zend_string_init_fast("set", 3);
    setter_name = zend_string_concat2(ZSTR_VAL(prefix),
                                      ZSTR_LEN(prefix),
                                      ZSTR_VAL(capitalized_property_name),
                                      ZSTR_LEN(capitalized_property_name));

    zend_string_release(capitalized_property_name);
    zend_string_release(prefix);

    return setter_name;
}

zend_string *get_getter_method_name(zend_string *property_name, zend_class_entry *parent_ce)
{
    zend_string *getter_name = NULL;

    char *cproperty_name = ZSTR_VAL(property_name);
    toupper(cproperty_name[0]);

    getter_name = zend_string_concat2("get", 3, cproperty_name, strlen(cproperty_name));
    zend_string *key = zend_string_tolower(getter_name);
    if (zend_hash_exists(&parent_ce->function_table, key)) {
        zend_string_release(getter_name);
        zend_string_release(key);
        return getter_name;
    } else {
        zend_string_release(getter_name);
        zend_string_release(key);
        return NULL;
    }
}

zend_string *get_property_class_name(zend_string *property_name, zend_class_entry *parent_ce)
{
    zend_property_info *property_info;
    zend_string *class_name = NULL;

    property_info = zend_hash_find_ptr(&parent_ce->properties_info, property_name);

    if (property_info != NULL && ZEND_TYPE_IS_SET(property_info->type) && ZEND_TYPE_HAS_NAME(property_info->type)) {
        class_name = ZEND_TYPE_NAME(property_info->type);
    }

    return class_name;
}

bool is_circular_reference(zval *object, zend_array *context)
{
    // zend_string *object_hash = php_spl_object_hash(Z_OBJ_P(object));
    // php_printf("Checking for Circular reference %s\n", ZSTR_VAL(object_hash));

    // zval *circular_reference_limit = zend_hash_find(context, ZSTR_INIT_LITERAL(CIRCULAR_REFERENCE_LIMIT_NAME, 0));

    // int limit = (circular_reference_limit && (Z_TYPE_P(circular_reference_limit) == IS_LONG))
    //                 ? Z_LVAL_P(circular_reference_limit)
    //                 : DEFAULT_CIRCULAR_REFERENCE_LIMIT;

    // zval *counters = zend_hash_find(context, ZSTR_INIT_LITERAL(CIRCULAR_REFERENCE_COUNTERS_NAME, 0));

    // if (counters == NULL) {
    //     // zend_hash_real_init(counters, 1);
    //     counters = emalloc(sizeof(zval));
    //     ZVAL_UNDEF(counters);
    //     array_init(counters);
    // }

    // zval *counter = zend_hash_find(Z_ARRVAL_P(counters), object_hash);
    // if (counter != NULL) {
    //     if (Z_TYPE_P(counter) == IS_LONG && Z_LVAL_P(counter) >= limit) {
    //         zend_hash_del(counters, object_hash);
    //         php_printf("\t\t\t\t\tCircular reference detected\n");
    //         return TRUE;
    //     }

    //     Z_LVAL_P(counter)++;
    //     // php_printf("\t\tIncrementing value\n");
    //     zend_hash_update(Z_ARR_P(counters), object_hash, counter);
    //     zend_hash_update(context, ZSTR_INIT_LITERAL(CIRCULAR_REFERENCE_COUNTERS_NAME, 0), counters);
    // } else {
    //     zval new_counter;
    //     ZVAL_LONG(&new_counter, 1);
    //     // php_printf("\t\tNew value\n");
    //     zend_hash_add_new(Z_ARR_P(counters), object_hash, &new_counter);
    //     zend_hash_update(context, ZSTR_INIT_LITERAL(CIRCULAR_REFERENCE_COUNTERS_NAME, 0), counters);
    // }

    return FALSE;
}

void handle_circular_reference(zval *object, zend_array *context, zval *retval)
{
    // zval *circular_reference_handler = zend_hash_str_find(context, "circular_reference_handler",
    // sizeof("circular_reference_handler") - 1); if (circular_reference_handler != NULL) {
    //     zval retval;
    //     zval params[2];
    //     char *is_callable_error = NULL;
    //     zend_fcall_info fci = empty_fcall_info;
    //     zend_fcall_info_cache fci_cache = empty_fcall_info_cache;
    //     fci.retval = &retval;
    //     fci.param_count = 2;
    //     fci.params = params;

    //     ZVAL_COPY(&params[0], object);
    //     ZVAL_COPY(&params[1], context);
    //     if (zend_fcall_info_init(circular_reference_handler, 0, &fci, &fci_cache,NULL , &is_callable_error) ==
    //     FAILURE) {
    //         if (is_callable_error) {
    //             zend_type_error("%s", is_callable_error);
    //             // efree(is_callable_error);
    //         } else {
    //             zend_type_error("User-supplied function must be a valid callback");
    //         }
    //         if (is_callable_error) {
    //             /* Possible error message */
    //             // efree(is_callable_error);
    //         }
    //         zend_throw_exception_ex(NULL, 0, "Invalid circular reference handler.");
    //     }
    // }

    // zend_string *class_name = Z_OBJCE_P(object)->name;
    // zval *circularReferenceLimit = zend_hash_str_find(context, "circular_reference_limit",
    // sizeof("circular_reference_limit") - 1); int limit = circularReferenceLimit != NULL &&
    // Z_TYPE_P(circularReferenceLimit) == IS_LONG ? Z_LVAL_P(circularReferenceLimit) :
    // DEFAULT_CIRCULAR_REFERENCE_LIMIT;

    // zend_string *message = zend_string_init_fast("A circular reference has been detected when serializing the object
    // of class \"", sizeof("A circular reference has been detected when serializing the object of class \"") - 1 +
    // ZSTR_LEN(class_name) + sizeof("\" (configured limit: ") - 1 + sizeof(").") - 1, 0); zend_string *debug_type =
    // zend_get_debug_type(Z_OBJ_P(object)); zend_string *limit_str = zend_long_to_str(limit); zend_string
    // *formatted_message = zend_string_alloc(ZSTR_LEN(message) + ZSTR_LEN(debug_type) + ZSTR_LEN(limit_str), 0);
    // sprintf(ZSTR_VAL(formatted_message), "A circular reference has been detected when serializing the object of class
    // %s%s\" (configured limit: %d).", ZSTR_VAL(message), ZSTR_VAL(debug_type), DEFAULT_CIRCULAR_REFERENCE_LIMIT);

    // zend_throw_exception_ex(NULL, 0, "%s", ZSTR_VAL(formatted_message));

    // zend_string_release(message);
    // zend_string_release(debug_type);
    // zend_string_release(limit_str);
    // zend_string_release(formatted_message);
}

zend_array *get_object_properties(zval *object)
{
    zend_class_entry *ce = Z_OBJCE_P(object);
    zend_array *properties = NULL;
    zend_array *all_properties;
    zval *property_val;
    zend_string *prop_name, *mangled_name;
    zval ce_zval;

    ALLOC_HASHTABLE(all_properties);
    zend_hash_init(all_properties, 8, NULL, ZVAL_PTR_DTOR, 0);

    do {
        properties = Z_OBJ_HANDLER_P(object, get_properties)(Z_OBJ_P(object));

        ZEND_HASH_FOREACH_STR_KEY_VAL(properties, prop_name, property_val)
        {
            /* If the property is protected or private, add to the result */
            if (Z_TYPE_P(property_val) != IS_NULL) {
                ZVAL_OBJ(&ce_zval, zend_objects_new(ce));
                zend_hash_add(all_properties, prop_name, &ce_zval);
            }
        }
        ZEND_HASH_FOREACH_END();
        ce = ce->parent;
    } while (ce != NULL);

    return all_properties;
}

zend_string *get_unmangled_property_name(zend_string *name)
{
    const char *prop_name, *class_name;
    zend_result result = zend_unmangle_property_name(name, &class_name, &prop_name);
    if (result == SUCCESS) {
        zend_string *zs_prop_name = zend_string_init_fast(prop_name, sizeof(prop_name));
        const char *unmangled_name_cstr = zend_get_unmangled_property_name(zs_prop_name);
        zend_string *unmangled_name = zend_string_init_fast(unmangled_name_cstr, strlen(unmangled_name_cstr));
        zend_string_release(zs_prop_name);
        return unmangled_name;
    }

    return name;
}

ZEND_METHOD(ObjectNormalizer, __construct)
{
    zval *options = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_ARRAY(options)
    ZEND_PARSE_PARAMETERS_END();

    if (ZEND_NUM_ARGS() > 0) {
        zend_update_property(Z_OBJCE_P(ZEND_THIS), Z_OBJ_P(ZEND_THIS), "options", strlen("options"), options);
    }
}

ZEND_METHOD(ObjectNormalizer, __destruct)
{
    // zval options;
    // zend_read_property(Z_OBJCE_P(ZEND_THIS), Z_OBJ_P(ZEND_THIS), "options", strlen("options"), 0, &options);
    // zval_ptr_dtor(&options);
}

// Function to normalize an object to an array
ZEND_METHOD(ObjectNormalizer, normalize)
{
    zval *obj, *zv;

    zval *context_param = NULL;
    HashTable context;
    bool destroy = FALSE;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_ZVAL(obj)
    Z_PARAM_OPTIONAL
    Z_PARAM_ARRAY(context_param)
    ZEND_PARSE_PARAMETERS_END();

    zv = zend_read_property(Z_OBJCE_P(ZEND_THIS), Z_OBJ_P(ZEND_THIS), "options", strlen("options"), 0, NULL);
    zend_hash_init(&context, 0, NULL, ZVAL_PTR_DTOR, 0);
    if (context_param != NULL) {
        zend_hash_merge(&context, Z_ARRVAL_P(context_param), (copy_ctor_func_t)zval_add_ref, 1);
        zval_ptr_dtor(context_param);
    }

    if (zv && Z_TYPE_P(zv) == IS_ARRAY && Z_ARRVAL_P(zv)->nNumOfElements > 0) {
        Z_TRY_ADDREF_P(zv);
        zend_hash_str_add_new(&context, "options", strlen("options"), zv);
    }

    normalize_object(obj, &context, return_value);
    // print_array(&context, 10);
    zend_hash_destroy(&context);
}

// Function to denormalize an array back to an object
ZEND_METHOD(ObjectNormalizer, denormalize)
{
    char *class_name_cstr;
    zval *arr = NULL;
    HashTable context;
    zval *context_param = NULL;
    size_t class_name_len;
    zend_string *cname = NULL;
    zend_class_entry *ce = NULL;
    bool is_array = FALSE;
    bool created = FALSE;

    ZEND_PARSE_PARAMETERS_START(2, 3)
    Z_PARAM_ZVAL(arr)
    Z_PARAM_STRING(class_name_cstr, class_name_len)
    Z_PARAM_OPTIONAL
    Z_PARAM_ARRAY(context_param)
    ZEND_PARSE_PARAMETERS_END();

    zend_hash_init(&context, 0, NULL, ZVAL_PTR_DTOR, 0);

    if (context_param != NULL) {
        zend_hash_merge(&context, Z_ARRVAL_P(context_param), (copy_ctor_func_t)zval_add_ref, 1);
        zval_ptr_dtor(context_param);
    }

    if (class_name_cstr[strlen(class_name_cstr) - 1] == ']' && class_name_cstr[strlen(class_name_cstr) - 2] == '[') {
        strcpy(class_name_cstr + strlen(class_name_cstr) - 2, "\0");
        is_array = TRUE;
    }

    cname = zend_string_init_fast(class_name_cstr, strlen(class_name_cstr));
    ce = zend_lookup_class(cname);

    if (!ce) {
        zend_throw_error(zend_ce_value_error, "Undefined class \"%s\".", ZSTR_VAL(cname));
        zend_string_release(cname);
        RETURN_THROWS();
    }
    if (ce->ce_flags & ZEND_ACC_INTERFACE) {
        zend_throw_error(zend_ce_value_error, "Can not instantiate object from interface \"%s\".", ZSTR_VAL(ce->name));
        zend_string_release(cname);
        RETURN_THROWS();
    }
    if (ce->ce_flags & (ZEND_ACC_IMPLICIT_ABSTRACT_CLASS | ZEND_ACC_EXPLICIT_ABSTRACT_CLASS)) {
        zend_throw_error(zend_ce_value_error,
                         "Can not instantiate object from abstract class \"%s\".",
                         ZSTR_VAL(ce->name));
        zend_string_release(cname);
        RETURN_THROWS();
    }
    if (ce->ce_flags & ZEND_ACC_TRAIT) {
        zend_throw_error(zend_ce_value_error, "Can not instantiate object from trait \"%s\".", ZSTR_VAL(ce->name));
        zend_string_release(cname);
        RETURN_THROWS();
    }
    if (ce->ce_flags & ZEND_ACC_ENUM) {
        zend_throw_error(zend_ce_value_error, "Can not instantiate object from enum \"%s\".", ZSTR_VAL(ce->name));
        zend_string_release(cname);
        RETURN_THROWS();
    }

    zval *object_to_populate = zend_hash_str_find(&context, OBJECT_TO_POPULATE, strlen(OBJECT_TO_POPULATE));
    if (object_to_populate) {
        ZVAL_COPY(return_value, object_to_populate);
        denormalize_array(arr, &context, return_value, ce, is_array, FALSE);
    } else {
        denormalize_array(arr, &context, return_value, ce, is_array, TRUE);
    }

    zend_string_release(cname);
    zend_hash_destroy(&context);
}

void register_object_normalizer_class()
{
    zend_class_entry object_normalizer_ce;
    zval property_options_default_value;

    // clang-format off
    static const zend_function_entry object_normalizer_methods[] = {
        PHP_ME(ObjectNormalizer, __construct, arginfo_class_Normalizer_ObjectNormalizer___construct, ZEND_ACC_PUBLIC)
        PHP_ME(ObjectNormalizer, __destruct, arginfo_class_Normalizer_ObjectNormalizer___destruct, ZEND_ACC_PUBLIC)
        PHP_ME(ObjectNormalizer, normalize, arginfo_class_Normalizer_ObjectNormalizer_normalize, ZEND_ACC_PUBLIC)
        PHP_ME(ObjectNormalizer, denormalize, arginfo_class_Normalizer_ObjectNormalizer_denormalize, ZEND_ACC_PUBLIC)
        PHP_FE_END
    };
    // clang-format on

    INIT_CLASS_ENTRY(object_normalizer_ce, "Normalizer\\ObjectNormalizer", object_normalizer_methods);
    object_normalizer_class_entry = zend_register_internal_class(&object_normalizer_ce);

    DECLARE_CLASS_STRING_CONSTANT(object_normalizer_class_entry, GROUPS, GROUPS, ZEND_ACC_PUBLIC);
    DECLARE_CLASS_STRING_CONSTANT(object_normalizer_class_entry,
                                  OBJECT_TO_POPULATE,
                                  OBJECT_TO_POPULATE,
                                  ZEND_ACC_PUBLIC);

    DECLARE_CLASS_STRING_CONSTANT(object_normalizer_class_entry, SKIP_NULL_VALUES, SKIP_NULL_VALUES, ZEND_ACC_PUBLIC);
    DECLARE_CLASS_STRING_CONSTANT(object_normalizer_class_entry,
                                  SKIP_UNINITIALIZED_VALUES,
                                  SKIP_UNINITIALIZED_VALUES,
                                  ZEND_ACC_PUBLIC);

    ZVAL_EMPTY_ARRAY(&property_options_default_value);
    zend_declare_property_string(object_normalizer_class_entry,
                                 "options",
                                 strlen("options"),
                                 &property_options_default_value,
                                 ZEND_ACC_PRIVATE);
}
