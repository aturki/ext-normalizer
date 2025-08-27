--TEST--
echo - Must handle conflicting Expose and Ignore attributes correctly
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php
class ConflictingAttributes
{
    #[Normalizer\Expose()]
    #[Normalizer\Ignore()]
    public string $conflicted = 'should_be_ignored';

    #[Normalizer\Expose()]
    public string $exposed = 'should_be_exposed';

    #[Normalizer\Ignore()]
    public string $ignored = 'should_be_ignored';

    public string $neither = 'should_not_appear';

    #[Normalizer\Expose()]
    #[Normalizer\Ignore()]
    public function getConflictedMethod(): string
    {
        return 'conflicted_method_result';
    }

    #[Normalizer\Expose()]
    public function getExposedMethod(): string
    {
        return 'exposed_method_result';
    }

    #[Normalizer\Ignore()]
    public function getIgnoredMethod(): string
    {
        return 'ignored_method_result';
    }

    public function getNeitherMethod(): string
    {
        return 'neither_method_result';
    }
}

class GroupConflicts
{
    #[Normalizer\Expose()]
    #[Normalizer\Groups(['admin'])]
    public string $adminOnly = 'admin_data';

    #[Normalizer\Expose()]
    #[Normalizer\Groups(['user', 'admin'])]
    public string $userAndAdmin = 'shared_data';

    #[Normalizer\Expose()]
    #[Normalizer\Groups(['guest'])]
    public string $guestOnly = 'guest_data';

    #[Normalizer\Expose()]
    public string $noGroup = 'no_group_data';
}

echo "=== Test 1: Conflicting Expose and Ignore (Ignore should win) ===\n";
$normalizer = new Normalizer\ObjectNormalizer();
$obj = new ConflictingAttributes();
$result = $normalizer->normalize($obj);
var_dump($result);

echo "\n=== Test 2: Groups with admin context ===\n";
$groupObj = new GroupConflicts();
$context = [
    Normalizer\ObjectNormalizer::GROUPS => ['admin']
];
$result = $normalizer->normalize($groupObj, $context);
var_dump($result);

echo "\n=== Test 3: Groups with user context ===\n";
$context = [
    Normalizer\ObjectNormalizer::GROUPS => ['user']
];
$result = $normalizer->normalize($groupObj, $context);
var_dump($result);

echo "\n=== Test 4: Groups with no context (should only show non-grouped) ===\n";
$result = $normalizer->normalize($groupObj);
var_dump($result);
?>
--EXPECT--
=== Test 1: Conflicting Expose and Ignore (Ignore should win) ===
array(2) {
  ["exposed"]=>
  string(17) "should_be_exposed"
  ["ExposedMethod"]=>
  string(21) "exposed_method_result"
}

=== Test 2: Groups with admin context ===
array(2) {
  ["adminOnly"]=>
  string(10) "admin_data"
  ["userAndAdmin"]=>
  string(11) "shared_data"
}

=== Test 3: Groups with user context ===
array(1) {
  ["userAndAdmin"]=>
  string(11) "shared_data"
}

=== Test 4: Groups with no context (should only show non-grouped) ===
array(1) {
  ["noGroup"]=>
  string(13) "no_group_data"
}
