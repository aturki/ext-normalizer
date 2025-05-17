#include "php.h"
#include "ext/standard/php_var.h"
#include "ext/date/php_date.h"
#include "ext/spl/php_spl.h"
#include "zend_attributes.h"
#include "zend_exceptions.h"
#include "zend_interfaces.h"
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

#define MAX_RECURSION_DEPTH 50 // Define a maximum recursion depth
#define CIRCULAR_REFERENCE_HASH_KEY "__circular_ref_detected"

zend_class_entry *object_normalizer_class_entry;

/**********************/
/* internal utilities */
/**********************/

bool must_normalize_property(HashTable *attributes, zend_array *context, zval *value)
{
    bool normalize = FALSE;  // Default to FALSE unless explicitly allowed
    bool use_symfony_attributes = FALSE;
    zval *skip_null_values = NULL;
    zval *skip_uninitialized_values = NULL;
    zend_attribute *ignore_attribute = NULL;
    zend_attribute *expose_attribute = NULL;
    zend_attribute *groups_attribute = NULL;

    // Check for options
    zval *options = zend_hash_str_find(context, "options", strlen("options"));
    if (options) {
        zval *zv_usa =
            zend_hash_str_find(Z_ARRVAL_P(options), "use_symfony_attributes", strlen("use_symfony_attributes"));
        if (zv_usa && Z_TYPE_P(zv_usa) == IS_TRUE) {
            use_symfony_attributes = TRUE;
        }
    }

    // Find applicable attributes
    if (attributes) {
        ignore_attribute = zend_get_attribute_str(attributes, IGNORE_ATTRIBUTE, strlen(IGNORE_ATTRIBUTE));
        groups_attribute = zend_get_attribute_str(attributes, GROUPS_ATTRIBUTE, strlen(GROUPS_ATTRIBUTE));
        expose_attribute = zend_get_attribute_str(attributes, EXPOSE_ATTRIBUTE, strlen(EXPOSE_ATTRIBUTE));
    }

    // Check skip conditions first

    // If property should be ignored, return false
    if (ignore_attribute) {
        return FALSE;
    }

    // Check if we should skip null values
    skip_null_values = zend_hash_str_find(context, SKIP_NULL_VALUES_VALUE, strlen(SKIP_NULL_VALUES_VALUE));
    if (skip_null_values && Z_TYPE_P(skip_null_values) == IS_TRUE) {
        if (value && Z_TYPE_P(value) == IS_NULL) {
            return FALSE;
        }
    }

    // Check if we should skip uninitialized values
    skip_uninitialized_values =
        zend_hash_str_find(context, SKIP_UNINITIALIZED_VALUES_VALUE, strlen(SKIP_UNINITIALIZED_VALUES_VALUE));
    if (skip_uninitialized_values && Z_TYPE_P(skip_uninitialized_values) == IS_TRUE) {
        if (value && Z_ISUNDEF_P(value)) {
            return FALSE;
        }
    }

    // Check if property should be exposed
    if (expose_attribute) {
        return TRUE;
    }

    // Check groups
    if (groups_attribute && (groups_attribute->argc > 0)) {
        zval groups;
        if (FAILURE == zend_get_attribute_value(&groups, groups_attribute, 0, NULL)) {
            if (EG(exception)) {
                zend_clear_exception();
            }
            return FALSE;
        }

        zval *requested_groups = zend_hash_str_find(context, GROUPS_CONST_VALUE, strlen(GROUPS_CONST_VALUE));
        if (!requested_groups) {
            // No groups specified in context, so don't normalize
            return FALSE;
        }

        zval function_name, retval_ptr;
        ZVAL_STRING(&function_name, "array_intersect");

        zval params[2];
        ZVAL_COPY(&params[0], requested_groups);
        ZVAL_COPY(&params[1], &groups);

        bool has_matching_groups = FALSE;
        if (call_user_function(EG(function_table), NULL, &function_name, &retval_ptr, 2, params) == SUCCESS) {
            has_matching_groups = Z_TYPE(retval_ptr) == IS_ARRAY && zend_hash_num_elements(Z_ARRVAL(retval_ptr)) > 0;
            zval_ptr_dtor(&retval_ptr);
        } else {
            if (EG(exception)) {
                zend_clear_exception();
            }
        }

        zval_ptr_dtor(&function_name);
        zval_ptr_dtor(&params[0]);
        zval_ptr_dtor(&params[1]);

        return has_matching_groups;
    }

    return expose_attribute != NULL; // Only normalize if explicitly exposed
}

