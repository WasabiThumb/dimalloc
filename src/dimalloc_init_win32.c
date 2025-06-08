#include "dimalloc.h"

#ifdef _WIN32
#include <Windows.h>

struct dim_pool_ {
    dim_pool_props_t props;
};

dim_pool dim_pool_create(size_t size) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);

    size_t page_size = (size_t) info.dwPageSize;
    size_t len = page_size * (size / page_size);
    if (size != len) len += page_size;
    size_t hsize = len / 9;
    size_t dsize = hsize << 3;

    LPVOID addr = VirtualAlloc(
        NULL,
        len,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );
    if (addr == NULL) {
        return NULL;
    }

    dim_pool ret = (dim_pool) malloc(sizeof(struct dim_pool_));
    if (ret == NULL) {
        VirtualFree(addr, 0, MEM_RELEASE);
        return NULL;
    }

    dim_pool_props_t *props = &ret->props;
    props->loc = (void *) addr;
    props->size = len;
    props->hsize = hsize;
    props->dsize = dsize;
    return ret;
}

void dim_pool_get_props(dim_pool pool, dim_pool_props_t *props) {
    memcpy(props, &pool->props, sizeof(dim_pool_props_t));
}

void dim_pool_destroy(dim_pool pool) {
    dim_pool_props_t *props = &pool->props;
    VirtualFree(props->loc, 0, MEM_RELEASE);
    free(pool);
}

#endif
