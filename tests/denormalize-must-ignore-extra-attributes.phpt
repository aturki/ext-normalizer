--TEST--
echo - Denormalize must ignore extra attributes if any
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php


class Foo
{
    public string $bar;
}

$normalizer = new Normalizer\ObjectNormalizer();
$object = $normalizer->denormalize(['bar' => 'bar', 'baz' => 2], Foo::class);
var_dump($object);
?>
--EXPECT--
object(Foo)#2 (1) {
  ["bar"]=>
  string(3) "bar"
}
