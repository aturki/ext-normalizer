--TEST--
echo - When denormalizing a not instantiable object, must throw an error
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php
enum UnitEnumDummy
{
  case A;
}

$normalizer = new Normalizer\ObjectNormalizer();
$normalized = $normalizer->denormalize([], UnitEnumDummy::class);

?>
--EXPECTF--
Fatal error: Uncaught ValueError: Can not instantiate object from enum "UnitEnumDummy". in %s/tests/denormalize-when-object-not-instantiable.php:8
Stack trace:
#0 %sdenormalize-when-object-not-instantiable.php(8): Normalizer\ObjectNormalizer->denormalize(Array, 'UnitEnumDummy')
#1 {main}
  thrown in %sdenormalize-when-object-not-instantiable.php on line 8
