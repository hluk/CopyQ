/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

#ifndef FAKEVIM_STANDALONE
#   include <utils/savedaction.h>
#endif

#include <QCoreApplication>
#include <QHash>
#include <QObject>
#include <QString>
#include <QSettings>
#include <QVariant>

namespace FakeVim {
namespace Internal {

class DummyAction final
{
public:
    DummyAction(void *parent);
    void setValue(const QVariant &value);
    QVariant value() const;
    void setDefaultValue(const QVariant &value);
    QVariant defaultValue() const;
    void setSettingsKey(const QString &group, const QString &key);
    QString settingsKey() const;
    void setCheckable(bool) {}

    void readSettings(QSettings *) {}
    void writeSettings(QSettings *) {}

    QVariant m_value;
    QVariant m_defaultValue;
    QString m_settingsGroup;
    QString m_settingsKey;
};

#ifdef FAKEVIM_STANDALONE
using FakeVimAction = DummyAction;
#else
using FakeVimAction = Utils::SavedAction;
#endif

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
    ConfigRelativeNumber,

    ConfigBlinkingCursor
};

class FakeVimSettings final
{
    Q_DECLARE_TR_FUNCTIONS(FakeVim)

public:
    FakeVimSettings();
    ~FakeVimSettings();
    void insertItem(int code, FakeVimAction *item,
        const QString &longname = QString(),
        const QString &shortname = QString());

    FakeVimAction *item(int code);
    FakeVimAction *item(const QString &name);
    QString trySetValue(const QString &name, const QString &value);

    void readSettings(QSettings *settings);
    void writeSettings(QSettings *settings);

private:
    void createAction(int code, const QVariant &value,
                      const QString &settingsKey = QString(),
                      const QString &shortKey = QString());

    QHash<int, FakeVimAction *> m_items;
    QHash<QString, int> m_nameToCode;
    QHash<int, QString> m_codeToName;
};

FakeVimSettings *theFakeVimSettings();
FakeVimAction *theFakeVimSetting(int code);

} // namespace Internal
} // namespace FakeVim
