#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

auto readLines(std::filesystem::path const& path) {
    auto r = std::vector<std::string>{};
    auto ifs = std::ifstream{path};
    for (std::string line; getline(ifs, line);) {
        r.push_back(line);
    }
    return r;
}

struct Node {
    std::vector<std::string> dependsOn;
    std::vector<std::string> provides;
};

auto createQueryPackages(std::filesystem::path p) {
//    std::system("pacman -Slq > /tmp/packages.txt");
    std::system("expac -Q '%n' > /tmp/packages.txt");

    auto ifs = std::ifstream{"/tmp/packages.txt"};

    auto pkgs = std::unordered_map<std::string, Node>{};

    for (std::string line; getline(ifs, line);) {
//        std::system(("pacman -Qi " + line + " | grep '^Depends On' | cut -d ':' -f 2 | tr ' ' '\n' | grep -v None | grep . > /tmp/slix-pkg.txt").c_str());
        std::system(("expac -Qs '%D' '^" + line + "$' | tr ' ' '\n' | grep . > /tmp/slix-pkg.txt").c_str());
        pkgs[line].dependsOn = readLines("/tmp/slix-pkg.txt");
        std::system(("expac -Qs '%P' '^" + line + "$' | tr ' ' '\n' | grep . > /tmp/slix-pkg.txt").c_str());
        pkgs[line].provides = readLines("/tmp/slix-pkg.txt");
    }

    auto ofs = std::ofstream{p};

    ofs << pkgs.size() << '\n';
    for (auto const& [key, value] : pkgs) {
        ofs << key << '\n';
        ofs << value.dependsOn.size() << '\n';
        for (auto const& v : value.dependsOn) {
            ofs << v << '\n';
        }
        ofs << value.provides.size() << '\n';
        for (auto const& v : value.provides) {
            ofs << v << '\n';
        }
    }
}

auto loadOrCreateQueryPackages(std::filesystem::path p) {
    if (!exists(p)) {
        createQueryPackages(p);
    }

    auto pkgs = std::unordered_map<std::string, Node>{};

    auto ifs = std::ifstream{p};

    std::string line;
    getline(ifs, line);
    auto max = std::stoi(line);
    for (size_t i{0}; i < max; ++i) {
        std::getline(ifs, line);
        auto& pkg = pkgs[line];
        {
            std::getline(ifs, line);
            auto ct = std::stoi(line);
            for (size_t j{0}; j < ct; ++j) {
                std::getline(ifs, line);
                pkg.dependsOn.push_back(line);
            }
        }
        {
            std::getline(ifs, line);
            auto ct = std::stoi(line);
            for (size_t j{0}; j < ct; ++j) {
                std::getline(ifs, line);
                pkg.provides.push_back(line);
            }
        }

    }
    return pkgs;
}


struct Graph {
    std::unordered_map<std::string, Node> data;
    std::unordered_map<std::string, std::string> providedBy;

    auto findCycle(std::string start) {
        auto discovered = std::unordered_set<std::string>{};
        auto stack      = std::vector<std::string>{start};
        auto marked     = std::unordered_set<std::string>{};
        return findCycle(start, stack, marked, discovered);
    }

    auto findCycle(std::string iter, std::vector<std::string>& stack, std::unordered_set<std::string>& marked, std::unordered_set<std::string>& discovered) -> std::optional<std::vector<std::string>> {
        if (!data.contains(iter)) {
            std::cout << "not a pkg: " << iter << "\n";
            return std::nullopt;
        }
        if (discovered.contains(iter)) return std::nullopt;
        if (marked.contains(iter)) {
             auto cycle = std::vector<std::string>{};
             for (size_t i{stack.size()}; i > 0; --i) {
                cycle.push_back(stack[i-1]);
                if (cycle.back() == iter) break;
             }
             return cycle;
        }
        marked.insert(iter);
        stack.push_back(iter);

        for (auto const& out : data.at(iter).dependsOn) {
            auto list = findCycle(out, stack, marked, discovered);
            if (list) return list;
        }

        marked.erase(iter);
        stack.pop_back();
        discovered.insert(iter);
        return std::nullopt;
    }

};

int main() {
    auto graph = Graph {
       .data = loadOrCreateQueryPackages("packageList.txt"),
    };
    for (auto const& [key, value] : graph.data) {
        for (auto const& p : value.provides) {
            auto iter = graph.providedBy.find(p);
            if (iter == graph.providedBy.end() || key == iter->second) {
                graph.providedBy[p] = key;
            } else {
                std::cout << "The property " << p << " is provided by " << key << " and " << iter->second << "\n";
            }
        }
    }

    for (auto const& [key, value] : graph.data) {
        auto cycle = graph.findCycle(key);
        if (cycle) {
            std::cout << "found cycle in: " << key << "\n";
            std::cout << cycle->back();
            for (auto const& x : cycle.value()) {
                std::cout << " -> " << x;
            }
            std::cout << "\n";
        }
    }
}
