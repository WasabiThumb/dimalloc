#include <errno.h>
#include "dimalloc.h"

// https://stackoverflow.com/questions/18298280/how-to-declare-a-variable-as-thread-local-portably
#ifndef thread_local
# if __STDC_VERSION__ >= 201112 && !defined __STDC_NO_THREADS__
#  define thread_local _Thread_local
# elif defined _WIN32 && ( \
       defined _MSC_VER || \
       defined __ICL || \
       defined __DMC__ || \
       defined __BORLANDC__ )
#  define thread_local __declspec(thread)
/* note that ICC (linux) and Clang are covered by __GNUC__ */
# elif defined __GNUC__ || \
       defined __SUNPRO_C || \
       defined __hpux || \
       defined __xlC__
#  define thread_local __thread
# else
#  error "Cannot define thread_local"
# endif
#endif

static thread_local dim_pool GLOBAL_POOL = NULL;

bool dim_init(size_t size) {
    if (GLOBAL_POOL != NULL) dim_pool_destroy(GLOBAL_POOL);
    if ((GLOBAL_POOL = dim_pool_create(size)) == NULL) {
        errno = ENOMEM;
        return false;
    }
    return true;
}

bool dim_get_props(dim_pool_props_t *props) {
    dim_pool pool = GLOBAL_POOL;
    if (pool == NULL) {
        errno = ENOTSUP;
        return false;
    }
    dim_pool_get_props(pool, props);
    return true;
}

void *dim_alloc(size_t size) {
    dim_pool pool = GLOBAL_POOL;
    if (pool == NULL) {
        errno = ENOTSUP;
        return NULL;
    }
    return dim_pool_alloc(pool, size);
}

void *dim_realloc(void *address, size_t size) {
    dim_pool pool = GLOBAL_POOL;
    if (pool == NULL) {
        errno = ENOTSUP;
        return NULL;
    }
    return dim_pool_realloc(pool, address, size);
}

void dim_free(void *address) {
    dim_pool pool = GLOBAL_POOL;
    if (pool == NULL) return;
    dim_pool_free(pool, address);
}
