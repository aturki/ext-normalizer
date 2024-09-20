--TEST--
echo - Can normalize and denormalize \DateTime
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php
class Foo
{
  public function __construct(
    #[Normalizer\Expose()] public \DateTime $date
  ) {}
}

$foo = new Foo(new \DateTime('2016/01/01', new \DateTimeZone('UTC')));
$normalizer = new Normalizer\ObjectNormalizer();

$normalized = $normalizer->normalize($foo);
var_dump($normalized)

?>
--EXPECT--
array(1) {
  ["date"]=>
  string(25) "2016-01-01T00:00:00+00:00"
}
