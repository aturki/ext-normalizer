#ifndef HELPERS_H
#define HELPERS_H

#include "zend.h"
#include "zend_attributes.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

void trim(char *str);
char *extract_template_type(const char *input);
bool check_array_intersection_string(zval *arr1, zval *arr2);
void print_array(zend_array *arr, int level);
void log_zend_attribute(zend_attribute *attr);
#endif // HELPERS_H
