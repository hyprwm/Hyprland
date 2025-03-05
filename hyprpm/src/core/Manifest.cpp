#include "Manifest.hpp"
#include <toml++/toml.hpp>
#include <iostream>

CManifest::CManifest(const eManifestType TYPE, const std::string& path) {
    auto manifest = toml::parse_file(path);

    if (TYPE == MANIFEST_HYPRLOAD) {
        for (auto const& [key, val] : manifest) {
            if (key.str().ends_with(".build"))
                continue;

            CManifest::SManifestPlugin plugin;
            plugin.name = key;
            m_vPlugins.push_back(plugin);
        }

        for (auto& plugin : m_vPlugins) {
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
                m_bGood = false;
                return;
            }
        }
    } else if (TYPE == MANIFEST_HYPRPM) {
        m_sRepository.name = manifest["repository"]["name"].value_or("");
        auto authors       = manifest["repository"]["authors"].as_array();
        if (authors) {
            for (auto&& a : *authors) {
                m_sRepository.authors.push_back(a.as_string()->value_or("?"));
            }
        } else {
            auto author = manifest["repository"]["author"].value_or("");
            if (!std::string{author}.empty())
                m_sRepository.authors.push_back(author);
        }

        auto pins = manifest["repository"]["commit_pins"].as_array();
        if (pins) {
            for (auto&& pin : *pins) {
                auto pinArr = pin.as_array();
                if (pinArr && pinArr->get(1))
                    m_sRepository.commitPins.push_back(std::make_pair<>(pinArr->get(0)->as_string()->get(), pinArr->get(1)->as_string()->get()));
            }
        }

        for (auto const& [key, val] : manifest) {
            if (key.str() == "repository")
                continue;

            CManifest::SManifestPlugin plugin;
            plugin.name = key;
            m_vPlugins.push_back(plugin);
        }

        for (auto& plugin : m_vPlugins) {
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
                m_bGood = false;
                return;
            }
        }
    } else {
        // ???
        m_bGood = false;
    }
}