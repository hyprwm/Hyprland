#include <re2/re2.h>
#include "DynamicPermissionManager.hpp"
#include <algorithm>
#include <wayland-server-core.h>
#include <expected>
#include <filesystem>
#include "../../Compositor.hpp"
#include "../../config/ConfigValue.hpp"

#include <hyprutils/string/String.hpp>
using namespace Hyprutils::String;

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
    wl_display_add_destroy_listener(g_pCompositor->m_wlDisplay, &m_destroyWrapper.listener);
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
        case PERMISSION_TYPE_PLUGIN: return "PERMISSION_TYPE_PLUGIN";
    }

    return "error";
}

static const char* permissionToHumanString(eDynamicPermissionType type) {
    switch (type) {
        case PERMISSION_TYPE_UNKNOWN: return "requesting an unknown permission";
        case PERMISSION_TYPE_SCREENCOPY: return "trying to capture your screen";
        case PERMISSION_TYPE_PLUGIN: return "trying to load a plugin";
    }

    return "error";
}

static std::expected<std::string, std::string> binaryNameForPid(pid_t pid) {
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

static std::expected<std::string, std::string> binaryNameForWlClient(wl_client* client) {
    pid_t pid = 0;
    wl_client_get_credentials(client, &pid, nullptr, nullptr);

    return binaryNameForPid(pid);
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

eDynamicPermissionAllowMode CDynamicPermissionManager::clientPermissionModeWithString(pid_t pid, const std::string& str, eDynamicPermissionType permission) {
    static auto PPERM = CConfigValue<Hyprlang::INT>("ecosystem:enforce_permissions");

    if (*PPERM == 0)
        return PERMISSION_RULE_ALLOW_MODE_ALLOW;

    const auto LOOKUP = binaryNameForPid(pid);

    Debug::log(TRACE, "CDynamicPermissionManager::clientHasPermission: checking permission {} for key {} (binary {})", permissionToString(permission), str,
               LOOKUP.has_value() ? LOOKUP.value() : "lookup failed: " + LOOKUP.error());

    // first, check if we have the client + perm combo in our cache.
    auto it = std::ranges::find_if(m_rules, [str, permission, pid](const auto& e) { return e->m_keyString == str && pid && pid == e->m_pid && e->m_type == permission; });
    if (it == m_rules.end()) {
        Debug::log(TRACE, "CDynamicPermissionManager::clientHasPermission: permission not cached, checking key");

        it = std::ranges::find_if(m_rules, [key = str, permission, &LOOKUP](const auto& e) {
            if (e->m_type != permission)
                return false; // wrong perm

            if (!e->m_binaryRegex)
                return false; // no regex

            // regex match
            if (RE2::FullMatch(key, *e->m_binaryRegex) || (LOOKUP.has_value() && RE2::FullMatch(LOOKUP.value(), *e->m_binaryRegex)))
                return true;

            return false;
        });

        if (it == m_rules.end())
            Debug::log(TRACE, "CDynamicPermissionManager::clientHasPermission: no rule for key");
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
    askForPermission(nullptr, str, permission, pid);

    return PERMISSION_RULE_ALLOW_MODE_PENDING;
}

void CDynamicPermissionManager::askForPermission(wl_client* client, const std::string& binaryPath, eDynamicPermissionType type, pid_t pid) {
    auto rule = m_rules.emplace_back(SP<CDynamicPermissionRule>(new CDynamicPermissionRule(client, type, PERMISSION_RULE_ALLOW_MODE_PENDING)));

    if (!client)
        rule->m_keyString = binaryPath;

    rule->m_pid = pid;

    std::string description = "";
    if (binaryPath.empty())
        description = std::format("An unknown application (wayland client ID 0x{:x}) is {}.", (uintptr_t)client, permissionToHumanString(type));
    else if (client) {
        std::string binaryName = binaryPath.contains("/") ? binaryPath.substr(binaryPath.find_last_of('/') + 1) : binaryPath;
        description            = std::format("An application <b>{}</b> ({}) is {}.", binaryName, binaryPath, permissionToHumanString(type));
    } else if (pid >= 0) {
        if (type == PERMISSION_TYPE_PLUGIN) {
            const auto LOOKUP = binaryNameForPid(pid);
            description       = std::format("An application <b>{}</b> is {}:<br/><b>{}</b>", LOOKUP.value_or("Unknown"), permissionToHumanString(type), binaryPath);
        } else {
            const auto LOOKUP = binaryNameForPid(pid);
            description       = std::format("An application <b>{}</b> ({}) is {}.", LOOKUP.value_or("Unknown"), binaryPath, permissionToHumanString(type));
        }
    } else
        description = std::format("An application is {}:<br/><b>{}</b>", permissionToHumanString(type), binaryPath);

    description += "<br/><br/>Do you want to allow this?";

    std::vector<std::string> options;

    if (!binaryPath.empty() && client) {
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

    rule->m_promise = rule->m_dialogBox->open();
    rule->m_promise->then([r = WP<CDynamicPermissionRule>(rule), binaryPath](SP<CPromiseResult<std::string>> pr) {
        if (!r)
            return;

        if (pr->hasError()) {
            // not reachable for now
            Debug::log(TRACE, "CDynamicPermissionRule: error spawning dialog box");
            if (r->m_promiseResolverForExternal)
                r->m_promiseResolverForExternal->reject("error spawning dialog box");
            r->m_promiseResolverForExternal.reset();
            return;
        }

        const std::string& result = pr->result();

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

        if (r->m_promiseResolverForExternal)
            r->m_promiseResolverForExternal->resolve(r->m_allowMode);

        r->m_promise.reset();
        r->m_promiseResolverForExternal.reset();
    });
}

SP<CPromise<eDynamicPermissionAllowMode>> CDynamicPermissionManager::promiseFor(wl_client* client, eDynamicPermissionType permission) {
    auto rule = std::ranges::find_if(m_rules, [&client, &permission](const auto& e) { return e->m_client == client && e->m_type == permission; });
    if (rule == m_rules.end())
        return nullptr;

    if (!(*rule)->m_promise)
        return nullptr;

    if ((*rule)->m_promiseResolverForExternal)
        return nullptr;

    return CPromise<eDynamicPermissionAllowMode>::make([rule](SP<CPromiseResolver<eDynamicPermissionAllowMode>> r) { (*rule)->m_promiseResolverForExternal = r; });
}

SP<CPromise<eDynamicPermissionAllowMode>> CDynamicPermissionManager::promiseFor(const std::string& key, eDynamicPermissionType permission) {
    auto rule = std::ranges::find_if(m_rules, [&key, &permission](const auto& e) { return e->m_keyString == key && e->m_type == permission; });
    if (rule == m_rules.end())
        return nullptr;

    if (!(*rule)->m_promise)
        return nullptr;

    if ((*rule)->m_promiseResolverForExternal)
        return nullptr;

    return CPromise<eDynamicPermissionAllowMode>::make([rule](SP<CPromiseResolver<eDynamicPermissionAllowMode>> r) { (*rule)->m_promiseResolverForExternal = r; });
}

SP<CPromise<eDynamicPermissionAllowMode>> CDynamicPermissionManager::promiseFor(pid_t pid, const std::string& key, eDynamicPermissionType permission) {
    auto rule = std::ranges::find_if(m_rules, [&pid, &permission, &key](const auto& e) { return e->m_pid == pid && e->m_keyString == key && e->m_type == permission; });
    if (rule == m_rules.end())
        return nullptr;

    if (!(*rule)->m_promise)
        return nullptr;

    if ((*rule)->m_promiseResolverForExternal)
        return nullptr;

    return CPromise<eDynamicPermissionAllowMode>::make([rule](SP<CPromiseResolver<eDynamicPermissionAllowMode>> r) { (*rule)->m_promiseResolverForExternal = r; });
}

void CDynamicPermissionManager::removeRulesForClient(wl_client* client) {
    std::erase_if(m_rules, [client](const auto& e) { return e->m_client == client; });
}
