/**
 * @file thr_storage.h
 * @author zhangyi83
 * @brief 用于辅助 hook malloc 的实现，定义必要的结构
 * @version 0.1
 * @date 2022-11-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#pragma once

#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <atomic>
#include <set>
#include <mutex>
#include <map>
#include <utility>
#include <vector>
#include <string>
#include <iostream>

namespace baidu {
namespace pavaro {
namespace resmon {

// 封装 pthread_getspecific 函数，便于后期多系统支持
#define my_pthread_getspecific_ptr(T, V) ((T)pthread_getspecific(V))
// 封装 pthread_setspecific 函数，便于后期多系统支持
#define my_pthread_setspecific_ptr(T, V) (pthread_setspecific(T, reinterpret_cast<void*>(V)))

/**
 * @brief 线程的内存使用信息
 * 
 */
struct THD {
 public:
    // 线程id
    uint64_t thread_id;
    // 内存的申请量，原子操作，保证读写正常
    std::atomic<uint64_t> allocated_acc;
    // 内存的释放量，原子操作，保证读写正常
    std::atomic<uint64_t> deallocated_acc;
};

class ThrStorage {
 public:
    ~ThrStorage() = default;
    ThrStorage(const ThrStorage&) = delete;
    ThrStorage& operator=(const ThrStorage&) = delete;
    ThrStorage(ThrStorage&&) = delete;
    ThrStorage& operator=(ThrStorage&&) = delete;
    // 单例模式
    static ThrStorage& get_instance() {
        static ThrStorage instance;  // Guaranteed to be destroyed.
        return instance;          // Instantiated on first use.
    }

    /**
     * @brief 添加线程的 thd 信息
     * 
     * @param thd 线程的内存信息指针
     */
    void add_thread_thd(THD* thd);

    /**
     * @brief 移除线程的 thd 信息
     * 
     * @param thd 线程的内存信息指针
     */
    void remove_thread_thd(THD* thd);

    /**
     * @brief 获取所有线程的内存信息
     * 
     * @return 所有线程的内存信息
     */
    std::map<uint64_t, std::pair<uint64_t, uint64_t>>
     get_all_threads_mem_info();

    /**
     * @brief 修改进程内存使用量
     * 
     * @param size 内存使用量
     */
    void add_process_mem_used(int64_t size) {
        process_mem_used_.fetch_add(size);
    }

    /**
     * @brief 获取进程的内存信息
     * 
     * @return int64_t 进程的内存信息
     */
    int64_t get_process_mem_used() {
        return process_mem_used_.load();
    }

    void add_allocate_addr(const std::string& type, void* point) {
        char addr[50];
        sprintf(addr, "%p", point);
        std::lock_guard<std::mutex> lck(ThrStorage::get_instance().allocate_mtx_);
        if (allocate_num_ >= 10000) {
            return;
        }
        for (int i = 0; i < strlen(addr); i++) {
            ThrStorage::get_instance().allocate_addrs_[ThrStorage::get_instance().allocate_num_][i] = addr[i];
        }
        for (int i = 0; i < type.size(); i++) {
            ThrStorage::get_instance().allocate_types_[ThrStorage::get_instance().allocate_num_][i] = type[i];
        }
        ThrStorage::get_instance().allocate_num_++;
    }

    void add_free_addr(void* point) {
        char addr[50];
        sprintf(addr, "%p", point);
        std::lock_guard<std::mutex> lck(ThrStorage::get_instance().free_mtx_);
        if (free_num_ >= 10000) {
            return;
        }
        for (int i = 0; i < strlen(addr); i++) {
            ThrStorage::get_instance().free_addrs_[ThrStorage::get_instance().free_num_][i] = addr[i];
        }
        ThrStorage::get_instance().free_num_++;
    }

    void print_addrs() {
        std::cout << "allocate addrs: " << std::endl;
        allocate_mtx_.lock();
        for (int i = 0; i < 10000 && i < allocate_num_; i++) {
            std::cout << "allocate type: " << allocate_types_[i] << ", addr: " << allocate_addrs_[i] << std::endl;
        }
        allocate_mtx_.unlock();
        std::cout << "free addrs: " << std::endl;
        free_mtx_.lock();
        for (int i = 0; i < free_num_ && i < 10000; i++) {
            std::cout << "free addr: " << free_addrs_[i] << std::endl;
        }
        free_mtx_.unlock();
    }

 private:
    ThrStorage() = default;

 private:
    std::mutex mtx_;
    // 进程的内存使用，统计所有进程内存使用之和
    std::atomic<int64_t> process_mem_used_;

 public:
    std::mutex allocate_mtx_;
    uint64_t allocate_num_ = 0;
    char allocate_types_[10000][20];
    char allocate_addrs_[10000][50];
    std::mutex free_mtx_;
    uint64_t free_num_ = 0;
    char free_addrs_[10000][50];
};

}  // namespace resmon
}  // namespace pavaro
}  // namespace baidu
