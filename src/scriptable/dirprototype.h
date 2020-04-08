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

#ifndef DIRPROTOTYPE_H
#define DIRPROTOTYPE_H

#include <QDir>
#include <QObject>
#include <QScriptable>
#include <QScriptValue>

class DirWrapper final : public QObject {
    Q_OBJECT
public:
    explicit DirWrapper(const QString &path) : m_dir(path) {}
    explicit DirWrapper(const QDir &dir) : m_dir(dir) {}

    QDir *dir() { return &m_dir; }
private:
    QDir m_dir;
};

class DirPrototype final : public QObject, public QScriptable
{
    Q_OBJECT
public:
    explicit DirPrototype(QObject *parent = nullptr);

public slots:
    QScriptValue absoluteFilePath(const QScriptValue &fileName) const;
    QScriptValue absolutePath() const;
    QScriptValue canonicalPath() const;
    bool cd(const QScriptValue &dirName);
    bool cdUp();
    uint count() const;
    QScriptValue dirName() const;
    QStringList entryList(const QStringList &nameFilters = QStringList()) const;
    bool exists(const QScriptValue &name) const;
    bool exists() const;
    QScriptValue filePath(const QScriptValue &fileName) const;
    bool isAbsolute() const;
    bool isReadable() const;
    bool isRelative() const;
    bool isRoot() const;
    bool makeAbsolute();
    bool mkdir(const QScriptValue &dirName) const;
    bool mkpath(const QScriptValue &dirPath) const;
    QStringList nameFilters() const;
    QScriptValue path() const;
    void refresh() const;
    QScriptValue relativeFilePath(const QScriptValue &fileName) const;
    bool remove(const QScriptValue &fileName);
    bool rename(const QScriptValue &oldName, const QScriptValue &newName);
    bool rmdir(const QScriptValue &dirName) const;
    bool rmpath(const QScriptValue &dirPath) const;
    void setNameFilters(const QStringList &nameFilters);
    void setPath(const QScriptValue &path);

    void addSearchPath(const QScriptValue &prefix, const QScriptValue &path) const;
    QScriptValue cleanPath(const QScriptValue &path) const;
    QScriptValue currentPath() const;
    QScriptValue fromNativeSeparators(const QScriptValue &pathName) const;
    QScriptValue home() const;
    QScriptValue homePath() const;
    bool isAbsolutePath(const QScriptValue &path) const;
    bool isRelativePath(const QScriptValue &path) const;
    bool match(const QScriptValue &filter, const QScriptValue &fileName) const;
    bool match(const QStringList &filters, const QScriptValue &fileName) const;
    QScriptValue root() const;
    QScriptValue rootPath() const;
    QStringList searchPaths(const QScriptValue &prefix) const;
    QScriptValue separator() const;
    bool setCurrent(const QScriptValue &path) const;
    void setSearchPaths(const QScriptValue &prefix, const QStringList &searchPaths) const;
    QScriptValue temp() const;
    QScriptValue tempPath() const;
    QScriptValue toNativeSeparators(const QScriptValue &pathName) const;

private:
    QDir *thisDir() const;

    QScriptValue newDir(const QDir &dir) const;
};

#endif // DIRPROTOTYPE_H
