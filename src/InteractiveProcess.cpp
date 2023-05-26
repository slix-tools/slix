#include "InteractiveProcess.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

namespace process {

struct InteractiveProcess::PImpl {
    int   fdp, fdc; // parent/child fd
    int   rc;
    char  input[65536];
    pid_t pid;
    int   status;

    std::filesystem::path oldCwd {"."};

    PImpl(std::vector<std::string> const& prog, std::filesystem::path const& _cwd) {
        fdp = posix_openpt(O_RDWR);
        if (fdp < 0) {
            throw std::runtime_error("InteractiveProcess: call posix_openpt() failed");
        }
        rc = grantpt(fdp);
        if (rc != 0) {
            throw std::runtime_error("InteractiveProcess: call grantpt() failed");
        }

        rc = unlockpt(fdp);
        if (rc != 0) {
            throw std::runtime_error("InteractiveProcess: call unlockpt() failed");
        }
        fdc = open(ptsname(fdp), O_RDWR);

        pid = fork();
        if (pid == 0) {
            oldCwd = std::filesystem::current_path();
            std::filesystem::current_path(_cwd);

            childProcess(prog);
        } else {
            parentProcess();
        }
    }

    ~PImpl() {
        std::filesystem::current_path(oldCwd);
    }
private:
    void childProcess(std::vector<std::string> const& _prog) {
        struct termios child_orig_term_settings; // Saved terminal settings
        struct termios new_term_settings; // Current terminal settings

        // Close the parent side of the PTY
        close(fdp);

        // Save the defaults parameters of the child side of the PTY
        rc = tcgetattr(fdc, &child_orig_term_settings);

        // Set RAW mode on child side of PTY
        new_term_settings = child_orig_term_settings;
        cfmakeraw (&new_term_settings);
        tcsetattr (fdc, TCSANOW, &new_term_settings);

        // The child side of the PTY becomes the standard input and outputs of the child process
        close(0); // Close standard input (current terminal)
        close(1); // Close standard output (current terminal)
        close(2); // Close standard error (current terminal)

        int error;
        error = dup(fdc); // PTY becomes standard input (0)
        if (error == -1) {
            throw std::runtime_error(std::string("Error in InteractiveProcess creating pipes: (") + std::to_string(errno) + ") " + strerror(errno));
        }

        error = dup(fdc); // PTY becomes standard output (1)
        if (error == -1) {
            throw std::runtime_error(std::string("Error in InteractiveProcess creating pipes: (") + std::to_string(errno) + ") " + strerror(errno));
        }

        error = dup(fdc); // PTY becomes standard error (2)
        if (error == -1) {
            throw std::runtime_error(std::string("Error in InteractiveProcess creating pipes: (") + std::to_string(errno) + ") " + strerror(errno));
        }


        // Now the original file descriptor is useless
        close(fdc);

        // Make the current process a new session leader
        setsid();

        // As the child is a session leader, set the controlling terminal to be the hild side of the PTY
        // (Mandatory for programs like the shell to make them manage correctly their outputs)
        ioctl(0, TIOCSCTTY, 1);

        std::string envPath = getenv("PATH");
        auto execStr = _prog[0];


        auto argv = std::vector<char const*>{};
        for (auto& a : _prog) {
            argv.push_back(a.c_str());
        }
        argv.push_back(nullptr);
        auto envp = std::vector<char const*>{};
        auto path = std::filesystem::current_path();
        std::string PATH="PATH=" + path.string() + "/usr/bin";
        std::string LD_LIBRARY_PATH="LD_LIBRARY_PATH=" + path.string() + "/usr/lib";
        envp.push_back(PATH.c_str());
        envp.push_back(LD_LIBRARY_PATH.c_str());
        envp.push_back(nullptr);

        execvpe(execStr.c_str(), (char**)argv.data(), (char**)envp.data());
        exit(127); /* only if execv fails */
    }
    void parentProcess() {
        fd_set fd_in;

        // Close the child side of the PTY
        close(fdc);

        while (true) {
            // Wait for data from standard input and parent side of PTY
            FD_ZERO(&fd_in);
            FD_SET(0, &fd_in);
            FD_SET(fdp, &fd_in);

            rc = select(fdp + 1, &fd_in, NULL, NULL, NULL);
            if (rc == -1) {
                throw std::runtime_error("InteractiveProcess: error on select()");
            }

            // If data on standard input
            if (FD_ISSET(0, &fd_in)) {
                rc = read(0, input, sizeof(input));
                if (rc == 0) { // end of file
                    close(fdp);
                    break;
                }
                if (rc < 0) {
                    throw std::runtime_error("error on reading standard input");
                }
                // Send data on the parent side of PTY
                int error = write(fdp, input, rc);
                if (error == -1) {
                    throw std::runtime_error(std::string("Error reading standard input: (") + std::to_string(errno) + ") " + strerror(errno));
                }
            }

            // If data on parent side of PTY
            if (FD_ISSET(fdp, &fd_in)) {

                rc = read(fdp, input, sizeof(input));
                if (rc < 0) {
                    waitpid(pid, &status, 0); /* wait for child to exit */
                    return;
                }
                if (rc > 0) {
                    // Send data on standard output
                    int error = write(1, input, rc);
                    if (error == -1) {
                        throw std::runtime_error(std::string("Error reading standard input: (") + std::to_string(errno) + ") " + strerror(errno));
                    }
                }
            }
        }
    }
};

InteractiveProcess::InteractiveProcess(std::vector<std::string> const& prog, std::filesystem::path const& _cwd)
    : pimpl { std::make_unique<PImpl>(prog, _cwd) }
{}

InteractiveProcess::~InteractiveProcess() = default;

void InteractiveProcess::sendSignal(int signal) {
    kill(pimpl->pid, signal);
}


}
