#pragma once

#include <string>
#include <vector>

enum eManifestType {
    MANIFEST_HYPRLOAD,
    MANIFEST_HYPRPM
};

class CManifest {
  public:
    CManifest(const eManifestType type, const std::string& path);

    struct SManifestPlugin {
        std::string              name;
        std::string              description;
        std::string              version;
        std::vector<std::string> authors;
        std::vector<std::string> buildSteps;
        std::string              output;
        bool                     failed = false;
    };

    struct {
        std::string                                      name;
        std::vector<std::string>                         authors;
        std::vector<std::pair<std::string, std::string>> commitPins;
    } m_sRepository;

    std::vector<SManifestPlugin> m_vPlugins;
    bool                         m_bGood = true;
};