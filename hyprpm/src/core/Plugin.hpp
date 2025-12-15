#pragma once

#include <string>
#include <vector>

struct SPlugin {
    std::string name;
    std::string filename;
    bool        enabled = false;
    bool        failed  = false;
};

struct SPluginRepository {
    std::string          url;
    std::string          rev;
    std::string          name;
    std::string          author;
    std::vector<SPlugin> plugins;
    std::string          hash;
};

enum ePluginRepoIdentifierType {
    IDENTIFIER_URL,
    IDENTIFIER_NAME,
    IDENTIFIER_AUTHOR_NAME
};

struct SPluginRepoIdentifier {
    ePluginRepoIdentifierType    type;
    std::string                  url    = "";
    std::string                  name   = "";
    std::string                  author = "";

    static SPluginRepoIdentifier fromString(const std::string& string);
    static SPluginRepoIdentifier fromUrl(const std::string& Url);
    static SPluginRepoIdentifier fromName(const std::string& name);
    static SPluginRepoIdentifier fromAuthorName(const std::string& author, const std::string& name);
    std::string                  toString() const;
    bool                         matches(const std::string& url, const std::string& name, const std::string& author) const;
};
