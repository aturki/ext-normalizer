#include "php.h"
#include "ext/standard/php_var.h"
#include "zend_attributes.h"
#include "helpers.h"

void trim(char *str)
{
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

char *extract_template_type(const char *input)
{
    static char result[100];
    const char *last_comma = strrchr(input, ',');
    const char *open_bracket = strrchr(input, '<');

    // If there is a comma, extract the type after the last comma
    if (last_comma) {
        strcpy(result, last_comma + 1);
    }
    // Otherwise, extract the type after the last angle bracket
    else if (open_bracket) {
        strcpy(result, open_bracket + 1);
    } else {
        strcpy(result, input);  // No special characters found, return the input
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

bool check_array_intersection_string(zval *arr1, zval *arr2)
{
    // php_var_dump(arr1, 10);
    // php_var_dump(arr2, 40);
    for (int i = 0; i < Z_ARRVAL_P(arr1)->nNumOfElements; i++) {
        Bucket b1 = Z_ARRVAL_P(arr1)->arData[i];
        for (int j = 0; j < Z_ARRVAL_P(arr2)->nNumOfElements; j++) {
            Bucket b2 = Z_ARRVAL_P(arr2)->arData[j];
            if (zend_string_equals(b1.val.value.str, b2.val.value.str)) {
                return TRUE;
            }
        }
    }

    return FALSE;
}


void print_array(zend_array* arr)
{
    for (int i = 0; i < arr->nNumOfElements; ++i)
    {
        Bucket b = arr->arData[i];

        php_printf("{%s: %s}\n", ZSTR_VAL(b.key), ZSTR_VAL(b.val.value.str));
    }
}


void log_zend_attribute(zend_attribute *attr) {
    // Check if the attribute is valid
    if (!attr) {
        php_printf("zend_attribute is NULL\n");
        return;
    }

    // Log the attribute name
    if (attr->name) {
        php_printf("Attribute Name: %s\n", ZSTR_VAL(attr->name));
    } else {
        php_printf("Attribute Name is NULL\n");
    }

    // Log the lowercase name (if available)
    if (attr->lcname) {
        php_printf("Attribute Lowercase Name: %s\n", ZSTR_VAL(attr->lcname));
    } else {
        php_printf("Attribute Lowercase Name is NULL\n");
    }

    // Log the number of arguments
    php_printf("Argument Count: %u\n", attr->argc);

    // Log each argument
    for (uint32_t i = 0; i < attr->argc; i++) {
        zval *arg = &attr->args[i].value;

        php_printf("Argument %u: ", i);
        switch (Z_TYPE_P(arg)) {
            case IS_STRING:
                php_printf("String: %s\n", Z_STRVAL_P(arg));
                break;
            case IS_LONG:
                php_printf("Long: %ld\n", Z_LVAL_P(arg));
                break;
            case IS_DOUBLE:
                php_printf("Double: %f\n", Z_DVAL_P(arg));
                break;
            case IS_TRUE:
            case IS_FALSE:
                php_printf("Boolean: %s\n", Z_TYPE_P(arg) == IS_TRUE ? "true" : "false");
                break;
            default:
                php_printf("Unknown Type (%d)\n", Z_TYPE_P(arg));
                break;
        }
    }

    // Log the flags
    php_printf("Flags: %u\n", attr->flags);

    // Log the offset (used for property attributes)
    php_printf("Offset: %u\n", attr->offset);

    // Separator for readability
    php_printf("-----------------\n");
}
