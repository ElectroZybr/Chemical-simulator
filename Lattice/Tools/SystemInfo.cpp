#include <Lattice/Tools/SystemInfo.hpp>
#include <Lattice/Tools/LogStyle.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_map>

#if defined(_WIN32)
#include <intrin.h>
#include <windows.h>
#elif defined(__linux__)
#include <cpuid.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#elif defined(__APPLE__)
#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif
#include <sys/sysctl.h>
#endif

namespace fs = std::filesystem;

namespace Lattice::CliSystemInfo {
    namespace {
#ifndef LL_CLI_ENGINE_VERSION
#define LL_CLI_ENGINE_VERSION "unknown"
#endif

#ifndef LL_CLI_BUILD_TYPE
#define LL_CLI_BUILD_TYPE "unknown"
#endif

#ifndef LL_CLI_COMPILER
#define LL_CLI_COMPILER "unknown"
#endif

        constexpr std::string_view kReset = LogStyle::Color::reset;
        constexpr std::string_view kDim = LogStyle::Color::tree;
        constexpr std::string_view kTitle = LogStyle::Color::header;
        constexpr std::string_view kLabel = LogStyle::Color::key;
        constexpr std::string_view kValue = LogStyle::Color::value;
        constexpr std::string_view kDevice = LogStyle::Color::device;

        std::string colorize(std::string_view text, std::string_view color) {
            return LogStyle::paint(text, color);
        }

        std::string trim(std::string text) {
            const auto begin = text.find_first_not_of(" \t\r\n");
            if (begin == std::string::npos) {
                return {};
            }
            const auto end = text.find_last_not_of(" \t\r\n");
            return text.substr(begin, end - begin + 1);
        }

        std::string trimCpuName(std::string name) {
            static constexpr std::array<std::string_view, 4> suffixes{
                " 16-Core Processor",
                " 12-Core Processor",
                " 8-Core Processor",
                " 6-Core Processor",
            };

            for (std::string_view suffix : suffixes) {
                if (name.size() >= suffix.size() && name.ends_with(suffix)) {
                    name.erase(name.size() - suffix.size());
                    break;
                }
            }
            return trim(name);
        }

        std::string readFirstLine(const fs::path& path) {
            std::ifstream input(path);
            std::string line;
            if (std::getline(input, line)) {
                return trim(line);
            }
            return {};
        }

        std::map<std::string, std::string> readKeyValueFile(const fs::path& path) {
            std::ifstream input(path);
            std::map<std::string, std::string> values;
            std::string line;
            while (std::getline(input, line)) {
                const std::size_t equals = line.find('=');
                if (equals == std::string::npos) {
                    continue;
                }

                std::string key = trim(line.substr(0, equals));
                std::string value = trim(line.substr(equals + 1));
                if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                    value = value.substr(1, value.size() - 2);
                }
                values.emplace(std::move(key), std::move(value));
            }
            return values;
        }

        uint64_t readUint64FromFile(const fs::path& path) {
            std::ifstream input(path);
            uint64_t value = 0;
            if (input >> value) {
                return value;
            }
            return 0;
        }

