#include "thr_storage.h"

namespace baidu {
namespace pavaro {
namespace resmon {

// 存储所有线程的 TLS 指针
static std::set<THD*> thd_list_;

void ThrStorage::add_thread_thd(THD* thd) {
    std::lock_guard<std::mutex> lck(mtx_);
    bool hava_thd = thd_list_.find(thd) != thd_list_.end();
    if (!hava_thd) {
        thd_list_.insert(thd);
    }
}

void ThrStorage::remove_thread_thd(THD* thd) {
    std::lock_guard<std::mutex> lck(mtx_);
    bool hava_thd = thd_list_.find(thd) != thd_list_.end();
    if (hava_thd) {
        thd_list_.erase(thd);
    }
}

std::map<uint64_t, std::pair<uint64_t, uint64_t>> ThrStorage::get_all_threads_mem_info() {
    std::map<uint64_t, std::pair<uint64_t, uint64_t>> threads_mem_info;
    std::lock_guard<std::mutex> lck(mtx_);
    for (const auto& x : thd_list_) {
        threads_mem_info[x->thread_id] = std::pair<uint64_t, uint64_t>(
            x->allocated_acc.exchange(0), x->deallocated_acc.exchange(0));
    }
    return threads_mem_info;
}

}  // namespace resmon
}  // namespace pavaro
}  // namespace baidu
