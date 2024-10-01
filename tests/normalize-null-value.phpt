--TEST--
echo - Can normalize null value
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php

$normalizer = new Normalizer\ObjectNormalizer();

$object = $normalizer->normalize(null);
var_dump($object);
?>
--EXPECT--
NULL
