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
$object = $normalizer->denormalize(['bar' => 'B', 'baz' => 2], Foo::class);
var_dump($object);
?>
--EXPECT--
object(Foo)#4 (2) {
  ["bar"]=>
  enum(Type::B)
  ["baz"]=>
  enum(Enum::B)
}
