#include "dimalloc.h"

#ifndef _WIN32
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

struct dim_pool_ {
    dim_pool_props_t props;
};

dim_pool dim_pool_create(size_t size) {
    size_t ps = sysconf(_SC_PAGE_SIZE);
    size_t len = ps * (size / ps);
    if (len != size) len += ps;
    size_t hlen = len / 9;
    size_t dlen = hlen << 3;

    void *addr = mmap(
            NULL,
            len,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0
    );
    if (addr == MAP_FAILED) {
        return NULL;
    }
    memset(addr, 0, hlen);

    dim_pool ret = (dim_pool) malloc(sizeof(struct dim_pool_));
    if (ret == NULL) {
        munmap(addr, len);
        return NULL;
    }

    dim_pool_props_t *props = &ret->props;
    props->loc = addr;
    props->size = len;
    props->hsize = hlen;
    props->dsize = dlen;
    return ret;
}

void dim_pool_get_props(dim_pool pool, dim_pool_props_t *props) {
    memcpy(props, &pool->props, sizeof(dim_pool_props_t));
}

void dim_pool_destroy(dim_pool pool) {
    dim_pool_props_t *props = &pool->props;
    munmap(props->loc, props->size);
    free(pool);
}

#endif
