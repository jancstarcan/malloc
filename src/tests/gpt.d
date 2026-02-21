#include "../mem.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPS 100000
#define SLOTS 256
#define MAX_SIZE 128

void safe_stress_test(void) {
    void* slots[SLOTS] = {0};
    size_t sizes[SLOTS] = {0};

    for (int i = 0; i < OPS; i++) {
        int idx = rand() % SLOTS;
        int action = rand() % 3;

        if (action == 0) { // malloc or realloc null slot
            if (slots[idx]) {
                free(slots[idx]);
                slots[idx] = NULL;
                sizes[idx] = 0;
            }

            size_t sz = (rand() % MAX_SIZE) + 1;
            slots[idx] = malloc(sz);
            assert(slots[idx]);
            sizes[idx] = sz;

            memset(slots[idx], 0xA5, sz); // Fill with pattern
        } else if (action == 1) { // free
            if (slots[idx]) {
                free(slots[idx]);
                slots[idx] = NULL;
                sizes[idx] = 0;
            }
        } else { // realloc
            size_t new_sz = (rand() % MAX_SIZE) + 1;
            void* old = slots[idx];
            slots[idx] = realloc(old, new_sz);
            assert(slots[idx]);
            sizes[idx] = new_sz;

            // Fill new or copied memory
            size_t min_sz = old ? ((new_sz < sizes[idx]) ? new_sz : sizes[idx]) : new_sz;
            memset(slots[idx], 0x5A, min_sz);
        }

        // Optional: check that all live blocks are intact
        for (int j = 0; j < SLOTS; j++) {
            if (slots[j]) {
                uint8_t* p = (uint8_t*)slots[j];
                for (size_t k = 0; k < sizes[j]; k++) {
                    (void)p[k]; // touch memory to catch segfaults
                }
            }
        }
    }

    // Free any remaining blocks
    for (int i = 0; i < SLOTS; i++) {
        if (slots[i])
            free(slots[i]);
    }
}

int main(void) {
    safe_stress_test();
    mm_print_stats();
    return 0;
}
