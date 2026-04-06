#pragma once

#include <string>
#include <filesystem>
#include <expected>

#include "./shared/Types.hpp"
#include "../helpers/memory/Memory.hpp"

#include "values/types/IValue.hpp"

namespace Config {

    struct SConfigOptionReply {
        // <type>* const*
        void* const*          dataptr   = nullptr;
        const std::type_info* type      = nullptr;
        bool                  setByUser = false;
    };

    enum eConfigManagerType : uint8_t {
        CONFIG_LEGACY = 0,
        CONFIG_LUA
    };

    class IConfigManager {
      protected:
        IConfigManager() = default;

      public:
        virtual ~IConfigManager() = default;

        virtual eConfigManagerType               type() = 0;

        virtual void                             init()   = 0;
        virtual void                             reload() = 0;
        virtual std::string                      verify() = 0;

        virtual int                              getDeviceInt(const std::string&, const std::string&, const std::string& fallback = "")    = 0;
        virtual float                            getDeviceFloat(const std::string&, const std::string&, const std::string& fallback = "")  = 0;
        virtual Vector2D                         getDeviceVec(const std::string&, const std::string&, const std::string& fallback = "")    = 0;
        virtual std::string                      getDeviceString(const std::string&, const std::string&, const std::string& fallback = "") = 0;
        virtual bool                             deviceConfigExplicitlySet(const std::string&, const std::string&)                         = 0;
        virtual bool                             deviceConfigExists(const std::string&)                                                    = 0;

        virtual SConfigOptionReply               getConfigValue(const std::string&) = 0;

        virtual std::string                      getMainConfigPath() = 0;
        virtual std::string                      currentConfigPath() = 0;
        virtual std::string                      getConfigString()   = 0;
        virtual const std::vector<std::string>&  getConfigPaths()    = 0;

        virtual bool                             configVerifPassed() = 0;

        virtual std::string                      getErrors() = 0;

        virtual std::expected<void, std::string> generateDefaultConfig(const std::filesystem::path&, bool safeMode = false) = 0;

        virtual void                             handlePluginLoads() = 0;

        virtual std::expected<void, std::string> registerPluginValue(void* handle, SP<Config::Values::IValue> value) = 0;
    };

    bool                initConfigManager();

    UP<IConfigManager>& mgr();
};