#include "php.h"
#include "ext/standard/php_var.h"
#include "ext/date/php_date.h"
#include "zend_attributes.h"
#include "zend_exceptions.h"
#include "zend.h"
#include "object_normalizer_arginfo.h"
#include "object_normalizer_ce.h"
#include "../helpers.h"
#include "../attributes/normalizer_attributes.h"

#define DATE_FORMAT_RFC3339_EXTENDED "Y-m-d\\TH:i:s.vP"
#define DEFAULT_CIRCULAR_REFERENCE_LIMIT 1

#define CLEAR_ZVAL(z)                                                                                                  \
    if (z) {                                                                                                           \
        zval_ptr_dtor(z);                                                                                              \
        z = NULL;                                                                                                      \
    }

zend_class_entry *object_normalizer_class_entry;

bool must_normalize(HashTable *attributes, zend_array *context, zval *value);
zend_string *get_setter_method_name(zend_string *property_name, zend_class_entry *parent_ce);
zend_string *get_getter_method_name(zend_string *property_name, zend_class_entry *parent_ce);
zend_string *get_property_class_name(zend_string *property_name, zend_class_entry *parent_ce);
zend_string *get_normalized_name(zend_string *property_name, HashTable *attributes);
zend_string *get_property_name_from_normalized_name(zend_string *normalized_name, zend_class_entry *ce);
bool is_circular_reference(zval *object, zend_array *context);
void handle_circular_reference(zval *object, zend_array *context, zval *retval);

void normalize_object(zval *input, zend_array *context, zval *retval)
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
        // if (is_circular_reference(input, context)) {
        //     handle_circular_reference(input, context, retval);
        // }
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
                        normalize = must_normalize(property_info->attributes, context, val);
                    } else {
                        normalize = FALSE;
                    }

                    if (normalize) {
                        zend_string *normalized_name = get_normalized_name(
                            zend_string_init(prop_name, sizeof(prop_name), 0), property_info->attributes);
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
                            if (Z_TYPE(Z_ARRVAL_P(val)->arPacked[0]) == IS_OBJECT) {
                                // Items in the array property are complex objects -> normalize
                                // each one of them
                                zval r, tmp;
                                array_init_size(&tmp, Z_ARRVAL_P(val)->nNumOfElements);
                                for (int i = 0; i < Z_ARRVAL_P(val)->nNumOfElements; i++) {
                                    normalize_object(&Z_ARRVAL_P(val)->arPacked[0], context, &r);
                                    add_index_array(&tmp, i, Z_ARRVAL(r));
                                }
                                add_assoc_array(retval, ZSTR_VAL(normalized_name), Z_ARRVAL(tmp));
                            } else {
                                // Object holds scalar values, just copy to output
                                add_assoc_array(retval, ZSTR_VAL(normalized_name), Z_ARRVAL_P(val));
                            }
                        } else if (Z_TYPE_P(val) == IS_OBJECT) {
                            if (zend_class_implements_interface(Z_OBJCE_P(val), php_date_get_interface_ce())) {
                                add_assoc_string(retval, ZSTR_VAL(normalized_name),
                                                 ZSTR_VAL(php_format_date(DATE_FORMAT_RFC3339_EXTENDED,
                                                                          sizeof(DATE_FORMAT_RFC3339_EXTENDED) - 1,
                                                                          Z_PHPDATE_P(val)->time->sse,
                                                                          Z_PHPDATE_P(val)->time->is_localtime)));
                            } else {
                                zend_string *getter_name = get_getter_method_name(key, Z_OBJCE_P(input));
                                if (getter_name) {
                                    zval getter_value;
                                    zend_function *func = zend_hash_find_ptr(&Z_OBJCE_P(input)->function_table,
                                                                             zend_string_tolower(getter_name));
                                    zend_call_known_instance_method_with_0_params(func, Z_OBJ_P(input), &getter_value);
                                    normalize_object(&getter_value, context, &normalized_sub_object);
                                    add_assoc_array(retval, ZSTR_VAL(normalized_name),
                                                    Z_ARRVAL_P(&normalized_sub_object));
                                } else {
                                    normalize_object(val, context, &normalized_sub_object);
                                    add_assoc_array(retval, ZSTR_VAL(normalized_name), Z_ARR_P(&normalized_sub_object));
                                }
                            }
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

void denormalize_array(zval *input, zend_array *context, zval *retval, zend_class_entry *ce, bool is_array)
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
            zend_string* property_name = get_property_name_from_normalized_name(key, ce);
            zend_property_info* property_info = zend_hash_find_ptr(&ce->properties_info, property_name);
            zend_string *setter_name = get_setter_method_name(property_name, ce);
            zend_string *property_class_name = get_property_class_name(property_name, ce);

            php_printf("Key %s (%s), of class %s\n", ZSTR_VAL(key), property_info->ce->name, ZSTR_VAL(ce->name));
            if (property_class_name && zend_class_implements_interface(property_info->ce, php_date_get_interface_ce())) {
                // php_printf("dealing with date\n");
            } else {
            try_again:
                if (Z_TYPE_P(val) == IS_TRUE) {
                    zend_update_property_bool(ce, Z_OBJ_P(retval), ZSTR_VAL(property_name), ZSTR_LEN(property_name), TRUE);
                } else if (Z_TYPE_P(val) == IS_FALSE) {
                    // php_printf("Key %s (bool), of class %s\n", ZSTR_VAL(key), ZSTR_VAL(ce->name));
                    zend_update_property_bool(ce, Z_OBJ_P(retval), ZSTR_VAL(property_name), ZSTR_LEN(property_name), FALSE);
                } else if (Z_TYPE_P(val) == IS_LONG) {
                    // php_printf("Key %s (long), of class %s\n", ZSTR_VAL(key), ZSTR_VAL(ce->name));
                    zend_update_property_long(ce, Z_OBJ_P(retval), ZSTR_VAL(property_name), ZSTR_LEN(property_name), Z_LVAL_P(val));
                } else if (Z_TYPE_P(val) == IS_DOUBLE) {
                    php_var_dump(val, 10);
                    // php_printf("Key %s (double), of class %s\n", ZSTR_VAL(key), ZSTR_VAL(ce->name));
                    zend_update_property_double(ce, Z_OBJ_P(retval), ZSTR_VAL(property_name), ZSTR_LEN(property_name), Z_DVAL_P(val));
                } else if (Z_TYPE_P(val) == IS_STRING) {
                    // php_printf("Key %s (string), of class %s\n", ZSTR_VAL(key), ZSTR_VAL(ce->name));
                    zend_update_property_string(ce, Z_OBJ_P(retval), ZSTR_VAL(property_name), ZSTR_LEN(property_name), Z_STRVAL_P(val));
                } else if (Z_TYPE_P(val) == IS_ARRAY) {
                    // php_printf("Key %s (array), of class %s\n", ZSTR_VAL(key), ZSTR_VAL(ce->name));


                    // Property should be denormalized as object
                    if (property_class_name) {
                        zend_class_entry *sub_ce = zend_lookup_class(property_class_name);
                        zval r;
                        denormalize_array(val, context, &r, sub_ce, FALSE);
                        zend_update_property(ce, Z_OBJ_P(retval), ZSTR_VAL(property_name), ZSTR_LEN(property_name), &r);
                    }
                } else if (Z_TYPE_P(val) == IS_REFERENCE) {
                    val = Z_REFVAL_P(val);
                    goto try_again;
                } else {
                    ZEND_UNREACHABLE();
                }
            }
        }
        ZEND_HASH_FOREACH_END();
    }
}

