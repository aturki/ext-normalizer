--TEST--
echo - Must handle self-referencing objects with circular reference handler
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php
class Node
{
    private ?Node $parent = null;
    private array $children = [];

    public function __construct(
        private string $name
    ) {
    }

    #[Normalizer\Expose()]
    public function getName(): string
    {
        return $this->name;
    }

    public function setParent(?Node $parent): void
    {
        $this->parent = $parent;
    }

    #[Normalizer\Expose()]
    public function getParent(): ?Node
    {
        return $this->parent;
    }

    public function addChild(Node $child): void
    {
        $this->children[] = $child;
        $child->setParent($this);
    }

    #[Normalizer\Expose()]
    public function getChildren(): array
    {
        return $this->children;
    }
}

// Create a self-referencing scenario: root node is its own parent
$root = new Node('root');
$root->setParent($root); // Self-reference

$normalizer = new Normalizer\ObjectNormalizer();

$context = [
    Normalizer\ObjectNormalizer::CIRCULAR_REFERENCE_HANDLER => function (object $object, ?string $format, array $context): string {
        if ($object instanceof Node) {
            return 'CIRCULAR:' . $object->getName();
        }
        throw new \Exception('Unexpected circular reference: ' . get_debug_type($object));
    },
];

$normalized = $normalizer->normalize($root, $context);
var_dump($normalized);
?>
--EXPECT--
array(3) {
  ["Name"]=>
  string(4) "root"
  ["Parent"]=>
  string(13) "CIRCULAR:root"
  ["Children"]=>
  array(0) {
  }
}
