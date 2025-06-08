#pragma once

/*
 * dimalloc.h
 *
 * Sets up fixed memory pool(s) for an application
 * and provides methods for allocating/freeing
 * within the pool. Pools may be created explicitly,
 * or the global pool may be used after calling dim_init.
 * Pools may not be used concurrently, and behavior
 * in multithreaded contexts is undefined. However,
 * the global pool is thread-local.
 *
 * Size Macros:
 * - DIM_1KB, DIM_KB(n)
 * - DIM_1MB, DIM_MB(n)
 * - DIM_1GB, DIM_GB(n)
 *
 * Pool Methods:
 * - dim_pool_create(size)
 * - dim_pool_alloc(pool, size)
 * - dim_pool_free(pool, pointer)
 * - dim_pool_destroy(pool)
 *
 * Global Pool Methods:
 * - dim_init(size)
 * - dim_alloc(size)
 * - dim_free(address)
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Size Macros
#define DIM_1KB 1024
#define DIM_1MB (1024 * DIM_1KB)
#define DIM_1GB (1024 * DIM_1MB)
#define DIM_KB(n) ((size_t) (((double) n) * ((double) DIM_1KB)))
#define DIM_MB(n) ((size_t) (((double) n) * ((double) DIM_1MB)))
#define DIM_GB(n) ((size_t) (((double) n) * ((double) DIM_1GB)))

// Pool

struct dim_pool_;

/**
 * A dimalloc memory pool
 */
typedef struct dim_pool_ *dim_pool;

/**
 * Cross-platform properties of a
 * dimalloc memory pool
 */
typedef struct dim_pool_props {
    /** Location of the pool data in the virtual address space */
   void *loc;

   /** Total size of the allocated space */
   size_t size;

   /** Size of the header data */
   size_t hsize;

   /** Number of usable bytes (at most size - hsize) */
   size_t dsize;
} dim_pool_props_t;

// Pool Methods

/**
 * Creates a new memory pool which can hold
 * at least "size" bytes, or NULL if allocation
 * failed.
 */
dim_pool dim_pool_create(size_t size);

/**
 * Reports the properties of the given pool.
 */
void dim_pool_get_props(dim_pool pool, dim_pool_props_t *props);

/**
 * Allocates "size" bytes of memory within the pool
 * and returns a pointer to the first byte within
 * the newly allocated block, or NULL if allocation
 * failed (out of space).
 */
void *dim_pool_alloc(dim_pool pool, size_t size);

/**
 * Frees a block of memory previously allocated
 * by dim_pool_alloc.
 */
void dim_pool_free(dim_pool pool, void *address);

/**
 * Destroys a pool previously created by
 * dim_pool_create.
 */
void dim_pool_destroy(dim_pool pool);

// Global Pool Methods

/**
 * Initializes the global pool such that it can hold
 * at least "size" bytes.
 */
bool dim_init(size_t size);

/**
 * Reports the properties of the global pool.
 */
bool dim_get_props(dim_pool_props_t *props);

/**
 * Allocates a block of memory of "size" bytes within
 * the global pool, or NULL if allocation failed (out of space).
 * The dm_init function must have been
 * called previously.
 */
void *dim_alloc(size_t size);

/**
 * Frees a block of memory previously allocated by
 * dim_alloc.
 */
void dim_free(void *address);

#ifdef __cplusplus
}
#endif