bool must_normalize(HashTable *attributes, zend_array *context, zval *value)
{
    bool normalize = FALSE;
    zval *skip_null_values = NULL;
    zval *skip_uninitialized_values = NULL;

    zend_attribute *ignore_attribute =
        zend_get_attribute_str(attributes, IGNORE_ATTRIBUTE, sizeof(IGNORE_ATTRIBUTE) - 1);

    zend_attribute *expose_attribute =
        zend_get_attribute_str(attributes, EXPOSE_ATTRIBUTE, sizeof(EXPOSE_ATTRIBUTE) - 1);

    zend_attribute *groups_attribute =
        zend_get_attribute_str(attributes, GROUPS_ATTRIBUTE, sizeof(GROUPS_ATTRIBUTE) - 1);

    skip_null_values = zend_hash_str_find(context, SKIP_NULL_VALUES_VALUE, sizeof(SKIP_NULL_VALUES_VALUE) - 1);
    skip_uninitialized_values =
        zend_hash_str_find(context, SKIP_UNINITIALIZED_VALUES_VALUE, sizeof(SKIP_UNINITIALIZED_VALUES_VALUE) - 1);

    if (skip_null_values != NULL && Z_TYPE_INFO_P(skip_null_values) == IS_TRUE && Z_TYPE_P(value) == IS_NULL) {
        normalize = FALSE;
    } else if (skip_uninitialized_values != NULL && Z_TYPE_INFO_P(skip_uninitialized_values) == IS_TRUE &&
               Z_TYPE_P(value) == IS_UNDEF) {
        normalize = FALSE;
    } else if (ignore_attribute) {
        // zval_ptr_dtor(ignore_attribute);
        normalize = FALSE;
    } else if (expose_attribute) {
        // zval_ptr_dtor(expose_attribute);
        normalize = TRUE;
    } else if (groups_attribute) {
        zend_attribute_arg p = groups_attribute->args[0];
        zval *requested_groups = zend_hash_str_find(context, GROUPS_CONST_VALUE, sizeof(GROUPS_CONST_VALUE) - 1);
        normalize = check_array_intersection_string(&p.value, requested_groups);
        // zval_ptr_dtor(groups_attribute);
    } else {
        normalize = FALSE;
    }

    CLEAR_ZVAL(skip_null_values);

    return normalize;
}

