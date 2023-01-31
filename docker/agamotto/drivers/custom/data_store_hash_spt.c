/*
 * Copyright 2015-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * data_store.c -- tree_map example usage
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "ex_common.h"
#include "map.h"
#include "map_hashmap_atomic.h"

POBJ_LAYOUT_BEGIN(data_store);
POBJ_LAYOUT_ROOT(data_store, struct store_root);
POBJ_LAYOUT_TOID(data_store, struct store_item);
POBJ_LAYOUT_END(data_store);

#define MAX_INSERTS 1000000

static uint64_t nkeys;
static uint64_t keys[MAX_INSERTS];

struct store_item {
    uint64_t item_data;
};

struct store_root {
    TOID(struct map) map;
};

/*
 * new_store_item -- transactionally creates and initializes new item
 */
static TOID(struct store_item) new_store_item(void) {
    TOID(struct store_item) item = TX_NEW(struct store_item);
    D_RW(item)->item_data = rand();

    return item;
}

/*
 * get_keys -- inserts the keys of the items by key order (sorted, descending)
 */
static int get_keys(uint64_t key, PMEMoid value, void *arg) {
    keys[nkeys++] = key;

    return 0;
}

/*
 * dec_keys -- decrements the keys count for every item
 */
static int dec_keys(uint64_t key, PMEMoid value, void *arg) {
    nkeys--;
    return 0;
}

/*
 * parse_map_type -- parse type of map
 */
static const struct map_ops *parse_map_type(const char *type) {
    return MAP_HASHMAP_ATOMIC;
}

int main(int argc, const char *argv[]) {
    if (argc < 2) {
        printf("usage: %s file-name nops\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];
    const struct map_ops *map_ops = MAP_HASHMAP_ATOMIC;

    int nops = atoi(argv[2]);

    PMEMobjpool *pop;
    srand((unsigned)time(NULL));

    int exist = 0;

    if (file_exists(path) != 0) {
        if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(data_store),
                                  PMEMOBJ_MIN_POOL, 0666)) == NULL) {
            perror("failed to create pool\n");
            return 1;
        }
    } else {
        exist = 1;
        if ((pop = pmemobj_open(path, POBJ_LAYOUT_NAME(data_store))) == NULL) {
            perror("failed to open pool\n");
            return 1;
        }
    }

    TOID(struct store_root) root = POBJ_ROOT(pop, struct store_root);
    printf("root->map=%p\n", &D_RW(root)->map);
    struct map_ctx *mapc = map_ctx_init(map_ops, pop);
    if (!mapc) {
        perror("cannot allocate map context\n");
        return 1;
    }
    int aborted = 0;

    TX_BEGIN(pop) {
        if (!exist)
            map_create(mapc, &D_RW(root)->map, NULL);
        else
            map_init(mapc, D_RW(root)->map);
    }
    TX_ONABORT {
        perror("transaction aborted\n");
        map_ctx_free(mapc);
        aborted = 1;
    }
    TX_END

    if (aborted) {
        fprintf(stderr, "cannot create map\n");
        return -1;
    }

    for (int i = 0; i < nops; ++i) {
        TX_BEGIN(pop) {
            // new_store_item is transactional!
            map_insert(mapc, D_RW(root)->map, rand(), new_store_item().oid);
        }
        TX_ONABORT {
            perror("transaction aborted\n");
            map_ctx_free(mapc);
            aborted = 1;
        }
        TX_END
        if (aborted) {
            fprintf(stderr, "error inserting items\n");
            return -1;
        }
    }

    /* count the items */
    map_foreach(mapc, D_RW(root)->map, get_keys, NULL);

    for (int i = 0; i < nkeys; ++i) {
        PMEMoid item = map_remove(mapc, D_RW(root)->map, keys[i]);

        assert(!OID_IS_NULL(item));
        assert(OID_INSTANCEOF(item, struct store_item));
    }
    uint64_t old_nkeys = nkeys;

    // tree should be empty
    map_foreach(mapc, D_RW(root)->map, dec_keys, NULL);
    assert(old_nkeys == nkeys);
    map_ctx_free(mapc);
    pmemobj_close(pop);

    return 0;
}
