#include "thr_storage.h"

namespace baidu {
namespace pavaro {
namespace resmon {

void ThrStorage::add_thread_thd(THD*) {
    return;
}

void ThrStorage::remove_thread_thd(THD*) {
    return;
}

std::map<uint64_t, std::pair<uint64_t, uint64_t>> ThrStorage::get_all_threads_mem_info() {
    return std::map<uint64_t, std::pair<uint64_t, uint64_t>>();
}

}  // namespace resmon
}  // namespace pavaro
}  // namespace baidu
