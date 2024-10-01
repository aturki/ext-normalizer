--TEST--
echo - Can normalize an object with an array of sub objects
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php

class B
{
  #[Symfony\Component\Serializer\Annotation\Groups(['GROUP_1'])]
  public string $foo = 'foo';

  #[Symfony\Component\Serializer\Attribute\Groups(['GROUP_1'])]
  public string $bar = 'bar';

  public function __construct(string $foo, string $bar)
  {
    $this->foo = $foo;
    $this->bar = $bar;
  }
}

class A
{
    #[Symfony\Component\Serializer\Attribute\Groups(['GROUP_1'])]
    public array $bs;


    public function __construct()
    {
      $this->bs = [
        new B('foo1', 'bar1'),
        new B('foo2', 'bar2'),
        new B('foo3', 'bar3'),
      ];
    }
}

$normalizer = new Normalizer\ObjectNormalizer(['use_symfony_attributes' => true]);
$a = new A();
$normalized = $normalizer->normalize($a, [Normalizer\ObjectNormalizer::GROUPS => ['GROUP_1']]);
var_dump($normalized);
?>
--EXPECT--
array(1) {
  ["bs"]=>
  array(3) {
    [0]=>
    array(2) {
      ["foo"]=>
      string(4) "foo1"
      ["bar"]=>
      string(4) "bar1"
    }
    [1]=>
    array(2) {
      ["foo"]=>
      string(4) "foo2"
      ["bar"]=>
      string(4) "bar2"
    }
    [2]=>
    array(2) {
      ["foo"]=>
      string(4) "foo3"
      ["bar"]=>
      string(4) "bar3"
    }
  }
}
