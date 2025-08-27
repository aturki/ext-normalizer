# Object (de)normalization for PHP

A PHP language extension that provides an efficient alternative to Symfony's Normalizer.

## Documentation

This extension provides a `Normalizer\ObjectNormalizer` class that can be used to normalize objects into arrays and denormalize arrays back into objects. The normalization process can be controlled using attributes.

## Installation

To install the extension, you can use the following commands:

```bash
phpize
./configure
make
make install
```

## Enabling the extension

You'll need to add `extension=normalizer.so` to your primary *php.ini* file.

```bash
# To see where .ini files are located
php -i | grep "\.ini"
```

---

You can also enable the extension temporarily using the command line:

```bash
php -d extension=normalizer.so
```

## Usage

Here is a basic example of how to use the `ObjectNormalizer`:

```php
<?php

use Normalizer\ObjectNormalizer;
use Normalizer\Expose;
use Normalizer\Groups;
use Normalizer\Ignore;
use Normalizer\MaxDepth;
use Normalizer\SerializedName;

class User
{
    #[Expose]
    #[SerializedName("user_id")]
    public int $id;

    #[Groups(["public"])]
    public string $name;

    #[Ignore]
    public string $password;

    #[Groups(["admin"])]
    public string $email;

    #[MaxDepth(1)]
    public Profile $profile;
}

class Profile
{
    #[Expose]
    public string $site;
}

$user = new User();
$user->id = 1;
$user->name = 'John Doe';
$user->password = 'secret';
$user->email = 'john.doe@example.com';
$user->profile = new Profile();
$user->profile->site = 'example.com';

$normalizer = new ObjectNormalizer();

// Normalize the object
$data = $normalizer->normalize($user, [ObjectNormalizer::GROUPS => ['public']]);

// $data will be:
// [
//     'user_id' => 1,
//     'name' => 'John Doe',
//     'profile' => [],
// ]

// Denormalize the array back to an object
$newUser = $normalizer->denormalize($data, User::class);

```

## Features

The normalization process can be controlled with the following attributes:

### `#[Expose]`

By default, all properties are ignored. The `#[Expose]` attribute marks a property to be included in the normalization.

### `#[Ignore]`

The `#[Ignore]` attribute marks a property to be excluded from the normalization.

### `#[Groups]`

The `#[Groups]` attribute allows you to conditionally include properties based on normalization groups. A property with a `#[Groups]` attribute will only be included if the group is passed to the `normalize` method.

```php
class MyObj
{
    #[Groups(['group1', 'group2'])]
    public string $foo;

    #[Groups(['group4'])]
    public string $anotherProperty;

    #[Groups(['group3'])]
    public function getBar() // is* and has* methods are also supported
    {
        return $this->bar;
    }
}

$normalizer = new Normalizer\ObjectNormalizer();

$obj = new MyObj();
$obj->foo = 'foo';
$obj->anotherProperty = 'property';

$array = $normalizer->normalize($obj, [Normalizer\ObjectNormalizer::GROUPS => ['group1', 'group4']]);

// Will return ['foo' => 'foo', 'anotherProperty' => 'property]
```

### `#[MaxDepth]`

The `#[MaxDepth]` attribute prevents the normalizer from going too deep into the object graph. When the max depth is reached, the normalizer will return an empty array.

### `#[SerializedName]`

The `#[SerializedName]` attribute allows you to specify a different name for a property in the normalized output.

## Contributing

Contributions are welcome! Please feel free to submit a pull request.

```bash
$ phpize
$ ./configure
$ make
$ make install
```

## Testing


``` bash
make
make test
```
## License

The MIT License (MIT). Please see [LICENSE](LICENSE) for more information.