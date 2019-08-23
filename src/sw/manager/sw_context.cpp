// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sw_context.h"

#include "settings.h"
#include "storage.h"
#include "storage_remote.h"

#include <primitives/executor.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "icontext");

extern bool gForceServerQuery;

namespace sw
{

//ISwContext::~ISwContext() = default;

SwManagerContext::SwManagerContext(const path &local_storage_root_dir)
{
    // first goes resolve cache
    cache_storage_id = storages.size();
    storages.emplace_back(std::make_unique<CachedStorage>());

    local_storage_id = storages.size();
    storages.emplace_back(std::make_unique<LocalStorage>(local_storage_root_dir));

    first_remote_storage_id = storages.size();
    // .0 for local counterpart
    // .1 for remote's temporary storage
    //if (!gForceServerQuery)
    //storages.emplace_back(std::make_unique<RemoteStorage>(
        //getLocalStorage(), "software-network.0", getLocalStorage().getDatabaseRootDir()));

    for (auto &r : Settings::get_user_settings().remotes)
    {
        storages.emplace_back(std::make_unique<RemoteStorageWithFallbackToRemoteResolving>(
            getLocalStorage(), getLocalStorage().getDatabaseRootDir(), r));
    }
}

SwManagerContext::~SwManagerContext() = default;

CachedStorage &SwManagerContext::getCachedStorage() const
{
    return dynamic_cast<CachedStorage&>(*storages[cache_storage_id]);
}

LocalStorage &SwManagerContext::getLocalStorage()
{
    return static_cast<LocalStorage&>(*storages[local_storage_id]);
}

const LocalStorage &SwManagerContext::getLocalStorage() const
{
    return static_cast<const LocalStorage&>(*storages[local_storage_id]);
}

std::vector<Storage *> SwManagerContext::getRemoteStorages()
{
    std::vector<Storage *> r;
    for (int i = first_remote_storage_id; i < storages.size(); i++)
        r.push_back(static_cast<Storage*>(storages[i].get()));
    return r;
}

std::vector<const Storage *> SwManagerContext::getRemoteStorages() const
{
    std::vector<const Storage *> r;
    for (int i = first_remote_storage_id; i < storages.size(); i++)
        r.push_back(static_cast<const Storage*>(storages[i].get()));
    return r;
}

std::unordered_map<UnresolvedPackage, PackagePtr> SwManagerContext::resolve(const UnresolvedPackages &pkgs) const
{
    if (pkgs.empty())
        return {};

    std::lock_guard lk(resolve_mutex);

    std::unordered_map<UnresolvedPackage, PackagePtr> resolved;
    auto pkgs2 = pkgs;
    // when there's new unresolved package available,
    // we must start from the beginning of storages!!!
    while (1)
    {
        bool again = false;
        for (const auto &[i, s] : enumerate(storages))
        {
            UnresolvedPackages unresolved;
            auto rpkgs = s->resolveWithDependencies(pkgs2, unresolved);
            again = !unresolved.empty() && pkgs2 != unresolved;
            resolved.merge(rpkgs);
            pkgs2 = std::move(unresolved);
            if (pkgs2.empty())
                break;
            if (again)
                break;
        }
        if (pkgs2.empty())
            break;
        if (!again)
            break;
    }

    // save existing results
    getCachedStorage().store(resolved);

    if (!pkgs2.empty())
    {
        String s;
        for (auto &d : pkgs2)
            s += d.toString() + ", ";
        if (!s.empty())
            s.resize(s.size() - 2);
        throw SW_RUNTIME_ERROR("Some packages (" + std::to_string(pkgs2.size()) + ") are unresolved: " + s);
    }

    return resolved;
}

std::unordered_map<UnresolvedPackage, LocalPackage> SwManagerContext::install(const UnresolvedPackages &pkgs) const
{
    auto m = resolve(pkgs);

    // two unresolved pkgs may point to single pkg,
    // so make pkgs unique
    std::unordered_set<Package*> pkgs2;
    for (auto &[u, p] : m)
        pkgs2.emplace(p.get());

    auto &e = getExecutor();
    Futures<void> fs;
    for (auto &p : pkgs2)
        fs.push_back(e.push([this, &p]{ install(*p); }));
    waitAndGet(fs);

    // install should be fast enough here
    std::unordered_map<UnresolvedPackage, LocalPackage> pkgs3;
    for (auto &[u, p] : m)
        pkgs3.emplace(u, install(*p));

    return pkgs3;
}

LocalPackage SwManagerContext::install(const Package &p) const
{
    return getLocalStorage().install(p);
}

/*bool SwManagerContext::isResolved(const UnresolvedPackage &pkg) const
{
    return resolved_packages.find(pkg) != resolved_packages.end();
}*/

LocalPackage SwManagerContext::resolve(const UnresolvedPackage &pkg) const
{
    return install(*resolve(UnresolvedPackages{ pkg }).find(pkg)->second);
}

}

