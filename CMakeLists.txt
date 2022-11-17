cmake_minimum_required(VERSION 3.10)
project(mem_hook)

include_directories(
    ./mem_hook
)

file(GLOB MEM_HOOK_SRC
    ./malloc_hook.cpp
    ./thr_storage.cpp
)

add_library(mem_hook SHARED ${MEM_HOOK_SRC})

target_link_libraries(mem_hook
    pthread
    dl    
)


file(GLOB DEFAULT_MEM_HOOK_SRC
    ./malloc_hook.cpp
    ./default_thr_storage.cpp
)

add_library(default_mem_hook SHARED ${DEFAULT_MEM_HOOK_SRC})

