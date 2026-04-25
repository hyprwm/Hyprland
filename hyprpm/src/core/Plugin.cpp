#include "Plugin.hpp"

SPluginRepoIdentifier SPluginRepoIdentifier::fromUrl(const std::string& url) {
    return SPluginRepoIdentifier{.type = IDENTIFIER_URL, .url = url};
}

SPluginRepoIdentifier SPluginRepoIdentifier::fromName(const std::string& name) {
    return SPluginRepoIdentifier{.type = IDENTIFIER_NAME, .name = name};
}

SPluginRepoIdentifier SPluginRepoIdentifier::fromAuthorName(const std::string& author, const std::string& name) {
    return SPluginRepoIdentifier{.type = IDENTIFIER_AUTHOR_NAME, .name = name, .author = author};
}

SPluginRepoIdentifier SPluginRepoIdentifier::fromString(const std::string& string) {
    if (string.find(':') != std::string::npos) {
        return SPluginRepoIdentifier{.type = IDENTIFIER_URL, .url = string};
    } else {
        auto slashPos = string.find('/');
        if (slashPos != std::string::npos) {
            std::string author = string.substr(0, slashPos);
            std::string name   = string.substr(slashPos + 1, string.size() - slashPos - 1);
            return SPluginRepoIdentifier{.type = IDENTIFIER_AUTHOR_NAME, .name = name, .author = author};
        } else {
            return SPluginRepoIdentifier{.type = IDENTIFIER_NAME, .name = string};
        }
    }
}

std::string SPluginRepoIdentifier::toString() const {
    switch (type) {
        case IDENTIFIER_NAME: return name;
        case IDENTIFIER_AUTHOR_NAME: return author + '/' + name;
        case IDENTIFIER_URL: return url;
    }

    return "";
}

bool SPluginRepoIdentifier::matches(const std::string& url, const std::string& name, const std::string& author) const {
    switch (type) {
        case IDENTIFIER_URL: return this->url == url;
        case IDENTIFIER_NAME: return this->name == name;
        case IDENTIFIER_AUTHOR_NAME: return this->author == author && this->name == name;
    }

    return false;
}
