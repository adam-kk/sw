// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "package.h"

#include <sw/support/filesystem.h>

#include <memory>
#include <mutex>
#include <vector>

namespace sw
{

struct IResolvableStorage;
struct CachedStorage;
struct LocalStorage;
struct ResolveResultWithDependencies;
struct ResolveRequest;

// sw_context_t?
/*struct SW_MANAGER_API ISwContext
{
    virtual ~ISwContext() = 0;

    virtual Package resolve(const UnresolvedPackage &) const = 0;
};*/

struct SW_MANAGER_API SwManagerContext// : ISwContext
{
    SwManagerContext(const path &local_storage_root_dir, bool allow_network);
    virtual ~SwManagerContext();

    LocalStorage &getLocalStorage();
    const LocalStorage &getLocalStorage() const;
    std::vector<IStorage *> getRemoteStorages() const;

    //
    void install(ResolveRequest &) const;
    //std::unordered_map<UnresolvedPackage, LocalPackage> install(const UnresolvedPackages &, bool use_cache = true) const;
    // what about ", bool use_cache = true"?
    LocalPackage install(const Package &) const;

    //ResolveResultWithDependencies resolve(const UnresolvedPackages &, bool use_cache = true) const;
    //LocalPackage resolve(const UnresolvedPackage &) const;
    //ResolveResultWithDependencies resolve(const UnresolvedPackages &, const std::vector<IStorage*> &) const;
    bool resolve(ResolveRequest &, bool use_cache) const;

    // lock file related
    void setCachedPackages(const std::unordered_map<UnresolvedPackage, PackageId> &) const;

    void addStorage(std::unique_ptr<IStorage>);

private:
    CachedStorage *cache_storage = nullptr;
    LocalStorage *local_storage = nullptr;
    int first_remote_storage_id;
    std::vector<std::unique_ptr<IStorage>> storages;
    mutable std::mutex resolve_mutex;

    CachedStorage &getCachedStorage() const;
};

template <typename F>
void resolveWithDependencies(std::vector<ResolveRequest> &v, F &&resolve)
{
    // simple unresolved package for now
    // (without settings)
    UnresolvedPackages s;
    while (1)
    {
        bool new_resolve = false;
        std::vector<ResolveRequest *> v2;
        for (auto &&rr : v)
        {
            if (rr.isResolved())
            {
                s.insert(rr.u);
                continue;
            }
            if (!resolve(rr))
                throw SW_RUNTIME_ERROR("Cannot resolve: " + rr.u.toString());
            auto inserted = s.insert(rr.u).second;
            new_resolve |= inserted;
            if (!inserted)
                continue;
            v2.push_back(&rr);
        }
        std::vector<ResolveRequest> v3;
        for (auto &&rr : v2)
        {
            auto &p = rr->getPackage();
            for (auto &d : p.getData().dependencies)
            {
                if (s.contains(d))
                    continue;
                ResolveRequest rr2;
                rr2.u = d;
                rr2.settings = rr->settings;
                v3.emplace_back(std::move(rr2));
            }
        }
        for (auto &&rr : v3)
            v.emplace_back(std::move(rr));
        if (!new_resolve)
            break;
    }
}

} // namespace sw
