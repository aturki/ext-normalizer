--TEST--
echo - Can use Symfony Attributes and Annotations
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
}

class A
{
    #[Symfony\Component\Serializer\Attribute\Groups(['GROUP_1'])]
    public B $embedded;

    #[Symfony\Component\Serializer\Attribute\Groups(['GROUP_2'])]
    public bool $flag = true;

    public function __construct()
    {
      $this->embedded = new B();
    }
}

$normalizer = new Normalizer\ObjectNormalizer(['use_symfony_attributes' => true]);
$a = new A();
$normalized = $normalizer->normalize($a, [Normalizer\ObjectNormalizer::GROUPS => ['GROUP_1']]);
var_dump($normalized);
?>
--EXPECT--
array(1) {
  ["embedded"]=>
  array(2) {
    ["foo"]=>
    string(3) "foo"
    ["bar"]=>
    string(3) "bar"
  }
}
