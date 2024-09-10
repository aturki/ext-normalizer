#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_var.h"
#include "zend.h"
#include "zend_attributes.h"
#include "zend_interfaces.h"
#include "php_normalizer.h"

#define NAMESPACE "ObjectNormalizer"

#define IGNORE_ATTRIBUTE "normalizer\\ignore"
#define EXPOSE_ATTRIBUTE "normalizer\\expose"
#define GROUPS_ATTRIBUTE "normalizer\\groups"

#define GROUPS_CONST_NAME "GROUPS"
#define GROUPS_CONST_VALUE "groups"


static zend_class_entry *ignore_attribute_class_entry = NULL;
static zend_class_entry *expose_attribute_class_entry = NULL;
static zend_class_entry *groups_attribute_class_entry = NULL;
static zend_class_entry *object_normalizer_class_entry = NULL;

// Forward Declarations
void normalize_object(zval *obj, zval *context, zval *retval);
void denormalize_object_ex(zval *input, zval *obj, zend_class_entry *ce);

bool check_array_intersection_string(zval* arr1, zval *arr2) {
    for (int i = 0; i < Z_ARRVAL_P(arr1)->nNumOfElements; i++) {
        Bucket b1 = Z_ARRVAL_P(arr1)->arData[i];
        for (int j = 0; j < Z_ARRVAL_P(arr2)->nNumOfElements; j++) {
            Bucket b2 = Z_ARRVAL_P(arr2)->arData[j];

            if (Z_STRVAL(b1.val) == Z_STRVAL(b2.val)) {
                return TRUE;
            }
        }
    }
    return FALSE;
}


// Function to trim leading and trailing spaces
void trim(char *str) {
    int len = strlen(str);

    // Trim leading spaces
    while (isspace((unsigned char)str[0])) {
        memmove(str, str + 1, len--);
    }

    // Trim trailing spaces
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }
}

// Function to extract the last type in a template collection
char* extract_template_type(const char* input) {
    static char result[100];
    const char *last_comma = strrchr(input, ',');
    const char *open_bracket = strrchr(input, '<');
    const char *close_bracket = strrchr(input, '>');

    // If there is a comma, extract the type after the last comma
    if (last_comma) {
        strcpy(result, last_comma + 1);
    }
    // Otherwise, extract the type after the last angle bracket
    else if (open_bracket) {
        strcpy(result, open_bracket + 1);
    } else {
        strcpy(result, input); // No special characters found, return the input
    }

    // Trim trailing '>' if it exists
    char *trailing_bracket = strrchr(result, '>');
    if (trailing_bracket) {
        *trailing_bracket = '\0';  // Remove the trailing '>'
    }

    // Trim any leading or trailing spaces
    trim(result);

    return result;
}

bool must_normalize(HashTable *attributes, zend_array *context)
{
    bool normalize = FALSE;
    zend_attribute *ignore_attribute = zend_get_attribute_str(
        attributes,
        IGNORE_ATTRIBUTE,
        sizeof(IGNORE_ATTRIBUTE) - 1);

    zend_attribute *expose_attribute = zend_get_attribute_str(
        attributes,
        EXPOSE_ATTRIBUTE,
        sizeof(EXPOSE_ATTRIBUTE) - 1);

    zend_attribute *groups_attribute = zend_get_attribute_str(
        attributes,
        GROUPS_ATTRIBUTE,
        sizeof(GROUPS_ATTRIBUTE) - 1);

    if (ignore_attribute)
    {
        normalize = FALSE;
    } else if (expose_attribute) {
        normalize = TRUE;
    } else if (groups_attribute) {
        zend_attribute_arg p = groups_attribute->args[0];
        zval *requested_groups = zend_hash_str_find(context, "groups", sizeof("groups")-1);
        normalize = check_array_intersection_string(&p.value, requested_groups);

    } else {
        normalize = FALSE;
    }

    return normalize;
}

