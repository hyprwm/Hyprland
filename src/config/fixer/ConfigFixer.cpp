#include "ConfigFixer.hpp"
#include "runner/ConfigFixRunner.hpp"

#include <hyprutils/os/File.hpp>

#include <zip.h>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <unordered_map>

using namespace Config::Supplementary;
using namespace Config;
using namespace Hyprutils::File;

const UP<CConfigFixer>& Supplementary::fixer() {
    static UP<CConfigFixer> fixer = makeUnique<CConfigFixer>();
    return fixer;
}

size_t CConfigFixer::validate(const std::vector<std::string>& paths) {
    size_t failed = 0;
    for (const auto& p : paths) {
        const auto FILE_CONTENT = readFileAsString(p);

        if (!FILE_CONTENT)
            continue;

        for (const auto& r : Supplementary::fixRunners) {
            failed += !r->check(*FILE_CONTENT);
        }
    }
    return failed;
}

std::expected<std::string, std::string> CConfigFixer::backupConfigs(const std::vector<std::string>& paths) {
    // TODO: helper
    const char* cacheHome = getenv("XDG_CACHE_HOME");
    std::string backupDir;

    if (cacheHome && cacheHome[0] != '\0')
        backupDir = std::string(cacheHome) + "/hyprland";
    else {
        const char* home = getenv("HOME");
        if (!home || home[0] == '\0')
            return std::unexpected("HOME environment variable not set");
        backupDir = std::string(home) + "/.cache/hyprland";
    }

    // create backup directory if it doesn't exist
    std::error_code ec;
    std::filesystem::create_directories(backupDir, ec);
    if (ec)
        return std::unexpected(std::format("Failed to create backup directory: {}", ec.message()));

    // generate zip filename with current date and epoch
    std::time_t now       = std::time(nullptr);
    std::tm*    localTime = std::localtime(&now);
    if (!localTime)
        return std::unexpected("Failed to get local time");

    std::string zipFilename = std::format("config_backup_{:02d}_{:02d}_{:04d}_{}.zip", localTime->tm_mday, localTime->tm_mon + 1, localTime->tm_year + 1900, now);
    std::string zipPath     = backupDir + "/" + zipFilename;

    // create zip archive
    int    errorp  = 0;
    zip_t* archive = zip_open(zipPath.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &errorp);
    if (!archive) {
        zip_error_t error;
        zip_error_init_with_code(&error, errorp);
        std::string errorMsg = std::format("Failed to create zip archive: {}", zip_error_strerror(&error));
        zip_error_fini(&error);
        return std::unexpected(std::move(errorMsg));
    }

    // track filenames to handle collisions
    std::unordered_map<std::string, int> filenameCount;

    // add files to the archive
    for (const auto& path : paths) {
        const auto FILE = readFileAsString(path);

        if (!FILE) // skip inaccessible files, we can't corrupt em
            continue;

        // extract filename from path to handle collisions
        std::filesystem::path fsPath(path);
        std::string           filename   = fsPath.filename().string();
        std::string           targetName = filename;

        if (filenameCount.contains(filename)) {
            filenameCount[filename]++;

            // split filename into name and extension
            size_t      dotPos    = filename.find_last_of('.');
            std::string baseName  = (dotPos != std::string::npos) ? filename.substr(0, dotPos) : filename;
            std::string extension = (dotPos != std::string::npos) ? filename.substr(dotPos) : "";

            targetName = baseName + "_" + std::to_string(filenameCount[filename]) + extension;
        } else
            filenameCount[filename] = 1;

        std::string zipEntryPath = "config/" + targetName;

        // create zip source from a text buffer we read
        zip_source_t* source = zip_source_buffer(archive, FILE->data(), FILE->size(), 0);
        if (!source) {
            zip_close(archive);
            std::filesystem::remove(zipPath, ec);
            return std::unexpected(std::format("Failed to create zip source for file: {}", path));
        }

        if (zip_file_add(archive, zipEntryPath.c_str(), source, ZIP_FL_ENC_UTF_8) < 0) {
            std::string errorMsg = std::format("Failed to add file to archive: {}", zip_strerror(archive));
            zip_source_free(source);
            zip_close(archive);
            std::filesystem::remove(zipPath, ec);
            return std::unexpected(std::move(errorMsg));
        }
    }

    if (zip_close(archive) < 0)
        return std::unexpected("Failed to finalize zip archive");

    return zipPath;
}

bool CConfigFixer::fix(const std::vector<std::string>& paths) {
    for (const auto& p : paths) {
        auto FILE_CONTENT = readFileAsString(p);

        if (!FILE_CONTENT)
            continue;

        std::string newFileContent = std::move(*FILE_CONTENT);

        for (const auto& r : Supplementary::fixRunners) {
            if (!r->check(newFileContent))
                newFileContent = r->run(newFileContent);
        }

        // write to file
        std::ofstream ofs(p, std::ios::trunc);
        ofs << newFileContent;
        ofs.close();
    }

    return validate(paths) == 0;
}