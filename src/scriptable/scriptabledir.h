// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SCRIPTABLEDIR_H
#define SCRIPTABLEDIR_H

#include <QDir>
#include <QObject>
#include <QJSValue>

class ScriptableDir final : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit ScriptableDir(const QString &path = QString());

    ~ScriptableDir();

public slots:
    QJSValue absoluteFilePath(const QJSValue &fileName);
    QJSValue absolutePath();
    QJSValue canonicalPath();
    bool cd(const QJSValue &dirName);
    bool cdUp();
    uint count();
    QJSValue dirName();
    QStringList entryList(const QStringList &nameFilters = QStringList());
    bool exists(const QJSValue &name);
    bool exists();
    QJSValue filePath(const QJSValue &fileName);
    bool isAbsolute();
    bool isReadable();
    bool isRelative();
    bool isRoot();
    bool makeAbsolute();
    bool mkdir(const QJSValue &dirName);
    bool mkpath(const QJSValue &dirPath);
    QStringList nameFilters();
    QJSValue path();
    void refresh();
    QJSValue relativeFilePath(const QJSValue &fileName);
    bool remove(const QJSValue &fileName);
    bool rename(const QJSValue &oldName, const QJSValue &newName);
    bool rmdir(const QJSValue &dirName);
    bool rmpath(const QJSValue &dirPath);
    void setNameFilters(const QStringList &nameFilters);
    void setPath(const QJSValue &path);

    void addSearchPath(const QJSValue &prefix, const QJSValue &path);
    QJSValue cleanPath(const QJSValue &path);
    QJSValue currentPath();
    QJSValue fromNativeSeparators(const QJSValue &pathName);
    QJSValue home();
    QJSValue homePath();
    bool isAbsolutePath(const QJSValue &path);
    bool isRelativePath(const QJSValue &path);
    bool match(const QJSValue &filter, const QJSValue &fileName);
    bool match(const QStringList &filters, const QJSValue &fileName);
    QJSValue root();
    QJSValue rootPath();
    QStringList searchPaths(const QJSValue &prefix);
    QJSValue separator();
    bool setCurrent(const QJSValue &path);
    void setSearchPaths(const QJSValue &prefix, const QStringList &searchPaths);
    QJSValue temp();
    QJSValue tempPath();
    QJSValue toNativeSeparators(const QJSValue &pathName);

private:
    QDir *self();
    QJSEngine *engine() const;

    QJSValue newDir(const QDir &dir) const;

    QDir *m_self = nullptr;
    QString m_path;
};

#endif // SCRIPTABLEDIR_H
