<?php

namespace Normalizer;

class ObjectNormalizer
{
    public const string GROUPS = 'GROUPS';
    public const string OBJECT_TO_POPULATE = 'OBJECT_TO_POPULATE';
    public const string SKIP_NULL_VALUES = 'SKIP_NULL_VALUES';
    public const string SKIP_UNINITIALIZED_VALUES = 'SKIP_UNINITIALIZED_VALUES';
    public const string DEFAULT_CONSTRUCTOR_ARGUMENTS = 'DEFAULT_CONSTRUCTOR_ARGUMENTS';
    public const string CIRCULAR_REFERENCE_HANDLER = 'CIRCULAR_REFERENCE_HANDLER';


    private array $options = [];

    public function __construct(array $options = []){}

    public function __destruct() {}

    public function normalize(object|array|null $object, array $context = []): array {}

    public function denormalize(array|null $data, string $class, array $context = []): object {}
}
