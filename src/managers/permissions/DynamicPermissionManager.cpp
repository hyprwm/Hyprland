#include <re2/re2.h>
#include "DynamicPermissionManager.hpp"
#include <algorithm>
#include <wayland-server-core.h>
#include <expected>
#include <filesystem>
#include "../../Compositor.hpp"
#include "../../config/ConfigValue.hpp"

#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/sysctl.h>
#endif

static void clientDestroyInternal(struct wl_listener* listener, void* data) {
    SDynamicPermissionRuleDestroyWrapper* wrap = wl_container_of(listener, wrap, listener);
    CDynamicPermissionRule*               rule = wrap->parent;
    g_pDynamicPermissionManager->removeRulesForClient(rule->client());
}

CDynamicPermissionRule::CDynamicPermissionRule(const std::string& binaryPathRegex, eDynamicPermissionType type, eDynamicPermissionAllowMode defaultAllowMode) :
    m_type(type), m_source(PERMISSION_RULE_SOURCE_CONFIG), m_binaryRegex(makeUnique<re2::RE2>(binaryPathRegex)), m_allowMode(defaultAllowMode) {
    ;
}

CDynamicPermissionRule::CDynamicPermissionRule(wl_client* const client, eDynamicPermissionType type, eDynamicPermissionAllowMode defaultAllowMode) :
    m_type(type), m_source(PERMISSION_RULE_SOURCE_RUNTIME_USER), m_client(client), m_allowMode(defaultAllowMode) {
    wl_list_init(&m_destroyWrapper.listener.link);
    m_destroyWrapper.listener.notify = ::clientDestroyInternal;
    m_destroyWrapper.parent          = this;
    wl_display_add_destroy_listener(g_pCompositor->m_sWLDisplay, &m_destroyWrapper.listener);
}

CDynamicPermissionRule::~CDynamicPermissionRule() {
    if (m_client) {
        wl_list_remove(&m_destroyWrapper.listener.link);
        wl_list_init(&m_destroyWrapper.listener.link);
    }

    if (m_dialogBox && m_dialogBox->isRunning())
        m_dialogBox->kill();
}

wl_client* CDynamicPermissionRule::client() const {
    return m_client;
}

static const char* permissionToString(eDynamicPermissionType type) {
    switch (type) {
        case PERMISSION_TYPE_UNKNOWN: return "PERMISSION_TYPE_UNKNOWN";
        case PERMISSION_TYPE_SCREENCOPY: return "PERMISSION_TYPE_SCREENCOPY";
    }

    return "error";
}

static const char* permissionToHumanString(eDynamicPermissionType type) {
    switch (type) {
        case PERMISSION_TYPE_UNKNOWN: return "requesting an unknown permission";
        case PERMISSION_TYPE_SCREENCOPY: return "trying to capture your screen";
    }

    return "error";
}

static std::expected<std::string, std::string> binaryNameForWlClient(wl_client* client) {
    pid_t pid = 0;
    wl_client_get_credentials(client, &pid, nullptr, nullptr);

    if (pid <= 0)
        return std::unexpected("No pid for client");

#if defined(KERN_PROC_PATHNAME)
    int mib[] = {
        CTL_KERN,
#if defined(__NetBSD__)
        KERN_PROC_ARGS,
        pid,
        KERN_PROC_PATHNAME,
#else
        KERN_PROC,
        KERN_PROC_PATHNAME,
        pid,
#endif
    };
    u_int  miblen        = sizeof(mib) / sizeof(mib[0]);
    char   exe[PATH_MAX] = "/nonexistent";
    size_t sz            = sizeof(exe);
    sysctl(mib, miblen, &exe, &sz, NULL, 0);
    std::string path = exe;
#else
    std::string path = std::format("/proc/{}/exe", (uint64_t)pid);
#endif
    std::error_code ec;

    std::string     fullPath = std::filesystem::canonical(path, ec);

    if (ec)
        return std::unexpected("canonical failed");

    return fullPath;
}

void CDynamicPermissionManager::clearConfigPermissions() {
    std::erase_if(m_rules, [](const auto& e) { return e->m_source == PERMISSION_RULE_SOURCE_CONFIG; });
}

void CDynamicPermissionManager::addConfigPermissionRule(const std::string& binaryName, eDynamicPermissionType type, eDynamicPermissionAllowMode mode) {
    m_rules.emplace_back(SP<CDynamicPermissionRule>(new CDynamicPermissionRule(binaryName, type, mode)));
}

