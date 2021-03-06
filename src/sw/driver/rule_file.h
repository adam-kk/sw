// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/builder/command.h>

#include <tuple>

namespace sw
{

struct RuleFiles;
namespace builder { struct Command; }

struct SW_DRIVER_CPP_API RuleFile
{
    //using AdditionalArguments = std::map<String, primitives::command::Arguments>;
    using AdditionalArguments = std::map<String, Strings>;

    RuleFile(const path &fn) : file(fn) {}

    AdditionalArguments &getAdditionalArguments() { return additional_arguments; }
    const AdditionalArguments &getAdditionalArguments() const { return additional_arguments; }

    bool operator<(const RuleFile &rhs) const { return std::tie(file, additional_arguments) < std::tie(rhs.file, rhs.additional_arguments); }
    bool operator==(const RuleFile &rhs) const { return std::tie(file, additional_arguments) == std::tie(rhs.file, rhs.additional_arguments); }
    //auto operator<=>(const RuleFile &rhs) const = default;

    const path &getFile() const { return file; }

    /*size_t getHash() const
    {
        size_t h = 0;
        hash_combine(h, std::hash<decltype(file)>()(file));
        hash_combine(h, std::hash<decltype(additional_arguments)>()(additional_arguments));
        return h;
    }*/

    // think, how we can implement this
    // new file is considered on loop or per rule when first seen?
    // seems like second option is correct
    //bool isNew() const {}
    //void setAge(int i) { age = i; }

    void setCommand(const std::shared_ptr<builder::Command> &);
    // user knows what he is doing
    void resetCommand(const std::shared_ptr<builder::Command> &);
    std::shared_ptr<builder::Command> getCommand(const RuleFiles &) const;

    void addDependency(const path &fn);
    const auto &getDependencies() const { return dependencies; }

private:
    path file;
    AdditionalArguments additional_arguments;
    //int new_ = true; // iteration
    std::shared_ptr<builder::Command> command;
    Files dependencies;

    Commands getDependencies1(const RuleFiles &) const;
};

struct SW_DRIVER_CPP_API RuleFiles
{
    using RFS = std::unordered_map<path, RuleFile>;

    RuleFile &addFile(const path &);
    auto contains(const path &p) const { return rfs.contains(p); }

    void addCommand(const path &output, const std::shared_ptr<builder::Command> &);
    Commands getCommands() const;

    auto begin() { return rfs.begin(); }
    auto end() { return rfs.end(); }

    auto begin() const { return rfs.begin(); }
    auto end() const { return rfs.end(); }

    auto empty() const { return rfs.empty(); }
    auto size() const { return rfs.size(); }

    void clear() { rfs.clear(); }
    auto erase(const path &p) { return rfs.erase(p); }
    void merge(RuleFiles &rhs);

private:
    RFS rfs;
    std::unordered_map<path, std::shared_ptr<builder::Command>> commands;

    friend struct RuleFile;
};

} // namespace sw
