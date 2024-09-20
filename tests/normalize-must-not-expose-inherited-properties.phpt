--TEST--
echo - Must not expose inherited private or protected properties
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php
class ParentClass
{
    #[Normalizer\Expose()]
    private int $number = 0;

    #[Normalizer\Expose()]
    protected bool $flag = true;

    #[Normalizer\Expose()]
    public int $exposedNumber = 1;
}
class Child extends ParentClass
{
    #[Normalizer\Expose()]
    public string $name = "A";

}

$normalizer = new Normalizer\ObjectNormalizer();
$normalized = $normalizer->normalize(new Child());
var_dump($normalized);
?>
--EXPECT--
array(4) {
  ["exposedNumber"]=>
  int(1)
  ["name"]=>
  string(1) "A"
}
