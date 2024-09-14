# Object (de)normalization for PHP

A PHP language extension that provides an efficient alternative to Symfony's Normalizer.

## Documentation

TODO

## Installation

TODO

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


## Contributing
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