        std::string readCommandOutput(const char* command) {
#if defined(_WIN32)
            FILE* pipe = _popen(command, "r");
#else
            FILE* pipe = popen(command, "r");
#endif
            if (!pipe) {
                return {};
            }

            std::string output;
            std::array<char, 256> buffer{};
            while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
                output += buffer.data();
            }

#if defined(_WIN32)
            _pclose(pipe);
#else
            pclose(pipe);
#endif
            return trim(output);
        }

        std::string detectOsName() {
#if defined(_WIN32)
            return "Windows";
#elif defined(__APPLE__)
            return "macOS";
#elif defined(__linux__)
            std::string osName = "Linux";
            const auto osRelease = readKeyValueFile("/etc/os-release");
            if (const auto it = osRelease.find("PRETTY_NAME"); it != osRelease.end() && !it->second.empty()) {
                osName = it->second;
            } else if (const auto it = osRelease.find("NAME"); it != osRelease.end() && !it->second.empty()) {
                osName = it->second;
            }

            struct utsname uts {};
            if (uname(&uts) == 0 && std::strlen(uts.release) > 0) {
                osName += " (kernel ";
                osName += uts.release;
                osName += ")";
            }
            return osName;
#else
            return "Unknown OS";
#endif
        }

        std::string detectArchName() {
#if defined(__x86_64__) || defined(_M_X64)
            return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
            return "arm64";
#elif defined(__i386__) || defined(_M_IX86)
            return "x86";
#elif defined(__arm__) || defined(_M_ARM)
            return "arm";
#else
            return "unknown";
#endif
        }

        std::string normalizeCompilerName(std::string compiler) {
            if (compiler.starts_with("GNU ")) {
                compiler.replace(0, 4, "GCC ");
            } else if (compiler.starts_with("Clang ")) {
                compiler.replace(0, 6, "Clang ");
            } else if (compiler.starts_with("AppleClang ")) {
                compiler.replace(0, 11, "Apple Clang ");
            } else if (compiler.starts_with("MSVC ")) {
                compiler.replace(0, 5, "MSVC ");
            }
            return compiler;
        }

        EngineInfo detectEngineInfo() {
            return EngineInfo{
                .version = LL_CLI_ENGINE_VERSION,
                .build = LL_CLI_BUILD_TYPE,
                .compiler = normalizeCompilerName(LL_CLI_COMPILER),
            };
        }

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
        uint64_t readXcr0() {
#if defined(_MSC_VER)
            return static_cast<uint64_t>(_xgetbv(0));
#elif defined(__GNUC__) || defined(__clang__)
            uint32_t eax = 0;
            uint32_t edx = 0;
            __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
            return (static_cast<uint64_t>(edx) << 32) | eax;
#else
            return 0;
#endif
        }

        std::string detectCpuBrandX86() {
            std::array<char, 49> brand{};

#if defined(_WIN32)
            std::array<int, 4> regs{};
            __cpuid(regs.data(), 0x80000000);
            const unsigned int maxLeaf = static_cast<unsigned int>(regs[0]);
            if (maxLeaf < 0x80000004) {
                return {};
            }

            __cpuid(regs.data(), 0x80000002);
            std::memcpy(brand.data() + 0, regs.data(), 16);
            __cpuid(regs.data(), 0x80000003);
            std::memcpy(brand.data() + 16, regs.data(), 16);
            __cpuid(regs.data(), 0x80000004);
            std::memcpy(brand.data() + 32, regs.data(), 16);
#else
            unsigned int eax = 0;
            unsigned int ebx = 0;
            unsigned int ecx = 0;
            unsigned int edx = 0;
            if (!__get_cpuid_max(0x80000000, nullptr)) {
                return {};
            }

            const unsigned int maxLeaf = __get_cpuid_max(0x80000000, nullptr);
            if (maxLeaf < 0x80000004) {
                return {};
            }

            __cpuid(0x80000002, eax, ebx, ecx, edx);
            std::memcpy(brand.data() + 0, &eax, sizeof(eax));
            std::memcpy(brand.data() + 4, &ebx, sizeof(ebx));
            std::memcpy(brand.data() + 8, &ecx, sizeof(ecx));
            std::memcpy(brand.data() + 12, &edx, sizeof(edx));

            __cpuid(0x80000003, eax, ebx, ecx, edx);
            std::memcpy(brand.data() + 16, &eax, sizeof(eax));
            std::memcpy(brand.data() + 20, &ebx, sizeof(ebx));
            std::memcpy(brand.data() + 24, &ecx, sizeof(ecx));
            std::memcpy(brand.data() + 28, &edx, sizeof(edx));

            __cpuid(0x80000004, eax, ebx, ecx, edx);
            std::memcpy(brand.data() + 32, &eax, sizeof(eax));
            std::memcpy(brand.data() + 36, &ebx, sizeof(ebx));
            std::memcpy(brand.data() + 40, &ecx, sizeof(ecx));
            std::memcpy(brand.data() + 44, &edx, sizeof(edx));
#endif

            return trim(std::string(brand.data()));
        }

        std::string detectSimdWidthX86() {
#if defined(_WIN32)
            std::array<int, 4> regs{};
            __cpuid(regs.data(), 1);
            const bool osxsave = (regs[2] & (1 << 27)) != 0;
            const bool avx = (regs[2] & (1 << 28)) != 0;

            bool avxOsSupport = false;
            bool zmmOsSupport = false;
            if (osxsave) {
                const uint64_t xcr0 = readXcr0();
                avxOsSupport = (xcr0 & 0x6) == 0x6;
                zmmOsSupport = (xcr0 & 0xE6) == 0xE6;
            }

            __cpuidex(regs.data(), 7, 0);
            const bool avx2 = (regs[1] & (1 << 5)) != 0;
            const bool avx512f = (regs[1] & (1 << 16)) != 0;
#else
            unsigned int eax = 0;
            unsigned int ebx = 0;
            unsigned int ecx = 0;
            unsigned int edx = 0;
            if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
                return "unknown";
            }

            const bool osxsave = (ecx & bit_OSXSAVE) != 0;
            const bool avx = (ecx & bit_AVX) != 0;

            bool avxOsSupport = false;
            bool zmmOsSupport = false;
            if (osxsave) {
                const uint64_t xcr0 = readXcr0();
                avxOsSupport = (xcr0 & 0x6) == 0x6;
                zmmOsSupport = (xcr0 & 0xE6) == 0xE6;
            }

            bool avx2 = false;
            bool avx512f = false;
            if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
                avx2 = (ebx & bit_AVX2) != 0;
                avx512f = (ebx & bit_AVX512F) != 0;
            }
