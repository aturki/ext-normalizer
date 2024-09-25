--TEST--
echo - Can normalize and denormalize BackedEnums
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php

enum Enum: int
{
    case A = 1;
    case B = 2;
}
enum Type: string
{
    case A = "A";
    case B = "B";
}
class Foo
{
    #[Normalizer\Expose()]
    public Type $bar = Type::A;
    public Enum $baz = Enum::A;
}

$normalizer = new Normalizer\ObjectNormalizer();
$normalized = $normalizer->normalize(new Foo());
var_dump($normalized);
?>
--EXPECT--
array(1) {
  ["bar"]=>
  string(1) "A"
}
