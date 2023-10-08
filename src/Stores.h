// SPDX-FileCopyrightText: 2023 S. G. Gottlieb <info.simon@gottliebtfreitag.de>
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "Store.h"

inline void storeInit() {
    auto storePath = getSlixConfigPath() / "stores";
    if (!exists(storePath)) {
        std::filesystem::create_directories(storePath);
    }
    bool foundStoreConfig = false;
    for (auto const& e : std::filesystem::directory_iterator{storePath}) {
        foundStoreConfig = true;
        break;
    }
    if (!foundStoreConfig) {
        fmt::print("no store config found, creating a default one\n");
        auto store = Store{};
        store.config = StoreConfig::defaultStoreConfig();
        store.name = "default";
        store.update();
        store.config.save(storePath / "default.yaml");
        store.state.save(getSlixStatePath() / store.name / "state.yaml");
    }
}
struct Stores {
    std::vector<Store> stores;
    std::unordered_set<std::string> installedPackages;
    struct PackageMarkings {
        bool explicitMarked{};
        size_t dependencyCount{};
        std::unordered_set<std::string> environmentFiles;
    };
    std::unordered_map<std::string, PackageMarkings> packagesMarked; // canonical packages -> list of environments, requiring (or empty, for default)

    Stores(std::filesystem::path storePath) {
        auto sortedPaths = std::set<std::string>{};
        for (auto const& e : std::filesystem::directory_iterator{storePath}) {
            sortedPaths.insert(e.path());
        }
        for (auto s : sortedPaths) {
            stores.emplace_back(s);
            for (auto const& [key, names] : stores.back().state.packages) {
                for (auto const& n : names) {
                    installedPackages.insert(n);
                }
            }
        }
        load(getSlixStatePath() / "stores.yaml");
    }

    /** \brief Save this store config into a file.
     *!TODO must be changed to an atomic write
     */
    void save(std::filesystem::path _path) {
        std::filesystem::create_directories(_path.parent_path());

        auto yaml = YAML::Node{};
        yaml["version"] = 1;
        for (auto [key, markings] : packagesMarked) {
            auto node = YAML::Node{};
            node["explicitMarked"]  = markings.explicitMarked;
            node["dependencyCount"] = markings.dependencyCount;
            for (auto l : markings.environmentFiles) {
                node["environmentFiles"].push_back(l);
            }
            yaml["packages"][key] = node;
        }

        auto ofs = std::ofstream{_path, std::ios::binary};
        auto emitter = YAML::Emitter{};
        emitter << yaml;
        fmt::print(ofs, "{}", emitter.c_str());
    }


    /** \brief load store config from file
     */
    void load(std::filesystem::path path) {
        auto yaml = YAML::LoadFile(path);
        if (yaml["version"].as<int>() == 1) {
            loadV1(yaml);
        } else {
            throw error_fmt("unknown file format");
        }
    }
private:
    void loadV1(YAML::Node yaml) {
        for (auto n : yaml["packages"]) {
            auto& marking = packagesMarked[n.first.as<std::string>()];
            marking.explicitMarked = n.second["explicitMarked"].as<bool>();
            marking.dependencyCount = n.second["dependencyCount"].as<size_t>();
            for (auto e : n.second["environmentFiles"]) {
                marking.environmentFiles.insert(e.as<std::string>());
            }
        }
    }
public:

    /** \brief check if a fully qualified package is installed
     */
    bool isInstalled(std::string pattern) const {
        // check if any store is already has it on the shelf
        return installedPackages.contains(pattern);
    }

    auto getPackagePath(std::string fullPackageName) const -> std::filesystem::path {
        auto posAt   = fullPackageName.rfind('@');
        auto posHash = fullPackageName.rfind('#');
        if (posAt == std::string::npos || posHash == std::string::npos) {
            throw error_fmt{"invalid package name {}", fullPackageName};
        }

        for (auto& store : stores) {
            if (store.isInstalled(fullPackageName)) {
                return store.getPackagePath(fullPackageName);
            }
        }
        throw error_fmt{"can't find any installed path for {}", fullPackageName};
    }

