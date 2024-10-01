<?php


require_once(__DIR__ . '/vendor/autoload.php');

ini_set("xdebug.mode", "off");

if (!extension_loaded('normalizer')) {
    echo "Exception normalizer is not loaded.\n";
    echo "use php -dextension=path/to/normalizer.so instead";
    exit;
}



use Doctrine\Common\Collections\ArrayCollection;
use Doctrine\Common\Collections\Collection;
use Normalizer\Expose;
use Normalizer\Groups;
use Normalizer\Ignore;
use Normalizer\ObjectNormalizer as NativeNormalizer;
use Normalizer\SerializedName;
use Symfony\Component\PropertyInfo\Extractor\PhpDocExtractor;
use Symfony\Component\PropertyInfo\Extractor\ReflectionExtractor;
use Symfony\Component\PropertyInfo\PropertyInfoExtractor;
use Symfony\Component\Serializer\Attribute\Groups as SfGroups;
use Symfony\Component\Serializer\Normalizer\ArrayDenormalizer;
use Symfony\Component\Serializer\Normalizer\DateTimeNormalizer;
use Symfony\Component\Serializer\Normalizer\ObjectNormalizer;
use Symfony\Component\Serializer\Serializer;

enum Type: string {
    case REGULAR = 'regular';
    case ADMIN = 'admin';
}


// dd(class_exists(SfGroups::class));
class Address
{
    #[SfGroups(['GROUP_1'])]
    public string $street;
    #[SfGroups(['GROUP_1'])]
    public string  $city;
    #[SfGroups(['GROUP_1'])]
    public string $zip;

    public function __construct($street = '', $city = '', $zip = '')
    {
        $this->street = $street;
        $this->city = $city;
        $this->zip = $zip;
    }
}

class User
{
    #[SerializedName('customer_name')]
    #[Expose()]
    public string $name;

    #[Expose()]
    public string $email;

    #[Ignore()]
    private int $age;

    #[SfGroups(['GROUP_1'])]
    public Address $address;

    // /** @var array<Address> */
    // #[SfGroups(['GROUP_2'])]
    // public array $addresses = [];

    public array $roles = ['ROLE_1', 'ROLE_2'];

    #[Expose()]
    public \DateTime $date;

    public function __construct($name = '', $email = '', $age = 0, $address = new Address())
    {
        $this->name = $name;
        $this->email = $email;
        $this->age = $age;
        $this->address = $address;
        // $this->addresses = [$address];
        $this->date = new \DateTime();
    }

    #[Expose()]
    public function getVirtualProperty()
    {
        return 'virtual';
    }

    public function getAge()
    {
        return $this->age;
    }
}

$extractor = new PropertyInfoExtractor([new ReflectionExtractor()], [new PhpDocExtractor(), new ReflectionExtractor(),]);

$normalizer = new ObjectNormalizer(null, null, null, $extractor);

$arrayDenormalizer = (new ArrayDenormalizer());
$arrayDenormalizer->setDenormalizer($normalizer);
$serializer = new Serializer([$normalizer, $arrayDenormalizer, new DateTimeNormalizer()]);
$normalizer->setSerializer($serializer);


$nativeNormalizer = new NativeNormalizer(['use_symfony_attributes' => true]);
$user = new User(
    "John Doe",
    "john.doe@example.com",
    30,
    new Address("123 Main St", "Anytown", "12345")
);

$users = [];
for ($i = 0; $i < 1; $i++) {
    $users[] = $user;
}


$currentMemory = memory_get_usage();
$sfNormalizationStart = microtime(true);
$normalized = $serializer->normalize($users);
$sfNormalizationEnd = microtime(true);
$sfNormalizationMemory = (memory_get_usage() - $currentMemory) / 1024 / 1024;
// dump("Symfony", $normalized);

$currentMemory = memory_get_usage();
$nativeNormalizationStart = microtime(true);
$normalized = $nativeNormalizer->normalize(
    $users,
    [
        NativeNormalizer::GROUPS => ['GROUP_1', 'GROUP_2'],
        NativeNormalizer::SKIP_NULL_VALUES => true,
        // NativeNormalizer::SKIP_UNINITIALIZED_VALUES => true,
    ]
);
$nativeNormalizationEnd = microtime(true);
$nativeNormalizationMemory = (memory_get_usage() - $currentMemory) / 1024 / 1024;
// dump("Native", $normalized);
$normalized[0]['roles'] = ['admin'];

// foreach ($normalized as &$item) {
//     $item['date'] = "2024-01-01";
// }
$currentMemory = memory_get_usage();
$nativeDenormalizationStart = microtime(true);
$denormalized = $nativeNormalizer->denormalize($normalized, User::class . '[]');
$nativeDenormalizationEnd = microtime(true);
$nativeDenormalizationMemory = (memory_get_usage() - $currentMemory) / 1024 / 1024;
// dump($denormalized);
$currentMemory = memory_get_usage();
$sfDenormalizationStart = microtime(true);
$denormalized = $normalizer->denormalize($normalized, User::class);
$sfDenormalizationEnd = microtime(true);
$sfDenormalizationMemory = (memory_get_usage() - $currentMemory) / 1024 / 1024;

// dump($denormalized);


echo sprintf("Native Normalization(s):     %f, Memory (MB): %f\n", ($nativeNormalizationEnd - $nativeNormalizationStart), $nativeNormalizationMemory);
echo sprintf("Symfony Normalization(s):    %f, Memory (MB): %f\n", ($sfNormalizationEnd - $sfNormalizationStart), $sfNormalizationMemory);
echo "\n\n";
echo sprintf("Native Normalization(s):     %f, Memory (MB): %f\n", ($nativeDenormalizationEnd - $nativeDenormalizationStart), $nativeDenormalizationMemory);
echo sprintf("Symfony Normalization(s):    %f, Memory (MB): %f\n", ($sfDenormalizationEnd - $sfDenormalizationStart), $sfDenormalizationMemory);


// Native Normalization:    0.000023s
// Symfony Normalization:   0.001013s


// Native Denormalization:  0.000002s
// Symfony Denormalization: 0.002940s

// Features
// AbstractNormalizer::OBJECT_TO_POPULATE
// AbstractObjectNormalizer::SKIP_NULL_VALUES
// AbstractObjectNormalizer::SKIP_UNINITIALIZED_VALUES

// #[MaxDepth(2)]
// #[SerializedName('customer_name')]


// DateTimeNormalizer  RFC3339
// DateTimeZoneNormalizer
// DataUriNormalizer
// DateIntervalNormalizer
// BackedEnumNormalizer
