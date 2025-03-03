#pragma once
#include <cstdint>
#include <string>
#include <vector>

enum eManifestType : uint8_t {
    MANIFEST_HYPRLOAD,
    MANIFEST_HYPRPM
};

class CManifest {
  public:
    CManifest(const eManifestType TYPE, const std::string& path);

    struct SManifestPlugin {
        std::string              name;
        std::string              description;
        std::string              version;
        std::vector<std::string> authors;
        std::vector<std::string> buildSteps;
        std::string              output;
        int                      since  = 0;
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