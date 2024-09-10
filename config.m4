PHP_ARG_ENABLE(normalizer, whether to enable the normalizer extension,
[  --enable-normalizer   Enable normalizer extension])

if test "$PHP_NORMALIZER" = "yes"; then
    PHP_NEW_EXTENSION(normalizer, normalizer.c, $ext_shared)
fi
