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

#ifndef DIRPROTOTYPE_H
#define DIRPROTOTYPE_H

#include <QDir>
#include <QObject>
#include <QScriptable>
#include <QScriptValue>

class DirWrapper : public QObject {
    Q_OBJECT
public:
    explicit DirWrapper(const QString &path) : m_dir(path) {}
    explicit DirWrapper(const QDir &dir) : m_dir(dir) {}

    QDir &dir() { return m_dir; }
private:
    QDir m_dir;
};

class DirPrototype : public QObject, public QScriptable
{
    Q_OBJECT
public:
    explicit DirPrototype(QObject *parent = 0);

public slots:
    QString absoluteFilePath(const QString &fileName) const;
    QString absolutePath() const;
    QString canonicalPath() const;
    bool cd(const QString &dirName);
    bool cdUp();
    uint count() const;
    QString dirName() const;
    QStringList entryList(const QStringList &nameFilters = QStringList()) const;
    bool exists(const QString &name) const;
    bool exists() const;
    QString filePath(const QString &fileName) const;
    bool isAbsolute() const;
    bool isReadable() const;
    bool isRelative() const;
    bool isRoot() const;
    bool makeAbsolute();
    bool mkdir(const QString &dirName) const;
    bool mkpath(const QString &dirPath) const;
    QStringList nameFilters() const;
    QString path() const;
    void refresh() const;
    QString relativeFilePath(const QString &fileName) const;
    bool remove(const QString &fileName);
    bool rename(const QString &oldName, const QString &newName);
    bool rmdir(const QString &dirName) const;
    bool rmpath(const QString &dirPath) const;
    void setNameFilters(const QStringList &nameFilters);
    void setPath(const QString &path);

    void addSearchPath(const QString &prefix, const QString &path) const;
    QString cleanPath(const QString &path) const;
    QString currentPath() const;
    QString fromNativeSeparators(const QString &pathName) const;
    QScriptValue home() const;
    QString homePath() const;
    bool isAbsolutePath(const QString &path) const;
    bool isRelativePath(const QString &path) const;
    bool match(const QString &filter, const QString &fileName) const;
    bool match(const QStringList &filters, const QString &fileName) const;
    QScriptValue root() const;
    QString rootPath() const;
    QStringList searchPaths(const QString &prefix) const;
    QChar separator() const;
    bool setCurrent(const QString &path) const;
    void setSearchPaths(const QString &prefix, const QStringList &searchPaths) const;
    QScriptValue temp() const;
    QString tempPath() const;
    QString toNativeSeparators(const QString &pathName) const;

private:
    QDir &thisDir() const;

    QScriptValue newDir(const QDir &dir) const;
};

#endif // DIRPROTOTYPE_H
