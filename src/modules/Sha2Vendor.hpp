#pragma once
// See third_party/sha2/LICENSE for the vendor's BSD-3-Clause notice.
#define sha224_init lucid_sha224_init
#define sha224_update lucid_sha224_update
#define sha224_final lucid_sha224_final
#define sha224 lucid_sha224
#define sha256_init lucid_sha256_init
#define sha256_update lucid_sha256_update
#define sha256_final lucid_sha256_final
#define sha256 lucid_sha256
#define sha384_init lucid_sha384_init
#define sha384_update lucid_sha384_update
#define sha384_final lucid_sha384_final
#define sha384 lucid_sha384
#define sha512_init lucid_sha512_init
#define sha512_update lucid_sha512_update
#define sha512_final lucid_sha512_final
#define sha512 lucid_sha512
#include "../../third_party/sha2/sha2.h"
#undef sha224_init
#undef sha224_update
#undef sha224_final
#undef sha224
#undef sha256_init
#undef sha256_update
#undef sha256_final
#undef sha256
#undef sha384_init
#undef sha384_update
#undef sha384_final
#undef sha384
#undef sha512_init
#undef sha512_update
#undef sha512_final
#undef sha512
