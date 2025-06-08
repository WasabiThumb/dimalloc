# dimalloc
An experimental C11 fixed memory pool, implemented with ``mmap``/``VirtualAlloc``.
Not ready for production.

## Example
```c
// Reserve at least 50MB for 
// the current thread
dim_init(DIM_MB(50));

// Allocate an 8KB buffer
void *buf = dim_alloc(DIM_KB(8));

// Free the buffer
dim_free(buf);
```

### With dedicated pools
```c
// Reserve 50 MB
dim_pool pool = dim_pool_create(DIM_MB(50));

// Allocate an 8KB buffer
void *buf = dim_pool_alloc(pool, DIM_KB(8));

// Free the buffer
dim_pool_free(pool, buf);

// IMPORTANT: Make sure to additionally release
// the pool's reserved address space
dim_pool_destroy(pool);
```

## License
```text
Copyright 2025 Wasabi Codes

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```
