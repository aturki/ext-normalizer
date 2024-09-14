--TEST--
echo - Can normalize protected and private attributes
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php
class A
{
    #[Normalizer\Expose()]
    private int $number = 0;

    #[Normalizer\Expose()]
    protected bool $flag = true;

    #[Normalizer\Expose()]
    public float $rate = 57.8;

    #[Normalizer\Expose()]
    public string $name = "A";

}

$normalizer = new Normalizer\ObjectNormalizer();
$normalized = $normalizer->normalize(new A());
var_dump($normalized);
?>
--EXPECT--
array(4) {
  ["number"]=>
  int(0)
  ["flag"]=>
  bool(true)
  ["rate"]=>
  float(57.8)
  ["name"]=>
  string(1) "A"
}