/**
 * Given a property name, return the normalized name for normalization defined by SerializedName attribute if defined
 */
zend_string *get_normalized_name(zend_string *property_name, HashTable *attributes)
{
    zend_string *normalized_name = NULL;
    zend_attribute *normalized_name_attribute =
        zend_get_attribute_str(attributes, SERIALIZED_NAME_ATTRIBUTE, sizeof(SERIALIZED_NAME_ATTRIBUTE) - 1);

    if (normalized_name_attribute) {
        normalized_name = Z_STR(normalized_name_attribute->args[0].value);
    }

    return normalized_name != NULL ? normalized_name : property_name;
}

/**
 * Reverse operation of get_normalized_name
 */
zend_string *get_property_name_from_normalized_name(zend_string *normalized_name, zend_class_entry *ce)
{
    zend_string *property_name = zend_string_copy(normalized_name);

    for(int i=0; i < ce->properties_info.nNumOfElements; i++)
    {
        Bucket b = ce->properties_info.arData[i];
        zend_property_info* property_info = zend_hash_find_ptr(&ce->properties_info, b.key);
        if (property_info) {
            zend_attribute *normalized_name_attribute = zend_get_attribute_str(property_info->attributes, SERIALIZED_NAME_ATTRIBUTE, sizeof(SERIALIZED_NAME_ATTRIBUTE) - 1);
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

    // zend_string_release(capitalized_property_name);
    return setter_name;
}

zend_string *get_getter_method_name(zend_string *property_name, zend_class_entry *parent_ce)
{
    zend_string *getter_name = NULL, *capitalized_property_name;

    unsigned char r = zend_toupper_ascii(ZSTR_VAL(property_name)[0]);
    if (r == ZSTR_VAL(property_name)[0]) {
        capitalized_property_name = zend_string_copy(property_name);
    } else {
        capitalized_property_name = zend_string_init(ZSTR_VAL(property_name), ZSTR_LEN(property_name), 0);
        ZSTR_VAL(capitalized_property_name)
        [0] = r;
    }
    getter_name = zend_string_init("get", 3, 0);
    getter_name = zend_string_concat2(ZSTR_VAL(getter_name), ZSTR_LEN(getter_name), ZSTR_VAL(capitalized_property_name),
                                      ZSTR_LEN(capitalized_property_name));

    if (zend_hash_exists(&parent_ce->function_table, zend_string_tolower(getter_name))) {
        return getter_name;
    } else {
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

bool is_circular_reference(zval *object, HashTable *context)
{
    // zend_string *object_hash = zend_object_hash(Z_OBJ_P(object));

    // zval *circular_reference_limit =
    //     zend_hash_str_find(context, "circular_reference_limit", sizeof("circular_reference_limit") - 1);
    // int limit = Z_TYPE_P(circular_reference_limit) == IS_LONG ? Z_LVAL_P(circular_reference_limit)
    //                                                           : DEFAULT_CIRCULAR_REFERENCE_LIMIT;

    // zval *counters =
    //     zend_hash_str_find(context, "circular_reference_counters", sizeof("circular_reference_counters") - 1);
    // zval *counter = zend_hash_find(counters, object_hash);

    // if (counter != NULL) {
    //     if (Z_TYPE_P(counter) == IS_LONG && Z_LVAL_P(counter) >= limit) {
    //         zend_hash_del(counters, object_hash);
    //         return 1;
    //     }

    //     Z_LVAL_P(counter)++;
    // } else {
    //     zval new_counter;
    //     ZVAL_LONG(&new_counter, 1);
    //     zend_hash_add(counters, object_hash, &new_counter);
    // }

    return 0;
}

void handle_circular_reference(zval *object, zend_array *context, zval *retval)
{
    // zval *circular_reference_handler = zend_hash_str_find(context, "circular_reference_handler", sizeof("circular_reference_handler") - 1);
    // if (circular_reference_handler != NULL) {
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
    //     if (zend_fcall_info_init(circular_reference_handler, 0, &fci, &fci_cache,NULL , &is_callable_error) == FAILURE) {
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
    // zval *circularReferenceLimit = zend_hash_str_find(context, "circular_reference_limit", sizeof("circular_reference_limit") - 1);
    // int limit = circularReferenceLimit != NULL && Z_TYPE_P(circularReferenceLimit) == IS_LONG ? Z_LVAL_P(circularReferenceLimit) : DEFAULT_CIRCULAR_REFERENCE_LIMIT;

    // zend_string *message = zend_string_init("A circular reference has been detected when serializing the object of class \"", sizeof("A circular reference has been detected when serializing the object of class \"") - 1 + ZSTR_LEN(class_name) + sizeof("\" (configured limit: ") - 1 + sizeof(").") - 1, 0);
    // zend_string *debug_type = zend_get_debug_type(Z_OBJ_P(object));
    // zend_string *limit_str = zend_long_to_str(limit);
    // zend_string *formatted_message = zend_string_alloc(ZSTR_LEN(message) + ZSTR_LEN(debug_type) + ZSTR_LEN(limit_str), 0);
    // sprintf(ZSTR_VAL(formatted_message), "A circular reference has been detected when serializing the object of class  %s%s\" (configured limit: %d).", ZSTR_VAL(message), ZSTR_VAL(debug_type), DEFAULT_CIRCULAR_REFERENCE_LIMIT);

    // zend_throw_exception_ex(NULL, 0, "%s", ZSTR_VAL(formatted_message));

    // zend_string_release(message);
    // zend_string_release(debug_type);
    // zend_string_release(limit_str);
    // zend_string_release(formatted_message);
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
        Z_PARAM_ARRAY(context)
    ZEND_PARSE_PARAMETERS_END();


    if (context == NULL) {
        context = emalloc(sizeof(zval));
        ZVAL_UNDEF(context);
        array_init(context);
    }

    normalize_object(obj, Z_ARRVAL_P(context), return_value); // Pass return_value to store result
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
    Z_PARAM_ARRAY(context)
    ZEND_PARSE_PARAMETERS_END();

    if (context == NULL) {
        context = emalloc(sizeof(zval));
        ZVAL_UNDEF(context);
        array_init(context);
    }

    if (className[strlen(className) - 1] == ']' && className[strlen(className) - 2] == '[') {
        strcpy(className + strlen(className) - 2, "\0");
        is_array = TRUE;
    }

    cname = zend_string_init(className, strlen(className), 0);
    ce = zend_lookup_class(cname);

    denormalize_array(arr, Z_ARRVAL_P(context), return_value, ce, is_array);

    // efree(context);
    // efree(arr);
    // zend_string_release(cname);

}

// ZEND_BEGIN_ARG_INFO(arginfo_object_normalizer___construct, 0)
// ZEND_END_ARG_INFO()

// ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_object_normalizer_normalize, 0, 1, MAY_BE_ARRAY)
// ZEND_ARG_TYPE_INFO(0, obj, IS_MIXED, 0)
// ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, context, IS_ARRAY, 0, "[]")
// ZEND_END_ARG_INFO()

// // Arginfo for denormalize_array()
// ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_object_normalizer_denormalize, 0, 2, MAY_BE_OBJECT)
// ZEND_ARG_TYPE_INFO(0, arr, IS_MIXED, 0)
// ZEND_ARG_TYPE_INFO(0, className, IS_STRING, 0)
// ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, context, IS_ARRAY, 0, "[]")
// ZEND_END_ARG_INFO()

void register_object_normalizer_class()
{
    zend_class_entry object_normalizer_ce;

    static const zend_function_entry object_normalizer_methods[] = {
        PHP_ME(ObjectNormalizer, __construct, arginfo_class_Normalizer_ObjectNormalizer___construct, ZEND_ACC_PUBLIC)
            PHP_ME(ObjectNormalizer, normalize, arginfo_class_Normalizer_ObjectNormalizer_normalize, ZEND_ACC_PUBLIC)
                PHP_ME(ObjectNormalizer, denormalize, arginfo_class_Normalizer_ObjectNormalizer_denormalize, ZEND_ACC_PUBLIC)
                    PHP_FE_END};

    INIT_CLASS_ENTRY(object_normalizer_ce, "Normalizer\\ObjectNormalizer", object_normalizer_methods);
    object_normalizer_class_entry = zend_register_internal_class(&object_normalizer_ce);

    DECLARE_CLASS_STRING_CONSTANT(object_normalizer_class_entry, GROUPS, GROUPS, ZEND_ACC_PUBLIC);
    DECLARE_CLASS_STRING_CONSTANT(object_normalizer_class_entry, OBJECT_TO_POPULATE, OBJECT_TO_POPULATE,
                                  ZEND_ACC_PUBLIC);
    DECLARE_CLASS_STRING_CONSTANT(object_normalizer_class_entry, SKIP_NULL_VALUES, SKIP_NULL_VALUES, ZEND_ACC_PUBLIC);
    DECLARE_CLASS_STRING_CONSTANT(object_normalizer_class_entry, SKIP_UNINITIALIZED_VALUES, SKIP_UNINITIALIZED_VALUES,
                                  ZEND_ACC_PUBLIC);
}
