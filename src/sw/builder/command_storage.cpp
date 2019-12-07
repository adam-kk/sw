// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "command_storage.h"

#include "file_storage.h"
#include "sw_context.h"

#include <sw/manager/storage.h>

#include <boost/thread/lock_types.hpp>
#include <primitives/emitter.h>
#include <primitives/date_time.h>
#include <primitives/debug.h>
#include <primitives/exceptions.h>
#include <primitives/lock.h>
#include <primitives/symbol.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "db_file");

#define COMMAND_DB_FORMAT_VERSION 4

namespace sw
{

static path getCurrentModuleName()
{
    return primitives::getModuleNameForSymbol(primitives::getCurrentModuleSymbol());
}

static String getCurrentModuleNameHash()
{
    return shorten_hash(blake2b_512(getCurrentModuleName().u8string()), 12);
}

static path getDir(const SwBuilderContext &swctx, bool local)
{
    if (local)
        return path(SW_BINARY_DIR) / "db" / "0.3.1";
    return swctx.getLocalStorage().storage_dir_tmp / "db" / "0.3.1";
}

static path getCommandsDbFilename(const SwBuilderContext &swctx, bool local)
{
    return getDir(swctx, local) / std::to_string(COMMAND_DB_FORMAT_VERSION) / "commands.bin";
}

static path getCommandsLogFileName(const SwBuilderContext &swctx, bool local)
{
    auto cfg = shorten_hash(blake2b_512(getCurrentModuleNameHash()), 12);
    return getDir(swctx, local) / std::to_string(COMMAND_DB_FORMAT_VERSION) / ("cmd_log_" + cfg + ".bin");
}

template <class T>
static void write_int(std::vector<uint8_t> &vec, T val)
{
    auto sz = vec.size();
    vec.resize(sz + sizeof(val));
    memcpy(&vec[sz], &val, sizeof(val));
}

static void write_str(std::vector<uint8_t> &vec, const String &val)
{
    auto sz = val.size() + 1;
    //write_int(vec, sz);
    auto vsz = vec.size();
    vec.resize(vsz + sz);
    memcpy(&vec[vsz], &val[0], sz);
}

Files CommandRecord::getImplicitInputs(detail::Storage &s) const
{
    Files files;
    for (auto &h : implicit_inputs)
    {
        boost::upgrade_lock lk(s.m_file_storage_by_hash);
        auto i = s.file_storage_by_hash.find(h);
        if (i == s.file_storage_by_hash.end())
            throw SW_RUNTIME_ERROR("no such file");
        auto p = i->second;
        lk.unlock();
        if (!p.empty())
            files.insert(p);
    }
    return files;
}

void CommandRecord::setImplicitInputs(const Files &files, detail::Storage &s)
{
    for (auto &f : files)
    {
        auto str = normalize_path(f);
        auto h = std::hash<String>()(str);
        implicit_inputs.insert(h);

        boost::upgrade_lock lk(s.m_file_storage_by_hash);
        auto i = s.file_storage_by_hash.find(h);
        if (i == s.file_storage_by_hash.end())
        {
            boost::upgrade_to_unique_lock lk2(lk);
            s.file_storage_by_hash[h] = str;
        }
    }
}

FileDb::FileDb(const SwBuilderContext &swctx)
    : swctx(swctx)
{
}

void FileDb::write(std::vector<uint8_t> &v, const CommandRecord &f, const detail::Storage &s)
{
    v.clear();

    if (f.hash == 0)
        return;

    //if (!std::is_trivially_copyable_v<decltype(f.mtime)>)
        //throw SW_RUNTIME_ERROR("x");

    write_int(v, f.hash);
#ifndef __APPLE__
    write_int(v, file_time_type2time_t(f.mtime));
#else
    write_int(v, *(__int128_t*)&f.mtime);
#endif

    auto n = f.implicit_inputs.size();
    write_int(v, n);
    for (auto &h : f.implicit_inputs)
    {
        boost::upgrade_lock lk(s.m_file_storage_by_hash);
        auto i = s.file_storage_by_hash.find(h);
        if (i == s.file_storage_by_hash.end())
            throw SW_RUNTIME_ERROR("no such file");
        auto p = i->second;
        lk.unlock();
        write_int(v, std::hash<String>()(normalize_path(p)));
    }
}

static String getFilesSuffix()
{
    return ".files";
}

static void load(const path &fn, Files &files, std::unordered_map<size_t, path> &files2, ConcurrentCommandStorage &commands)
{
    // files
    if (fs::exists(path(fn) += getFilesSuffix()))
    {
        primitives::BinaryStream b;
        b.load(path(fn) += getFilesSuffix());
        while (!b.eof())
        {
            size_t sz; // record size
            b.read(sz);
            if (!b.has(sz))
            {
                fs::resize_file(path(fn) += getFilesSuffix(), b.index() - sizeof(sz));
                break; // record is in bad shape
            }

            if (sz == 0)
                continue;

            String s;
            b.read(s);
            files.insert(s);

            files2[std::hash<String>()(s)] = s;
        }
    }

    // commands
    if (fs::exists(fn))
    {
        primitives::BinaryStream b;
        b.load(fn);
        while (!b.eof())
        {
            size_t sz; // record size
            b.read(sz);
            if (!b.has(sz))
            {
                fs::resize_file(path(fn) += getFilesSuffix(), b.index() - sizeof(sz));
                break; // record is in bad shape
            }

            if (sz == 0)
                continue;

            size_t h;
            b.read(h);

            auto r = commands.insert(h);
            r.first->hash = h;

            //if (!std::is_trivially_copyable_v<decltype(r.first->mtime)>)
                //throw SW_RUNTIME_ERROR("x");

#ifndef __APPLE__
            time_t m;
            b.read(m);
            r.first->mtime = time_t2file_time_type(m);
#else
            __int128_t m;
            b.read(m);
            r.first->mtime = *(fs::file_time_type*)&m;
#endif

            size_t n;
            b.read(n);
            r.first->implicit_inputs.reserve(n);
            while (n--)
            {
                b.read(h);
                auto &f = files2[h];
                if (!f.empty())
                {
                    //r.first->implicit_inputs.insert(files2[h]);
                    r.first->implicit_inputs.insert(h);
                }
            }
        }
    }
}

void FileDb::load(Files &files, std::unordered_map<size_t, path> &files2, ConcurrentCommandStorage &commands, bool local) const
{
    sw::load(getCommandsDbFilename(swctx, local), files, files2, commands);
    sw::load(getCommandsLogFileName(swctx, local), files, files2, commands);
}

void FileDb::save(const Files &files, const detail::Storage &s, ConcurrentCommandStorage &commands, bool local) const
{
    std::vector<uint8_t> v;

    // files
    {
        primitives::BinaryStream b(10'000'000); // reserve amount
        for (auto &f : files)
        {
            auto s = normalize_path(f);
            auto sz = s.size() + 1;
            b.write(sz);
            b.write(s);
        }
        if (!b.empty())
        {
            auto p = getCommandsDbFilename(swctx, local) += getFilesSuffix();
            fs::create_directories(p.parent_path());
            b.save(p);
        }
    }

    // commands
    {
        primitives::BinaryStream b(10'000'000); // reserve amount
        for (const auto &[k, r] : commands)
        {
            write(v, r, s);
            auto sz = v.size();
            b.write(sz);
            b.write(v.data(), v.size());
        }
        if (!b.empty())
        {
            auto p = getCommandsDbFilename(swctx, local);
            fs::create_directories(p.parent_path());
            b.save(p);
        }
    }

    error_code ec;
    fs::remove(getCommandsLogFileName(swctx, local), ec);
    fs::remove(getCommandsLogFileName(swctx, local) += getFilesSuffix(), ec);
}

detail::FileHolder::FileHolder(const path &fn)
    : f(fn, "ab"), fn(fn)
{
    // goes first
    // but maybe remove?
    //if (setvbuf(f.getHandle(), NULL, _IONBF, 0) != 0)
    //throw RUNTIME_EXCEPTION("Cannot disable log buffering");

    // Opening a file in append mode doesn't set the file pointer to the file's
    // end on Windows. Do that explicitly.
    fseek(f.getHandle(), 0, SEEK_END);
}

detail::FileHolder::~FileHolder()
{
    f.close();

    error_code ec; // remove ec? but multiple processes may be writing into this log? or not?
    fs::remove(fn, ec);
}

CommandStorage::CommandStorage(const SwBuilderContext &swctx)
    : swctx(swctx), fdb(swctx)
{
    load();
}

CommandStorage::~CommandStorage()
{
    try
    {
        closeLogs();
        save();
    }
    catch (std::exception &e)
    {
        LOG_ERROR(logger, "Error during command db save: " << e.what());
    }
}

void CommandStorage::async_command_log(const CommandRecord &r, bool local)
{
    static std::vector<uint8_t> v;

    swctx.getFileStorageExecutor().push([this, local, &r]
    {
        auto &s = getInternalStorage(local);

        {
            // write record to vector v
            fdb.write(v, r, s);

            auto &l = s.getCommandLog(swctx, local);
            auto sz = v.size();
            fwrite(&sz, sizeof(sz), 1, l.f.getHandle());
            fwrite(&v[0], sz, 1, l.f.getHandle());
            fflush(l.f.getHandle());
        }

        {
            auto &l = s.getFileLog(swctx, local);
            for (auto &f : r.getImplicitInputs(s))
            {
                auto r = s.file_storage.insert(f);
                if (!r.second)
                    continue;
                auto s = normalize_path(f);
                auto sz = s.size() + 1;
                fwrite(&sz, sizeof(sz), 1, l.f.getHandle());
                fwrite(&s[0], sz, 1, l.f.getHandle());
                fflush(l.f.getHandle());
            }
        }
    });
}

void detail::Storage::closeLogs()
{
    commands.reset();
    files.reset();
}

void CommandStorage::closeLogs()
{
    global.closeLogs();
    local.closeLogs();
}

detail::FileHolder &detail::Storage::getCommandLog(const SwBuilderContext &swctx, bool local)
{
    if (!commands)
        commands = std::make_unique<FileHolder>(getCommandsLogFileName(swctx, local));
    return *commands;
}

detail::FileHolder &detail::Storage::getFileLog(const SwBuilderContext &swctx, bool local)
{
    if (!files)
        files = std::make_unique<FileHolder>(getCommandsLogFileName(swctx, local) += getFilesSuffix());
    return *files;
}

void CommandStorage::load()
{
    fdb.load(local.file_storage, local.file_storage_by_hash, local.storage, true);
    fdb.load(global.file_storage, global.file_storage_by_hash, global.storage, false);
}

void CommandStorage::save()
{
    fdb.save(local.file_storage, local, local.storage, true);
    fdb.save(global.file_storage, global, global.storage, false);
}

ConcurrentCommandStorage &CommandStorage::getStorage(bool local)
{
    return getInternalStorage(local).storage;
}

detail::Storage &CommandStorage::getInternalStorage(bool local_)
{
    return local_ ? local : global;
}

}
