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


// Function to extract the last type in a template collection
char *extract_template_type(const char *input)
{
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
        *trailing_bracket = '\0'; // Remove the trailing '>'
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

            if (Z_STRVAL(b1.val) == Z_STRVAL(b2.val)) {
                return TRUE;
            }
        }
    }
    return FALSE;
}


void pretty_print_array(zend_array *arr)
{
    zend_ulong num_key;
    zend_string *str_key;
    zval *value;

    ZEND_HASH_FOREACH_KEY_VAL(arr, num_key, str_key, value)
    {
        if (str_key) {
            printf("Key: %s, Value: ", ZSTR_VAL(str_key));
        } else {
            printf("Key: %lu, Value: ", num_key);
        }

        switch (Z_TYPE_P(value)) {
            case IS_NULL:
                printf("NULL\n");
                break;
            case IS_LONG:
                printf("%ld\n", Z_LVAL_P(value));
                break;
            case IS_DOUBLE:
                printf("%f\n", Z_DVAL_P(value));
                break;
            case IS_STRING:
                printf("%s\n", Z_STRVAL_P(value));
                break;
            case IS_ARRAY:
                printf("Array\n");
                break;
            case IS_OBJECT:
                printf("Object\n");
                break;
            case IS_TRUE:
                printf("%s\n""true");
            case IS_FALSE:
                printf("%s\n""false");
                break;
            case IS_RESOURCE:
                printf("Resource\n");
                break;
            default:
                printf("Unknown\n");
                break;
        }
    }
    ZEND_HASH_FOREACH_END();
}
