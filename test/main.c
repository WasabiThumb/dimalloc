#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "dimalloc.h"

// Size of the heap
#define HEAP_SIZE DIM_KB(4)

// Maximum number of blocks to allocate before freeing old ones
#define BLOCK_LIMIT 32

// Block sizes to cycle through
static size_t BLOCK_SIZES[] = { 8, 4, 12, 2, 14 };
#define BLOCK_SIZE_COUNT (sizeof(BLOCK_SIZES) / sizeof(size_t))

// Maximum column count per row of dump
#define DUMP_COLS 16

//

void dump(const void *buf, size_t len) {
    const unsigned char *bytes = (const unsigned char *) buf;
    unsigned char old[DUMP_COLS];
    unsigned char tmp[DUMP_COLS];
    size_t head = 0;
    size_t stride = DUMP_COLS;
    bool loop = true;
    unsigned int collapsed = 0;

    while (loop) {
        size_t end = head + stride;
        if (end >= len) {
            end = len;
            stride = len - head;
            loop = false;
        }

        memcpy(tmp, &bytes[head], stride);
        if (head && loop && memcmp(tmp, old, DUMP_COLS) == 0) {
            collapsed++;
            goto eol;
        } else if (collapsed != 0) {
            printf("*\n");
            collapsed = 0;
        }

        printf("%016zx | ", head);

        for (size_t i=0; i < stride; i++) {
            unsigned int c = tmp[i];
            printf("%02x ", c);
        }

        for (size_t i=stride; i < DUMP_COLS; i++) {
            printf("   ");
        }

        printf("| ");

        for (size_t i=0; i < stride; i++) {
            unsigned char c = tmp[i];
            if (c < 0x20 || c >= 0x7F) c = '.';
            printf("%c", c);
        }

        printf("\n");
        eol:
        head = end;
        memcpy(old, tmp, DUMP_COLS);
    }
}

void dump_pool(void) {
    dim_pool_props_t props;
    dim_get_props(&props);
    dump(props.loc, props.size);
}

int main(void) {
    dim_init(HEAP_SIZE);

    void *block_buf[BLOCK_LIMIT];
    int block_buf_len = 0;

    unsigned char head = 0;
    size_t next_size;
    void *next;
    do {
        next_size = BLOCK_SIZES[head % BLOCK_SIZE_COUNT];
        next = dim_alloc(next_size);
        memset(next, (int) head, next_size);

        if (block_buf_len == BLOCK_LIMIT) {
            dim_free(block_buf[0]);
            memmove(&block_buf[0], &block_buf[1], (BLOCK_LIMIT - 1) * sizeof(void *));
            block_buf[BLOCK_LIMIT - 1] = next;
        } else {
            block_buf[block_buf_len++] = next;
        }
    } while ((++head) != 0);

    dump_pool();
    return 0;
}
