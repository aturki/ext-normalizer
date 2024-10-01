--TEST--
echo - Denormalize operation must call __construct
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php


class Foo
{
    public string $bar;
    public bool $baz;

    public function __construct()
    {
        $this->baz = true;
    }
}

$normalizer = new Normalizer\ObjectNormalizer();
$object = $normalizer->denormalize(['bar' => 'bar'], Foo::class);
var_dump($object);
?>
--EXPECT--
object(Foo)#2 (2) {
  ["bar"]=>
  string(3) "bar"
  ["baz"]=>
  bool(true)
}
