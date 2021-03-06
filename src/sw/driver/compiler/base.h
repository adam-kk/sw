// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "../options.h"
#include "../options_cl.h"
#include "../program.h"
#include "../types.h"

#include <sw/builder/os.h>

#include <primitives/exceptions.h>

#include <memory>

namespace sw
{

namespace builder
{
struct Command;
}

namespace driver
{
struct Command;
}

struct BuildSettings;
struct Build;
struct SwBuilderContext;
struct Target;
struct NativeCompiledTarget;
struct NativeLinker;

// compilers

struct SW_DRIVER_CPP_API CompilerBaseProgram : Program
{
    //String Prefix;
    //String Extension;

    CompilerBaseProgram();
    CompilerBaseProgram(const CompilerBaseProgram &);

    std::shared_ptr<builder::Command> prepareCommand(const Target &t);
    std::shared_ptr<builder::Command> getCommand() const override;

protected:
    std::shared_ptr<driver::Command> cmd;
    bool prepared = false;

    virtual void prepareCommand1(const Target &t) = 0;
};

struct SW_DRIVER_CPP_API Compiler : CompilerBaseProgram
{
    using CompilerBaseProgram::CompilerBaseProgram;
    virtual ~Compiler() = default;
};

struct SW_DRIVER_CPP_API NativeCompiler
    : Compiler
    , NativeCompilerOptions
{
    CompilerType Type = CompilerType::Unspecified;

    using Compiler::Compiler;
    virtual ~NativeCompiler() = default;

    virtual void setSourceFile(const path &input_file, const path &output_file) = 0;

    void merge(const NativeCompiledTarget &t);

protected:
    mutable Files dependencies;
};

// linkers

struct SW_DRIVER_CPP_API Linker : CompilerBaseProgram
{
    using CompilerBaseProgram::CompilerBaseProgram;
    virtual ~Linker() = default;
};

struct SW_DRIVER_CPP_API NativeLinker : Linker,
    NativeLinkerOptions
{
    LinkerType Type = LinkerType::Unspecified;

    String Prefix;
    String Suffix;

    using Linker::Linker;

    virtual void setObjectFiles(const FilesOrdered &files) = 0; // actually this is addObjectFiles()

    virtual path getOutputFile() const = 0;
    virtual void setOutputFile(const path &out) = 0;

    virtual path getImportLibrary() const = 0;
    virtual void setImportLibrary(const path &out) = 0;
};

}
