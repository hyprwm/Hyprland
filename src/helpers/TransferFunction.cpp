#include "TransferFunction.hpp"
#include <string>
#include <unordered_map>

static std::unordered_map<std::string, NTransferFunction::eTF> const table = {{"default", NTransferFunction::TF_DEFAULT}, {"0", NTransferFunction::TF_DEFAULT},
                                                                              {"auto", NTransferFunction::TF_AUTO},       {"srgb", NTransferFunction::TF_SRGB},
                                                                              {"3", NTransferFunction::TF_SRGB},          {"gamma22", NTransferFunction::TF_GAMMA22},
                                                                              {"1", NTransferFunction::TF_GAMMA22},       {"gamma22force", NTransferFunction::TF_FORCED_GAMMA22},
                                                                              {"2", NTransferFunction::TF_FORCED_GAMMA22}};

NTransferFunction::eTF                                               NTransferFunction::fromString(const std::string tfName) {
    auto it = table.find(tfName);
    if (it == table.end())
        return NTransferFunction::TF_DEFAULT;
    return it->second;
}

std::string NTransferFunction::toString(eTF tf) {
    for (const auto& [key, value] : table) {
        if (value == tf)
            return key;
    }
    return "";
}
