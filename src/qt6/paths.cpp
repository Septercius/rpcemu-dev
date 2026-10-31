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
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <QDir>
#include <QFileInfo>

#ifdef _MSC_VER
#define PATH_MAX 1024
#endif

#include "paths.h"
#include "platform-common.h"
#include "rpcemu.h"

void path_append(char *buffer, const char *path)
{
    QDir dir(buffer);
    QString fullPath = dir.absoluteFilePath(path);
    
    QByteArray ba = fullPath.toUtf8();
    char *data = ba.data();

    strcpy(buffer, data);
}

void path_append_data(char *buffer, const char *path)
{
    QDir dataDir = QDir(rpcemu_get_datadir());
    QString fullPath = dataDir.absoluteFilePath(path);
 
    QByteArray ba = fullPath.toUtf8();
    char *data = ba.data();
 
    strcpy(buffer, data);
}

bool path_directory_exists(const char *path)
{
    QDir dir(path);
    return dir.exists();
}

const char *path_get_extension(const char *path)
{
    QFileInfo info(path);
    
    QString extension = info.suffix();
    QByteArray ba = extension.toUtf8();
    
    char *buffer = strdup(ba.data());
    return buffer;
}

const char *path_get_filename(const char *path)
{
    QFileInfo info(path);

    QString fileName = info.fileName();
    QByteArray ba = fileName.toUtf8();

    char *buffer = strdup(ba.data());
    return buffer;
}

void path_join(char *buffer, const char *left, const char *right)
{
    QDir dir(left);
    QString fullPath = dir.absoluteFilePath(right);

    QByteArray ba = fullPath.toUtf8();
    char *data = ba.data();

    strcpy(buffer, data);
}

void path_join_data(char *buffer, const char *left, const char *right)
{
    QDir dataDir = QDir(rpcemu_get_datadir());
    QDir interim = QDir(dataDir.absoluteFilePath(left));

    QString fullPath = interim.absoluteFilePath(right);
    
    QByteArray ba = fullPath.toUtf8();
    char *data = ba.data();
 
    strcpy(buffer, data);
}

char *path_make_absolute(const char *path)
{
    QString p = QDir::fromNativeSeparators(path);
    QDir dir(p);
    QString absolutePath;

    if (dir.isRelative())
    {
#ifdef RPCEMU_PLATFORM_MACOS
        // Paths on the Mac are relative to the data directory.
        QString root(rpcemu_get_datadir());
        QDir fullPath = QDir::cleanPath(root + QDir::separator() + dir.path());

        absolutePath = fullPath.path();
#else
        // Paths on other platforms are relative to wherever the application is running.
        dir.makeAbsolute();
        absolutePath = dir.path();
#endif /* RPCEMU_PLATFORM_MACOS */
    }
    else
    {
        absolutePath = dir.path();
    }

    QString nativePath = QDir::toNativeSeparators(absolutePath);

    QByteArray ba = nativePath.toUtf8();
    char *buffer = strdup(ba.data());

    return buffer;
}
