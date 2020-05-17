/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "scriptabledir.h"

#include <QJSEngine>

ScriptableDir::ScriptableDir(const QString &path)
    : m_path(path)
{
}

ScriptableDir::~ScriptableDir()
{
    delete m_self;
}

QJSValue ScriptableDir::absoluteFilePath(const QJSValue &fileName)
{
    return self()->absoluteFilePath(fileName.toString());
}

QJSValue ScriptableDir::absolutePath()
{
    return self()->absolutePath();
}

QJSValue ScriptableDir::canonicalPath()
{
    return self()->canonicalPath();
}

bool ScriptableDir::cd(const QJSValue &dirName)
{
    return self()->cd(dirName.toString());
}

bool ScriptableDir::cdUp()
{
    return self()->cdUp();
}

uint ScriptableDir::count()
{
    return self()->count();
}

QJSValue ScriptableDir::dirName()
{
    return self()->dirName();
}

QStringList ScriptableDir::entryList(const QStringList &nameFilters)
{
    return self()->entryList(nameFilters);
}

bool ScriptableDir::exists(const QJSValue &name)
{
    return self()->exists(name.toString());
}

bool ScriptableDir::exists()
{
    return self()->exists();
}

QJSValue ScriptableDir::filePath(const QJSValue &fileName)
{
    return self()->filePath(fileName.toString());
}

bool ScriptableDir::isAbsolute()
{
    return self()->isAbsolute();
}

bool ScriptableDir::isReadable()
{
    return self()->isReadable();
}

bool ScriptableDir::isRelative()
{
    return self()->isRelative();
}

bool ScriptableDir::isRoot()
{
    return self()->isRoot();
}

bool ScriptableDir::makeAbsolute()
{
    return self()->makeAbsolute();
}

bool ScriptableDir::mkdir(const QJSValue &dirName)
{
    return self()->mkdir(dirName.toString());
}

bool ScriptableDir::mkpath(const QJSValue &dirPath)
{
    return self()->mkpath(dirPath.toString());
}

QStringList ScriptableDir::nameFilters()
{
    return self()->nameFilters();
}

QJSValue ScriptableDir::path()
{
    return self()->path();
}

void ScriptableDir::refresh()
{
    self()->refresh();
}

QJSValue ScriptableDir::relativeFilePath(const QJSValue &fileName)
{
    return self()->relativeFilePath(fileName.toString());
}

bool ScriptableDir::remove(const QJSValue &fileName)
{
    return self()->remove(fileName.toString());
}

bool ScriptableDir::rename(const QJSValue &oldName, const QJSValue &newName)
{
    return self()->rename(oldName.toString(), newName.toString());
}

bool ScriptableDir::rmdir(const QJSValue &dirName)
{
    return self()->rmdir(dirName.toString());
}

bool ScriptableDir::rmpath(const QJSValue &dirPath)
{
    return self()->rmpath(dirPath.toString());
}

void ScriptableDir::setNameFilters(const QStringList &nameFilters)
{
    self()->setNameFilters(nameFilters);
}

void ScriptableDir::setPath(const QJSValue &path)
{
    self()->setPath(path.toString());
}

void ScriptableDir::addSearchPath(const QJSValue &prefix, const QJSValue &path)
{
    QDir::addSearchPath(prefix.toString(), path.toString());
}

QJSValue ScriptableDir::cleanPath(const QJSValue &path)
{
    return QDir::cleanPath(path.toString());
}

QJSValue ScriptableDir::currentPath()
{
    return QDir::currentPath();
}

QJSValue ScriptableDir::fromNativeSeparators(const QJSValue &pathName)
{
    return QDir::fromNativeSeparators(pathName.toString());
}

QJSValue ScriptableDir::home()
{
    return newDir(QDir::home());
}

QJSValue ScriptableDir::homePath()
{
    return QDir::homePath();
}

bool ScriptableDir::isAbsolutePath(const QJSValue &path)
{
    return QDir::isAbsolutePath(path.toString());
}

bool ScriptableDir::isRelativePath(const QJSValue &path)
{
    return QDir::isRelativePath(path.toString());
}

bool ScriptableDir::match(const QJSValue &filter, const QJSValue &fileName)
{
    return QDir::match(filter.toString(), fileName.toString());
}

bool ScriptableDir::match(const QStringList &filters, const QJSValue &fileName)
{
    return QDir::match(filters, fileName.toString());
}

QJSValue ScriptableDir::root()
{
    return newDir(QDir::root());
}

QJSValue ScriptableDir::rootPath()
{
    return QDir::rootPath();
}

QStringList ScriptableDir::searchPaths(const QJSValue &prefix)
{
    return QDir::searchPaths(prefix.toString());
}

QJSValue ScriptableDir::separator()
{
    return QString(QDir::separator());
}

bool ScriptableDir::setCurrent(const QJSValue &path)
{
    return QDir::setCurrent(path.toString());
}

void ScriptableDir::setSearchPaths(const QJSValue &prefix, const QStringList &searchPaths)
{
    QDir::setSearchPaths(prefix.toString(), searchPaths);
}

QJSValue ScriptableDir::temp()
{
    return newDir(QDir::temp());
}

QJSValue ScriptableDir::tempPath()
{
    return QDir::tempPath();
}

QJSValue ScriptableDir::toNativeSeparators(const QJSValue &pathName)
{
    return QDir::toNativeSeparators(pathName.toString());
}

QDir *ScriptableDir::self()
{
    if (!m_self)
        m_self = new QDir(m_path);
    return m_self;
}

QJSEngine *ScriptableDir::engine() const
{
    return qjsEngine(this);
}

QJSValue ScriptableDir::newDir(const QDir &dir) const
{
    return engine()->newQObject( new ScriptableDir(dir.absolutePath()) );
}