/**
 * Determine if a method should be normalized as a virtual property
 * Similar to must_normalize_property but for methods
 */
bool must_normalize_function(HashTable *attributes, zend_array *context)
{
    bool normalize = FALSE; // Default to FALSE unless explicitly allowed
    zend_attribute *expose_attribute = NULL;
    zend_attribute *groups_attribute = NULL;

    if (attributes) {
        expose_attribute = zend_get_attribute_str(attributes, EXPOSE_ATTRIBUTE, strlen(EXPOSE_ATTRIBUTE));
        groups_attribute = zend_get_attribute_str(attributes, GROUPS_ATTRIBUTE, strlen(GROUPS_ATTRIBUTE));
    }

    // Check if method should be exposed
    if (expose_attribute) {
        return TRUE;
    }

    // Check groups
    if (groups_attribute && (groups_attribute->argc > 0)) {
        zval groups;
        if (FAILURE == zend_get_attribute_value(&groups, groups_attribute, 0, NULL)) {
            if (EG(exception)) {
                zend_clear_exception();
            }
            return FALSE;
        }

        zval *requested_groups = zend_hash_str_find(context, GROUPS_CONST_VALUE, strlen(GROUPS_CONST_VALUE));
        if (!requested_groups) {
            // No groups specified in context, so don't normalize
            zval_ptr_dtor(&groups);
            return FALSE;
        }

        zval function_name, retval_ptr;
        ZVAL_STRING(&function_name, "array_intersect");

        zval params[2];
        ZVAL_COPY(&params[0], requested_groups);
        ZVAL_COPY(&params[1], &groups);

        bool has_matching_groups = FALSE;
        if (call_user_function(EG(function_table), NULL, &function_name, &retval_ptr, 2, params) == SUCCESS) {
            has_matching_groups = Z_TYPE(retval_ptr) == IS_ARRAY && zend_hash_num_elements(Z_ARRVAL(retval_ptr)) > 0;
            zval_ptr_dtor(&retval_ptr);
        } else {
            if (EG(exception)) {
                zend_clear_exception();
            }
        }

        zval_ptr_dtor(&function_name);
        zval_ptr_dtor(&params[0]);
        zval_ptr_dtor(&params[1]);
        zval_ptr_dtor(&groups);

        return has_matching_groups;
    }

    // For methods, only normalize if explicitly exposed (unlike properties)
    return FALSE;
}

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
    zend_ulong recursion_depth = 0;
    zval *recursion_depth_zv;

    // Circular reference detection and recursion depth management
    recursion_depth_zv = zend_hash_str_find(context, "recursion_depth", strlen("recursion_depth"));
    if (recursion_depth_zv) {
        recursion_depth = Z_LVAL_P(recursion_depth_zv);
    }

    if (recursion_depth > MAX_RECURSION_DEPTH) {
        zend_throw_error(NULL, "Maximum recursion depth exceeded during normalization");
        ZVAL_NULL(retval);
        return;
    }

    if (Z_TYPE_P(input) == IS_OBJECT) {
        zend_ulong object_handle = Z_OBJ_HANDLE_P(input);
        if (zend_hash_index_exists(context, object_handle)) {
            // Circular reference detected
            ZVAL_NEW_STR(retval, zend_string_init("*RECURSION*", strlen("*RECURSION*"), 0));
            return;
        }
        // Add object to seen set for this path
        zval tmp_obj_marker; ZVAL_TRUE(&tmp_obj_marker);
        zend_hash_index_add_new(context, object_handle, &tmp_obj_marker);
    }

    zval new_recursion_depth_zv;
    ZVAL_LONG(&new_recursion_depth_zv, recursion_depth + 1);
    zend_hash_str_update(context, "recursion_depth", strlen("recursion_depth"), &new_recursion_depth_zv);

    // We have a collection of items to normalize
    if (Z_TYPE_P(input) == IS_ARRAY) {
        array_init_size(retval, zend_hash_num_elements(Z_ARRVAL_P(input)));
        zend_ulong idx;
        zend_string *key;
        zval *val, r;

        ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(input), idx, key, val) {
            ZVAL_UNDEF(&r);
            normalize_object(val, context, &r);

            if (key) {
                add_assoc_zval(retval, ZSTR_VAL(key), &r);
            } else {
                add_index_zval(retval, idx, &r);
            }
        } ZEND_HASH_FOREACH_END();
    } else if (Z_TYPE_P(input) == IS_NULL) {
        ZVAL_NULL(retval);
    } else if (Z_TYPE_P(input) == IS_OBJECT) {
        object_properties = Z_OBJ_P(input)->handlers->get_properties(Z_OBJ_P(input));

        if (!object_properties) {
            ZVAL_NULL(retval);
        } else {
            zend_ulong num;
            zend_string *key = NULL;
            zval *val = NULL;

            array_init(retval);

            ZEND_HASH_FOREACH_KEY_VAL(object_properties, num, key, val)
            {
                if (!key) continue; // Skip numeric keys

                ZVAL_DEREF(val);
                bool normalize = FALSE;
                zend_property_info *property_info = NULL;

                if (Z_TYPE_P(val) == IS_INDIRECT) {
                    val = Z_INDIRECT_P(val);
                    property_info = zend_get_property_info_for_slot(Z_OBJ_P(input), val);
                }

                // Skip if value is undefined and we're not looking for specific attributes
                if (Z_ISUNDEF_P(val) && !property_info) {
                    continue;
                }

                zend_string *unmangled_name = get_unmangled_property_name(key);
                HashTable *prop_attributes = (property_info != NULL) ? property_info->attributes : NULL;

                // Determine if this property should be normalized
                normalize = must_normalize_property(prop_attributes, context, val);

                // Get the normalized name (using SerializedName if applicable)
                zend_string *normalized_name = get_normalized_name(unmangled_name, prop_attributes, FALSE);

                if (normalize) {
                    zval getter_value; ZVAL_UNDEF(&getter_value);
                    zend_string *getter_name = get_getter_method_name(unmangled_name, Z_OBJCE_P(input));

                    if (getter_name) {
                        zend_string *getter_name_lower_case = zend_string_tolower(getter_name);
                        zend_call_method_with_0_params(Z_OBJ_P(input),
                                                      Z_OBJCE_P(input),
                                                      NULL,
                                                      ZSTR_VAL(getter_name_lower_case),
                                                      &getter_value);
                        zend_string_release(getter_name_lower_case);
                        zend_string_release(getter_name);

                        if (EG(exception)) {
                            zval_ptr_dtor(&getter_value);
                            zend_string_release(normalized_name);
                            zend_string_release(unmangled_name);
                            zend_clear_exception();
                            continue;
                        }
                    } else {
                        // No getter, use the property value directly
                        ZVAL_COPY(&getter_value, val);
                    }

                    // Dereference if it's a reference
                    zval *val_to_process = &getter_value;
                    int deref_count = 0;
                    while (Z_ISREF_P(val_to_process) && deref_count < 10) {
                        val_to_process = Z_REFVAL_P(val_to_process);
                        deref_count++;
                    }
                    if (deref_count >= 10) {
                        zend_throw_error(NULL, "Maximum reference depth exceeded during normalization");
                        zval_ptr_dtor(&getter_value);
                        zend_string_release(normalized_name);
                        zend_string_release(unmangled_name);
                        continue;
                    }

                    // Process based on type
                    switch (Z_TYPE_P(val_to_process)) {
                        case IS_NULL:
                            add_assoc_null(retval, ZSTR_VAL(normalized_name));
                            break;

                        case IS_TRUE:
                            add_assoc_bool(retval, ZSTR_VAL(normalized_name), 1);
                            break;

                        case IS_FALSE:
                            add_assoc_bool(retval, ZSTR_VAL(normalized_name), 0);
                            break;

                        case IS_LONG:
                            add_assoc_long(retval, ZSTR_VAL(normalized_name), Z_LVAL_P(val_to_process));
                            break;

                        case IS_DOUBLE:
                            add_assoc_double(retval, ZSTR_VAL(normalized_name), Z_DVAL_P(val_to_process));
                            break;

                        case IS_STRING:
                            add_assoc_stringl(retval, ZSTR_VAL(normalized_name), Z_STRVAL_P(val_to_process), Z_STRLEN_P(val_to_process));
                            break;

                        case IS_ARRAY: {
                            zval sub_array;
                            array_init(&sub_array);

                            zend_ulong idx;
                            zend_string *key;
                            zval *elem;

                            ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(val_to_process), idx, key, elem) {
                                zval normalized_elem;
                                ZVAL_UNDEF(&normalized_elem);

                                normalize_object(elem, context, &normalized_elem);

                                if (key) {
                                    add_assoc_zval(&sub_array, ZSTR_VAL(key), &normalized_elem);
                                } else {
                                    add_index_zval(&sub_array, idx, &normalized_elem);
                                }
                            } ZEND_HASH_FOREACH_END();

                            add_assoc_zval(retval, ZSTR_VAL(normalized_name), &sub_array);
                            break;
                        }

                        case IS_OBJECT: {
                            if (zend_class_implements_interface(Z_OBJCE_P(val_to_process), php_date_get_interface_ce())) {
                                zend_string *str_date = php_format_date(DATE_FORMAT_RFC3339_EXTENDED,
                                                                      sizeof(DATE_FORMAT_RFC3339_EXTENDED) - 1,
                                                                      Z_PHPDATE_P(val_to_process)->time->sse,
                                                                      Z_PHPDATE_P(val_to_process)->time->is_localtime);
                                add_assoc_str(retval, ZSTR_VAL(normalized_name), str_date);
                            } else if (zend_class_implements_interface(Z_OBJCE_P(val_to_process), zend_ce_backed_enum)) {
                                zval *case_value = zend_enum_fetch_case_value(Z_OBJ_P(val_to_process));
                                if (Z_OBJCE_P(val_to_process)->enum_backing_type == IS_LONG) {
                                    add_assoc_long(retval, ZSTR_VAL(normalized_name), Z_LVAL_P(case_value));
                                } else {
                                    ZEND_ASSERT(Z_OBJCE_P(val_to_process)->enum_backing_type == IS_STRING);
                                    add_assoc_str(retval, ZSTR_VAL(normalized_name), zend_string_copy(Z_STR_P(case_value)));
                                }
                            } else {
                                zval sub_object;
                                ZVAL_UNDEF(&sub_object);
                                normalize_object(val_to_process, context, &sub_object);
                                add_assoc_zval(retval, ZSTR_VAL(normalized_name), &sub_object);
                            }
                            break;
                        }

                        case IS_RESOURCE:
                            add_assoc_string(retval, ZSTR_VAL(normalized_name), "Resource");
                            break;

                        default:
                            // For any other type or UNDEF, add as null
                            add_assoc_null(retval, ZSTR_VAL(normalized_name));
                            break;
                    }

                    zval_ptr_dtor(&getter_value);
                }

                zend_string_release(normalized_name);
                zend_string_release(unmangled_name);
            }
            ZEND_HASH_FOREACH_END();

            // Now process method-based (virtual) properties
            zend_function *func;
            zend_string *normalized_name;
            ZEND_HASH_MAP_FOREACH_PTR(&Z_OBJCE_P(input)->function_table, func) {
                if (zend_string_starts_with_cstr(func->common.function_name, "get", 3) ||
                    zend_string_starts_with_cstr(func->common.function_name, "has", 3) ||
                    zend_string_starts_with_cstr(func->common.function_name, "is", 2)) {
                    normalized_name = get_normalized_name(func->common.function_name, func->common.attributes, TRUE);

                    if ((func->common.fn_flags & ZEND_ACC_PUBLIC) && !(func->common.fn_flags & ZEND_ACC_CTOR) &&
                        must_normalize_function(func->common.attributes, context)) {
                        zval rv;
                        ZVAL_UNDEF(&rv);
                        zend_call_method_if_exists(Z_OBJ_P(input), func->common.function_name, &rv, 0, NULL);

                        if (!EG(exception) && !Z_ISUNDEF(rv)) {
                            add_assoc_zval(retval, ZSTR_VAL(normalized_name), &rv);
                        } else if (EG(exception)) {
                            zend_clear_exception();
                            if (!Z_ISUNDEF(rv)) {
                                zval_ptr_dtor(&rv);
                            }
                        }
                    }
                    zend_string_release(normalized_name);
                }
            }
            ZEND_HASH_FOREACH_END();
        }
    } else {
        // For any other type, convert to a simple value
        switch (Z_TYPE_P(input)) {
            case IS_TRUE:
                ZVAL_TRUE(retval);
                break;
            case IS_FALSE:
                ZVAL_FALSE(retval);
                break;
            case IS_LONG:
                ZVAL_LONG(retval, Z_LVAL_P(input));
                break;
            case IS_DOUBLE:
                ZVAL_DOUBLE(retval, Z_DVAL_P(input));
                break;
            case IS_STRING:
                ZVAL_STR(retval, zend_string_copy(Z_STR_P(input)));
                break;
            case IS_RESOURCE:
                ZVAL_STRING(retval, "Resource");
                break;
            default:
                ZVAL_NULL(retval);
                break;
        }
    }

    // If input is an object, remove from seen set
    if (Z_TYPE_P(input) == IS_OBJECT) {
        zend_hash_index_del(context, Z_OBJ_HANDLE_P(input));
    }

    // Clean up object_properties if needed
    if (object_properties) {
        zend_array_release(object_properties);
    }

    // Restore recursion depth
    if (recursion_depth_zv) {
        ZVAL_LONG(recursion_depth_zv, recursion_depth);
    } else {
        zend_hash_str_del(context, "recursion_depth", strlen("recursion_depth"));
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
        array_init_size(retval, zend_hash_num_elements(Z_ARRVAL_P(input)));

        zend_ulong idx;
        zend_string *key;
        zval *val_from_hash;
        int i = 0;

        // Use safer iteration method
        ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(input), idx, key, val_from_hash) {
            zval r;
            ZVAL_UNDEF(&r);

            denormalize_array(val_from_hash, context, &r, ce, FALSE, TRUE);

            // Add to return array - if key is string use it, otherwise use numeric index
            if (key) {
                add_assoc_zval(retval, ZSTR_VAL(key), &r);
            } else {
                add_index_zval(retval, idx, &r);
            }

            i++;
        } ZEND_HASH_FOREACH_END();
    } else {
        if (Z_TYPE_P(input) == IS_NULL) {
            ZVAL_NULL(retval);
        } else {
            if (do_init) {
                object_init_ex(retval, ce);
                if (ce->constructor) {
                    zend_call_known_instance_method_with_0_params(ce->constructor, Z_OBJ_P(retval), NULL);
                    if (EG(exception)) {
                        zend_clear_exception();
                    }
                }
            }

            zend_string *key;
            zend_ulong num;
            zval *val_from_hash;

            ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(input), num, key, val_from_hash) {
                if (!key) continue; // Skip numeric keys

                zval *val = val_from_hash;
                while (Z_ISREF_P(val)) {
                    val = Z_REFVAL_P(val);
                }

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
                        // Public property, continue with normal handling
                    } else if (property_info->flags & ZEND_ACC_PROTECTED) {
                        // Protected property, use the parent class
                    } else if (property_info->flags & ZEND_ACC_PRIVATE) {
                        property_ce = property_info->ce;
                    }

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
                        zend_update_property_double(property_ce,
                                                    Z_OBJ_P(retval),
                                                    ZSTR_VAL(property_name),
                                                    ZSTR_LEN(property_name),
                                                    Z_DVAL_P(val));
                    } else if (Z_TYPE_P(val) == IS_STRING) {
                        denormalize_string_value(property_name, property_class_name, property_ce, val, retval);
                    } else if (Z_TYPE_P(val) == IS_ARRAY) {
                        denormalize_array_value(property_name, property_class_name, property_ce, val, retval, context);
                    } else if (Z_TYPE_P(val) == IS_NULL) {
                        zend_update_property_null(property_ce,
                                                  Z_OBJ_P(retval),
                                                  ZSTR_VAL(property_name),
                                                  ZSTR_LEN(property_name));
                    } else {
                        // Unsupported type, for safety set to null
                        zend_update_property_null(property_ce,
                                                  Z_OBJ_P(retval),
                                                  ZSTR_VAL(property_name),
                                                  ZSTR_LEN(property_name));
                    }
                }

                zend_string_release(setter_name);
                zend_string_release(property_name);
                if (property_class_name != NULL) {
                    zend_string_release(property_class_name);
                }
            } ZEND_HASH_FOREACH_END();
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
    if (property_class_name) {
        zend_class_entry *sub_ce = zend_lookup_class(property_class_name);
        zval tmp; ZVAL_UNDEF(&tmp);
        if (sub_ce) {
            denormalize_array(val, context, &tmp, sub_ce, FALSE, TRUE);
            zend_update_property(ce, Z_OBJ_P(retval), ZSTR_VAL(property_name), ZSTR_LEN(property_name), &tmp);
        } else {
            zend_update_property_null(ce, Z_OBJ_P(retval), ZSTR_VAL(property_name), ZSTR_LEN(property_name));
        }
        zval_ptr_dtor(&tmp);
    } else {
        Z_TRY_ADDREF_P(val);
        zend_update_property(ce, Z_OBJ_P(retval), ZSTR_VAL(property_name), ZSTR_LEN(property_name), val);
    }
}

