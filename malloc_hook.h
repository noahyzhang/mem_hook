/**
 * @file malloc_hook.h
 * @author zhangyi83
 * @brief 实现内存管理函数的 hook 功能，需要通过 preload 的方式加载
 * @version 0.1
 * @date 2022-11-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

/**
 * 注意：dlsym 内部使用了 calloc，因此 hook calloc 时不能使用 dlsym
 * 否则会 core 掉，或者死循环导致的栈溢出
 * 因此使用 dlsym 寻找符号的方法，一定要注意
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief hook malloc 函数
 * 
 * @param __size 申请的内存大小
 * @return void* 内存地址
 */
extern void *malloc(size_t __size) __THROW __attribute_malloc__ __wur;

/**
 * @brief hook calloc 函数
 * 
 * @param __nmemb 元素个数
 * @param __size 元素大小
 * @return void* 内存地址
 */
extern void *calloc(size_t __nmemb, size_t __size)
    __THROW __attribute_malloc__ __wur;

/**
 * @brief hook realloc 函数
 * 
 * @param __ptr 原始内存地址
 * @param __size 内存大小
 * @return void* 新内存地址
 */
extern void *realloc(void *__ptr, size_t __size)
    __THROW __attribute_warn_unused_result__;

/**
 * @brief hook reallocarray 函数
 * 
 * @param __ptr 原始内存地址
 * @param __nmemb 元素个数
 * @param __size 元素大小
 * @return void* 新内存地址
 */
extern void *reallocarray(void *__ptr, size_t __nmemb, size_t __size)
     __THROW __attribute_warn_unused_result__;

/**
 * @brief 分配size大小的字节，并将分配的内存地址存放在memptr中
 * 分配的内存的地址将是alignment的倍数，且必须是2的幂次方和sizeof(void*)的倍数
 * 这个地址可以传递给 free。如果 size 为0，则 *__memptr 中的值要么是 NULL，要么是一个唯一的指针值
 */
extern int posix_memalign(void **__memptr, size_t __alignment, size_t __size) __THROW __nonnull((1)) __wur;

/**
 * @brief 用法与memalign函数相同，但是size大小应该alignment的倍数。如果分配失败返回NULL
 * 
 */
extern void *aligned_alloc(size_t __alignment, size_t __size)
    __THROW __attribute_malloc__ __attribute_alloc_size__((2)) __wur;

/**
 * @brief 分配 size 大小的内存，返回已分配的内存地址指针
 *   其内存地址将是 __alignment 的倍数，且必须是 2 的幂次方
 *   如果分配失败返回 NULL
 * @param __alignment 对齐数
 * @param __size 内存大小
 * @return void* 内存地址
 */
extern void *memalign(size_t __alignment, size_t __size)
    __THROW __attribute_malloc__ __wur;

/* Allocate SIZE bytes on a page boundary.  */
/**
 * @brief 分配size大小的字节，返回已分配的内存地址指针，其内存地址将是页大小(page size)的倍数。
 *   如果分配失败返回NULL
 * @param __size 内存大小
 * @return void* 内存地址
 */
extern void *valloc(size_t __size) __THROW __attribute_malloc__ __wur;

/**
 * @brief 用法与valloc相似.如果分配失败返回NULL
 * 
 * @param __size 内存大小
 * @return void* 内存地址
 */
extern void *pvalloc(size_t __size) __THROW __attribute_malloc__ __wur;

/**
 * @brief hook free 函数
 * 
 * @param __ptr 内存地址
 */
extern void free(void *__ptr) __THROW;

/**
 * @brief hook pthread_create 函数
 * 
 */
extern int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
    void *(*start_routine) (void *), void *arg)__THROWNL __nonnull((1, 3));


#ifdef __cplusplus
}
#endif
