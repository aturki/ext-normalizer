/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: a98eb0badf4a3958bb23a4be2f5d54b3b182ff15 */

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Normalizer_ObjectNormalizer___construct, 0, 0, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Normalizer_ObjectNormalizer___destruct, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Normalizer_ObjectNormalizer_normalize, 0, 1, IS_ARRAY, 0)
	ZEND_ARG_TYPE_MASK(0, object, MAY_BE_OBJECT|MAY_BE_ARRAY|MAY_BE_NULL, NULL)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, context, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Normalizer_ObjectNormalizer_denormalize, 0, 2, IS_OBJECT, 0)
	ZEND_ARG_TYPE_INFO(0, data, IS_ARRAY, 1)
	ZEND_ARG_TYPE_INFO(0, class, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, context, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()