void denormalize_string_value(zend_string *property_name,
                              zend_string *property_class_name,
                              zend_class_entry *ce,
                              zval *val,
                              zval *retval)
{
    if (property_class_name) {
        zend_class_entry *sub_ce = zend_lookup_class(property_class_name);
        if (sub_ce) {
            if (zend_class_implements_interface(sub_ce, zend_ce_backed_enum)) {
                zval enum_value; ZVAL_UNDEF(&enum_value);
                zval case_value; ZVAL_UNDEF(&case_value);

                ZVAL_STR_COPY(&enum_value, Z_STR_P(val));

                zend_function *func = zend_hash_str_find_ptr(&sub_ce->function_table, "tryfrom", strlen("tryFrom"));
                if (func != NULL) {
                    zend_call_known_function(func, NULL, sub_ce, &case_value, 1, &enum_value, NULL);
                    if (EG(exception)) {
                        zend_clear_exception();
                        zend_value_error("Failed to create enum %s from value \"%s\"", ZSTR_VAL(sub_ce->name), Z_STRVAL(enum_value));
                    } else if (Z_TYPE(case_value) == IS_OBJECT && instanceof_function(Z_OBJCE(case_value), sub_ce)) {
                        zend_update_property(ce, Z_OBJ_P(retval), ZSTR_VAL(property_name), ZSTR_LEN(property_name), &case_value);
                    } else {
                        zend_value_error("\"%s\" is not a valid backing value for enum \"%s\"",
                                         Z_STRVAL(enum_value), ZSTR_VAL(sub_ce->name));
                    }
                } else {
                    zend_value_error("Enum %s does not have a tryFrom method.", ZSTR_VAL(sub_ce->name));
                }
                zval_ptr_dtor(&enum_value);
                zval_ptr_dtor(&case_value);
            } else if (zend_class_implements_interface(sub_ce, php_date_get_interface_ce())) {
                zval date_value; ZVAL_UNDEF(&date_value);
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
        if (sub_ce && zend_class_implements_interface(sub_ce, zend_ce_backed_enum)) {
            zval enum_value; ZVAL_UNDEF(&enum_value);
            zval case_value; ZVAL_UNDEF(&case_value);
            ZVAL_LONG(&enum_value, Z_LVAL_P(val));

            zend_function *func = zend_hash_str_find_ptr(&sub_ce->function_table, "tryfrom", strlen("tryfrom"));
            if (func != NULL) {
                zend_call_known_function(func, NULL, sub_ce, &case_value, 1, &enum_value, NULL);
                if (EG(exception)) {
                    zend_clear_exception();
                    zend_value_error("Failed to create enum %s from value %lld", ZSTR_VAL(sub_ce->name), Z_LVAL(enum_value));
                } else if (Z_TYPE(case_value) == IS_OBJECT && instanceof_function(Z_OBJCE(case_value), sub_ce)) {
                    zend_update_property(ce, Z_OBJ_P(retval), ZSTR_VAL(property_name), ZSTR_LEN(property_name), &case_value);
                } else {
                    zend_value_error("\"%lld\" is not a valid backing value for enum \"%s\"",
                                     Z_LVAL(enum_value), ZSTR_VAL(sub_ce->name));
                }
            } else {
                zend_value_error("Enum %s does not have a tryFrom method.", ZSTR_VAL(sub_ce->name));
            }
            zval_ptr_dtor(&enum_value);
            zval_ptr_dtor(&case_value);
        }
    } else {
        zend_update_property_long(ce, Z_OBJ_P(retval), ZSTR_VAL(property_name), ZSTR_LEN(property_name), Z_LVAL_P(val));
    }
}

/**
 * Given a property name, return the normalized name for normalization defined by SerializedName attribute if defined
 */
zend_string *get_normalized_name(zend_string *property_name, HashTable *attributes, bool is_function)
{
    if (attributes == NULL) {
        return zend_string_copy(property_name);
    }

    zend_attribute *normalized_name_attribute =
        zend_get_attribute_str(attributes, SERIALIZED_NAME_ATTRIBUTE, strlen(SERIALIZED_NAME_ATTRIBUTE));

    if (normalized_name_attribute && normalized_name_attribute->argc > 0) {
        // Return the serialized name from the attribute's first argument
        return zend_string_copy(Z_STR(normalized_name_attribute->args[0].value));
    }

    if (is_function) {
        // For getter methods like "getProperty", "hasProperty", "isProperty", extract the property name
        size_t prefix_len = 0;
        if (zend_string_starts_with_cstr(property_name, "get", 3)) {
            prefix_len = 3;
        } else if (zend_string_starts_with_cstr(property_name, "has", 3)) {
            prefix_len = 3;
        } else if (zend_string_starts_with_cstr(property_name, "is", 2)) {
            prefix_len = 2;
        }

        if (prefix_len > 0) {
            size_t prop_len = ZSTR_LEN(property_name) - prefix_len;
            if (prop_len > 0) {
                zend_string *normalized = zend_string_alloc(prop_len, 0);
                memcpy(ZSTR_VAL(normalized), ZSTR_VAL(property_name) + prefix_len, prop_len);
                // For function-based properties, ensure first letter is capitalized
                ZSTR_VAL(normalized)[0] = zend_toupper_ascii(ZSTR_VAL(normalized)[0]);
                ZSTR_VAL(normalized)[prop_len] = '\0';
                return normalized;
            }
        }
    }

    return zend_string_copy(property_name);
}

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
    size_t new_len = ZSTR_LEN(property_name) + 3;

    zend_string *getter_name = zend_string_alloc(new_len, 0);

    memcpy(ZSTR_VAL(getter_name), "get", 3);
    memcpy(ZSTR_VAL(getter_name) + 3, ZSTR_VAL(property_name), ZSTR_LEN(property_name));

    ZSTR_VAL(getter_name)[3] = toupper(ZSTR_VAL(getter_name)[3]);
    ZSTR_VAL(getter_name)[new_len] = '\0';

    zend_string *key = zend_string_tolower(getter_name);
    if (zend_hash_exists(&parent_ce->function_table, key)) {
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
    return FALSE;
}

void handle_circular_reference(zval *object, zend_array *context, zval *retval)
{
    zend_throw_error(NULL, "Circular reference detected during normalization.");
    ZVAL_NULL(retval);
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
        zend_string *zs_prop_name = zend_string_init_fast(prop_name, strlen(prop_name));
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
}

ZEND_METHOD(ObjectNormalizer, normalize)
{
    zval *obj, *zv;

    zval *context_param = NULL;
    HashTable context;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_ZVAL(obj)
    Z_PARAM_OPTIONAL
    Z_PARAM_ARRAY(context_param)
    ZEND_PARSE_PARAMETERS_END();

    zv = zend_read_property(Z_OBJCE_P(ZEND_THIS), Z_OBJ_P(ZEND_THIS), "options", strlen("options"), 0, NULL);
    zend_hash_init(&context, 0, NULL, ZVAL_PTR_DTOR, 0);

    if (context_param != NULL) {
        zend_hash_merge(&context, Z_ARRVAL_P(context_param), (copy_ctor_func_t)zval_add_ref, 1);
    }

    if (zv && Z_TYPE_P(zv) == IS_ARRAY && Z_ARRVAL_P(zv)->nNumOfElements > 0) {
        Z_TRY_ADDREF_P(zv);
        zend_hash_str_add_new(&context, "options", strlen("options"), zv);
    }

    normalize_object(obj, &context, return_value);

    zend_hash_destroy(&context);
}

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

    ZEND_PARSE_PARAMETERS_START(2, 3)
    Z_PARAM_ZVAL(arr)
    Z_PARAM_STRING(class_name_cstr, class_name_len)
    Z_PARAM_OPTIONAL
    Z_PARAM_ARRAY(context_param)
    ZEND_PARSE_PARAMETERS_END();

    zend_hash_init(&context, 0, NULL, ZVAL_PTR_DTOR, 0);

    if (context_param != NULL) {
        zend_hash_merge(&context, Z_ARRVAL_P(context_param), (copy_ctor_func_t)zval_add_ref, 1);
    }

    if (class_name_len >= 2 && class_name_cstr[class_name_len - 1] == ']' && class_name_cstr[class_name_len - 2] == '[') {
        is_array = TRUE;
        cname = zend_string_init(class_name_cstr, class_name_len - 2, 0);
    } else {
        cname = zend_string_init(class_name_cstr, class_name_len, 0);
    }

    if (!cname) {
        zend_throw_error(NULL, "Failed to initialize class name string.");
        zend_hash_destroy(&context);
        RETURN_THROWS();
    }

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

    static const zend_function_entry object_normalizer_methods[] = {
        PHP_ME(ObjectNormalizer, __construct, arginfo_class_Normalizer_ObjectNormalizer___construct, ZEND_ACC_PUBLIC)
        PHP_ME(ObjectNormalizer, __destruct, arginfo_class_Normalizer_ObjectNormalizer___destruct, ZEND_ACC_PUBLIC)
        PHP_ME(ObjectNormalizer, normalize, arginfo_class_Normalizer_ObjectNormalizer_normalize, ZEND_ACC_PUBLIC)
        PHP_ME(ObjectNormalizer, denormalize, arginfo_class_Normalizer_ObjectNormalizer_denormalize, ZEND_ACC_PUBLIC)
        PHP_FE_END
    };

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
