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

#define FAKEVIM_STANDALONE

#ifdef FAKEVIM_STANDALONE
#   include "private/fakevim_export.h"
#else
#   include <utils/savedaction.h>
#endif

#include <QCoreApplication>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVariant>

namespace FakeVim {
namespace Internal {

#ifdef FAKEVIM_STANDALONE
class FAKEVIM_EXPORT FvBaseAspect
{
public:
    FvBaseAspect();
    virtual ~FvBaseAspect() {}

    void setValue(const QVariant &value);
    QVariant value() const;
    void setDefaultValue(const QVariant &value);
    QVariant defaultValue() const;
    void setSettingsKey(const QString &group, const QString &key);
    QString settingsKey() const;
    void setCheckable(bool) {}
    void setDisplayName(const QString &) {}
    void setToolTip(const QString &) {}

private:
    QVariant m_value;
    QVariant m_defaultValue;
    QString m_settingsGroup;
    QString m_settingsKey;
};

class FvBoolAspect : public FvBaseAspect
{
public:
    bool value() const { return FvBaseAspect::value().toBool(); }
};

class FvIntegerAspect : public FvBaseAspect
{
public:
    qint64 value() const { return FvBaseAspect::value().toLongLong(); }
};

class FvStringAspect : public FvBaseAspect
{
public:
    QString value() const { return FvBaseAspect::value().toString(); }
};

class FvAspectContainer : public FvBaseAspect
{
public:
};

#else

using FvAspectContainer = Utils::AspectContainer;
using FvBaseAspect = Utils::BaseAspect;
using FvBoolAspect = Utils::BoolAspect;
using FvIntegerAspect = Utils::IntegerAspect;
using FvStringAspect = Utils::StringAspect;

#endif

class FAKEVIM_EXPORT FakeVimSettings final : public FvAspectContainer
{
    Q_DECLARE_TR_FUNCTIONS(FakeVim)

public:
    FakeVimSettings();
    ~FakeVimSettings();

    FvBaseAspect *item(const QString &name);
    QString trySetValue(const QString &name, const QString &value);

    FvBoolAspect useFakeVim;
    FvBoolAspect readVimRc;
    FvStringAspect vimRcPath;

    FvBoolAspect startOfLine;
    FvIntegerAspect tabStop;
    FvBoolAspect hlSearch;
    FvBoolAspect smartTab;
    FvIntegerAspect shiftWidth;
    FvBoolAspect expandTab;
    FvBoolAspect autoIndent;
    FvBoolAspect smartIndent;

    FvBoolAspect incSearch;
    FvBoolAspect useCoreSearch;
    FvBoolAspect smartCase;
    FvBoolAspect ignoreCase;
    FvBoolAspect wrapScan;

    // command ~ behaves as g~
    FvBoolAspect tildeOp;

    // indent  allow backspacing over autoindent
    // eol     allow backspacing over line breaks (join lines)
    // start   allow backspacing over the start of insert; CTRL-W and CTRL-U
    //         stop once at the start of insert.
    FvStringAspect backspace;

    // @,48-57,_,192-255
    FvStringAspect isKeyword;

    // other actions
    FvBoolAspect showMarks;
    FvBoolAspect passControlKey;
    FvBoolAspect passKeys;
    FvStringAspect clipboard;
    FvBoolAspect showCmd;
    FvIntegerAspect scrollOff;
    FvBoolAspect relativeNumber;
    FvStringAspect formatOptions;

    // Plugin emulation
    FvBoolAspect emulateVimCommentary;
    FvBoolAspect emulateReplaceWithRegister;
    FvBoolAspect emulateExchange;
    FvBoolAspect emulateArgTextObj;
    FvBoolAspect emulateSurround;

    FvBoolAspect blinkingCursor;

private:
    void setup(FvBaseAspect *aspect, const QVariant &value,
                      const QString &settingsKey,
                      const QString &shortName,
                      const QString &label);

    QHash<QString, FvBaseAspect *> m_nameToAspect;
    QHash<FvBaseAspect *, QString> m_aspectToName;
};

FAKEVIM_EXPORT FakeVimSettings *fakeVimSettings();

} // namespace Internal
} // namespace FakeVim