#endif

            if (avx512f && zmmOsSupport) {
                return "AVX-512 (512-bit)";
            }
            if ((avx2 || avx) && avxOsSupport) {
                return avx2 ? "AVX2 (256-bit)" : "AVX (256-bit)";
            }
            return "SSE (128-bit)";
        }
#endif

        std::string detectCpuName() {
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
            if (const std::string brand = detectCpuBrandX86(); !brand.empty()) {
                return trimCpuName(brand);
            }
#endif

#if defined(__linux__)
            std::ifstream cpuinfo("/proc/cpuinfo");
            std::string line;
            while (std::getline(cpuinfo, line)) {
                if (line.rfind("model name", 0) == 0) {
                    const auto pos = line.find(':');
                    if (pos != std::string::npos) {
                        return trimCpuName(trim(line.substr(pos + 1)));
                    }
                }
            }
#elif defined(__APPLE__)
            char buffer[256]{};
            size_t size = sizeof(buffer);
            if (sysctlbyname("machdep.cpu.brand_string", buffer, &size, nullptr, 0) == 0) {
                return trimCpuName(trim(std::string(buffer, size > 0 ? size - 1 : 0)));
            }
#endif

            return "Unknown CPU";
        }

        std::string detectSimdWidth() {
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
            return detectSimdWidthX86();
#elif defined(__aarch64__) || defined(_M_ARM64)
            return "NEON (128-bit)";
#elif defined(__arm__) || defined(_M_ARM)
            return "NEON/VFP (128-bit)";
#else
            return "unknown";
#endif
        }

        unsigned int detectCpuThreads() {
            return std::thread::hardware_concurrency();
        }

        unsigned int detectCpuCores() {
#if defined(__linux__)
            std::ifstream cpuinfo("/proc/cpuinfo");
            std::string line;
            std::string physicalId = "0";
            std::string coreId;
            std::set<std::pair<std::string, std::string>> cores;
            while (std::getline(cpuinfo, line)) {
                if (line.empty()) {
                    if (!coreId.empty()) {
                        cores.emplace(physicalId, coreId);
                    }
                    physicalId = "0";
                    coreId.clear();
                    continue;
                }
                if (line.rfind("physical id", 0) == 0) {
                    const auto pos = line.find(':');
                    if (pos != std::string::npos) {
                        physicalId = trim(line.substr(pos + 1));
                    }
                } else if (line.rfind("core id", 0) == 0) {
                    const auto pos = line.find(':');
                    if (pos != std::string::npos) {
                        coreId = trim(line.substr(pos + 1));
                    }
                }
            }
            if (!coreId.empty()) {
                cores.emplace(physicalId, coreId);
            }
            if (!cores.empty()) {
                return static_cast<unsigned int>(cores.size());
            }

            cpuinfo.clear();
            cpuinfo.seekg(0);
            while (std::getline(cpuinfo, line)) {
                if (line.rfind("cpu cores", 0) == 0) {
                    const auto pos = line.find(':');
                    if (pos != std::string::npos) {
                        return static_cast<unsigned int>(std::max(1, std::stoi(trim(line.substr(pos + 1)))));
                    }
                }
            }
#elif defined(__APPLE__)
            int value = 0;
            size_t size = sizeof(value);
            if (sysctlbyname("hw.physicalcpu", &value, &size, nullptr, 0) == 0 && value > 0) {
                return static_cast<unsigned int>(value);
            }
#elif defined(_WIN32)
            DWORD len = 0;
            GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &len);
            if (len > 0) {
                std::vector<char> buffer(len);
                if (GetLogicalProcessorInformationEx(
                        RelationProcessorCore,
                        reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()),
                        &len) != 0) {
                    unsigned int count = 0;
                    DWORD offset = 0;
                    while (offset < len) {
                        const auto* info =
                            reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset);
                        if (info->Relationship == RelationProcessorCore) {
                            ++count;
                        }
                        offset += info->Size;
                    }
                    if (count > 0) {
                        return count;
                    }
                }
            }
