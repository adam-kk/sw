// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "package.h"

#include <sw/support/filesystem.h>

#include <primitives/date_time.h>

namespace sw
{

struct LocalPackage;
struct PackageId;
struct PackageData;
struct Remote;

namespace vfs
{

/*struct SW_MANAGER_API VirtualFileSystem
{
    virtual ~VirtualFileSystem() = default;

    virtual void writeFile(const PackageId &pkg, const path &local_file, const path &vfs_file) const = 0;
};

// default fs
struct SW_MANAGER_API LocalFileSystem : VirtualFileSystem
{
};

// more than one destination
struct SW_MANAGER_API VirtualFileSystemMultiplexer : VirtualFileSystem
{
    std::vector<std::shared_ptr<VirtualFileSystem>> filesystems;

    void writeFile(const PackageId &pkg, const path &local_file, const path &vfs_file) const override
    {
        for (auto &fs : filesystems)
            fs->writeFile(pkg, local_file, vfs_file);
    }
};*/

struct SW_MANAGER_API File
{
    virtual ~File() = default;

    virtual bool copy(const path &to) const = 0;
};

} // namespace vfs

struct SW_MANAGER_API Directories
{
    path storage_dir;
#define DIR(x) path storage_dir_##x;
#include "storage_directories.inl"
#undef DIR

    Directories(const path &root);

    path getDatabaseRootDir() const;
};

enum class StorageFileType
{
    // or Archive or DataArchive
    SourceArchive       =   0x1,

    // db?
    // binary files
    // dbg info
    // data files
    // config files
    // used files
};

SW_MANAGER_API
String toUserString(StorageFileType);

struct PackagesDatabase;
struct ServiceDatabase;

struct StorageSchema
{
    StorageSchema(int hash_version, int hash_path_version)
        : hash_version(hash_version), hash_path_version(hash_path_version)
    {}

    int getHashVersion() const { return hash_version; }
    int getHashPathFromHashVersion() const { return hash_path_version; }

private:
    int hash_version;
    int hash_path_version;
};

struct SoftwareNetworkStorageSchema : StorageSchema
{
    SoftwareNetworkStorageSchema() : StorageSchema(1, 1) {}
};

struct SW_MANAGER_API IStorage
{
    virtual ~IStorage() = default;

    /// storage schema/settings/capabilities/versions
    virtual const StorageSchema &getSchema() const = 0;

    /// resolve packages from this storage
    virtual std::unordered_map<UnresolvedPackage, PackagePtr> resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const = 0;

    /// load package data from this storage
    virtual PackageDataPtr loadData(const PackageId &) const = 0;

    // non virtual methods

    /// resolve packages from this storage with their dependencies
    std::unordered_map<UnresolvedPackage, PackagePtr> resolveWithDependencies(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const;
};

struct SW_MANAGER_API IResolvableStorageWithName : IStorage
{
    virtual ~IResolvableStorageWithName() = default;

    virtual String getName() const = 0;
};

struct SW_MANAGER_API IStorage2 : IResolvableStorageWithName
{
    virtual ~IStorage2() = default;

    //

    /// get file from this storage
    virtual std::unique_ptr<vfs::File> getFile(const PackageId &id, StorageFileType) const = 0;

    // ?

    //virtual LocalPackage download(const PackageId &) const = 0;
    //virtual LocalPackage install(const Package &) const = 0;

    // data exchange

    // get predefined file
    //virtual void get(const IStorage &source, const PackageId &id, StorageFileType) = 0;

    /// get specific file from storage from package directory
    //virtual void get(const IStorage &source, const PackageId &id, const path &from_rel_path, const path &to_file) = 0;

};

struct SW_MANAGER_API Storage : IStorage2
{
    Storage(const String &name);

    String getName() const override { return name; }

private:
    String name;
};

struct SW_MANAGER_API StorageWithPackagesDatabase : Storage
{
    StorageWithPackagesDatabase(const String &name, const path &db_dir);
    virtual ~StorageWithPackagesDatabase();

    PackageDataPtr loadData(const PackageId &) const override;
    //void get(const IStorage &source, const PackageId &id, StorageFileType) override;
    std::unordered_map<UnresolvedPackage, PackagePtr> resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const override;

//protected:?
    PackagesDatabase &getPackagesDatabase() const;

private:
    std::unique_ptr<PackagesDatabase> pkgdb;
    mutable std::mutex m;
    mutable std::unordered_map<PackageId, PackageData> data;
};

struct SW_MANAGER_API LocalStorageBase : StorageWithPackagesDatabase
{
    LocalStorageBase(const String &name, const path &db_dir);
    virtual ~LocalStorageBase();

    const StorageSchema &getSchema() const override { return schema; }

    virtual LocalPackage install(const Package &) const = 0;
    std::unique_ptr<vfs::File> getFile(const PackageId &id, StorageFileType) const override;

    void deletePackage(const PackageId &id) const;

private:
    StorageSchema schema;
};

struct SW_MANAGER_API OverriddenPackagesStorage : LocalStorageBase
{
    const LocalStorage &ls;

    OverriddenPackagesStorage(const LocalStorage &ls, const path &db_dir);
    virtual ~OverriddenPackagesStorage();

    LocalPackage install(const Package &) const override;
    LocalPackage install(const PackageId &, const PackageData &) const;
    bool isPackageInstalled(const Package &p) const;

    std::unordered_set<LocalPackage> getPackages() const;
    void deletePackageDir(const path &sdir) const;
};

struct SW_MANAGER_API LocalStorage : Directories, LocalStorageBase
{
    LocalStorage(const path &local_storage_root_dir);
    virtual ~LocalStorage();

    //LocalPackage download(const PackageId &) const override;
    void remove(const LocalPackage &) const;
    LocalPackage install(const Package &) const override;
    void get(const IStorage2 &source, const PackageId &id, StorageFileType) const /* override*/;
    bool isPackageInstalled(const Package &id) const;
    bool isPackageOverridden(const PackageId &id) const;
    LocalPackage getGroupLeader(const LocalPackage &id) const;
    PackageDataPtr loadData(const PackageId &) const override;
    std::unordered_map<UnresolvedPackage, PackagePtr> resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const override;

    OverriddenPackagesStorage &getOverriddenPackagesStorage();
    const OverriddenPackagesStorage &getOverriddenPackagesStorage() const;

private:
    OverriddenPackagesStorage ovs;

    void migrateStorage(int from, int to);
};

struct CachedStorage : IStorage
{
    virtual ~CachedStorage() = default;

    void store(const std::unordered_map<UnresolvedPackage, PackagePtr> &);
    std::unordered_map<UnresolvedPackage, PackagePtr> resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const override;

    const StorageSchema &getSchema() const override { SW_UNREACHABLE; }
    PackageDataPtr loadData(const PackageId &) const override { SW_UNREACHABLE; }

private:
    mutable std::unordered_map<UnresolvedPackage, PackagePtr> resolved_packages;
};

} // namespace sw
