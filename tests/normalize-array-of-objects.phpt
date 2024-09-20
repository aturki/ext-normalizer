--TEST--
echo - Can normalize an array of objects
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
$array = [
  new Foo(),
  new Foo(),
  new Foo(),
  new Foo(),
];
$normalized = $normalizer->normalize($array);
var_dump($normalized);
?>
--EXPECT--
array(4) {
  [0]=>
  array(1) {
    ["bar"]=>
    string(3) "bar"
  }
  [1]=>
  array(1) {
    ["bar"]=>
    string(3) "bar"
  }
  [2]=>
  array(1) {
    ["bar"]=>
    string(3) "bar"
  }
  [3]=>
  array(1) {
    ["bar"]=>
    string(3) "bar"
  }
}
