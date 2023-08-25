#include "error_fmt.h"
#include "slix.h"

#include "fsx/Writer.h"
#include "fsx/Reader.h"

#include <set>

namespace {
void app();

auto cli = clice::Argument{ .args   = "archive",
                            .desc   = "creates a gar file",
                            .cb     = app,
};

auto cliInput = clice::Argument{ .parent = &cli,
                                 .args   = "--input",
                                 .desc   = "path to the folder, that should be archived",
                                 .value  = std::filesystem::path{},
};

auto cliOutput = clice::Argument{ .parent = &cli,
                                  .args   = "--output",
                                  .desc   = "name of the file that is being outputed",
                                  .value  = std::filesystem::path{},
};

void addFolder(std::filesystem::path const& _path, std::filesystem::path const& _rootPath, fsx::Writer& writer) {
    auto files = std::map<std::string, std::string>{};
    auto folders = std::set<std::string>{};
    for (auto const& dir_entry : std::filesystem::directory_iterator{_path, std::filesystem::directory_options::skip_permission_denied}) {
        auto name = dir_entry.path().filename().string();
        if (name == "." || name == "..") continue;
        files[(_path / name).string()] = (_path / name).lexically_proximate(_rootPath).string();
        if (dir_entry.is_directory() && !dir_entry.is_symlink()) {
            folders.insert((_path / name).string());
        }
    }

    for (auto const& [key, value] : files) {
        writer.addPathAs(key, value);
    }
    for (auto const& v : folders) {
        addFolder(v, _rootPath, writer);
    }
}


void app() {
    if (!cliInput) throw error_fmt{"parameter \"--input\" is missing"};
    if (!exists(*cliInput)) throw error_fmt{"input path {} doesn't exists", *cliInput};
    if (!exists(*cliInput / "meta/name.txt" )) throw error_fmt{"file with name {} doesn't exists", *cliInput / "meta/name.txt"};
    if (!exists(*cliInput / "meta/version.txt" )) throw error_fmt{"file with version {} doesn't exists", *cliInput / "meta/version.txt"};
    if (!exists(*cliInput / "meta/dependencies.txt" )) throw error_fmt{"dependency list {} doesn't exists", *cliInput / "meta/dependencies.txt"};

    if (!exists(*cliInput / "rootfs" )) throw error_fmt{"rootfs {} doesn't exists", *cliInput / "rootfs"};
    if (!cliOutput) {
        throw error_fmt{"parameter \"--output\" is missing"};
    }

    auto wfs = fsx::Writer{*cliOutput};
    addFolder(*cliInput, *cliInput, wfs);
    wfs.close();
}
}