    /** \brief Fetch a package dependencies
     *
     * \param pattern: full qualified package name
     * \return: list of packages and their stores
     */
    auto getPackageAndDependencies(std::string pattern) -> std::tuple<Store*, std::unordered_set<std::string>> {
        auto posAt   = pattern.rfind('@');
        auto posHash = pattern.rfind('#');
        if (posAt == std::string::npos || posHash == std::string::npos) {
            throw error_fmt{"package is not fully qualified {}\n", pattern};
        }

        auto res = std::unordered_set<std::string>{};

        auto name    = pattern.substr(0, posAt);
        auto version = pattern.substr(posAt+1, posHash-posAt-1);
        auto hash    = pattern.substr(posHash+1);

        res.insert(pattern);

        // Fetch store and name of dependencies
        auto [store, dependencies] = [&]() -> std::tuple<Store*, std::vector<std::string>> {
            for (auto& store : stores) {
                auto index = store.loadPackageIndex();
                if (auto iter = index.packages.find(name); iter != index.packages.end()) {
                    for (auto const& info : iter->second) {
                        if (info.version == version and info.hash == hash) {
                            return {&store, info.dependencies};
                        }
                    }
                }
            }
            throw error_fmt{"couldn't find any store offering the package {}", pattern};
        }();

        for (auto const& d : dependencies) {
            res.insert(d);
        }
        return {store, res};
    }

    /** Will install the package matching the pattern (latest, or specific version if fully qualified)
     *
     * \param pattern:        full qualified name or exact name
     * \param file:           package is required by environmentFile (empty if not belonging to any environment file)
     */
    bool install(std::string pattern, std::string file) {
        bool explicitMarked = file.empty();
        auto posAt   = pattern.rfind('@');
        auto posHash = pattern.rfind('#');
        if (posAt == std::string::npos || posHash == std::string::npos) {
            auto names = findExactName(pattern);
            if (names.empty()) throw error_fmt{"could not find any package with name {}\n", pattern};
            if (names.size() > 1) throw error_fmt{"find multiple packages which could be the latest one, with name {}\n", pattern};
            pattern = names[0];
            posAt   = pattern.rfind('@');
            posHash = pattern.rfind('#');
        }

        //Dependency also
        auto [store, dependencies] = getPackageAndDependencies(pattern);

        bool newlyInstalled = false;
        for (auto const& p : dependencies) {
            if (!isInstalled(p)) {
                store->install(p, cliVerbose);
                newlyInstalled = true;
            }
            installedPackages.insert(p);
            packagesMarked[p].dependencyCount += 1;
            if (p == pattern) {
                packagesMarked[p].environmentFiles.insert(file);
                if (explicitMarked and !packagesMarked[p].explicitMarked) {
                    packagesMarked[p].explicitMarked = true;
                    newlyInstalled = true;
                }
            }
        }
        store->state.save(getSlixStatePath() / store->name / "state.yaml");
        return newlyInstalled;
    }

    /** Will remove the package matching the pattern (latest, or specific version if fully qualified)
     *
     * \param pattern:        full qualified name or exact name
     * \param file:           package is required by environmentFile (empty if not belonging to any environment file)
     */
    bool remove(std::string pattern, std::string file) {
        bool explicitMarked = file.empty();
        auto posAt   = pattern.rfind('@');
        auto posHash = pattern.rfind('#');
        if (posAt == std::string::npos || posHash == std::string::npos) {
            auto names = findExactName(pattern);
            if (names.empty()) throw error_fmt{"could not find any package with name {}\n", pattern};
            if (names.size() > 1) throw error_fmt{"find multiple packages which could be the latest one, with name {}\n", pattern};
            pattern = names[0];
            posAt   = pattern.rfind('@');
            posHash = pattern.rfind('#');
        }

        //Dependency also
        auto [store, dependencies] = getPackageAndDependencies(pattern);

        bool removed = false;

        for (auto const& p : dependencies) {
            packagesMarked[p].dependencyCount -= 1;
            if (explicitMarked && packagesMarked[p].explicitMarked) {
                packagesMarked[p].explicitMarked = false;
                removed = true;
            }
            if (!explicitMarked) {
                packagesMarked[p].environmentFiles.erase(file);
            }
            if (packagesMarked[p].dependencyCount == 0) {
                store->remove(p);
                installedPackages.erase(p);
                packagesMarked.erase(p);
            }

        }
        store->state.save(getSlixStatePath() / store->name / "state.yaml");
        return removed;
    }


