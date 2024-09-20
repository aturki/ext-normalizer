--TEST--
echo - Groups attribute applicability
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php
use Normalizer\Groups;

#[Groups]
class Dummy
{
  public string $foo;
  public string $baz;
}

$data = [
    'foo' => 'notfoo',
    'baz' => 'baz',
];

$normalizer = new Normalizer\ObjectNormalizer();
$object = $normalizer->denormalize($data, Dummy::class);
var_dump($object);

?>
--EXPECTF--
Fatal error: Attribute "Normalizer\Groups" cannot target class (allowed targets: method, property) in %sgroups-attribute-applicability.php on line %d
