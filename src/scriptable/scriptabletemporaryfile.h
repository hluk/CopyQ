// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SCRIPTABLETEMPORARYFILE_H
#define SCRIPTABLETEMPORARYFILE_H

#include "scriptablefile.h"

class QJSValue;
class QTemporaryFile;

class ScriptableTemporaryFile final : public ScriptableFile
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit ScriptableTemporaryFile(const QString &path = QString());

public slots:
    bool autoRemove();
    QString fileTemplate();
    void setAutoRemove(bool autoRemove);
    void setFileTemplate(const QJSValue &name);

    QFile *self() override;

private:
    QTemporaryFile *tmpFile();

    QTemporaryFile *m_self = nullptr;
};

#endif // SCRIPTABLETEMPORARYFILE_H