void normalize_object(zval *input, zval* context, zval *retval)
{
    HashTable *object_properties;

    object_properties = zend_get_properties_for(input, ZEND_PROP_PURPOSE_DEBUG);
    zend_class_entry *ce = Z_OBJCE_P(input);
    zend_object *zobj = Z_OBJ_P(input);
    array_init(retval);

    if (object_properties)
    {
        zend_ulong num;
        zend_string *key;
        zval *val;
        zval normalized_sub_object, tmp;

        ZEND_HASH_FOREACH_KEY_VAL(object_properties, num, key, val)
        {
            zval rv;
            bool normalize = FALSE;
            zval *property = zend_read_property(ce, Z_OBJ_P(input), ZSTR_VAL(key), ZSTR_LEN(key), 1, &rv);
            zend_property_info *property_info = NULL;

            if (Z_TYPE_P(val) == IS_INDIRECT)
            {
                val = Z_INDIRECT_P(val);
                property_info = zend_get_typed_property_info_for_slot(Z_OBJ_P(input), val);
            }
            if (!Z_ISUNDEF_P(val) || property_info)
            {
                const char *prop_name, *class_name;
                /*int unmangle = */ zend_unmangle_property_name(key, &class_name, &prop_name);
                const char *unmangled_name_cstr = zend_get_unmangled_property_name(prop_name);
                zend_string *unmangled_name = zend_string_init(unmangled_name_cstr, strlen(unmangled_name_cstr), false);

                if (property_info && property_info->attributes) {
                    normalize = must_normalize(property_info->attributes, Z_ARRVAL_P(context));
                } else {
                    normalize = FALSE;
                }

                if (normalize) {

                try_again:
                    switch (Z_TYPE_P(val))
                    {
                    case IS_NULL:
                        add_assoc_null(retval, prop_name);
                        break;
                    case IS_TRUE:
                        add_assoc_bool(retval, prop_name, TRUE);
                        break;
                    case IS_FALSE:
                        add_assoc_bool(retval, prop_name, FALSE);
                        break;
                    case IS_LONG:
                        add_assoc_long(retval, prop_name, Z_LVAL_P(val));
                        break;
                    case IS_DOUBLE:
                        add_assoc_double(retval, prop_name, Z_DVAL_P(val));
                        break;
                    case IS_STRING:
                        add_assoc_string(retval, prop_name, Z_STRVAL_P(val));
                        break;
                    case IS_RESOURCE:
                        break;
                    case IS_ARRAY:
                        add_assoc_array(retval, prop_name, Z_ARRVAL_P(val));
                        break;
                    case IS_OBJECT:
                        ZVAL_COPY_VALUE(&tmp, val);
                        normalize_object(&tmp, context, &normalized_sub_object);
                        add_assoc_array(retval, prop_name, Z_ARR_P(&normalized_sub_object));
                        break;
                    case IS_REFERENCE:
                        // For references, remove the reference wrapper and try again.
                        // Yes, you are allowed to use goto for this purpose!
                        val = Z_REFVAL_P(val);
                        goto try_again;
                        EMPTY_SWITCH_DEFAULT_CASE() // Assert that all types are handled.
                    }
                }
            }
        }
        ZEND_HASH_FOREACH_END();
        zend_release_properties(object_properties);
    }
}

