#include "Manifest.hpp"
#include <toml++/toml.hpp>
#include <algorithm>

// Alphanumerics and -_ allowed for plugin names. No magic names.
// [A-Za-z0-9\-_]*
static bool validManifestName(const std::string_view& n) {
    return std::ranges::all_of(n, [](const char& c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_' || c == '=' || (c >= '0' && c <= '9'); });
}

CManifest::CManifest(const eManifestType type, const std::string& path) {
    auto manifest = toml::parse_file(path);

    if (type == MANIFEST_HYPRLOAD) {
        for (auto const& [key, val] : manifest) {
            if (key.str().ends_with(".build"))
                continue;

            CManifest::SManifestPlugin plugin;

            if (!validManifestName(key.str())) {
                m_good = false;
                return;
            }

            plugin.name = key;
            m_plugins.push_back(plugin);
        }

        for (auto& plugin : m_plugins) {
            plugin.description = manifest[plugin.name]["description"].value_or("?");
            plugin.version     = manifest[plugin.name]["version"].value_or("?");
            plugin.output      = manifest[plugin.name]["build"]["output"].value_or("?");
            auto authors       = manifest[plugin.name]["authors"].as_array();
            if (authors) {
                for (auto&& a : *authors) {
                    plugin.authors.push_back(a.as_string()->value_or("?"));
                }
            } else {
                auto author = manifest[plugin.name]["author"].value_or("");
                if (!std::string{author}.empty())
                    plugin.authors.push_back(author);
            }
            auto buildSteps = manifest[plugin.name]["build"]["steps"].as_array();
            if (buildSteps) {
                for (auto&& s : *buildSteps) {
                    plugin.buildSteps.push_back(s.as_string()->value_or("?"));
                }
            }

            if (plugin.output.empty() || plugin.buildSteps.empty()) {
                m_good = false;
                return;
            }
        }
    } else if (type == MANIFEST_HYPRPM) {
        m_repository.name = manifest["repository"]["name"].value_or("");
        auto authors      = manifest["repository"]["authors"].as_array();
        if (authors) {
            for (auto&& a : *authors) {
                m_repository.authors.push_back(a.as_string()->value_or("?"));
            }
        } else {
            auto author = manifest["repository"]["author"].value_or("");
            if (!std::string{author}.empty())
                m_repository.authors.push_back(author);
        }

        auto pins = manifest["repository"]["commit_pins"].as_array();
        if (pins) {
            for (auto&& pin : *pins) {
                auto pinArr = pin.as_array();
                if (pinArr && pinArr->get(1))
                    m_repository.commitPins.push_back(std::make_pair<>(pinArr->get(0)->as_string()->get(), pinArr->get(1)->as_string()->get()));
            }
        }

        for (auto const& [key, val] : manifest) {
            if (key.str() == "repository")
                continue;

            CManifest::SManifestPlugin plugin;

            if (!validManifestName(key.str())) {
                m_good = false;
                return;
            }

            plugin.name = key;
            m_plugins.push_back(plugin);
        }

        for (auto& plugin : m_plugins) {
            plugin.description = manifest[plugin.name]["description"].value_or("?");
            plugin.output      = manifest[plugin.name]["output"].value_or("?");
            plugin.since       = manifest[plugin.name]["since_hyprland"].value_or(0);
            auto authors       = manifest[plugin.name]["authors"].as_array();
            if (authors) {
                for (auto&& a : *authors) {
                    plugin.authors.push_back(a.as_string()->value_or("?"));
                }
            } else {
                auto author = manifest[plugin.name]["author"].value_or("");
                if (!std::string{author}.empty())
                    plugin.authors.push_back(author);
            }
            auto buildSteps = manifest[plugin.name]["build"].as_array();
            if (buildSteps) {
                for (auto&& s : *buildSteps) {
                    plugin.buildSteps.push_back(s.as_string()->value_or("?"));
                }
            }

            if (plugin.output.empty() || plugin.buildSteps.empty()) {
                m_good = false;
                return;
            }
        }
    } else {
        // ???
        m_good = false;
    }
}