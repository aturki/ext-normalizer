/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 79d68cc0b8f120593aa4e05272f4194a036c33a8 */

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Normalizer_ObjectNormalizer___construct, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Normalizer_ObjectNormalizer_normalize, 0, 1, IS_ARRAY, 0)
	ZEND_ARG_TYPE_MASK(0, object, MAY_BE_OBJECT|MAY_BE_ARRAY, NULL)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, context, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Normalizer_ObjectNormalizer_denormalize, 0, 2, IS_OBJECT, 0)
	ZEND_ARG_TYPE_INFO(0, data, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, class, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, context, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()