void denormalize_object_ex(zval *input, zval *retval, zend_class_entry *ce)
{
    object_init_ex(retval, ce);
    zend_string *key;
    zend_ulong num;
    zval *val;

    ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(input), key, val)
    {
        if (Z_TYPE_P(val) == IS_ARRAY)
        {
            zend_property_info *property_info;
            zend_string *class_name;
            if ((property_info = zend_hash_find_ptr(&ce->properties_info, key)) != NULL)
            {
                // Check if the property has a type hint and if it is a class type
                if (ZEND_TYPE_IS_SET(property_info->type))
                {
                    if (ZEND_TYPE_HAS_NAME(property_info->type))
                    {
                        zval sub_object_return;
                        class_name = ZEND_TYPE_NAME(property_info->type);

                        zend_class_entry *collection_ce = zend_lookup_class(class_name);
                        if (zend_class_implements_interface(collection_ce, zend_ce_arrayaccess)) {

                            zval* collection;
                            zval offset_set_fn;
                            zval params[2];
                            char *raw_template_class_name = extract_template_type(ZSTR_VAL(property_info->doc_comment));
                            zend_string *template_class_name = zend_string_init(raw_template_class_name, sizeof(raw_template_class_name)-1, 0);
                            zend_class_entry *template_ce = zend_lookup_class(template_class_name);
                            object_init_ex(collection, collection_ce);
                            zend_call_known_instance_method_with_0_params(
                                collection_ce->constructor,
                                collection,
                                NULL
                            );

                            ZVAL_STRING(&offset_set_fn, "offsetSet");
                            for (int i = 0; i < Z_ARRVAL_P(val)->nNumOfElements; i++) {
                                zval item;
                                Bucket p = Z_ARRVAL_P(val)->arData[i];
                                denormalize_object_ex(&p.val, &item, template_ce);

                                // Prepare the parameters for the offsetSet function call
                                ZVAL_LONG(&params[0], i);
                                ZVAL_COPY(&params[1], &item);
                                zval r;
                                zend_function *fn = collection_ce->arrayaccess_funcs_ptr->zf_offsetset;
                                zend_call_known_instance_method_with_2_params(
                                    fn,
                                    collection,
                                    // collection_ce,
                                    // "offsetset",
                                    NULL,
                                    &params[0],
                                    &params[1]
                                );
                            }

                            // zval sub_object_return;
                            // denormalize_object_ex(val, &sub_object_return, collection_ce);
                            // object_init_ex(&sub_object_return, collection_ce);

                        } else {
                            denormalize_object_ex(val, &sub_object_return, collection_ce);
                        }
                        zend_update_property(ce, Z_OBJ_P(retval), ZSTR_VAL(key), ZSTR_LEN(key), &sub_object_return);
                    } else {
                        php_printf("\tType has no name for %s\n", ZSTR_VAL(key));
                    }
                }
                else
                {
                    zend_update_property(ce, Z_OBJ_P(retval), ZSTR_VAL(key), ZSTR_LEN(key), val);
                }
            } else {
                // php_error_docref(NULL, E_WARNING, "Property '%s' not found", prop_name);
                // RETURN_NULL();
            }
        }
        else
        {
            switch (Z_TYPE_P(val))
            {
            case IS_TRUE:
                zend_update_property_bool(ce, Z_OBJ_P(retval), ZSTR_VAL(key), ZSTR_LEN(key), TRUE);
                break;
            case IS_FALSE:
                zend_update_property_bool(ce, Z_OBJ_P(retval), ZSTR_VAL(key), ZSTR_LEN(key), FALSE);
                break;
            case IS_LONG:
                zend_update_property_long(ce, Z_OBJ_P(retval), ZSTR_VAL(key), ZSTR_LEN(key), Z_LVAL_P(val));
                break;
            case IS_DOUBLE:
                zend_update_property_double(ce, Z_OBJ_P(retval), ZSTR_VAL(key), ZSTR_LEN(key), Z_DVAL_P(val));
                break;
            case IS_STRING:
                zend_update_property_string(ce, Z_OBJ_P(retval), ZSTR_VAL(key), ZSTR_LEN(key), Z_STRVAL_P(val));
                break;
            default:
                break;
            }
        }
    }
    ZEND_HASH_FOREACH_END();
}

/********************************* ATTRIBUTES *******************************************************/
ZEND_METHOD(Ignore, __construct)
{
    ZEND_PARSE_PARAMETERS_NONE();
}

ZEND_METHOD(Expose, __construct)
{
    ZEND_PARSE_PARAMETERS_NONE();
}

ZEND_METHOD(MaxDepth, __construct)
{
    ZEND_PARSE_PARAMETERS_NONE();
}

ZEND_METHOD(Groups, __construct)
{
    HashTable *scope;
    ZEND_PARSE_PARAMETERS_START(0, 2)
    Z_PARAM_ARRAY_HT(scope)
    ZEND_PARSE_PARAMETERS_END();

    zend_update_property_ex(groups_attribute_class_entry, Z_OBJ_P(ZEND_THIS), zend_string_init("scope", sizeof("scope")-1, 0), scope);
}

/********************************* ATTRIBUTES *******************************************************/

/********************************* ObjectNormalizer *******************************************************/

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

    normalize_object(obj, context, return_value); // Pass return_value to store result
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

    ZEND_PARSE_PARAMETERS_START(2, 3)
    Z_PARAM_ZVAL(arr)
    Z_PARAM_STRING(className, className_len)
    Z_PARAM_OPTIONAL
    Z_PARAM_ARRAY_OR_NULL(context)
    ZEND_PARSE_PARAMETERS_END();

    cname = zend_string_init(className, strlen(className), 0);
    ce = zend_lookup_class(cname);

    denormalize_object_ex(arr, return_value, ce);
}
/********************************* ObjectNormalizer *******************************************************/


zend_module_entry normalizer_module_entry = {
    STANDARD_MODULE_HEADER,
    "normalizer",
    NULL, // Register functions
    PHP_MINIT(normalizer),
    PHP_MSHUTDOWN(normalizer),
    PHP_RINIT(normalizer),     // Request init
    PHP_RSHUTDOWN(normalizer), // Request shutdown
    PHP_MINFO(normalizer),
    PHP_NORMALIZER_VERSION,
    STANDARD_MODULE_PROPERTIES};

