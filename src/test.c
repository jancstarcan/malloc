#include "mem.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPS 100000

void integrity_test(void);
void exhaustion(void);
void rand_test(void);
void mmap_test(void);
void shrink(void);
void grow(void);

int main(void) {
	// integrity_test();
	// exhaustion();
	rand_test();
	// mmap_test();
	// shrink();
	// grow();

	return 0;
}

void integrity_test(void) {
	uint8_t* a = malloc(64);
	uint8_t* b = malloc(64);
	uint8_t* c = malloc(64);

	memset(a, 0xAA, 64);
	memset(b, 0xBB, 64);
	memset(c, 0xCC, 64);

	free(b);

	for (int i = 0; i < 64; i++) {
		assert(a[i] == 0xAA);
		assert(c[i] == 0xCC);
	}

	free(a);
	free(c);
}

void exhaustion(void) {
	void* blocks[1024];
	int count = 0;

	for (; count < 1024; count++) {
		blocks[count] = malloc(64);
	}

	for (int i = 0; i < count; i += 2)
		free(blocks[i]);

	for (int i = 0; i < count / 2; i++)
		assert(malloc(64));

	for (int i = 1; i < count; i += 2)
		free(blocks[i]);
}

void rand_test(void) {
	void* slots[256] = {0};

	for (int i = 0; i < OPS; i++) {
		int idx = rand() % 256;
		int action = rand() % 3;

		if (action == 0) {
			if (slots[idx])
				free(slots[idx]);
			slots[idx] = malloc((rand() % 128) + 1);
			assert(slots[idx]);
		} else if (action == 1) {
			free(slots[idx]);
			slots[idx] = NULL;
		} else {
			size_t sz = (rand() % 128) + 1;
			slots[idx] = realloc(slots[idx], sz);
			assert(slots[idx]);
		}
	}

	for (int i = 0; i < 256; i++)
		free(slots[i]);
}

void mmap_test(void) {
	const size_t mib250 = 268435456;
	void* slots[4];
	for (int i = 0; i < 4; i++)
		slots[i] = malloc(mib250);
	for (int i = 0; i < 4; i++)
		free(slots[i]);
}

void shrink(void) {
	uint8_t* p = malloc(128);
	for (int i = 0; i < 128; i++)
		p[i] = (uint8_t)i;

	p = realloc(p, 32);
	assert(p);

	for (int i = 0; i < 32; i++)
		assert(p[i] == (uint8_t)i);

	free(p);
}

void grow(void) {
	uint8_t* p = malloc(32);
	for (int i = 0; i < 32; i++)
		p[i] = 0x5A;

	p = realloc(p, 256);
	assert(p);

	for (int i = 0; i < 32; i++)
		assert(p[i] == 0x5A);

	free(p);
}
