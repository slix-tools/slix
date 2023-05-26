#pragma once

#include <memory>
#include <filesystem>
#include <string>
#include <vector>

namespace process {

class InteractiveProcess final {
private:
    struct PImpl;
    std::unique_ptr<PImpl> pimpl;
public:
    InteractiveProcess(std::vector<std::string> const& prog, std::filesystem::path const& _cwd = std::filesystem::current_path());

    ~InteractiveProcess();
    InteractiveProcess(InteractiveProcess const&) = delete;
    InteractiveProcess(InteractiveProcess&&) = delete;
    InteractiveProcess& operator=(InteractiveProcess const&) = delete;
    InteractiveProcess& operator=(InteractiveProcess&&) = delete;

    void sendSignal(int signal);
};

}