#endif
            const unsigned int threads = detectCpuThreads();
            return threads > 0 ? threads : 0;
        }

        uint64_t detectTotalRamBytes() {
#if defined(_WIN32)
            MEMORYSTATUSEX status{};
            status.dwLength = sizeof(status);
            if (GlobalMemoryStatusEx(&status) != 0) {
                return static_cast<uint64_t>(status.ullTotalPhys);
            }
#elif defined(__linux__)
            struct sysinfo info {};
            if (sysinfo(&info) == 0) {
                return static_cast<uint64_t>(info.totalram) * static_cast<uint64_t>(info.mem_unit);
            }
#elif defined(__APPLE__)
            uint64_t value = 0;
            size_t size = sizeof(value);
            if (sysctlbyname("hw.memsize", &value, &size, nullptr, 0) == 0) {
                return value;
            }
#endif
            return 0;
        }

        std::string gpuVendorNameFromId(std::string_view vendorId) {
            if (vendorId == "0x10de") return "NVIDIA";
            if (vendorId == "0x1002" || vendorId == "0x1022") return "AMD";
            if (vendorId == "0x8086") return "Intel";
            if (vendorId == "0x5143") return "Qualcomm";
            if (vendorId == "0x13b5") return "ARM";
            if (vendorId == "0x1ae0") return "Apple";
            return {};
        }

        std::string toLower(std::string text) {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return text;
        }

        std::vector<std::string> splitCsvLine(const std::string& line) {
            std::vector<std::string> fields;
            std::string current;
            bool inQuotes = false;
            for (char ch : line) {
                if (ch == '"') {
                    inQuotes = !inQuotes;
                    continue;
                }
                if (ch == ',' && !inQuotes) {
                    fields.push_back(trim(current));
                    current.clear();
                    continue;
                }
                current += ch;
            }
            fields.push_back(trim(current));
            return fields;
        }

        std::map<std::string, std::string> readColonKeyValueFile(const fs::path& path) {
            std::ifstream input(path);
            std::map<std::string, std::string> values;
            std::string line;
            while (std::getline(input, line)) {
                const std::size_t colon = line.find(':');
                if (colon == std::string::npos) {
                    continue;
                }
                values.emplace(trim(line.substr(0, colon)), trim(line.substr(colon + 1)));
            }
            return values;
        }

        uint64_t parseCapacityStringToBytes(std::string text) {
            text = trim(text);
            if (text.empty()) {
                return 0;
            }

            const std::string lower = toLower(text);
            std::size_t numberEnd = 0;
            while (numberEnd < lower.size() && (std::isdigit(static_cast<unsigned char>(lower[numberEnd])) || lower[numberEnd] == '.')) {
                ++numberEnd;
            }
            if (numberEnd == 0) {
                return 0;
            }

            const double value = std::strtod(lower.c_str(), nullptr);
            const std::string unit = trim(lower.substr(numberEnd));
            if (unit.starts_with("tb")) return static_cast<uint64_t>(value * 1024.0 * 1024.0 * 1024.0 * 1024.0);
            if (unit.starts_with("gb")) return static_cast<uint64_t>(value * 1024.0 * 1024.0 * 1024.0);
            if (unit.starts_with("mb")) return static_cast<uint64_t>(value * 1024.0 * 1024.0);
            if (unit.starts_with("kb")) return static_cast<uint64_t>(value * 1024.0);
            if (unit == "b" || unit.empty()) return static_cast<uint64_t>(value);
            return 0;
        }

        uint64_t extractVramFromGpuName(std::string_view name) {
            const std::string lower = toLower(std::string(name));
            for (std::size_t i = 0; i < lower.size(); ++i) {
                if (!std::isdigit(static_cast<unsigned char>(lower[i]))) {
                    continue;
                }
                std::size_t j = i;
                while (j < lower.size() && (std::isdigit(static_cast<unsigned char>(lower[j])) || lower[j] == '.')) {
                    ++j;
                }
                if (j + 1 >= lower.size()) {
                    continue;
                }
                if ((lower[j] == 'g' && lower[j + 1] == 'b') || (lower[j] == 'm' && lower[j + 1] == 'b')) {
                    return parseCapacityStringToBytes(lower.substr(i, j + 2 - i));
                }
            }
            return 0;
        }

        void enrichNvidiaGpuInfo(std::vector<GpuInfo>& gpus) {
#if defined(__linux__) || defined(_WIN32)
            const std::string output = readCommandOutput(
                "nvidia-smi --query-gpu=pci.bus_id,name,driver_version,memory.total,multiprocessor_count --format=csv,noheader,nounits 2>/dev/null");
            if (output.empty()) {
                return;
            }

            std::unordered_map<std::string, GpuInfo*> byPci;
            for (GpuInfo& gpu : gpus) {
                if (!gpu.pciAddress.empty()) {
                    byPci.emplace(toLower(gpu.pciAddress), &gpu);
                }
            }

            std::istringstream stream(output);
            std::string line;
            while (std::getline(stream, line)) {
                line = trim(line);
                if (line.empty()) {
                    continue;
                }

                const std::vector<std::string> fields = splitCsvLine(line);
                if (fields.size() < 5) {
                    continue;
                }

                const std::string pci = toLower(fields[0]);
                auto it = byPci.find(pci);
                if (it == byPci.end()) {
                    continue;
                }

                GpuInfo& gpu = *it->second;
                if (!fields[1].empty() && fields[1] != "[Not Supported]") {
                    gpu.name = fields[1];
                }
                if (!fields[2].empty() && fields[2] != "[Not Supported]") {
                    gpu.driver = fields[2];
                }
                if (!fields[3].empty() && fields[3] != "[Not Supported]") {
                    const uint64_t mib = static_cast<uint64_t>(std::strtoull(fields[3].c_str(), nullptr, 10));
                    if (mib > 0) {
                        gpu.vramBytes = mib * 1024ull * 1024ull;
                    }
                }
                if (!fields[4].empty() && fields[4] != "[Not Supported]") {
                    gpu.computeInfo = fields[4] + " SMs";
                }
            }
#endif
        }

