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

#include "fakevimactions.h"
#include "fakevimhandler.h"

// Please do not add any direct dependencies to other Qt Creator code  here.
// Instead emit signals and let the FakeVimPlugin channel the information to
// Qt Creator. The idea is to keep this file here in a "clean" state that
// allows easy reuse with any QTextEdit or QPlainTextEdit derived class.


#include <utils/qtcassert.h>

#include <QDebug>

using namespace Utils;

namespace FakeVim {
namespace Internal {

DummyAction::DummyAction(void *)
{
}

void DummyAction::setValue(const QVariant &value)
{
    m_value = value;
}

QVariant DummyAction::value() const
{
    return m_value;
}

void DummyAction::setDefaultValue(const QVariant &value)
{
    m_defaultValue = value;
}

QVariant DummyAction::defaultValue() const
{
    return m_defaultValue;
}

void DummyAction::setSettingsKey(const QString &group, const QString &key)
{
    m_settingsGroup = group;
    m_settingsKey = key;
}

QString DummyAction::settingsKey() const
{
    return m_settingsKey;
}

FakeVimSettings::FakeVimSettings()
{
    // Specific FakeVim settings
    createAction(ConfigReadVimRc,  false,     "ReadVimRc");
    createAction(ConfigVimRcPath,  QString(), "VimRcPath");
#ifndef FAKEVIM_STANDALONE
    createAction(ConfigUseFakeVim, false,     "UseFakeVim");
    item(ConfigUseFakeVim)->setText(tr("Use Vim-style Editing"));
    item(ConfigReadVimRc)->setText(tr("Read .vimrc"));
    item(ConfigVimRcPath)->setText(tr("Path to .vimrc"));
#endif
    createAction(ConfigShowMarks,      false, "ShowMarks",      "sm");
    createAction(ConfigPassControlKey, false, "PassControlKey", "pck");
    createAction(ConfigPassKeys,       true,  "PassKeys",       "pk");

    // Emulated Vsetting
    createAction(ConfigStartOfLine,    true,  "StartOfLine",    "sol");
    createAction(ConfigTabStop,        8,     "TabStop",        "ts");
    createAction(ConfigSmartTab,       false, "SmartTab",       "sta");
    createAction(ConfigHlSearch,       true,  "HlSearch",       "hls");
    createAction(ConfigShiftWidth,     8,     "ShiftWidth",     "sw");
    createAction(ConfigExpandTab,      false, "ExpandTab",      "et");
    createAction(ConfigAutoIndent,     false, "AutoIndent",     "ai");
    createAction(ConfigSmartIndent,    false, "SmartIndent",    "si");
    createAction(ConfigIncSearch,      true,  "IncSearch",      "is");
    createAction(ConfigUseCoreSearch,  false, "UseCoreSearch",  "ucs");
    createAction(ConfigSmartCase,      false, "SmartCase",      "scs");
    createAction(ConfigIgnoreCase,     false, "IgnoreCase",     "ic");
    createAction(ConfigWrapScan,       true,  "WrapScan",       "ws");
    createAction(ConfigTildeOp,        false, "TildeOp",        "top");
    createAction(ConfigShowCmd,        true,  "ShowCmd",        "sc");
    createAction(ConfigRelativeNumber, false, "RelativeNumber", "rnu");
    createAction(ConfigBlinkingCursor, false, "BlinkingCursor", "cb");
    createAction(ConfigScrollOff,      0,     "ScrollOff",      "so");
    createAction(ConfigBackspace,      QString("indent,eol,start"), "ConfigBackspace", "bs");
    createAction(ConfigIsKeyword,      QString("@,48-57,_,192-255,a-z,A-Z"), "IsKeyword", "isk");
    createAction(ConfigClipboard,      QString(), "Clipboard", "cb");
}

FakeVimSettings::~FakeVimSettings()
{
    qDeleteAll(m_items);
}

void FakeVimSettings::insertItem(int code, FakeVimAction *item,
    const QString &longName, const QString &shortName)
{
    QTC_ASSERT(!m_items.contains(code), qDebug() << code; return);
    m_items[code] = item;
    if (!longName.isEmpty()) {
        m_nameToCode[longName] = code;
        m_codeToName[code] = longName;
    }
    if (!shortName.isEmpty())
        m_nameToCode[shortName] = code;
}

void FakeVimSettings::readSettings(QSettings *settings)
{
    foreach (FakeVimAction *item, m_items)
        item->readSettings(settings);
}

void FakeVimSettings::writeSettings(QSettings *settings)
{
    foreach (FakeVimAction *item, m_items)
        item->writeSettings(settings);
}

FakeVimAction *FakeVimSettings::item(int code)
{
    QTC_ASSERT(m_items.value(code, 0), qDebug() << "CODE: " << code; return nullptr);
    return m_items.value(code, 0);
}

FakeVimAction *FakeVimSettings::item(const QString &name)
{
    return m_items.value(m_nameToCode.value(name, -1), 0);
}

QString FakeVimSettings::trySetValue(const QString &name, const QString &value)
{
    int code = m_nameToCode.value(name, -1);
    if (code == -1)
        return tr("Unknown option: %1").arg(name);
    if (code == ConfigTabStop || code == ConfigShiftWidth) {
        if (value.toInt() <= 0)
            return tr("Argument must be positive: %1=%2")
                    .arg(name).arg(value);
    }
    FakeVimAction *act = item(code);
    if (!act)
        return tr("Unknown option: %1").arg(name);
    act->setValue(value);
    return QString();
}

void FakeVimSettings::createAction(int code, const QVariant &value,
                                   const QString &settingsKey,
                                   const QString &shortKey)
{
    auto item = new FakeVimAction(nullptr);
    item->setValue(value);
    item->setSettingsKey("FakeVim", settingsKey);
    item->setDefaultValue(value);
    item->setCheckable(value.canConvert<bool>());
    insertItem(code, item, settingsKey.toLower(), shortKey);
}

FakeVimSettings *theFakeVimSettings()
{
    static FakeVimSettings s;
    return &s;
}

FakeVimAction *theFakeVimSetting(int code)
{
    return theFakeVimSettings()->item(code);
}

} // namespace Internal
} // namespace FakeVim
