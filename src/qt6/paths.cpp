/*
 RPCEmu - An Acorn system emulator

 Copyright (C) 2016-2017 Peter Howkins

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "rpcemu.h"
#include "paths.h"

#ifdef _MSC_VER
#define PATH_MAX 1024
#else
#include <unistd.h>
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

const char* path_extract_filename(const char* path)
{
    QFileInfo info(path);

    QString fileName = info.fileName();
    QByteArray ba = fileName.toUtf8();

    char* buffer = strdup(ba.data());
    return buffer;
}

void path_join(const char* str1, const char* str2, char* buffer)
{
    QString first = str1;
    QString second = str2;

    QString fullPath = QDir::cleanPath(first + QDir::separator() + second);

    QByteArray ba = fullPath.toUtf8();
    char *data = ba.data();

    strcpy(buffer, data);
}

void path_join_to(char* str1, const char* str2)
{
    QString first = str1;
    QString second = str2;

    QString fullPath = QDir::cleanPath(first + QDir::separator() + second);

    QByteArray ba = fullPath.toUtf8();
    char *data = ba.data();

    strcpy(str1, data);
}

char* path_resolve(const char* path)
{
    QString p = QDir::fromNativeSeparators(path);
    QDir dir(p);
    QString absolutePath;

    if (dir.isRelative())
    {
#if defined(Q_OS_MACOS)
        // Paths on the Mac are relative to the data directory.
        QString root(rpcemu_get_datadir());
        QDir fullPath = QDir::cleanPath(root + QDir::separator() + dir.path());

        absolutePath = fullPath.path();
#else
        // Paths on other platforms are relative to wherever the application is running.
        dir.makeAbsolute();
        absolutePath = dir.path();
#endif
    }
    else
    {
        absolutePath = dir.path();
    }

    QString nativePath = QDir::toNativeSeparators(absolutePath);

    QByteArray ba = nativePath.toUtf8();
    char* buffer = strdup(ba.data());

    return buffer;
}

int path_validate(const char* path)
{
    QDir dir(path);
    return dir.exists();
}
