<?php

namespace Normalizer;

class ObjectNormalizer
{
    public const string GROUPS = 'GROUPS';
    public const string OBJECT_TO_POPULATE = 'OBJECT_TO_POPULATE';
    public const string SKIP_NULL_VALUES = 'SKIP_NULL_VALUES';
    public const string SKIP_UNINITIALIZED_VALUES = 'SKIP_UNINITIALIZED_VALUES';

    public function __construct(){}

    public function normalize(object|array $object, array $context = []): array {}

    public function denormalize(array $data, string $class, array $context = []): object {}
}
