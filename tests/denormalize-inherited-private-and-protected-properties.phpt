--TEST--
echo - Can denormalize inherited private and protected properties
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php
class ParentClass
{
    public string $public;
    protected string $protected;
    private string $private;

    public function __construct(string $public, string $protected, string $private)
    {
      $this->public = $public;
      $this->protected = $protected;
      $this->private = $private;
    }
}

class ChildClass extends ParentClass
{
}

$data = [
    'public' => 'public',
    'protected' => 'protected',
    'private' => 'private',
];

$normalizer = new Normalizer\ObjectNormalizer();
$object = $normalizer->denormalize($data, ChildClass::class);
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
