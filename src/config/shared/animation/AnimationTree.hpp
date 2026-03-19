#pragma once

#include <string>

#include "../../../helpers/memory/Memory.hpp"

#include <hyprutils/animation/AnimationConfig.hpp>

namespace Config {
    class CAnimationTreeController {
      public:
        CAnimationTreeController();

        const std::unordered_map<std::string, SP<Hyprutils::Animation::SAnimationPropertyConfig>>& getAnimationConfig();
        SP<Hyprutils::Animation::SAnimationPropertyConfig>                                         getAnimationPropertyConfig(const std::string&);

        //
        void reset();

        //
        void setConfigForNode(const std::string& name, bool enabled, float speed, const std::string& bezier, const std::string& style = "");
        bool nodeExists(const std::string& name);

      private:
        Hyprutils::Animation::CAnimationConfigTree m_animationTree;
    };

    UP<CAnimationTreeController>& animationTree();
};