--TEST--
echo - Can denormalize nested objects
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php
class Two
{
    public string $three;
}

class One
{
    public Two $two;
    public string $four;
}
class Dummy
{
  public One $one;
  public string $foo;
  public string $baz;
}

$data = [
    'one' => [
        'two' => [
            'three' => 'foo',
        ],
        'four' => 'quux',
    ],
    'foo' => 'notfoo',
    'baz' => 'baz',
];

$normalizer = new Normalizer\ObjectNormalizer();
$object = $normalizer->denormalize($data, Dummy::class);
var_dump($object);

?>
--EXPECT--
object(Dummy)#2 (3) {
  ["one"]=>
  object(One)#3 (2) {
    ["two"]=>
    object(Two)#4 (1) {
      ["three"]=>
      string(3) "foo"
    }
    ["four"]=>
    string(4) "quux"
  }
  ["foo"]=>
  string(6) "notfoo"
  ["baz"]=>
  string(3) "baz"
}
