#ifndef HELPERS_H
#define HELPERS_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "zend.h"


void trim(char *str);
char *extract_template_type(const char *input);
bool check_array_intersection_string(zval *arr1, zval *arr2);
#endif // HELPERS_H