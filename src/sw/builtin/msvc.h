#include "vs_instance_helpers.h"

struct msvc_instance1 {
    path root;
    package_version vs_version;
};

struct msvc {
    std::vector<msvc_instance1> msvc;

    void detect(auto &&swctx) {
        auto instances = enumerate_vs_instances();
        for (auto &&i : instances) {
            path root = i.VSInstallLocation;
            auto preview = i.VSInstallLocation.contains(L"Preview");
            if (preview) {
                // continue;
            }
            auto d = root / "VC" / "Tools" / "MSVC";
            for (auto &&p : fs::directory_iterator{d}) {
                if (!package_version{p.path().filename().string()}.is_branch()) {
                    msvc.emplace_back(d / p.path(), package_version{package_version::number_version{
                                                        path{i.Version}.string(), preview ? "preview"s : ""s}});
                }
            }
        }
    }
};
