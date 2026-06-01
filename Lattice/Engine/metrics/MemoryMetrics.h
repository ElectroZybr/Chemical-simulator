#pragma once

#if defined(_WIN32) || defined(_WIN64)
// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on
#ifdef NEAR
#undef NEAR
#endif
#ifdef FAR
#undef FAR
#endif
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#elif defined(__APPLE__) && defined(__MACH__)
#include <cstdio>

#include <mach/mach.h>
#include <sys/resource.h>
#include <unistd.h>
#elif defined(__unix__) || defined(__unix) || defined(unix)
#include <cstdio>

#include <sys/resource.h>
#include <unistd.h>
#endif

class MemoryMetrics {
public:
    static size_t getRSS() {
#if defined(_WIN32) || defined(_WIN64)
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
            return static_cast<size_t>(pmc.WorkingSetSize);
        }
#elif defined(__APPLE__) && defined(__MACH__)
        struct mach_task_basic_info info;
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS) {
            return static_cast<size_t>(info.resident_size);
        }
#elif defined(__linux__) || defined(__unix__)
        long pages = 0;
        long rss = 0;
        FILE* fp = fopen("/proc/self/statm", "r");
        if (fp) {
            if (fscanf(fp, "%ld %ld", &pages, &rss) == 2) {
                fclose(fp);
                return static_cast<size_t>(rss) * static_cast<size_t>(sysconf(_SC_PAGESIZE));
            }
            fclose(fp);
        }
#endif
        return 0;
    }
};
