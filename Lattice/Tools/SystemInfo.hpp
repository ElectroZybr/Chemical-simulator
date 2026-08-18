#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace Lattice::CliSystemInfo {
    struct EngineInfo {
        std::string version;
        std::string build;
        std::string compiler;
    };

    struct CpuInfo {
        std::string name;
        std::string simd;
        unsigned int cores = 0;
        unsigned int threads = 0;
    };

    struct GpuInfo {
        std::string name;
        std::string driver;
        std::string pciAddress;
        std::string computeInfo;
        uint64_t vramBytes = 0;
    };

    struct SystemInfo {
        std::string os;
        std::string arch;
        uint64_t totalRamBytes = 0;
        EngineInfo engine;
        CpuInfo cpu;
        std::vector<GpuInfo> gpus;
    };

    SystemInfo collectSystemInfo();
    void printSystemInfo(std::ostream& out, const SystemInfo& info);
    void printSystemInfo(std::ostream& out);
}
