/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#include "dirprototype.h"

#include "dirclass.h"

#include <QScriptEngine>

Q_DECLARE_METATYPE(DirWrapper*)
Q_DECLARE_METATYPE(ScriptableClassBase*)

DirPrototype::DirPrototype(QObject *parent)
    : QObject(parent)
{
}

QScriptValue DirPrototype::absoluteFilePath(const QScriptValue &fileName) const
{
    return thisDir().absoluteFilePath(fileName.toString());
}

QScriptValue DirPrototype::absolutePath() const
{
    return thisDir().absolutePath();
}

QScriptValue DirPrototype::canonicalPath() const
{
    return thisDir().canonicalPath();
}

bool DirPrototype::cd(const QScriptValue &dirName)
{
    return thisDir().cd(dirName.toString());
}

bool DirPrototype::cdUp()
{
    return thisDir().cdUp();
}

uint DirPrototype::count() const
{
    return thisDir().count();
}

QScriptValue DirPrototype::dirName() const
{
    return thisDir().dirName();
}

QStringList DirPrototype::entryList(const QStringList &nameFilters) const
{
    return thisDir().entryList(nameFilters);
}

bool DirPrototype::exists(const QScriptValue &name) const
{
    return thisDir().exists(name.toString());
}

bool DirPrototype::exists() const
{
    return thisDir().exists();
}

QScriptValue DirPrototype::filePath(const QScriptValue &fileName) const
{
    return thisDir().filePath(fileName.toString());
}

bool DirPrototype::isAbsolute() const
{
    return thisDir().isAbsolute();
}

bool DirPrototype::isReadable() const
{
    return thisDir().isReadable();
}

bool DirPrototype::isRelative() const
{
    return thisDir().isRelative();
}

bool DirPrototype::isRoot() const
{
    return thisDir().isRoot();
}

bool DirPrototype::makeAbsolute()
{
    return thisDir().makeAbsolute();
}

bool DirPrototype::mkdir(const QScriptValue &dirName) const
{
    return thisDir().mkdir(dirName.toString());
}

bool DirPrototype::mkpath(const QScriptValue &dirPath) const
{
    return thisDir().mkpath(dirPath.toString());
}

QStringList DirPrototype::nameFilters() const
{
    return thisDir().nameFilters();
}

QScriptValue DirPrototype::path() const
{
    return thisDir().path();
}

void DirPrototype::refresh() const
{
    return thisDir().refresh();
}

QScriptValue DirPrototype::relativeFilePath(const QScriptValue &fileName) const
{
    return thisDir().relativeFilePath(fileName.toString());
}

bool DirPrototype::remove(const QScriptValue &fileName)
{
    return thisDir().remove(fileName.toString());
}

bool DirPrototype::rename(const QScriptValue &oldName, const QScriptValue &newName)
{
    return thisDir().rename(oldName.toString(), newName.toString());
}

bool DirPrototype::rmdir(const QScriptValue &dirName) const
{
    return thisDir().rmdir(dirName.toString());
}

bool DirPrototype::rmpath(const QScriptValue &dirPath) const
{
    return thisDir().rmpath(dirPath.toString());
}

void DirPrototype::setNameFilters(const QStringList &nameFilters)
{
    return thisDir().setNameFilters(nameFilters);
}

void DirPrototype::setPath(const QScriptValue &path)
{
    return thisDir().setPath(path.toString());
}

void DirPrototype::addSearchPath(const QScriptValue &prefix, const QScriptValue &path) const
{
    return QDir::addSearchPath(prefix.toString(), path.toString());
}

QScriptValue DirPrototype::cleanPath(const QScriptValue &path) const
{
    return QDir::cleanPath(path.toString());
}

QScriptValue DirPrototype::currentPath() const
{
    return QDir::currentPath();
}

QScriptValue DirPrototype::fromNativeSeparators(const QScriptValue &pathName) const
{
    return QDir::fromNativeSeparators(pathName.toString());
}

QScriptValue DirPrototype::home() const
{
    return newDir(QDir::home());
}

QScriptValue DirPrototype::homePath() const
{
    return QDir::homePath();
}

bool DirPrototype::isAbsolutePath(const QScriptValue &path) const
{
    return QDir::isAbsolutePath(path.toString());
}

bool DirPrototype::isRelativePath(const QScriptValue &path) const
{
    return QDir::isRelativePath(path.toString());
}

bool DirPrototype::match(const QScriptValue &filter, const QScriptValue &fileName) const
{
    return QDir::match(filter.toString(), fileName.toString());
}

bool DirPrototype::match(const QStringList &filters, const QScriptValue &fileName) const
{
    return QDir::match(filters, fileName.toString());
}

QScriptValue DirPrototype::root() const
{
    return newDir(QDir::root());
}

QScriptValue DirPrototype::rootPath() const
{
    return QDir::rootPath();
}

QStringList DirPrototype::searchPaths(const QScriptValue &prefix) const
{
    return QDir::searchPaths(prefix.toString());
}

QChar DirPrototype::separator() const
{
    return QDir::separator();
}

bool DirPrototype::setCurrent(const QScriptValue &path) const
{
    return QDir::setCurrent(path.toString());
}

void DirPrototype::setSearchPaths(const QScriptValue &prefix, const QStringList &searchPaths) const
{
    return QDir::setSearchPaths(prefix.toString(), searchPaths);
}

QScriptValue DirPrototype::temp() const
{
    return newDir(QDir::temp());
}

QScriptValue DirPrototype::tempPath() const
{
    return QDir::tempPath();
}

QScriptValue DirPrototype::toNativeSeparators(const QScriptValue &pathName) const
{
    return QDir::toNativeSeparators(pathName.toString());
}

QDir &DirPrototype::thisDir() const
{
    return qscriptvalue_cast<DirWrapper*>(thisObject().data())->dir();
}

QScriptValue DirPrototype::newDir(const QDir &dir) const
{
    QScriptValue ctor = engine()->globalObject().property("Dir");
    ScriptableClassBase *cls = qscriptvalue_cast<ScriptableClassBase*>(ctor.data());
    return cls ? cls->newInstance(new DirWrapper(dir)) : QScriptValue();
}
