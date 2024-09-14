--TEST--
echo - Must ignore null values if ObjectNormalizer::SKIP_NULL_VALUES is passed
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php
class A
{
    #[Normalizer\Expose()]
    public int $number = 0;

    #[Normalizer\Expose()]
    public ?string $name = null;

}

$normalizer = new Normalizer\ObjectNormalizer();
$normalized = $normalizer->normalize(new A(), [Normalizer\ObjectNormalizer::SKIP_NULL_VALUES => true]);
var_dump($normalized);
?>
--EXPECT--
array(1) {
  ["number"]=>
  int(0)
}
