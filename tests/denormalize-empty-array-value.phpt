--TEST--
echo - Can denormalize null value
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php

class Foo {}
$normalizer = new Normalizer\ObjectNormalizer();

$object = $normalizer->denormalize([], Foo::class);
var_dump($object);
?>
--EXPECT--
object(Foo)#2 (0) {
}