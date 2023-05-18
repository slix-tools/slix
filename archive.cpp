#include "fsx/Writer.h"
#include "fsx/Reader.h"

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

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "invalid call\n";
        return 0;
    }
    auto path = std::filesystem::path{argv[1]};
    auto wfs = fsx::Writer{path.string() + ".gar"};
    addFolder(path, path, wfs);
    wfs.close();

    auto rfs = fsx::Reader{path.string() + ".gar"};

    for (auto entry = rfs.readNext(); entry; entry = rfs.readNext()) {
        auto const& [header, name, offset] = *entry;
        std::cout << "read file: " << name << "\n";
    }
}
