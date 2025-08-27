--TEST--
echo - Must handle circular references when a handler is provided
--SKIPIF--
<?php if (!extension_loaded('normalizer')) die('skip ext/normalizer must be installed'); ?>
--FILE--
<?php
class Organization
{
    public function __construct(
        private string $name,
        private array $members = []
    ) {
    }

    #[Normalizer\Expose()]
    public function getName(): string
    {
        return $this->name;
    }

    public function addMember(Member $member): void
    {
        $this->members[] = $member;
    }

    #[Normalizer\Expose()]
    public function getMembers(): array
    {
        return $this->members;
    }
}

class Member
{
    private Organization $organization;

    public function __construct(
        private string $name
    ) {
    }

    #[Normalizer\Expose()]
    public function getName(): string
    {
        return $this->name;
    }

    public function setOrganization(Organization $organization): void
    {
        $this->organization = $organization;
    }

    #[Normalizer\Expose()]
    public function getOrganization(): Organization
    {
        return $this->organization;
    }
}


$organization = new Organization('Acme Org');
$member = new Member('John');

$organization->addMember($member);
$member->setOrganization($organization);

$normalizer = new Normalizer\ObjectNormalizer();

$context = [
    Normalizer\ObjectNormalizer::CIRCULAR_REFERENCE_HANDLER => function (object $object, ?string $format, array $context): string {
        if (!$object instanceof Organization) {
            throw new \Exception('A circular reference has been detected when serializing the object of class "'.get_debug_type($object).'".');
        }

        // serialize the nested Organization with only the name (and not the members)
        return $object->getName();
    },
];

$normalized = $normalizer->normalize($organization, $context);
var_dump($normalized);
?>
--EXPECT--
array(2) {
  ["Name"]=>
  string(8) "Acme Org"
  ["Members"]=>
  array(1) {
    [0]=>
    array(2) {
      ["Name"]=>
      string(4) "John"
      ["Organization"]=>
      string(8) "Acme Org"
    }
  }
}
