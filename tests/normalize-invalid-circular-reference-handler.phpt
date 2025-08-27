--TEST--
echo - Must handle invalid circular reference handler gracefully
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php
class Node
{
    private ?Node $next = null;

    public function __construct(
        private string $name
    ) {
    }

    #[Normalizer\Expose()]
    public function getName(): string
    {
        return $this->name;
    }

    public function setNext(Node $next): void
    {
        $this->next = $next;
    }

    #[Normalizer\Expose()]
    public function getNext(): ?Node
    {
        return $this->next;
    }
}

// Create circular reference
$node1 = new Node('first');
$node2 = new Node('second');
$node1->setNext($node2);
$node2->setNext($node1); // Creates circular reference

$normalizer = new Normalizer\ObjectNormalizer();

echo "=== Test 1: Non-callable handler ===\n";
try {
    $context = [
        Normalizer\ObjectNormalizer::CIRCULAR_REFERENCE_HANDLER => "not_a_function"
    ];
    $result = $normalizer->normalize($node1, $context);
    echo "ERROR: Should have thrown an exception\n";
} catch (Error $e) {
    echo "Caught expected error: " . $e->getMessage() . "\n";
}

echo "\n=== Test 2: Handler that throws exception ===\n";
try {
    $context = [
        Normalizer\ObjectNormalizer::CIRCULAR_REFERENCE_HANDLER => function ($object, $format, $context) {
            throw new \RuntimeException("Handler failed!");
        }
    ];
    $result = $normalizer->normalize($node1, $context);
    echo "ERROR: Should have thrown an exception\n";
} catch (\RuntimeException $e) {
    echo "Caught expected runtime exception: " . $e->getMessage() . "\n";
} catch (Error $e) {
    echo "Caught expected error: " . $e->getMessage() . "\n";
}

echo "\n=== Test 3: No handler provided ===\n";
try {
    $context = []; // No circular reference handler
    $result = $normalizer->normalize($node1, $context);
    echo "ERROR: Should have thrown an exception\n";
} catch (Error $e) {
    echo "Caught expected error: " . $e->getMessage() . "\n";
}

echo "\n=== Test 4: Handler with wrong signature ===\n";
try {
    $context = [
        Normalizer\ObjectNormalizer::CIRCULAR_REFERENCE_HANDLER => function () {
            return "no_params";
        }
    ];
    $result = $normalizer->normalize($node1, $context);
    echo "Result with wrong signature handler: ";
    var_dump($result);
} catch (Error $e) {
    echo "Caught error: " . $e->getMessage() . "\n";
}
?>
--EXPECT--
=== Test 1: Non-callable handler ===
Caught expected error: Circular reference detected during normalization.

=== Test 2: Handler that throws exception ===
Caught expected runtime exception: Handler failed!

=== Test 3: No handler provided ===
Caught expected error: Circular reference detected during normalization.

=== Test 4: Handler with wrong signature ===
Result with wrong signature handler: array(2) {
  ["Name"]=>
  string(5) "first"
  ["Next"]=>
  array(2) {
    ["Name"]=>
    string(6) "second"
    ["Next"]=>
    string(9) "no_params"
  }
}
