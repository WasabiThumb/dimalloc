#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "dimalloc.h"


// START Bit counting

#if defined(__GNUC__)
#define DEFN_CLZ32_    { return v == 0 ? 32 : __builtin_clz(v); }
#define DEFN_CTZ32_    { return v == 0 ? 32 : __builtin_ctz(v); }
#define DEFN_POPCNT32_ { return __builtin_popcount(v); }
#elif defined(_MSC_VER) && defined(_M_X64)
#define DEFN_CLZ32_    { return (int) _lzcnt_u32(v); }
#define DEFN_CTZ32_    { return (int) _tzcnt_u32(v); }
#define DEFN_POPCNT32_ { return (int) __popcnt(v); }
#elif defined(_MSC_VER) && defined(M_IX86)
#define DEFN_CLZ32_    { unsigned long ignored; return v == 0 ? 32 : (_BitScanReverse(&ignored, v) ^ 31); }
#define DEFN_CTZ32_    { unsigned long ignored; return v == 0 ? 32 : _BitScanForward(&ignored, v); }
/* i686 has no popcnt intrinsic */
#endif

#ifndef DEFN_POPCNT32_
/* https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel */
#define DEFN_POPCNT32_ {                         \
    v = (v & 0x55555555) + ((v >> 1) & 0x55555555); \
    v = (v & 0x33333333) + ((v >> 2) & 0x33333333); \
    v = (v & 0x0F0F0F0F) + ((v >> 4) & 0x0F0F0F0F); \
    v = (v & 0x00FF00FF) + ((v >> 8) & 0x00FF00FF); \
    v = (v & 0x0000FFFF) + ((v >>16) & 0x0000FFFF); \
    return (int) v;                                 \
}
#endif

#ifndef DEFN_CLZ32_
#define DEFN_CLZ32_ { \
    v = v | (v >> 1);    \
    v = v | (v >> 2);    \
    v = v | (v >> 4);    \
    v = v | (v >> 8);    \
    v = v | (v >> 16);   \
    v = ~v;              \
    DEFN_POPCNT32_    \
}
#endif

#ifndef DEFN_CTZ32_
/* https://graphics.stanford.edu/~seander/bithacks.html#ZerosOnRightParallel */
#define DEFN_CTZ32_ {         \
    int c = 32;                  \
    v &= -(int32_t) v;           \
    if (v) c--;                  \
    if (v & 0x0000FFFF) c -= 16; \
    if (v & 0x00FF00FF) c -= 8;  \
    if (v & 0x0F0F0F0F) c -= 4;  \
    if (v & 0x33333333) c -= 2;  \
    if (v & 0x55555555) c -= 1;  \
    return c;                    \
}
#endif

static inline int clz32(uint32_t v) DEFN_CLZ32_
static inline int ctz32(uint32_t v) DEFN_CTZ32_
static inline int clz8(uint8_t v) { return clz32(((uint32_t) v) & 0xFF) - 24;         }
static inline int ctz8(uint8_t v) { return v == 0 ? 8 : ctz32(((uint32_t) v) & 0xFF); }

// END Bit counting

// START Internals

#define PREFIX_MAX 9

static size_t init_prefix(size_t value, char *prefix) {
    uint64_t v = (uint64_t) value;
    size_t len = 0;
    unsigned char c;
    bool loop = true;
    while (loop) {
        if (len == (PREFIX_MAX - 1)) {
            c = (unsigned char) v;
        } else {
            c = (unsigned char) (v & 0x7F);
            v >>= 7;
            if (v != 0) {
                c |= 0x80;
            } else {
                loop = false;
            }
        }
        prefix[len++] = (char) c;
    }
    return len;
}

static bool dim_pool_locate_small(const dim_pool_props_t *props, int len, uintptr_t *ret) {
    uintptr_t loc = (uintptr_t) props->loc;
    uintptr_t hlen = (uintptr_t) props->hsize;
    unsigned char *header = (unsigned char *) loc;

    if (hlen == 0) return false;
    if (clz8(header[0]) >= len) {
        *ret = 0;
        return true;
    }

    uint8_t a, b;
    for (uintptr_t i=1; i < hlen; i++) {
        a = header[i - 1];
        b = header[i    ];

        int avail = ctz8(a);
        uintptr_t offset = (i << 3) - avail;
        avail += clz8(b);

        if (avail >= len) {
            *ret = offset;
            return true;
        }
    }

    return false;
}

