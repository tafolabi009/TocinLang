#pragma once

#include "../error/error_handler.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace package {

/**
 * @brief Package version
 */
struct Version {
    int major;
    int minor;
    int patch;
    std::string prerelease;
    std::string build;
    
    Version(int maj = 0, int min = 0, int pat = 0, 
            const std::string& pre = "", const std::string& bld = "")
        : major(maj), minor(min), patch(pat), prerelease(pre), build(bld) {}
    
    std::string toString() const;
    bool operator<(const Version& other) const;
    bool operator==(const Version& other) const;
};

/**
 * @brief Package dependency
 */
struct Dependency {
    std::string name;
    std::string version;
    std::string source;
    bool optional;
    std::vector<std::string> features;
    
    Dependency(const std::string& n, const std::string& v = "")
        : name(n), version(v), optional(false) {}
};

/**
 * @brief Package metadata
 */
struct PackageMetadata {
    std::string name;
    Version version;
    std::string description;
    std::string author;
    std::string license;
    std::string repository;
    std::vector<std::string> keywords;
    std::vector<Dependency> dependencies;
    std::vector<Dependency> devDependencies;
    std::vector<std::string> files;
    std::string entryPoint;
    std::string main;
    std::string types;
    
    PackageMetadata() : version(0, 0, 0) {}
};

/**
 * @brief Package information
 */
struct PackageInfo {
    PackageMetadata metadata;
    std::string path;
    bool installed;
    std::string installedVersion;
    std::vector<std::string> files;
    
    PackageInfo() : installed(false) {}
};

/**
 * @brief Package registry
 */
class PackageRegistry {
private:
    std::string registryUrl;
    std::unordered_map<std::string, PackageInfo> packages;
    
public:
    PackageRegistry(const std::string& url = "https://registry.tocin.dev");
    
    /**
     * @brief Search for packages
     */
    std::vector<PackageInfo> search(const std::string& query);
    
    /**
     * @brief Get package information
     */
    std::optional<PackageInfo> getPackage(const std::string& name, const std::string& version = "");
    
    /**
     * @brief Get latest version of a package
     */
    std::optional<Version> getLatestVersion(const std::string& name);
    
    /**
     * @brief Get available versions
     */
    std::vector<Version> getVersions(const std::string& name);
    
    /**
     * @brief Download package
     */
    bool downloadPackage(const std::string& name, const std::string& version, const std::string& targetPath);
    
private:
    std::string makeRequest(const std::string& endpoint);
    bool parsePackageInfo(const std::string& json, PackageInfo& info);
};

/**
 * @brief Package manager
 */
class PackageManager {
private:
    std::string projectPath;
    std::string cachePath;
    std::unique_ptr<PackageRegistry> registry;
    std::unordered_map<std::string, PackageInfo> installedPackages;
    error::ErrorHandler& errorHandler;
    
public:
    PackageManager(const std::string& project, error::ErrorHandler& eh);
    ~PackageManager();
    
    /**
     * @brief Initialize package manager
     */
    bool initialize();
    
    /**
     * @brief Install package
     */
    bool install(const std::string& name, const std::string& version = "");
    
    /**
     * @brief Install all dependencies
     */
    bool installAll();
    
    /**
     * @brief Uninstall package
     */
    bool uninstall(const std::string& name);
    
    /**
     * @brief Update package
     */
    bool update(const std::string& name);
    
    /**
     * @brief Update all packages
     */
    bool updateAll();
    
    /**
     * @brief List installed packages
     */
    std::vector<PackageInfo> listInstalled();
    
    /**
     * @brief Search packages
     */
    std::vector<PackageInfo> search(const std::string& query);
    
    /**
     * @brief Get package info
     */
    std::optional<PackageInfo> getPackage(const std::string& name);
    
    /**
     * @brief Add dependency
     */
    bool addDependency(const std::string& name, const std::string& version = "", bool dev = false);
    
    /**
     * @brief Remove dependency
     */
    bool removeDependency(const std::string& name, bool dev = false);
    
    /**
     * @brief Create new package
     */
    bool createPackage(const std::string& name, const std::string& description = "");
    
    /**
     * @brief Publish package
     */
    bool publish();
    
    /**
     * @brief Build package
     */
    bool build();
    
    /**
     * @brief Test package
     */
    bool test();
    
    /**
     * @brief Clean package
     */
    bool clean();
    
    /**
     * @brief Get dependency tree
     */
    std::string getDependencyTree();
    
    /**
     * @brief Check for updates
     */
    std::vector<std::pair<std::string, std::string>> checkUpdates();
    
    /**
     * @brief Lock dependencies
     */
    bool lockDependencies();
    
    /**
     * @brief Install from lock file
     */
    bool installFromLock();
    
private:
    bool readPackageFile();
    bool writePackageFile();
    bool readLockFile();
    bool writeLockFile();
    std::string resolveDependencies(const std::vector<Dependency>& deps);
    bool downloadAndInstall(const std::string& name, const std::string& version);
    std::string getPackagePath(const std::string& name, const std::string& version);
    bool validatePackage(const PackageInfo& info);
    std::string generateLockFile();
};

} // namespace package 