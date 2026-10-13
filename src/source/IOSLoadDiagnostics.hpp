#pragma once
#ifdef SOURCE_IOS
#include <mach/mach.h>
#include <cstdio>
#include <unistd.h>
#include <time.h>
inline void IOSLoadCheckpoint(const char *phase) {
    task_vm_info_data_t info = {};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    const kern_return_t result = task_info(mach_task_self(), TASK_VM_INFO,
        reinterpret_cast<task_info_t>(&info), &count);
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    std::fprintf(stderr, "iOS load: pid=%d time=%lld.%03ld phase=%s footprintMiB=%llu residentMiB=%llu status=%d\n",
        getpid(), static_cast<long long>(now.tv_sec), now.tv_nsec / 1000000, phase,
        static_cast<unsigned long long>(info.phys_footprint / 1048576),
        static_cast<unsigned long long>(info.resident_size / 1048576), result);
    std::fflush(stderr);
}
#else
inline void IOSLoadCheckpoint(const char *) {}
#endif
