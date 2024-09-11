<?php


require_once(__DIR__ .'//vendor/autoload.php');


if (!extension_loaded('normalizer')) {
    echo "Exception normalizer is not loaded.\n";
    echo "use php -dextension=path/to/normalizer.so instead";
    exit;
}

ini_set("xdebug.mode", "off");

use Normalizer\Expose;
use Normalizer\Groups;
use Normalizer\Ignore;
use Normalizer\ObjectNormalizer as NativeNormalizer;
use Symfony\Component\PropertyInfo\Extractor\PhpDocExtractor;
use Symfony\Component\PropertyInfo\Extractor\ReflectionExtractor;
use Symfony\Component\PropertyInfo\PropertyInfoExtractor;
use Symfony\Component\Serializer\Normalizer\ArrayDenormalizer;
use Symfony\Component\Serializer\Normalizer\ObjectNormalizer;
use Symfony\Component\Serializer\Serializer;


class Address {
    #[Groups(['GROUP_1'])]
    public string $street;
    #[Groups(['GROUP_1'])]
    public string  $city;
    #[Groups(['GROUP_1'])]
    public string $zip;

    public function __construct($street, $city, $zip) {
        $this->street = $street;
        $this->city = $city;
        $this->zip = $zip;
    }
}

#[Expose()]
class User {
    #[Expose()]
    // #[SerializedName('customer_name')]
    public string $name;
    #[Expose()]
    public string $email;
    #[Ignore()]
    private int $age;
    #[Groups(['GROUP_1'])]
    public Address $address;
    /** @var array<Address> */
    #[Expose()]
    public array $addresses = [];
    public array $roles = ['ROLE_1', 'ROLE_2'];

    public function __construct($name, $email, $age, $address) {
        $this->name = $name;
        $this->email = $email;
        $this->age = $age;
        $this->address = $address;
        $this->addresses = [$address];
    }

    public function getAge() {
        return $this->age;
    }
    public function setAge(int $age) {
        $this->age = $age;
    }

    public function setAddresses(array $addresses) {
        $this->addresses = $addresses;
    }
}

$extractor = new PropertyInfoExtractor([new ReflectionExtractor()], [new PhpDocExtractor(), new ReflectionExtractor(),]);

$normalizer = new ObjectNormalizer(null, null, null, $extractor);

$arrayDenormalizer = (new ArrayDenormalizer());
$arrayDenormalizer->setDenormalizer($normalizer);
$serializer = new Serializer([$normalizer, $arrayDenormalizer]);
$normalizer->setSerializer($serializer);

$nativeNormalizer = new NativeNormalizer();
$user = new User(
    "John Doe",
    "john.doe@example.com",
    30,
    new Address("123 Main St", "Anytown", "12345")
);

$users = [];
for ($i = 0; $i < 10; $i++) {
    $users[] = $user;
}


$sfNormalizationStart = microtime(true);
$normalized = $normalizer->normalize($user);
$sfNormalizationEnd = microtime(true);

$nativeNormalizationStart = microtime(true);
$normalized = $nativeNormalizer->normalize($users, [NativeNormalizer::GROUPS => ['GROUP_1', 'GROUP_2']]);
$nativeNormalizationEnd = microtime(true);
// dump($normalized);


$nativeDenormalizationStart = microtime(true);
$denormalized = $nativeNormalizer->denormalize($normalized, User::class . '[]');
$nativeDenormalizationEnd = microtime(true);
// dump($denormalized);

$sfDenormalizationStart = microtime(true);
// $denormalized = $normalizer->denormalize($normalized, User::class);
$sfDenormalizationEnd = microtime(true);

// dump($denormalized);


echo "Native Normalization:    " . sprintf("%fs", ($nativeNormalizationEnd - $nativeNormalizationStart)) . "\n";
echo "Symfony Normalization:   " . sprintf("%fs", ($sfNormalizationEnd - $sfNormalizationStart)) . "\n";
echo "\n\n";
echo "Native Denormalization:  " . sprintf("%fs", ($nativeDenormalizationEnd - $nativeDenormalizationStart)) . "\n";
echo "Symfony Denormalization: " . sprintf("%fs", ($sfDenormalizationEnd - $sfDenormalizationStart)) . "\n";


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
