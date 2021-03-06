/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
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

// os

#if defined(_WIN32)
#define CPPAN_OS_WIN32
#endif

#if defined(_WIN64)
#define CPPAN_OS_WIN64
#endif

#if defined(CPPAN_OS_WIN32) || defined(CPPAN_OS_WIN64)
#define CPPAN_OS_WINDOWS
#endif

#if defined(__CYGWIN__)
#define CPPAN_OS_CYGWIN
#endif

#if defined(CPPAN_OS_WINDOWS) && !defined(CPPAN_OS_CYGWIN)
#define CPPAN_OS_WINDOWS_NO_CYGWIN
#endif

#if defined(__MINGW32__)
#define CPPAN_OS_MINGW32
#endif

#if defined(__linux)
#define CPPAN_OS_LINUX
#endif

#if defined(__APPLE__)
#define CPPAN_OS_APPLE
#endif

#if defined(__FreeBSD__)
#define CPPAN_OS_FREEBSD
#endif

#if defined(__OpenBSD__)
#define CPPAN_OS_OPENBSD
#endif

#if defined(__NetBSD__)
#define CPPAN_OS_NETBSD
#endif

#if defined(__sun)
#define CPPAN_OS_SOLARIS
#endif

#if defined(__hpux)
#define CPPAN_OS_HPUX
#endif

#if defined(_AIX)
#define CPPAN_OS_AIX
#endif

#if defined(__unix)
#define CPPAN_OS_UNIX
#endif

#if defined(__posix)
#define CPPAN_OS_POSIX
#endif

// arch

#if defined(CPPAN_OS_WIN64)
#define CPPAN_ARCH_64
#elif defined(CPPAN_OS_WIN32)
#define CPPAN_ARCH_32
#endif

// current compiler

#ifdef _MSC_VER
#define CPPAN_COMPILER_MSVC
#endif

// runtime settings

#ifdef NDEBUG
#define CPPAN_RELEASE
#endif

#ifndef CPPAN_RELEASE
#define CPPAN_DEBUG
#endif
