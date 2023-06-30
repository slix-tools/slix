#include "error_fmt.h"
#include "slix.h"

#include "fsx/Writer.h"
#include "fsx/Reader.h"

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
    for (auto const& dir_entry : std::filesystem::directory_iterator{_path, std::filesystem::directory_options::skip_permission_denied}) {
        auto name = dir_entry.path().filename().string();
        if (name == "." || name == "..") continue;
        writer.addPathAs(_path / name, (_path / name).lexically_proximate(_rootPath));
        if (dir_entry.is_directory() && !dir_entry.is_symlink()) {
            addFolder(_path / name, _rootPath, writer);
        }
    }
}


void app() {
    if (!cliInput) throw error_fmt{"parameter \"--input\" is missing"};
    if (!exists(*cliInput)) throw error_fmt{"input path {} doesn't exists", *cliInput};
    if (!exists(*cliInput / "dependencies.txt" )) throw error_fmt{"dependency list {} doesn't exists", *cliInput / "dependencies.txt"};
    if (!exists(*cliInput / "rootfs" )) throw error_fmt{"rootfs {} doesn't exists", *cliInput / "rootfs"};
    if (!cliOutput) {
        throw error_fmt{"parameter \"--output\" is missing"};
    }

    auto wfs = fsx::Writer{*cliOutput};
    addFolder(*cliInput, *cliInput, wfs);
    wfs.close();
}
}
