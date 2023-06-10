#include "slix.h"

#include "fsx/Writer.h"
#include "fsx/Reader.h"

namespace {
void app();

auto cli = clice::Argument{ .arg    = "archive",
                            .desc   = "creates a gar file",
                            .cb     = app,
};

auto cliInput = clice::Argument{ .arg    = "--input",
                                 .desc   = "path to the folder, that should be archived",
                                 .value  = std::string{},
};

auto cliOutput = clice::Argument{ .arg    = "--output",
                                  .desc   = "name of the file that is being outputed",
                                  .value  = std::string{},
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
    if (!cliInput) throw std::runtime_error("parameter \"--input\" is missing");
    if (!std::filesystem::exists(*cliInput)) throw std::runtime_error("input path " + *cliInput + " doesn't exists");
    if (!std::filesystem::exists(*cliInput + "/dependencies.txt" )) throw std::runtime_error("dependency list " + *cliInput + "/dependencies.txt doesn't exists");
    if (!std::filesystem::exists(*cliInput + "/rootfs" )) throw std::runtime_error("rootfs " + *cliInput + "/rootfs doesn't exists");
    if (!cliOutput) {
        throw std::runtime_error("parameter \"--output\" is missing");
    }

    auto path = std::filesystem::path{*cliInput};
    auto wfs = fsx::Writer{*cliOutput};
    addFolder(path, path, wfs);
    wfs.close();
}
}
