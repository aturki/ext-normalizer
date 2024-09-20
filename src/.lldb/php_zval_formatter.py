import lldb
import sys

# Define zval types for better readability
IS_UNDEF = 0
IS_NULL = 1
IS_FALSE = 2
IS_TRUE = 3
IS_LONG = 4
IS_DOUBLE = 5
IS_STRING = 6
IS_ARRAY = 7
IS_OBJECT = 8
IS_RESOURCE = 9
IS_REFERENCE = 10
IS_CONSTANT_AST = 11

# Mapping from zval type to string representation
zval_type_map = {
    IS_UNDEF: "UNDEF",
    IS_NULL: "NULL",
    IS_FALSE: "FALSE",
    IS_TRUE: "TRUE",
    IS_LONG: "LONG",
    IS_DOUBLE: "DOUBLE",
    IS_STRING: "STRING",
    IS_ARRAY: "ARRAY",
    IS_OBJECT: "OBJECT",
    IS_RESOURCE: "RESOURCE",
    IS_REFERENCE: "REFERENCE",
    IS_CONSTANT_AST: "CONSTANT_AST"
}

def zval_formatter(valobj, internal_dict):
    try:
        # Get the zval type
        type_flag = valobj.GetChildMemberWithName('u1').GetChildMemberWithName('v').GetValueAsUnsigned(0)
        zval_type = zval_type_map.get(type_flag, "UNKNOWN")

        # Handle different types for better display
        if zval_type == "STRING":
            string_data = valobj.GetChildMemberWithName('value').GetChildMemberWithName('str')
            string_val = string_data.GetSummary()
            return f"String({string_val})"
        elif zval_type == "LONG":
            long_data = valobj.GetChildMemberWithName('value').GetChildMemberWithName('lval')
            return f"Long({long_data.GetValue()})"
        elif zval_type == "DOUBLE":
            double_data = valobj.GetChildMemberWithName('value').GetChildMemberWithName('dval')
            return f"Double({double_data.GetValue()})"
        elif zval_type == "FALSE":
            return "Boolean(FALSE)"
        elif zval_type == "TRUE":
            return "Boolean(TRUE)"
        elif zval_type == "NULL":
            return "NULL"
        elif zval_type == "ARRAY":
            return "Array"
        elif zval_type == "OBJECT":
            return "Object"
        else:
            return f"Unknown zval type: {zval_type}"
    except Exception as e:
        return "Error processing zval: {}".format(e)

# Register the zval_formatter function as a custom summary provider
def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('type summary add php_zval -F php_zval_formatter.zval_formatter')
    return
