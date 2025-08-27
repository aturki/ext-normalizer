--TEST--
echo - Must respect Max Depth constraint if ObjectNormalizer::ENABLE_MAX_DEPTH is passed
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php
class Person
{
  #[Normalizer\Expose()]
    public string $name;

    #[Normalizer\MaxDepth(1)]
    #[Normalizer\Expose()]
    public ?self $parent;

    public function __construct(string $name, ?self $parent = null)
    {
        $this->name = $name;
        $this->parent = $parent;
    }
}

$greatGrandmother = new Person('Elizabeth', null);
$grandmother = new Person('Jane', $greatGrandmother);
$mother = new Person('Sophie', $grandmother);
$child = new Person('Joe', $mother);

$normalizer = new Normalizer\ObjectNormalizer();

$context = [Normalizer\ObjectNormalizer::ENABLE_MAX_DEPTH => true];
$normalized = $normalizer->normalize($child, $context);
var_dump($normalized);
?>
--EXPECT--
array(2) {
  ["name"]=>
  string(3) "Joe"
  ["parent"]=>
  array(2) {
    ["name"]=>
    string(6) "Sophie"
    ["parent"]=>
    array(0) {
    }
  }
}