static bool dim_pool_locate_large(const dim_pool_props_t *props, size_t len, uintptr_t *ret) {
    uintptr_t loc = (uintptr_t) props->loc;
    uintptr_t hlen = (uintptr_t) props->hsize;
    uintptr_t offset = UINTPTR_MAX;
    uintptr_t avail = 0;
    uint8_t c;
    int nb;

    for (uintptr_t i=0; i < hlen; i++) {
        c = *((uint8_t *) (loc + i));
        if (offset != UINTPTR_MAX) {
            nb = clz8(c);
            avail += nb;
            if (avail >= len) {
                *ret = offset;
                return true;
            } else if (nb == 8) {
                continue;
            }
        }
        nb = ctz8(c);
        if (nb == 0) {
            offset = UINTPTR_MAX;
            continue;
        }
        offset = (i * 8) + (8 - nb);
        avail = (uintptr_t) nb;
        if (avail >= len) {
            *ret = offset;
            return true;
        }
    }

    return false;
}

static void dim_pool_header_set(const dim_pool_props_t *props, uintptr_t off, uintptr_t len, bool value) {
    uintptr_t loc = (uintptr_t) props->loc;
    unsigned char *header = (unsigned char *) loc;

    uintptr_t end_bit = off + len;
    uintptr_t end_byte = end_bit >> 3;
    uintptr_t start_byte = off >> 3;
    unsigned char mask;

    if (start_byte == end_byte) {
        int a = (int) (off % 8);
        int b = (int) (end_bit % 8);
        mask = (0xFF >> a) & (0xFF << (7 - b));
        if (value) {
            header[start_byte] |= mask;
        } else {
            header[start_byte] &= (~mask);
        }
    } else {
        mask = 0xFF >> (off & 7);
        if (value) {
            header[start_byte] |= mask;
        } else {
            header[start_byte] &= (~mask);
        }

        mask = (0xFF00 >> (end_bit & 7)) & 0xFF;
        if (value) {
            header[end_byte] |= mask;
        } else {
            header[end_byte] &= (~mask);
        }

        if (end_byte > (start_byte + 1)) {
            memset(
                    header + start_byte + 1,
                    value ? 0xFF : 0x00,
                    end_byte - start_byte - 1
            );
        }
    }
}

static bool check_homogenous(const void *region, size_t region_len) {
    const unsigned char *bytes = (const unsigned char *) region;

    // Special case for odd initial length
    if (region_len & 1 && bytes[0] != bytes[region_len - 1]) {
        return false;
    }

    // Continuously divide region in half and compare with upper half
    size_t part = region_len >> 1;
    while (part) {
        if (memcmp(&bytes[0], &bytes[part], part) != 0) return false;
        part >>= 1;
    }

    return true;
}

static bool dim_pool_header_check(const dim_pool_props_t *props, uintptr_t off, uintptr_t len, bool value) {
    uintptr_t loc = (uintptr_t) props->loc;
    unsigned char *header = (unsigned char *) loc;

    uintptr_t end_bit = off + len;
    uintptr_t end_byte = end_bit >> 3;
    uintptr_t start_byte = off >> 3;
    unsigned char mask;

    if (start_byte == end_byte) {
        int a = (int) (off % 8);
        int b = (int) (end_bit % 8);
        mask = (0xFF >> a) & (0xFF << (7 - b));
        return (header[start_byte] & mask) == (value ? mask : 0);
    } else {
        mask = 0xFF >> (off & 7);
        if ((header[start_byte] & mask) != (value ? mask : 0)) return false;

        mask = (0xFF00 >> (end_bit & 7)) & 0xFF;
        if ((header[end_byte] & mask) != (value ? mask : 0)) return false;

        if (end_byte > (start_byte + 1)) {
            if (header[start_byte + 1] != (value ? 0xFF : 0x00)) return false;
            if (!check_homogenous(header + start_byte + 1, end_byte - start_byte - 1)) return false;
        }

        return true;
    }
}