#if defined(__linux__)
        void enrichNvidiaGpuInfoFromProc(std::vector<GpuInfo>& gpus) {
            const fs::path nvidiaRoot("/proc/driver/nvidia");
            if (!fs::exists(nvidiaRoot / "gpus")) {
                return;
            }

            std::string driverVersion;
            const auto versionInfo = readColonKeyValueFile(nvidiaRoot / "version");
            const auto versionIt = versionInfo.find("NVRM version");
            if (versionIt != versionInfo.end()) {
                const std::string& versionLine = versionIt->second;
                const std::size_t modulePos = versionLine.find("Kernel Module");
                if (modulePos != std::string::npos) {
                    driverVersion = trim(versionLine.substr(modulePos + std::string("Kernel Module").size()));
                    const std::size_t datePos = driverVersion.find("  ");
                    if (datePos != std::string::npos) {
                        driverVersion = trim(driverVersion.substr(0, datePos));
                    }
                } else {
                    driverVersion = versionLine;
                }
            }

            std::unordered_map<std::string, GpuInfo*> byPci;
            for (GpuInfo& gpu : gpus) {
                if (!gpu.pciAddress.empty()) {
                    byPci.emplace(toLower(gpu.pciAddress), &gpu);
                }
            }

            for (const auto& entry : fs::directory_iterator(nvidiaRoot / "gpus")) {
                if (!entry.is_directory()) {
                    continue;
                }
                const std::string pci = toLower(entry.path().filename().string());
                auto it = byPci.find(pci);
                if (it == byPci.end()) {
                    continue;
                }

                GpuInfo& gpu = *it->second;
                const auto info = readColonKeyValueFile(entry.path() / "information");
                const auto modelIt = info.find("Model");
                if (modelIt != info.end() && !modelIt->second.empty()) {
                    gpu.name = modelIt->second;
                }
                if (gpu.driver.empty() && !driverVersion.empty()) {
                    gpu.driver = driverVersion;
                }
                if (gpu.vramBytes == 0) {
                    gpu.vramBytes = extractVramFromGpuName(gpu.name);
                }
            }
        }