eDynamicPermissionAllowMode CDynamicPermissionManager::clientPermissionMode(wl_client* client, eDynamicPermissionType permission) {

    static auto PPERM = CConfigValue<Hyprlang::INT>("ecosystem:enforce_permissions");

    if (*PPERM == 0)
        return PERMISSION_RULE_ALLOW_MODE_ALLOW;

    const auto LOOKUP = binaryNameForWlClient(client);

    Debug::log(TRACE, "CDynamicPermissionManager::clientHasPermission: checking permission {} for client {:x} (binary {})", permissionToString(permission), (uintptr_t)client,
               LOOKUP.has_value() ? LOOKUP.value() : "lookup failed: " + LOOKUP.error());

    // first, check if we have the client + perm combo in our cache.
    auto it = std::ranges::find_if(m_rules, [client, permission](const auto& e) { return e->m_client == client && e->m_type == permission; });
    if (it == m_rules.end()) {
        Debug::log(TRACE, "CDynamicPermissionManager::clientHasPermission: permission not cached, checking binary name");

        if (!LOOKUP.has_value())
            Debug::log(TRACE, "CDynamicPermissionManager::clientHasPermission: binary name check failed");
        else {
            const auto BINNAME = LOOKUP.value().contains("/") ? LOOKUP.value().substr(LOOKUP.value().find_last_of('/') + 1) : LOOKUP.value();
            Debug::log(TRACE, "CDynamicPermissionManager::clientHasPermission: binary path {}, name {}", LOOKUP.value(), BINNAME);

            it = std::ranges::find_if(m_rules, [clientBinaryPath = LOOKUP.value(), permission](const auto& e) {
                if (e->m_type != permission)
                    return false; // wrong perm

                if (!e->m_binaryPath.empty() && e->m_binaryPath == clientBinaryPath)
                    return true; // matches binary path

                if (!e->m_binaryRegex)
                    return false; // wl_client* rule

                // regex match
                if (RE2::FullMatch(clientBinaryPath, *e->m_binaryRegex))
                    return true;

                return false;
            });

            if (it == m_rules.end())
                Debug::log(TRACE, "CDynamicPermissionManager::clientHasPermission: no rule for binary");
            else {
                if ((*it)->m_allowMode == PERMISSION_RULE_ALLOW_MODE_ALLOW) {
                    Debug::log(TRACE, "CDynamicPermissionManager::clientHasPermission: permission allowed by config rule");
                    return PERMISSION_RULE_ALLOW_MODE_ALLOW;
                } else if ((*it)->m_allowMode == PERMISSION_RULE_ALLOW_MODE_DENY) {
                    Debug::log(TRACE, "CDynamicPermissionManager::clientHasPermission: permission denied by config rule");
                    return PERMISSION_RULE_ALLOW_MODE_DENY;
                } else if ((*it)->m_allowMode == PERMISSION_RULE_ALLOW_MODE_PENDING) {
                    Debug::log(TRACE, "CDynamicPermissionManager::clientHasPermission: permission pending by config rule");
                    return PERMISSION_RULE_ALLOW_MODE_PENDING;
                } else
                    Debug::log(TRACE, "CDynamicPermissionManager::clientHasPermission: permission ask by config rule");
            }
        }
    } else if ((*it)->m_allowMode == PERMISSION_RULE_ALLOW_MODE_ALLOW) {
        Debug::log(TRACE, "CDynamicPermissionManager::clientHasPermission: permission allowed before by user");
        return PERMISSION_RULE_ALLOW_MODE_ALLOW;
    } else if ((*it)->m_allowMode == PERMISSION_RULE_ALLOW_MODE_DENY) {
        Debug::log(TRACE, "CDynamicPermissionManager::clientHasPermission: permission denied before by user");
        return PERMISSION_RULE_ALLOW_MODE_DENY;
    } else if ((*it)->m_allowMode == PERMISSION_RULE_ALLOW_MODE_PENDING) {
        Debug::log(TRACE, "CDynamicPermissionManager::clientHasPermission: permission pending before by user");
        return PERMISSION_RULE_ALLOW_MODE_PENDING;
    }

    // if we are here, we need to ask.
    askForPermission(client, LOOKUP.value_or(""), permission);

    return PERMISSION_RULE_ALLOW_MODE_PENDING;
}

void CDynamicPermissionManager::askForPermission(wl_client* client, const std::string& binaryPath, eDynamicPermissionType type) {
    auto        rule = m_rules.emplace_back(SP<CDynamicPermissionRule>(new CDynamicPermissionRule(client, type, PERMISSION_RULE_ALLOW_MODE_PENDING)));

    std::string description = "";
    if (binaryPath.empty())
        description = std::format("An unknown application (wayland client ID 0x{:x}) is {}.", (uintptr_t)client, permissionToHumanString(type));
    else {
        std::string binaryName = binaryPath.contains("/") ? binaryPath.substr(binaryPath.find_last_of('/') + 1) : binaryPath;
        description            = std::format("An application <b>{}</b> ({}) is {}.", binaryName, binaryPath, permissionToHumanString(type));
    }

    description += "<br/><br/>Do you want to allow this?";

    std::vector<std::string> options;

    if (!binaryPath.empty()) {
        description += "<br/><br/><i>Hint: you can set persistent rules for these in the Hyprland config file.</i>";
        options = {"Deny", "Allow and remember app", "Allow once"};
    } else
        options = {"Deny", "Allow"};

    rule->m_dialogBox = CAsyncDialogBox::create("Permission request", description, options);

    if (!rule->m_dialogBox) {
        Debug::log(ERR, "CDynamicPermissionManager::askForPermission: hyprland-qtutils likely missing, cannot ask! Disabling permission control...");
        rule->m_allowMode = PERMISSION_RULE_ALLOW_MODE_ALLOW;
        return;
    }

    rule->m_dialogBox->open([r = WP<CDynamicPermissionRule>(rule), binaryPath](std::string result) {
        if (!r)
            return;

        Debug::log(TRACE, "CDynamicPermissionRule: user returned {}", result);

        if (result.starts_with("Allow once"))
            r->m_allowMode = PERMISSION_RULE_ALLOW_MODE_ALLOW;
        else if (result.starts_with("Deny")) {
            r->m_allowMode  = PERMISSION_RULE_ALLOW_MODE_DENY;
            r->m_binaryPath = binaryPath;
        } else if (result.starts_with("Allow and remember")) {
            r->m_allowMode  = PERMISSION_RULE_ALLOW_MODE_ALLOW;
            r->m_binaryPath = binaryPath;
        } else if (result.starts_with("Allow"))
            r->m_allowMode = PERMISSION_RULE_ALLOW_MODE_ALLOW;
    });
}

void CDynamicPermissionManager::removeRulesForClient(wl_client* client) {
    std::erase_if(m_rules, [client](const auto& e) { return e->m_client == client; });
}
