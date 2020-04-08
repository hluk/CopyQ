/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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
    const auto v = thisDir();
    return v ? v->absoluteFilePath(fileName.toString()) : QScriptValue();
}

QScriptValue DirPrototype::absolutePath() const
{
    const auto v = thisDir();
    return v ? v->absolutePath() : QScriptValue();
}

QScriptValue DirPrototype::canonicalPath() const
{
    const auto v = thisDir();
    return v ? v->canonicalPath() : QScriptValue();
}

bool DirPrototype::cd(const QScriptValue &dirName)
{
    const auto v = thisDir();
    return v && v->cd(dirName.toString());
}

bool DirPrototype::cdUp()
{
    const auto v = thisDir();
    return v && v->cdUp();
}

uint DirPrototype::count() const
{
    const auto v = thisDir();
    return v ? v->count() : 0;
}

QScriptValue DirPrototype::dirName() const
{
    const auto v = thisDir();
    return v ? v->dirName() : QScriptValue();
}

QStringList DirPrototype::entryList(const QStringList &nameFilters) const
{
    const auto v = thisDir();
    return v ? v->entryList(nameFilters) : QStringList();
}

bool DirPrototype::exists(const QScriptValue &name) const
{
    const auto v = thisDir();
    return v && v->exists(name.toString());
}

bool DirPrototype::exists() const
{
    const auto v = thisDir();
    return v && v->exists();
}

QScriptValue DirPrototype::filePath(const QScriptValue &fileName) const
{
    const auto v = thisDir();
    return v ? v->filePath(fileName.toString()) : QScriptValue();
}

bool DirPrototype::isAbsolute() const
{
    const auto v = thisDir();
    return v && v->isAbsolute();
}

bool DirPrototype::isReadable() const
{
    const auto v = thisDir();
    return v && v->isReadable();
}

bool DirPrototype::isRelative() const
{
    const auto v = thisDir();
    return v && v->isRelative();
}

bool DirPrototype::isRoot() const
{
    const auto v = thisDir();
    return v && v->isRoot();
}

bool DirPrototype::makeAbsolute()
{
    const auto v = thisDir();
    return v && v->makeAbsolute();
}

bool DirPrototype::mkdir(const QScriptValue &dirName) const
{
    const auto v = thisDir();
    return v && v->mkdir(dirName.toString());
}

bool DirPrototype::mkpath(const QScriptValue &dirPath) const
{
    const auto v = thisDir();
    return v && v->mkpath(dirPath.toString());
}

QStringList DirPrototype::nameFilters() const
{
    const auto v = thisDir();
    return v ? v->nameFilters() : QStringList();
}

QScriptValue DirPrototype::path() const
{
    const auto v = thisDir();
    return v ? v->path() : QScriptValue();
}

void DirPrototype::refresh() const
{
    const auto v = thisDir();
    if (v)
        v->refresh();
}

QScriptValue DirPrototype::relativeFilePath(const QScriptValue &fileName) const
{
    const auto v = thisDir();
    return v ? v->relativeFilePath(fileName.toString()) : QScriptValue();
}

bool DirPrototype::remove(const QScriptValue &fileName)
{
    const auto v = thisDir();
    return v && v->remove(fileName.toString());
}

bool DirPrototype::rename(const QScriptValue &oldName, const QScriptValue &newName)
{
    const auto v = thisDir();
    return v && v->rename(oldName.toString(), newName.toString());
}

bool DirPrototype::rmdir(const QScriptValue &dirName) const
{
    const auto v = thisDir();
    return v && v->rmdir(dirName.toString());
}

bool DirPrototype::rmpath(const QScriptValue &dirPath) const
{
    const auto v = thisDir();
    return v && v->rmpath(dirPath.toString());
}

void DirPrototype::setNameFilters(const QStringList &nameFilters)
{
    const auto v = thisDir();
    if (v)
        v->setNameFilters(nameFilters);
}

void DirPrototype::setPath(const QScriptValue &path)
{
    const auto v = thisDir();
    if (v)
        v->setPath(path.toString());
}

void DirPrototype::addSearchPath(const QScriptValue &prefix, const QScriptValue &path) const
{
    QDir::addSearchPath(prefix.toString(), path.toString());
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

QScriptValue DirPrototype::separator() const
{
    return QString(QDir::separator());
}

bool DirPrototype::setCurrent(const QScriptValue &path) const
{
    return QDir::setCurrent(path.toString());
}

void DirPrototype::setSearchPaths(const QScriptValue &prefix, const QStringList &searchPaths) const
{
    QDir::setSearchPaths(prefix.toString(), searchPaths);
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

QDir *DirPrototype::thisDir() const
{
    const auto wrapper = qscriptvalue_cast<DirWrapper*>(thisObject().data());
    if (!wrapper && engine() && engine()->currentContext())
        engine()->currentContext()->throwError("Invalid Dir object");
    return wrapper ? wrapper->dir() : nullptr;
}

QScriptValue DirPrototype::newDir(const QDir &dir) const
{
    QScriptValue ctor = engine()->globalObject().property("Dir");
    ScriptableClassBase *cls = qscriptvalue_cast<ScriptableClassBase*>(ctor.data());
    return cls ? cls->newInstance(new DirWrapper(dir)) : QScriptValue();
}
