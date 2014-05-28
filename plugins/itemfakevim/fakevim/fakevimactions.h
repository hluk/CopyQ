/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#ifndef FAKEVIM_ACTIONS_H
#define FAKEVIM_ACTIONS_H

#ifndef FAKEVIM_STANDALONE
#   include <utils/savedaction.h>
#endif

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariant>

namespace FakeVim {
namespace Internal {

#ifdef FAKEVIM_STANDALONE
namespace Utils {

class SavedAction : public QObject
{
    Q_OBJECT

public:
    SavedAction(QObject *parent);
    void setValue(const QVariant &value);
    QVariant value() const;
    void setDefaultValue(const QVariant &value);
    QVariant defaultValue() const;
    void setSettingsKey(const QString &key);
    QString settingsKey() const;

    QVariant m_value;
    QVariant m_defaultValue;
    QString m_settingsKey;
};

} // namespace Utils
#endif // FAKEVIM_STANDALONE

enum FakeVimSettingsCode
{
    ConfigUseFakeVim,
    ConfigReadVimRc,
    ConfigVimRcPath,

    ConfigStartOfLine,
    ConfigHlSearch,
    ConfigTabStop,
    ConfigSmartTab,
    ConfigShiftWidth,
    ConfigExpandTab,
    ConfigAutoIndent,
    ConfigSmartIndent,

    ConfigIncSearch,
    ConfigUseCoreSearch,
    ConfigSmartCase,
    ConfigIgnoreCase,
    ConfigWrapScan,

    // command ~ behaves as g~
    ConfigTildeOp,

    // indent  allow backspacing over autoindent
    // eol     allow backspacing over line breaks (join lines)
    // start   allow backspacing over the start of insert; CTRL-W and CTRL-U
    //         stop once at the start of insert.
    ConfigBackspace,

    // @,48-57,_,192-255
    ConfigIsKeyword,

    // other actions
    ConfigShowMarks,
    ConfigPassControlKey,
    ConfigPassKeys,
    ConfigClipboard,
    ConfigShowCmd,
    ConfigScrollOff,
    ConfigRelativeNumber
};

class FakeVimSettings : public QObject
{
    Q_OBJECT

public:
    FakeVimSettings();
    ~FakeVimSettings();
    void insertItem(int code, Utils::SavedAction *item,
        const QString &longname = QString(),
        const QString &shortname = QString());

    Utils::SavedAction *item(int code);
    Utils::SavedAction *item(const QString &name);
    QString trySetValue(const QString &name, const QString &value);

#ifndef FAKEVIM_STANDALONE
    void readSettings(QSettings *settings);
    void writeSettings(QSettings *settings);
#endif

private:
    QHash<int, Utils::SavedAction *> m_items;
    QHash<QString, int> m_nameToCode;
    QHash<int, QString> m_codeToName;
};

FakeVimSettings *theFakeVimSettings();
Utils::SavedAction *theFakeVimSetting(int code);

} // namespace Internal
} // namespace FakeVim

#endif // FAKEVIM_ACTTIONS_H
