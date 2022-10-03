#pragma once

#include <string>
#include <vector>

struct VSInstanceInfo {
    std::wstring InstanceId;
    std::wstring VSInstallLocation;
    std::wstring Version;
    uint64_t ullVersion{0};
    bool IsWin10SDKInstalled{false};
    bool IsWin81SDKInstalled{false};
};

std::vector<VSInstanceInfo> enumerate_vs_instances();
