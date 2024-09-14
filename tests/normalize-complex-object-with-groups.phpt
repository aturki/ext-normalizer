--TEST--
echo - Can normalize simple object structure with groups
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php
class B
{
  #[Normalizer\Groups(['GROUP_1'])]
  public string $foo = 'foo';

  #[Normalizer\Groups(['GROUP_2'])]
  public string $bar = 'bar';
}

class A
{
    #[Normalizer\Groups(['GROUP_1'])]
    public B $embedded;

    #[Normalizer\Groups(['GROUP_2'])]
    public bool $flag = true;

    public function __construct()
    {
      $this->embedded = new B();
    }
}

$normalizer = new Normalizer\ObjectNormalizer();
$a = new A();
$normalized = $normalizer->normalize($a, [Normalizer\ObjectNormalizer::GROUPS => ['GROUP_1']]);
var_dump($normalized);
?>
--EXPECT--
array(1) {
  ["embedded"]=>
  array(1) {
    ["foo"]=>
    string(3) "foo"
  }
}