    /** List All files from environment file
     *
     * \param file:           package is required by environmentFile
     */
    auto listPackagesFromEnvironment(std::string environmentFile) const {
        auto packages = std::unordered_set<std::string>{};
        // list all packages from this environment
        for (auto const& [name, markings] : packagesMarked) {
            if (markings.environmentFiles.contains(environmentFile)) {
                packages.insert(name);
            }
        }
        return packages;
    }

    /** Get explicit installed packages
     */
    auto getExplicitInstalledPackages() const -> std::unordered_set<std::string> {
        auto res = std::unordered_set<std::string>{};
        for (auto const& [name, markings] : packagesMarked) {
            if (markings.explicitMarked) {
                res.insert(name);
            }
        }
        return res;
    }


    /** Get installed environments files
     */
    auto getInstalledEnvironmentFiles() const -> std::unordered_set<std::string> {
        auto res = std::unordered_set<std::string>{};
        for (auto const& [name, markings] : packagesMarked) {
            for (auto const& e : markings.environmentFiles) {
                res.insert(e);
            }
        }
        return res;
    }

    /** finds an exact packages
     *
     * returns first: a list of potential stores
     *         secnod: a store that has it installed
     */
    auto findExactPattern(std::string pattern) {
        auto result = std::tuple<std::vector<Store*>, Store*>{};

        auto posAt   = pattern.rfind('@');
        auto posHash = pattern.rfind('#');
        if (posAt   == std::string::npos) return result;
        if (posHash == std::string::npos) return result;
        auto name    = pattern.substr(0, posAt);
        auto version = pattern.substr(posAt+1, posHash-posAt-1);
        auto hash    = pattern.substr(posHash+1);

        // check if any store is already has it on the shelf
        for (auto& store : stores) {
            if (auto iter = store.state.packages.find(name); iter != store.state.packages.end()) {
                if (iter->second.count(pattern) > 0) {
                    std::get<1>(result) = &store;
                    break;
                }
            }
        }

        // check if any store could order it
        for (auto& store : stores) {
            auto index = store.loadPackageIndex();
            if (auto iter = index.packages.find(name); iter != index.packages.end()) {
                for (auto const& info : iter->second) {
                    if (info.version == version and info.hash == hash) {
                        std::get<0>(result).emplace_back(&store);
                    }
                }
            }
        }
        return result;
    }

    auto findExactName(std::string name) -> std::vector<std::string> {
        // Check if name is already fully qualified
        auto posAt   = name.rfind('@');
        auto posHash = name.rfind('#');
        if (posAt   != std::string::npos
            && posHash != std::string::npos
            && posHash > posAt) {
                return {name};
        }

        auto result = std::vector<std::string>{};

        // check if any store could order it
        for (auto& store : stores) {
            auto index = store.loadPackageIndex();
            if (auto iter = index.packages.find(name); iter != index.packages.end()) {
                auto info = iter->second.back();
                result.emplace_back(fmt::format("{}@{}#{}", name, info.version, info.hash));
            }
        }
        return result;
    }

    auto findWithPrefix(std::string name) -> std::vector<std::string> {
        auto result = std::vector<std::string>{};

        // check if any store could order it
        for (auto& store : stores) {
            auto index = store.loadPackageIndex();
            for (auto [key, value] : index.packages) {
                if (key.starts_with(name)) {
                    auto info = value.back();
                    result.emplace_back(fmt::format("{}@{}#{}", key, info.version, info.hash));
                }
            }
        }
        return result;
    }

    /** Finds an updated packages
     *
     * \param pattern: fully qualified name
     */
    auto findUpgrade(std::string pattern) const -> std::string {
        // Split of name
        auto posAt   = pattern.rfind('@');
        auto posHash = pattern.rfind('#');
        if (posAt   == std::string::npos
            || posHash == std::string::npos
            || posAt > posHash) {
            throw error_fmt{"package {} is invalid format", pattern};
        }
        auto name    = pattern.substr(0, posAt);
        auto version = pattern.substr(posAt+1, posHash-posAt-1);
        auto hash    = pattern.substr(posHash+1);

        for (auto& store : stores) {
            auto index = store.loadPackageIndex();
            if (auto iter = index.packages.find(name); iter != index.packages.end()) {
                auto info = iter->second.back();
                return fmt::format("{}@{}#{}", name, info.version, info.hash);
            }
        }

        throw error_fmt{"Can't find any package with name {}", pattern};
    }



};

