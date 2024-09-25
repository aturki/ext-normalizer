--TEST--
echo - Can denormalize to an array of objects
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php

class Foo
{
    #[Normalizer\Expose()]
    public string $bar = 'bar';
}

$normalizer = new Normalizer\ObjectNormalizer();
$data = [
  ['bar' => 'bar'],
  ['bar' => 'bar'],
  ['bar' => 'bar'],
  ['bar' => 'bar'],
];
$objects = $normalizer->denormalize($data, Foo::class . '[]');
var_dump($objects);
?>
--EXPECT--
array(4) {
  [0]=>
  object(Foo)#2 (1) {
    ["bar"]=>
    string(3) "bar"
  }
  [1]=>
  object(Foo)#3 (1) {
    ["bar"]=>
    string(3) "bar"
  }
  [2]=>
  object(Foo)#4 (1) {
    ["bar"]=>
    string(3) "bar"
  }
  [3]=>
  object(Foo)#5 (1) {
    ["bar"]=>
    string(3) "bar"
  }
}
