--TEST--
echo - Can normalize and denormalize BackedEnums
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php

enum Enum: int
{
    case A = 1;
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
$object = $normalizer->denormalize($normalized, Foo::class);
var_dump($object);
?>
--EXPECT--
array(1) {
  ["bar"]=>
  string(1) "A"
}
object(Foo)#3 (1) {
  ["bar"]=>
  enum(Type::A)
}
