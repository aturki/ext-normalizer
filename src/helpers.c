#include "php.h"
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