#endif

        std::vector<GpuInfo> detectGpuInfo() {
#if defined(__linux__)
            std::vector<GpuInfo> gpus;
            const fs::path drmRoot("/sys/class/drm");
            if (!fs::exists(drmRoot)) {
                return {GpuInfo{.name = "unknown"}};
            }

            std::set<std::string> seenPci;
            for (const auto& entry : fs::directory_iterator(drmRoot)) {
                const std::string nodeName = entry.path().filename().string();
                if (nodeName.rfind("card", 0) != 0 || nodeName.find('-') != std::string::npos) {
                    continue;
                }

                const fs::path devicePath = entry.path() / "device";
                if (!fs::exists(devicePath)) {
                    continue;
                }

                GpuInfo gpu;
                std::error_code ec;
                const fs::path resolved = fs::weakly_canonical(devicePath, ec);
                if (!ec) {
                    gpu.pciAddress = resolved.filename().string();
                    if (!seenPci.insert(gpu.pciAddress).second) {
                        continue;
                    }
                }

                const fs::path driverLink = fs::read_symlink(devicePath / "driver", ec);
                if (!ec) {
                    gpu.driver = driverLink.filename().string();
                }

                std::string gpuName;
                if (!gpu.pciAddress.empty()) {
                    const std::string lspciLine =
                        readCommandOutput(("sh -lc \"lspci -s " + gpu.pciAddress + " 2>/dev/null | head -n1\"").c_str());
                    if (!lspciLine.empty()) {
                        const std::size_t firstColon = lspciLine.find(':');
                        const std::size_t secondColon =
                            firstColon == std::string::npos ? std::string::npos : lspciLine.find(':', firstColon + 1);
                        gpuName = secondColon != std::string::npos ? trim(lspciLine.substr(secondColon + 1)) : lspciLine;
                    }
                }

                if (gpuName.empty()) {
                    const std::string vendorId = readFirstLine(devicePath / "vendor");
                    const std::string deviceId = readFirstLine(devicePath / "device");
                    gpuName = gpuVendorNameFromId(vendorId);
                    if (!deviceId.empty()) {
                        if (!gpuName.empty()) gpuName += ' ';
                        gpuName += "(" + deviceId + ")";
                    }
                }

                gpu.name = gpuName.empty() ? "unknown" : gpuName;
                gpu.vramBytes = readUint64FromFile(devicePath / "mem_info_vram_total");
                if (gpu.vramBytes == 0) {
                    gpu.vramBytes = readUint64FromFile(devicePath / "mem_info_vis_vram_total");
                }

                gpus.push_back(std::move(gpu));
            }

            if (gpus.empty()) {
                gpus.push_back(GpuInfo{.name = "unknown"});
            }
            enrichNvidiaGpuInfo(gpus);
            enrichNvidiaGpuInfoFromProc(gpus);
            return gpus;
#elif defined(__APPLE__)
            std::vector<GpuInfo> gpus;
            const std::string output = readCommandOutput("system_profiler SPDisplaysDataType 2>/dev/null");
            std::istringstream stream(output);
            std::string line;
            GpuInfo current;
            bool inGpu = false;
            while (std::getline(stream, line)) {
                const std::string text = trim(line);
                if (text.rfind("Chipset Model:", 0) == 0) {
                    if (inGpu) {
                        gpus.push_back(current);
                        current = {};
                    }
                    current.name = trim(text.substr(std::string("Chipset Model:").size()));
                    inGpu = true;
                } else if (text.rfind("VRAM", 0) == 0) {
                    const auto colon = text.find(':');
                    if (colon != std::string::npos) {
                        const std::string value = trim(text.substr(colon + 1));
                        current.driver = value;
                    }
                }
            }
            if (inGpu) {
                gpus.push_back(current);
            }
            if (gpus.empty()) {
                gpus.push_back(GpuInfo{.name = "unknown"});
            }
            return gpus;
#elif defined(_WIN32)
            std::vector<GpuInfo> gpus;
            const std::string output = readCommandOutput("wmic path win32_VideoController get Name,AdapterRAM /format:list 2>NUL");
            std::istringstream stream(output);
            std::string line;
            GpuInfo current;
            while (std::getline(stream, line)) {
                line = trim(line);
                if (line.empty()) {
                    if (!current.name.empty()) {
                        gpus.push_back(current);
                        current = {};
                    }
                    continue;
                }
                if (line.rfind("Name=", 0) == 0) {
                    current.name = trim(line.substr(5));
                } else if (line.rfind("AdapterRAM=", 0) == 0) {
                    current.vramBytes = static_cast<uint64_t>(std::strtoull(line.c_str() + 11, nullptr, 10));
                }
            }
            if (!current.name.empty()) {
                gpus.push_back(current);
            }
            if (gpus.empty()) {
                gpus.push_back(GpuInfo{.name = "unknown"});
            }
            enrichNvidiaGpuInfo(gpus);
            return gpus;
#else
            return {GpuInfo{.name = "unknown"}};
#endif
        }

        std::string formatBytes(uint64_t bytes) {
            if (bytes == 0) {
                return "unknown";
            }

            static constexpr std::array<const char*, 5> units{"B", "KB", "MB", "GB", "TB"};
            double value = static_cast<double>(bytes);
            std::size_t unitIndex = 0;
            while (value >= 1024.0 && unitIndex + 1 < units.size()) {
                value /= 1024.0;
                ++unitIndex;
            }

            std::ostringstream out;
            out << std::fixed << std::setprecision(unitIndex >= 3 ? 1 : 0) << value << ' ' << units[unitIndex];
            return out.str();
        }

        std::string formatVram(uint64_t bytes) {
            return formatBytes(bytes);
        }
    }

    SystemInfo collectSystemInfo() {
        SystemInfo info;
        info.os = detectOsName();
        info.arch = detectArchName();
        info.totalRamBytes = detectTotalRamBytes();
        info.engine = detectEngineInfo();
        info.cpu.name = detectCpuName();
        info.cpu.simd = detectSimdWidth();
        info.cpu.cores = detectCpuCores();
        info.cpu.threads = detectCpuThreads();
        info.gpus = detectGpuInfo();
        return info;
    }

    void printSystemInfo(std::ostream& out, const SystemInfo& info) {
        out << colorize("System", kTitle) << '\n';
        out << colorize("├─", kDim) << ' ' << colorize("OS:", kLabel) << ' ' << colorize(info.os, kValue) << '\n';
        out << colorize("├─", kDim) << ' ' << colorize("Arch:", kLabel) << ' ' << colorize(info.arch, kValue) << '\n';
        out << colorize("├─", kDim) << ' ' << colorize("RAM:", kLabel) << ' ' << colorize(formatBytes(info.totalRamBytes), kValue) << '\n';
        out << colorize("├─", kDim) << ' ' << colorize("Engine", kTitle) << '\n';
        out << colorize("│", kDim) << "  " << colorize("├─", kDim) << ' ' << colorize("Version:", kLabel) << ' '
            << colorize(info.engine.version, kValue) << '\n';
        out << colorize("│", kDim) << "  " << colorize("├─", kDim) << ' ' << colorize("Build:", kLabel) << ' '
            << colorize(info.engine.build, kValue) << '\n';
        out << colorize("│", kDim) << "  " << colorize("└─", kDim) << ' ' << colorize("Compiler:", kLabel) << ' '
            << colorize(info.engine.compiler, kValue) << '\n';
        out << colorize("└─", kDim) << ' ' << colorize("Devices", kTitle) << '\n';
        out << "   " << colorize("├─", kDim) << ' ' << colorize("CPU[0]:", kDevice) << ' ' << colorize(info.cpu.name, kValue) << '\n';
        out << "   " << colorize("│", kDim) << "  " << colorize("├─", kDim) << ' ' << colorize("Cores:", kLabel) << ' '
            << colorize(info.cpu.cores == 0 ? std::string("unknown") : std::to_string(info.cpu.cores), kValue) << '\n';
        out << "   " << colorize("│", kDim) << "  " << colorize("├─", kDim) << ' ' << colorize("Threads:", kLabel) << ' '
            << colorize(info.cpu.threads == 0 ? std::string("unknown") : std::to_string(info.cpu.threads), kValue) << '\n';
        out << "   " << colorize("│", kDim) << "  " << colorize("└─", kDim) << ' ' << colorize("SIMD:", kLabel) << ' '
            << colorize(info.cpu.simd, kValue) << '\n';

        if (info.gpus.empty()) {
            out << "   " << colorize("└─", kDim) << ' ' << colorize("GPU[0]:", kDevice) << ' ' << colorize("unknown", kValue) << '\n';
            return;
        }

        for (std::size_t i = 0; i < info.gpus.size(); ++i) {
            const bool lastGpu = i + 1 == info.gpus.size();
            const std::string branch = lastGpu ? "   └─ " : "   ├─ ";
            const std::string pipe = lastGpu ? "      " : "   │  ";
            const GpuInfo& gpu = info.gpus[i];

            out << colorize(branch.substr(0, 3), kReset) << colorize(branch.substr(3), kDim) << colorize("GPU[" + std::to_string(i) + "]:", kDevice) << ' '
                << colorize(gpu.name, kValue) << '\n';
            if (!gpu.pciAddress.empty()) {
                out << colorize(pipe.substr(0, pipe.size() - 3), kReset) << colorize(pipe.substr(pipe.size() - 3), kDim)
                    << colorize("├─", kDim) << ' ' << colorize("PCI:", kLabel) << ' ' << colorize(gpu.pciAddress, kValue) << '\n';
            }
            if (!gpu.driver.empty()) {
                out << colorize(pipe.substr(0, pipe.size() - 3), kReset) << colorize(pipe.substr(pipe.size() - 3), kDim)
                    << colorize("├─", kDim) << ' ' << colorize("Driver:", kLabel) << ' ' << colorize(gpu.driver, kValue) << '\n';
            }
            if (!gpu.computeInfo.empty()) {
                out << colorize(pipe.substr(0, pipe.size() - 3), kReset) << colorize(pipe.substr(pipe.size() - 3), kDim)
                    << colorize("├─", kDim) << ' ' << colorize("Compute:", kLabel) << ' ' << colorize(gpu.computeInfo, kValue) << '\n';
            }
            out << colorize(pipe.substr(0, pipe.size() - 3), kReset) << colorize(pipe.substr(pipe.size() - 3), kDim)
                << colorize("└─", kDim) << ' ' << colorize("VRAM:", kLabel) << ' ' << colorize(formatVram(gpu.vramBytes), kValue) << '\n';
        }
        out << '\n';
    }

    void printSystemInfo(std::ostream& out) {
        printSystemInfo(out, collectSystemInfo());
    }
}
