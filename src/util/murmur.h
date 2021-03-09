#pragma once
#include <stddef.h>
#include <stdint.h>
uint32_t murmur3_32(const void* key, size_t len, uint32_t seed);

