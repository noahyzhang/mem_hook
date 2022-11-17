#include <dlfcn.h>
#include <malloc.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#include <utility>
#include <string>
#include "malloc_hook.h"
#include "thr_storage.h"

// Thread Memory Usage
#if defined(__APPLE__)
#define _msize(p) malloc_size(p)
#elif !defined(_WIN32)
#define _msize(p) malloc_usable_size(p)
#endif

// 定义需要 hook 的函数类型
typedef void* (*malloc_type)(size_t size);
typedef void* (*calloc_type)(size_t nmemb, size_t size);
typedef void* (*realloc_type)(void* ptr, size_t size);
typedef void* (*reallocarray_type)(void* ptr, size_t nmemb, size_t size);
typedef int (*posix_memalign_type)(void **__memptr, size_t __alignment, size_t __size);
typedef void* (*aligned_alloc_type)(size_t __alignment, size_t __size);
typedef void* (*memalign_type)(size_t __alignment, size_t __size);
typedef void* (*valloc_type)(size_t __size);
typedef void* (*pvalloc_type)(size_t __size);
typedef void (*free_type)(void* ptr);
typedef int (*pthread_create_type)(pthread_t*, const pthread_attr_t*, void*(*start_routine)(void*), void*);
typedef void* (*start_routine_type)(void*);

// 将命名空间引用进来
using baidu::pavaro::resmon::THD;
using baidu::pavaro::resmon::ThrStorage;

// 定义线程局部缓存
__thread THD* thd = nullptr;

/**
 * @brief 默认的变更线程内存信息的函数
 * 
 * @param size 内存申请或释放的大小
 * @param is_allocated 如果为申请内存则为 true，释放内存为 false
 */
void default_malloc_size_func(size_t size, bool is_allocated) {
    if (thd) {
        if (is_allocated) {
            thd->allocated_acc.fetch_add(size);
        } else {
            thd->deallocated_acc.fetch_add(size);
        }
    }
    int64_t wrapper_size = is_allocated == true ? size : -size;
    ThrStorage::get_instance().add_process_mem_used(wrapper_size);
}

void* malloc(size_t size) __THROW {
    static malloc_type real_malloc = reinterpret_cast<malloc_type>(dlsym(RTLD_NEXT, "malloc"));
    if (!real_malloc) {
        return nullptr;
    }
    void* point = real_malloc(size);
    if (point) {
        default_malloc_size_func(_msize(point), true);
        ThrStorage::get_instance().add_allocate_addr("malloc", point);
    }
    return point;
}

/**
 * 对于 hook calloc 的几点说明
 * 问题点：dlsym 底层实现调用了 calloc 进行内存分配。因此 hook calloc 时需要注意不能出现死循环
 * 
 * 当前解决方案如下：
 * 1. 在全局开辟一块内存空间，用来做为 dlsym 内部需要 calloc 的空间
 * 2. 设置 my_init_calloc_hook 函数的属性为 constructor，使其自动调用，且在 main 函数之前调用，实现初始化
 *    并且设置 constructor 构造函数的优先级为 1。constructor 的优先级，数值越小，越先调用；destructor 的优先级，数值越大，越先调用
 *    这个优先级 [1, 100] 范围是保留的，最小只能使用 101
 *    因此 my_init_calloc_hook 函数“尽可能”保证了当前代码是当前进程第一个调用 calloc 的位置
 *    当 dlsym 内部调用 calloc 时，此时 real_calloc 为空，则使用全局提前开辟好的空间，8192 字节大小够用
 * 3. 当 dlsym 寻找 calloc 符号找不到时，出错情况下，为了保证 calloc 函数的逻辑。
 *    使用全局变量 is_gather_calloc_ptr_error 做为判断。使业务调用 calloc 直接返回 null，而不是 calloc_ptr_buffer
 * 4. 对于在 my_init_calloc_hook 执行前，有调用 calloc 的行为，我们一般默认返回 nullptr。如何实现呢？
 *    使用全局变量 is_init_calloc，只有在进入 my_init_calloc_hook 后才将其置为 true
 *    保证了在 my_init_hook 之前调用 calloc 的场景都返回 nullptr
 * 
 * 存在问题：
 *    此解决方案在 my_init_calloc_hook 执行后，calloc 才可用
 */
static unsigned char calloc_ptr_buffer[8192] = {0};
static calloc_type real_calloc = nullptr;
static bool is_gather_calloc_ptr_error = false;
static bool is_init_calloc = false;

__attribute__((constructor(101))) static void my_init_calloc_hook(void) {
    is_init_calloc = true;
    static calloc_type tmp_calloc_ptr = reinterpret_cast<calloc_type>(dlsym(RTLD_NEXT, "calloc"));
    if (tmp_calloc_ptr != nullptr) {
        real_calloc = tmp_calloc_ptr;
    } else {
        // dlsym 出错的情况
        is_gather_calloc_ptr_error = true;
    }
}

void* calloc(size_t nmemb, size_t size) __THROW {
    if (real_calloc == nullptr) {
        if (is_gather_calloc_ptr_error) return nullptr;
        if (is_init_calloc) return calloc_ptr_buffer;
        return nullptr;
    }
    void* point = real_calloc(nmemb, size);
    if (point) {
        default_malloc_size_func(_msize(point), true);
        ThrStorage::get_instance().add_allocate_addr("calloc", point);
    }
    return point;
}