static bool dim_pool_read_meta(const dim_pool_props_t *props, const void *address, uintptr_t *off, uintptr_t *ps, uintptr_t *rs) {
    uintptr_t pool_loc = (uintptr_t) props->loc;
    uintptr_t pool_hsize = (uintptr_t) props->hsize;
    uintptr_t pool_dsize = (uintptr_t) props->dsize;
    uintptr_t loc = (uintptr_t) address;

    if (loc                           <  pool_loc  ) return false;
    if ((loc - pool_loc)              <  pool_hsize) return false;
    if ((loc - pool_hsize - pool_loc) >= pool_dsize) return false;

    uintptr_t region_size = 0;
    uintptr_t prefix_size = 0;
    int shift = 0;

    unsigned char c;
    bool loop = true;
    while (loop) {
        c = *((unsigned char *) (--loc));
        if (shift == 56) {
            loop = false;
        } else {
            loop = ((c & 0x80) != 0);
            c &= 0x7F;
        }
        region_size |= (((uintptr_t) c) << shift);
        shift += 7;
        prefix_size++;
    }

    *off = loc - pool_hsize - pool_loc;
    *ps = prefix_size;
    *rs = region_size;
    return true;
}

// END Internals

// START API

void *dim_pool_alloc(dim_pool pool, size_t size) {
    dim_pool_props_t props;
    dim_pool_get_props(pool, &props);

    char prefix[PREFIX_MAX];
    size_t prefix_len = init_prefix(size, prefix);
    size_t total_len = size + prefix_len;

    uintptr_t offset;
    if (total_len < 8) {
        if (!dim_pool_locate_small(&props, (int) total_len, &offset)) {
            errno = ENOMEM;
            return NULL;
        }
    } else {
        if (!dim_pool_locate_large(&props, total_len, &offset)) {
            errno = ENOMEM;
            return NULL;
        }
    }

    dim_pool_header_set(&props, offset, total_len, true);
    char *region = (char *) (((uintptr_t) props.loc) + ((uintptr_t) props.hsize) + offset);
    for (size_t i=0; i < prefix_len; i++) {
        region[prefix_len - 1 - i] = prefix[i];
    }

    return (void *) (region + prefix_len);
}

void *dim_pool_realloc(dim_pool pool, void *address, size_t size) {
    if (address == NULL)
        return dim_pool_alloc(pool, size);

    dim_pool_props_t props;
    dim_pool_get_props(pool, &props);

    uintptr_t off, ps, rs;
    if (!dim_pool_read_meta(&props, address, &off, &ps, &rs)) {
        // Address lies outside valid region
        errno = EINVAL;
        return NULL;
    }

    if (size < rs) {
        // Shrink in-place
        dim_pool_header_set(&props, off + ps + size, rs - size, false);
        return address;
    } else if (size == rs) {
        // No change
        return address;
    } else if (dim_pool_header_check(&props, off + ps + rs, size - rs, false)) {
        // Grow in-place
        dim_pool_header_set(&props, off, ps + size, true);
        return address;
    } else {
        // Grow by moving
        void *dest = dim_pool_alloc(pool, size);

        if (dest == NULL) return NULL;
        memcpy(dest, address, rs);

        // Free the original address (faster than dim_pool_free as preconditions are met)
        dim_pool_header_set(&props, off, ps + rs, false);

        return dest;
    }
}

void dim_pool_free(dim_pool pool, void *address) {
    dim_pool_props_t props;
    dim_pool_get_props(pool, &props);
    uintptr_t off, ps, rs;
    if (!dim_pool_read_meta(&props, address, &off, &ps, &rs)) {
        // Address lies outside valid region
        errno = EINVAL;
        return;
    }
    if (ps == 0 || !dim_pool_header_check(&props, off, ps + rs, true)) {
        // Double free
        errno = EINVAL;
        return;
    }
    dim_pool_header_set(&props, off, ps + rs, false);
    *(((unsigned char *) address) - 1) = 0;
}

// END API
