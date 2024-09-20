--TEST--
echo - Can normalize and denormalize  \stdClass objects
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php

$normalizer = new Normalizer\ObjectNormalizer();
$normalized = $normalizer->normalize(new \stdClass());
var_dump($normalized);

$object = $normalizer->denormalize([], \stdClass::class);
var_dump($object);
?>
--EXPECT--
array(0) {
}
object(stdClass)#2 (0) {
}