void* realloc(void* ptr, size_t size) __THROW {
    static realloc_type real_realloc = reinterpret_cast<realloc_type>(dlsym(RTLD_NEXT, "realloc"));
    if (!real_realloc) {
        return nullptr;
    }
    if (!ptr) return malloc(size);
    // 需要在调用真实的 realloc 之前获取，否则老的 ptr 指针可能会被释放
    size_t origin_size = _msize(ptr);
    void* new_ptr = real_realloc(ptr, size);
    if (new_ptr) {
        // 只有在重新存储块的时候，才更新
        // 如果当前存储块可用，_msize(ptr)获取的即为存储块的大小
        if (new_ptr != ptr) {
            int free_size = _msize(new_ptr) - origin_size;
            if (free_size > 0) {
                default_malloc_size_func(free_size, true);
                ThrStorage::get_instance().add_allocate_addr("realloc", new_ptr);
            }
        }
    }
    return new_ptr;
}

int posix_memalign(void **memptr, size_t alignment, size_t size) __THROW {
    static posix_memalign_type real_posix_memalign =
        reinterpret_cast<posix_memalign_type>(dlsym(RTLD_NEXT, "posix_memalign"));
    if (!real_posix_memalign) {
        return -1;
    }
    int res = real_posix_memalign(memptr, alignment, size);
    if (res == 0) {
        default_malloc_size_func(_msize(*memptr), true);
        ThrStorage::get_instance().add_allocate_addr("posix_memalign", *memptr);
    }
    return res;
}

void* aligned_alloc(size_t alignment, size_t size) __THROW {
    static aligned_alloc_type real_aligned_alloc =
        reinterpret_cast<aligned_alloc_type>(dlsym(RTLD_NEXT, "aligned_alloc"));
    if (!real_aligned_alloc) {
        return nullptr;
    }
    void* res = real_aligned_alloc(alignment, size);
    if (res) {
        default_malloc_size_func(_msize(res), true);
        ThrStorage::get_instance().add_allocate_addr("aligned_alloc", res);
    }
    return res;
}

void* memalign(size_t alignment, size_t size) __THROW {
    static memalign_type real_memalign = reinterpret_cast<memalign_type>(dlsym(RTLD_NEXT, "memalign"));
    if (!real_memalign) {
        return nullptr;
    }
    void* res = real_memalign(alignment, size);
    if (res) {
        default_malloc_size_func(_msize(res), true);
        ThrStorage::get_instance().add_allocate_addr("memalign", res);
    }
    return res;
}

void* valloc(size_t size) __THROW {
    static valloc_type real_valloc = reinterpret_cast<valloc_type>(dlsym(RTLD_NEXT, "valloc"));
    if (!real_valloc) {
        return nullptr;
    }
    void* res = real_valloc(size);
    if (res) {
        default_malloc_size_func(_msize(res), true);
        ThrStorage::get_instance().add_allocate_addr("valloc", res);
    }
    return res;
}

void* pvalloc(size_t size) __THROW {
    static pvalloc_type real_pvalloc = reinterpret_cast<pvalloc_type>(dlsym(RTLD_NEXT, "pvalloc"));
    if (!real_pvalloc) {
        return nullptr;
    }
    void* res = real_pvalloc(size);
    if (res) {
        default_malloc_size_func(_msize(res), true);
        ThrStorage::get_instance().add_allocate_addr("pvalloc", res);
    }
    return res;
}

void* reallocarray(void *ptr, size_t nmemb, size_t size) __THROW {
    static reallocarray_type real_reallocarray = reinterpret_cast<reallocarray_type>(dlsym(RTLD_NEXT, "reallocarray"));
    if (!real_reallocarray) {
        return nullptr;
    }
    if (!ptr) return malloc(nmemb*size);
    size_t origin_size = _msize(ptr);
    void* new_ptr = real_reallocarray(ptr, nmemb, size);
    if (new_ptr) {
        if (new_ptr != ptr) {
            int free_size = _msize(new_ptr) - origin_size;
            if (free_size > 0) {
                default_malloc_size_func(free_size, true);
                ThrStorage::get_instance().add_allocate_addr("reallocarray", new_ptr);
            }
        }
    }
    return new_ptr;
}

void free(void* ptr) __THROW {
    static free_type real_free = reinterpret_cast<free_type>(dlsym(RTLD_NEXT, "free"));
    if (!real_free) {
        return;
    }
    if (ptr) {
        default_malloc_size_func(_msize(ptr), false);
        ThrStorage::get_instance().add_free_addr(ptr);
        real_free(ptr);
    }
}

void* thr_func(void* arg) {
    THD* tmp_thd = reinterpret_cast<THD*>(malloc(sizeof(THD)));
    if (tmp_thd) {
        tmp_thd->thread_id = syscall(SYS_gettid);
        tmp_thd->allocated_acc = 0;
        tmp_thd->deallocated_acc = 0;
        ThrStorage::get_instance().add_thread_thd(tmp_thd);
        thd = tmp_thd;
    }
    // 调用业务原本的线程启动函数
    void** origin_arg = reinterpret_cast<void**>(arg);
    start_routine_type business_func = reinterpret_cast<start_routine_type>(origin_arg[0]);
    void* business_arg = origin_arg[1];
    auto res = business_func(business_arg);
    free(origin_arg);
    // 线程退出前需要移除线程的 thd 信息
    if (tmp_thd) {
        ThrStorage::get_instance().remove_thread_thd(tmp_thd);
        free(tmp_thd);
    }
    return res;
}

extern int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
    void *(*start_routine) (void *), void *arg)__THROWNL {
    static pthread_create_type real_pthread_create =
        reinterpret_cast<pthread_create_type>(dlsym(RTLD_NEXT, "pthread_create"));
    if (!real_pthread_create) {
        return -1;
    }
    void** new_arg = reinterpret_cast<void**>(malloc(2 * sizeof(void*)));
    new_arg[0] = reinterpret_cast<void*>(start_routine);
    new_arg[1] = arg;
    return real_pthread_create(thread, attr, thr_func, new_arg);
}
