/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#include "fakevimactions.h"
#include "fakevimhandler.h"

// Please do not add any direct dependencies to other Qt Creator code  here.
// Instead emit signals and let the FakeVimPlugin channel the information to
// Qt Creator. The idea is to keep this file here in a "clean" state that
// allows easy reuse with any QTextEdit or QPlainTextEdit derived class.


#include <utils/qtcassert.h>

#include <QDebug>
#include <QObject>
#include <QCoreApplication>

#ifdef FAKEVIM_STANDALONE
using namespace FakeVim::Internal::Utils;
#else
using namespace Utils;
#endif

///////////////////////////////////////////////////////////////////////
//
// FakeVimSettings
//
///////////////////////////////////////////////////////////////////////

namespace FakeVim {
namespace Internal {

typedef QLatin1String _;

#ifdef FAKEVIM_STANDALONE
namespace Utils {

SavedAction::SavedAction(QObject *parent)
    : QObject(parent)
{
}

void SavedAction::setValue(const QVariant &value)
{
    m_value = value;
}

QVariant SavedAction::value() const
{
    return m_value;
}

} // namespace Utils
#endif // FAKEVIM_STANDALONE

FakeVimSettings::FakeVimSettings()
{}

FakeVimSettings::~FakeVimSettings()
{
    qDeleteAll(m_items);
}

void FakeVimSettings::insertItem(int code, SavedAction *item,
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

#ifndef FAKEVIM_STANDALONE
void FakeVimSettings::readSettings(QSettings *settings)
{
    foreach (SavedAction *item, m_items)
        item->readSettings(settings);
}

void FakeVimSettings::writeSettings(QSettings *settings)
{
    foreach (SavedAction *item, m_items)
        item->writeSettings(settings);
}
#endif // FAKEVIM_STANDALONE

SavedAction *FakeVimSettings::item(int code)
{
    QTC_ASSERT(m_items.value(code, 0), qDebug() << "CODE: " << code; return 0);
    return m_items.value(code, 0);
}

SavedAction *FakeVimSettings::item(const QString &name)
{
    return m_items.value(m_nameToCode.value(name, -1), 0);
}

QString FakeVimSettings::trySetValue(const QString &name, const QString &value)
{
    int code = m_nameToCode.value(name, -1);
    if (code == -1)
        return FakeVimHandler::tr("Unknown option: %1").arg(name);
    if (code == ConfigTabStop || code == ConfigShiftWidth) {
        if (value.toInt() <= 0)
            return FakeVimHandler::tr("Argument must be positive: %1=%2")
                    .arg(name).arg(value);
    }
    SavedAction *act = item(code);
    if (!act)
        return FakeVimHandler::tr("Unknown option: %1").arg(name);
    act->setValue(value);
    return QString();
}

SavedAction *createAction(FakeVimSettings *instance, int code, const QVariant &value,
                          const QString &settingsKey = QString(),
                          const QString &shortKey = QString())
{
    SavedAction *item = new SavedAction(instance);
    item->setValue(value);
#ifndef FAKEVIM_STANDALONE
    item->setSettingsKey(_("FakeVim"), settingsKey);
    item->setDefaultValue(value);
    item->setCheckable( value.canConvert<bool>() );
#endif
    instance->insertItem(code, item, settingsKey.toLower(), shortKey);
    return item;
}

FakeVimSettings *theFakeVimSettings()
{
    static FakeVimSettings *s = 0;
    if (s)
        return s;

    s = new FakeVimSettings;

    // Specific FakeVim settings
    createAction(s, ConfigReadVimRc,  false,     _("ReadVimRc"));
    createAction(s, ConfigVimRcPath,  QString(), _("VimRcPath"));
#ifndef FAKEVIM_STANDALONE
    createAction(s, ConfigUseFakeVim, false,     _("UseFakeVim"));
    s->item(ConfigUseFakeVim)->setText(QCoreApplication::translate("FakeVim::Internal",
        "Use Vim-style Editing"));
    s->item(ConfigReadVimRc)->setText(QCoreApplication::translate("FakeVim::Internal",
        "Read .vimrc"));
    s->item(ConfigVimRcPath)->setText(QCoreApplication::translate("FakeVim::Internal",
        "Path to .vimrc"));
#endif
    createAction(s, ConfigShowMarks,      false, _("ShowMarks"),      _("sm"));
    createAction(s, ConfigPassControlKey, false, _("PassControlKey"), _("pck"));
    createAction(s, ConfigPassKeys,       true,  _("PassKeys"),       _("pk"));

    // Emulated Vim setting
    createAction(s, ConfigStartOfLine,    true,  _("StartOfLine"),   _("sol"));
    createAction(s, ConfigTabStop,        8,     _("TabStop"),       _("ts"));
    createAction(s, ConfigSmartTab,       false, _("SmartTab"),      _("sta"));
    createAction(s, ConfigHlSearch,       true,  _("HlSearch"),      _("hls"));
    createAction(s, ConfigShiftWidth,     8,     _("ShiftWidth"),    _("sw"));
    createAction(s, ConfigExpandTab,      false, _("ExpandTab"),     _("et"));
    createAction(s, ConfigAutoIndent,     false, _("AutoIndent"),    _("ai"));
    createAction(s, ConfigSmartIndent,    false, _("SmartIndent"),   _("si"));
    createAction(s, ConfigIncSearch,      true,  _("IncSearch"),     _("is"));
    createAction(s, ConfigUseCoreSearch,  false, _("UseCoreSearch"), _("ucs"));
    createAction(s, ConfigSmartCase,      false, _("SmartCase"),     _("scs"));
    createAction(s, ConfigIgnoreCase,     false, _("IgnoreCase"),    _("ic"));
    createAction(s, ConfigWrapScan,       true,  _("WrapScan"),      _("ws"));
    createAction(s, ConfigTildeOp,        false, _("TildeOp"),       _("top"));
    createAction(s, ConfigShowCmd,        true,  _("ShowCmd"),       _("sc"));
    createAction(s, ConfigScrollOff,      0,     _("ScrollOff"),     _("so"));
    createAction(s, ConfigBackspace,      _("indent,eol,start"), _("ConfigBackspace"), _("bs"));
    createAction(s, ConfigIsKeyword,      _("@,48-57,_,192-255,a-z,A-Z"), _("IsKeyword"), _("isk"));
    createAction(s, ConfigClipboard,      QString(), _("Clipboard"), _("cb"));

    return s;
}

SavedAction *theFakeVimSetting(int code)
{
    return theFakeVimSettings()->item(code);
}

} // namespace Internal
} // namespace FakeVim
