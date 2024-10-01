--TEST--
echo - Can normalize virtual properties
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php

class Foo
{
    #[Normalizer\Expose()]
    public string $bar = 'bar';

    #[Normalizer\Expose()]
    public function getBaz(): string
    {
      return "baz";
    }

    public function getQux(): string
    {
      return "qux";
    }
}

$normalizer = new Normalizer\ObjectNormalizer();
$foo =  new Foo();

$normalized = $normalizer->normalize($foo);
var_dump($normalized);
?>
--EXPECT--
array(2) {
  ["bar"]=>
  string(3) "bar"
  ["Baz"]=>
  string(3) "baz"
}