#ifdef COMPILE_DL_NORMALIZER
ZEND_GET_MODULE(normalizer)
#endif


/* REGISTER Expose Attribute */

ZEND_BEGIN_ARG_INFO(arginfo_expose_attribute_construct, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry expose_attribute_functions[] = {
    PHP_ME(Expose, __construct, arginfo_expose_attribute_construct, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* REGISTER Expose Attribute */

/* REGISTER Ignore Attribute */

ZEND_BEGIN_ARG_INFO(arginfo_ignore_attribute_construct, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry ignore_attribute_functions[] = {
    PHP_ME(Ignore, __construct, arginfo_ignore_attribute_construct, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* REGISTER Ignore Attribute */

/* REGISTER Ignore Attribute */

ZEND_BEGIN_ARG_INFO(arginfo_groups_attribute_construct, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry groups_attribute_functions[] = {
    PHP_ME(Ignore, __construct, arginfo_groups_attribute_construct, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* REGISTER Ignore Attribute */

/* REGISTER ObjectNormalizer class */

ZEND_BEGIN_ARG_INFO(arginfo_object_normalizer_construct, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_object_normalizer_normalize, 0, 0, 2)
ZEND_ARG_TYPE_INFO(0, obj, IS_MIXED, 0)
ZEND_ARG_TYPE_INFO(0, obj, IS_ARRAY, 1)
ZEND_END_ARG_INFO()

// Arginfo for denormalize_object()
ZEND_BEGIN_ARG_INFO_EX(arginfo_object_normalizer_denormalize, 0, 0, 2)
ZEND_ARG_TYPE_INFO(0, arr, IS_MIXED, 0)
ZEND_ARG_TYPE_INFO(0, className, IS_STRING, 0)
ZEND_END_ARG_INFO()


static const zend_function_entry object_normalizer_functions[] = {
    PHP_ME(ObjectNormalizer, __construct, arginfo_object_normalizer_construct, ZEND_ACC_PUBLIC)
    PHP_ME(ObjectNormalizer, normalize, arginfo_object_normalizer_normalize, ZEND_ACC_PUBLIC)
    PHP_ME(ObjectNormalizer, denormalize, arginfo_object_normalizer_denormalize, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void init_object_normalizer_class()
{
    zend_class_entry object_normalizer_ce;

    INIT_CLASS_ENTRY(object_normalizer_ce, "Normalizer\\ObjectNormalizer", object_normalizer_functions);
    object_normalizer_class_entry = zend_register_internal_class(&object_normalizer_ce);


    zval const_GROUPS_value;
	zend_string *const_GROUPS_value_str = zend_string_init("groups", strlen("groups"), 1);
	ZVAL_STR(&const_GROUPS_value, const_GROUPS_value_str);
	zend_string *const_GROUPS_name = zend_string_init("GROUPS", strlen("GROUPS"), 1);

	zend_declare_typed_class_constant(
        object_normalizer_class_entry,
        const_GROUPS_name,
        &const_GROUPS_value,
        ZEND_ACC_PUBLIC,
        NULL,
        (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_STRING)
    );

	zend_string_release(const_GROUPS_name);
}

/* REGISTER ObjectNormalizer class */
PHP_MINIT_FUNCTION(normalizer)
{
    zend_class_entry expose_ce, ignore_ce, groups_ce;
    INIT_CLASS_ENTRY(expose_ce, "Normalizer\\Expose", expose_attribute_functions);
    expose_attribute_class_entry = zend_register_internal_class(&expose_ce);

    INIT_CLASS_ENTRY(ignore_ce, "Normalizer\\Ignore", ignore_attribute_functions);
    ignore_attribute_class_entry = zend_register_internal_class(&ignore_ce);

    INIT_CLASS_ENTRY(groups_ce, "Normalizer\\Groups", groups_attribute_functions);
    groups_attribute_class_entry = zend_register_internal_class(&groups_ce);

    init_object_normalizer_class();

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(normalizer)
{
    return SUCCESS;
}

PHP_RINIT_FUNCTION(normalizer)
{
#if defined(COMPILE_DL_NORMALIZER) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(normalizer)
{
    return SUCCESS;
}

PHP_MINFO_FUNCTION(normalizer)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "normalizer support", "enabled");
    php_info_print_table_header(2, "normalizer version", PHP_NORMALIZER_VERSION);
    php_info_print_table_end();
}
