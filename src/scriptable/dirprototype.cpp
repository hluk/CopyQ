/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

QString DirPrototype::absoluteFilePath(const QString &fileName) const
{
    return thisDir().absoluteFilePath(fileName);
}

QString DirPrototype::absolutePath() const
{
    return thisDir().absolutePath();
}

QString DirPrototype::canonicalPath() const
{
    return thisDir().canonicalPath();
}

bool DirPrototype::cd(const QString &dirName)
{
    return thisDir().cd(dirName);
}

bool DirPrototype::cdUp()
{
    return thisDir().cdUp();
}

uint DirPrototype::count() const
{
    return thisDir().count();
}

QString DirPrototype::dirName() const
{
    return thisDir().dirName();
}

QStringList DirPrototype::entryList(const QStringList &nameFilters) const
{
    return thisDir().entryList(nameFilters);
}

bool DirPrototype::exists(const QString &name) const
{
    return thisDir().exists(name);
}

bool DirPrototype::exists() const
{
    return thisDir().exists();
}

QString DirPrototype::filePath(const QString &fileName) const
{
    return thisDir().filePath(fileName);
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

bool DirPrototype::mkdir(const QString &dirName) const
{
    return thisDir().mkdir(dirName);
}

bool DirPrototype::mkpath(const QString &dirPath) const
{
    return thisDir().mkpath(dirPath);
}

QStringList DirPrototype::nameFilters() const
{
    return thisDir().nameFilters();
}

QString DirPrototype::path() const
{
    return thisDir().path();
}

void DirPrototype::refresh() const
{
    return thisDir().refresh();
}

QString DirPrototype::relativeFilePath(const QString &fileName) const
{
    return thisDir().relativeFilePath(fileName);
}

bool DirPrototype::remove(const QString &fileName)
{
    return thisDir().remove(fileName);
}

bool DirPrototype::rename(const QString &oldName, const QString &newName)
{
    return thisDir().rename(oldName, newName);
}

bool DirPrototype::rmdir(const QString &dirName) const
{
    return thisDir().rmdir(dirName);
}

bool DirPrototype::rmpath(const QString &dirPath) const
{
    return thisDir().rmpath(dirPath);
}

void DirPrototype::setNameFilters(const QStringList &nameFilters)
{
    return thisDir().setNameFilters(nameFilters);
}

void DirPrototype::setPath(const QString &path)
{
    return thisDir().setPath(path);
}

void DirPrototype::addSearchPath(const QString &prefix, const QString &path) const
{
    return QDir::addSearchPath(prefix, path);
}

QString DirPrototype::cleanPath(const QString &path) const
{
    return QDir::cleanPath(path);
}

QString DirPrototype::currentPath() const
{
    return QDir::currentPath();
}

QString DirPrototype::fromNativeSeparators(const QString &pathName) const
{
    return QDir::fromNativeSeparators(pathName);
}

QScriptValue DirPrototype::home() const
{
    return newDir(QDir::home());
}

QString DirPrototype::homePath() const
{
    return QDir::homePath();
}

bool DirPrototype::isAbsolutePath(const QString &path) const
{
    return QDir::isAbsolutePath(path);
}

bool DirPrototype::isRelativePath(const QString &path) const
{
    return QDir::isRelativePath(path);
}

bool DirPrototype::match(const QString &filter, const QString &fileName) const
{
    return QDir::match(filter, fileName);
}

bool DirPrototype::match(const QStringList &filters, const QString &fileName) const
{
    return QDir::match(filters, fileName);
}

QScriptValue DirPrototype::root() const
{
    return newDir(QDir::root());
}

QString DirPrototype::rootPath() const
{
    return QDir::rootPath();
}

QStringList DirPrototype::searchPaths(const QString &prefix) const
{
    return QDir::searchPaths(prefix);
}

QChar DirPrototype::separator() const
{
    return QDir::separator();
}

bool DirPrototype::setCurrent(const QString &path) const
{
    return QDir::setCurrent(path);
}

void DirPrototype::setSearchPaths(const QString &prefix, const QStringList &searchPaths) const
{
    return QDir::setSearchPaths(prefix, searchPaths);
}

QScriptValue DirPrototype::temp() const
{
    return newDir(QDir::temp());
}

QString DirPrototype::tempPath() const
{
    return QDir::tempPath();
}

QString DirPrototype::toNativeSeparators(const QString &pathName) const
{
    return QDir::toNativeSeparators(pathName);
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
