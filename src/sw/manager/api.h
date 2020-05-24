// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * SW - Build System and Package Manager
 * Copyright (C) 2016-2018 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "package.h"

#include <sw/support/package_data.h>
#include <sw/support/specification.h>

namespace sw
{

struct IStorage;

struct Api
{
    virtual ~Api() = 0;

    virtual std::unordered_map<UnresolvedPackage, PackagePtr>
    resolvePackages(
        const UnresolvedPackages &pkgs,
        UnresolvedPackages &unresolved_pkgs,
        std::unordered_map<PackageId, PackageData> &data, const IStorage &) const = 0;
    virtual void addVersion(const PackagePath &prefix, const PackageDescriptionMap &pkgs, const SpecificationFiles &) const = 0;
};

}
