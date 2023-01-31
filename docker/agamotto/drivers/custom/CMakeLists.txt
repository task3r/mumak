cmake_minimum_required(VERSION 3.1) 

set(LIBMAP_SRC
  map.c
  map.h
  ex_common.h
)

set(BTREE
  data_store_btree.c
  map_btree.c
  map_btree.h
  btree_map.c
  btree_map.h
)

set(BTREE_SPT
  data_store_btree_spt.c
  map_btree.c
  map_btree.h
  btree_map.c
  btree_map.h
)


set(RBTREE
  data_store_rbtree.c
  map_rbtree.c
  map_rbtree.h
  rbtree_map.c
  rbtree_map.h
)

set(RBTREE_SPT
  data_store_rbtree_spt.c
  map_rbtree.c
  map_rbtree.h
  rbtree_map.c
  rbtree_map.h
)

set(HASH
  data_store_hash.c
  map_hashmap_atomic.c
  map_hashmap_atomic.h
  hashmap_atomic.c
  hashmap_atomic.h
  hashmap.h
  hashmap_internal.h
)

set(HASH_SPT
  data_store_hash_spt.c
  map_hashmap_atomic.c
  map_hashmap_atomic.h
  hashmap_atomic.c
  hashmap_atomic.h
  hashmap.h
  hashmap_internal.h
)

include_directories(${PMDK_INCLUDE_DIR})
link_directories(${PMDK_LIB_DIR})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O1 -Xclang -disable-llvm-passes -D__NO_STRING_INLINES -D_FORTIFY_SOURCE=0 -U__OPTIMIZE__")

klee_add_test_executable(TARGET btree
  SOURCES ${BTREE} ${LIBMAP_SRC}
  EXTRA_LIBS pmemobj pmem)

klee_add_test_executable(TARGET btree_spt
    SOURCES ${BTREE_SPT} ${LIBMAP_SRC}
  EXTRA_LIBS pmemobj pmem)

klee_add_test_executable(TARGET rbtree
  SOURCES ${RBTREE} ${LIBMAP_SRC}
  EXTRA_LIBS pmemobj pmem)

klee_add_test_executable(TARGET rbtree_spt
    SOURCES ${RBTREE_SPT} ${LIBMAP_SRC}
  EXTRA_LIBS pmemobj pmem)


klee_add_test_executable(TARGET hashmap_atomic
  SOURCES ${HASH} ${LIBMAP_SRC}
  EXTRA_LIBS pmemobj pmem)

klee_add_test_executable(TARGET hashmap_atomic_spt
    SOURCES ${HASH_SPT} ${LIBMAP_SRC}
  EXTRA_LIBS pmemobj pmem)
