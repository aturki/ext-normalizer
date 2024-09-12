PHP_ARG_ENABLE(normalizer, whether to enable the normalizer extension,
[  --enable-normalizer   Enable normalizer extension])

if test "$PHP_NORMALIZER" != "no"; then
    PHP_NEW_EXTENSION(
        normalizer,
        normalizer.c \
        src/helpers.c \
        src/attributes/expose_ce.c \
        src/attributes/ignore_ce.c \
        src/attributes/max_depth_ce.c \
        src/attributes/groups_ce.c \
        src/normalizer/object_normalizer_ce.c,
        $ext_shared,,
    )

    PHP_ADD_BUILD_DIR($ext_builddir/src, 1)
    PHP_ADD_BUILD_DIR($ext_builddir/src/attributes, 1)
    PHP_ADD_BUILD_DIR($ext_builddir/src/normalizer, 1)
fi
