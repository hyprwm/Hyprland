#pragma once

#include "../../macros.hpp"
#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/AsyncDialogBox.hpp"
#include <vector>
#include <wayland-server-core.h>
#include <optional>

// NOLINTNEXTLINE
namespace re2 {
    class RE2;
};

enum eDynamicPermissionType : uint8_t {
    PERMISSION_TYPE_UNKNOWN = 0,
    PERMISSION_TYPE_SCREENCOPY,
};

enum eDynamicPermissionRuleSource : uint8_t {
    PERMISSION_RULE_SOURCE_UNKNOWN = 0,
    PERMISSION_RULE_SOURCE_CONFIG,
    PERMISSION_RULE_SOURCE_RUNTIME_USER,
};

enum eDynamicPermissionAllowMode : uint8_t {
    PERMISSION_RULE_ALLOW_MODE_UNKNOWN = 0,
    PERMISSION_RULE_ALLOW_MODE_DENY,
    PERMISSION_RULE_ALLOW_MODE_ASK,
    PERMISSION_RULE_ALLOW_MODE_ALLOW,
    PERMISSION_RULE_ALLOW_MODE_PENDING, // popup is open
};

class CDynamicPermissionRule;

struct SDynamicPermissionRuleDestroyWrapper {
    wl_listener             listener;
    CDynamicPermissionRule* parent = nullptr;
};

class CDynamicPermissionRule {
  public:
    ~CDynamicPermissionRule();

    wl_client* client() const;

  private:
    // config rule
    CDynamicPermissionRule(const std::string& binaryPathRegex, eDynamicPermissionType type, eDynamicPermissionAllowMode defaultAllowMode = PERMISSION_RULE_ALLOW_MODE_ASK);
    // user rule
    CDynamicPermissionRule(wl_client* const client, eDynamicPermissionType type, eDynamicPermissionAllowMode defaultAllowMode = PERMISSION_RULE_ALLOW_MODE_ASK);

    const eDynamicPermissionType         m_type       = PERMISSION_TYPE_UNKNOWN;
    const eDynamicPermissionRuleSource   m_source     = PERMISSION_RULE_SOURCE_UNKNOWN;
    wl_client* const                     m_client     = nullptr;
    std::string                          m_binaryPath = "";
    UP<re2::RE2>                         m_binaryRegex;

    eDynamicPermissionAllowMode          m_allowMode = PERMISSION_RULE_ALLOW_MODE_ASK;
    SP<CAsyncDialogBox>                  m_dialogBox; // for pending

    SDynamicPermissionRuleDestroyWrapper m_destroyWrapper;

    friend class CDynamicPermissionManager;
};

class CDynamicPermissionManager {
  public:
    void clearConfigPermissions();
    void addConfigPermissionRule(const std::string& binaryPath, eDynamicPermissionType type, eDynamicPermissionAllowMode mode);

    // if the rule is "ask", or missing, will pop up a dialog and return false until the user agrees.
    // (will continue returning false if the user does not agree, of course.)
    eDynamicPermissionAllowMode clientPermissionMode(wl_client* client, eDynamicPermissionType permission);

    void                        removeRulesForClient(wl_client* client);

  private:
    void askForPermission(wl_client* client, const std::string& binaryName, eDynamicPermissionType type);

    //
    std::vector<SP<CDynamicPermissionRule>> m_rules;
};

inline UP<CDynamicPermissionManager> g_pDynamicPermissionManager;