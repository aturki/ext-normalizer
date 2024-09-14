--TEST--
echo - Can normalize simple object structure with basic types
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php
class A
{
    #[Normalizer\Expose()]
    public int $number = 0;

    #[Normalizer\Expose()]
    public bool $flag = true;

    #[Normalizer\Expose()]
    public float $rate = 57.8;

    #[Normalizer\Expose()]
    public string $name = "A";

    #[Normalizer\Expose()]
    public array $arr = [1, 3];

    #[Normalizer\Ignore()]
    public array $ignored = [1, 3];

}

$normalizer = new Normalizer\ObjectNormalizer();
$normalized = $normalizer->normalize(new A());
var_dump($normalized);
?>
--EXPECT--
array(5) {
  ["number"]=>
  int(0)
  ["flag"]=>
  bool(true)
  ["rate"]=>
  float(57.8)
  ["name"]=>
  string(1) "A"
  ["arr"]=>
  array(2) {
    [0]=>
    int(1)
    [1]=>
    int(3)
  }
}
