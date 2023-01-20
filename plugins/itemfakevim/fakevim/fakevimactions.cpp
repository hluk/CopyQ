// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "fakevimactions.h"
#include "fakevimhandler.h"

// Please do not add any direct dependencies to other Qt Creator code  here.
// Instead emit signals and let the FakeVimPlugin channel the information to
// Qt Creator. The idea is to keep this file here in a "clean" state that
// allows easy reuse with any QTextEdit or QPlainTextEdit derived class.

namespace FakeVim {
namespace Internal {

#ifdef FAKEVIM_STANDALONE
FvBaseAspect::FvBaseAspect()
{
}

void FvBaseAspect::setValue(const QVariant &value)
{
    m_value = value;
}

QVariant FvBaseAspect::value() const
{
    return m_value;
}

void FvBaseAspect::setDefaultValue(const QVariant &value)
{
    m_defaultValue = value;
    m_value = value;
}

QVariant FvBaseAspect::defaultValue() const
{
    return m_defaultValue;
}

void FvBaseAspect::setSettingsKey(const QString &group, const QString &key)
{
    m_settingsGroup = group;
    m_settingsKey = key;
}

QString FvBaseAspect::settingsKey() const
{
    return m_settingsKey;
}

// unused but kept for compile
void setAutoApply(bool ) {}
#endif

FakeVimSettings::FakeVimSettings()
{
    setAutoApply(false);

#ifndef FAKEVIM_STANDALONE
    setup(&useFakeVim,     false, "UseFakeVim",     {},    tr("Use FakeVim"));
#endif
    // Specific FakeVim settings
    setup(&readVimRc,      false, "ReadVimRc",      {},    tr("Read .vimrc from location:"));
    setup(&vimRcPath,      QString(), "VimRcPath",  {},    {}); // tr("Path to .vimrc")
    setup(&showMarks,      false, "ShowMarks",      "sm",  tr("Show position of text marks"));
    setup(&passControlKey, false, "PassControlKey", "pck", tr("Pass control keys"));
    setup(&passKeys,       true,  "PassKeys",       "pk",  tr("Pass keys in insert mode"));

    // Emulated Vsetting
    setup(&startOfLine,    true,  "StartOfLine",    "sol", tr("Start of line"));
    setup(&tabStop,        8,     "TabStop",        "ts",  tr("Tabulator size:"));
    setup(&smartTab,       false, "SmartTab",       "sta", tr("Smart tabulators"));
    setup(&hlSearch,       true,  "HlSearch",       "hls", tr("Highlight search results"));
    setup(&shiftWidth,     8,     "ShiftWidth",     "sw",  tr("Shift width:"));
    setup(&expandTab,      false, "ExpandTab",      "et",  tr("Expand tabulators"));
    setup(&autoIndent,     false, "AutoIndent",     "ai",  tr("Automatic indentation"));
    setup(&smartIndent,    false, "SmartIndent",    "si",  tr("Smart indentation"));
    setup(&incSearch,      true,  "IncSearch",      "is",  tr("Incremental search"));
    setup(&useCoreSearch,  false, "UseCoreSearch",  "ucs", tr("Use search dialog"));
    setup(&smartCase,      false, "SmartCase",      "scs", tr("Use smartcase"));
    setup(&ignoreCase,     false, "IgnoreCase",     "ic",  tr("Use ignorecase"));
    setup(&wrapScan,       true,  "WrapScan",       "ws",  tr("Use wrapscan"));
    setup(&tildeOp,        false, "TildeOp",        "top", tr("Use tildeop"));
    setup(&showCmd,        true,  "ShowCmd",        "sc",  tr("Show partial command"));
    setup(&relativeNumber, false, "RelativeNumber", "rnu", tr("Show line numbers relative to cursor"));
    setup(&blinkingCursor, false, "BlinkingCursor", "bc",  tr("Blinking cursor"));
    setup(&scrollOff,      0,     "ScrollOff",      "so",  tr("Scroll offset:"));
    setup(&backspace,      "indent,eol,start",
                                  "Backspace",      "bs",  tr("Backspace:"));
    setup(&isKeyword,      "@,48-57,_,192-255,a-z,A-Z",
                                  "IsKeyword",      "isk", tr("Keyword characters:"));
    setup(&clipboard,      {},    "Clipboard",      "cb",  tr(""));
    setup(&formatOptions,  {},    "formatoptions",  "fo",  tr(""));

    // Emulated plugins
    setup(&emulateVimCommentary, false, "commentary", {}, "vim-commentary");
    setup(&emulateReplaceWithRegister, false, "ReplaceWithRegister", {}, "ReplaceWithRegister");
    setup(&emulateExchange, false, "exchange", {}, "vim-exchange");
    setup(&emulateArgTextObj, false, "argtextobj", {}, "argtextobj.vim");
    setup(&emulateSurround, false, "surround", {}, "vim-surround");

    // Some polish
    useFakeVim.setDisplayName(tr("Use Vim-style Editing"));

    relativeNumber.setToolTip(tr("Displays line numbers relative to the line containing "
        "text cursor."));

    passControlKey.setToolTip(tr("Does not interpret key sequences like Ctrl-S in FakeVim "
        "but handles them as regular shortcuts. This gives easier access to core functionality "
        "at the price of losing some features of FakeVim."));

    passKeys.setToolTip(tr("Does not interpret some key presses in insert mode so that "
        "code can be properly completed and expanded."));

    tabStop.setToolTip(tr("Vim tabstop option."));

#ifndef FAKEVIM_STANDALONE
    backspace.setDisplayStyle(FvStringAspect::LineEditDisplay);
    isKeyword.setDisplayStyle(FvStringAspect::LineEditDisplay);

    const QString vimrcDefault = QLatin1String(
#ifdef Q_OS_UNIX
        "$HOME/.vimrc"
#else
        "%USERPROFILE%\\_vimrc"
#endif
    )
    vimRcPath.setExpectedKind(PathChooser::File);
    vimRcPath.setToolTip(tr("Keep empty to use the default path, i.e. "
               "%USERPROFILE%\\_vimrc on Windows, ~/.vimrc otherwise."));
    vimRcPath.setPlaceHolderText(tr("Default: %1").arg(vimrcDefault));
    vimRcPath.setDisplayStyle(FvStringAspect::PathChooserDisplay);
#endif
}

FakeVimSettings::~FakeVimSettings() = default;

FvBaseAspect *FakeVimSettings::item(const QString &name)
{
    return m_nameToAspect.value(name, nullptr);
}

QString FakeVimSettings::trySetValue(const QString &name, const QString &value)
{
    FvBaseAspect *aspect = m_nameToAspect.value(name, nullptr);
    if (!aspect)
        return tr("Unknown option: %1").arg(name);
    if (aspect == &tabStop || aspect == &shiftWidth) {
        if (value.toInt() <= 0)
            return tr("Argument must be positive: %1=%2")
                    .arg(name).arg(value);
    }
    aspect->setValue(value);
    return QString();
}

void FakeVimSettings::setup(FvBaseAspect *aspect,
                            const QVariant &value,
                            const QString &settingsKey,
                            const QString &shortName,
                            const QString &labelText)
{
    aspect->setSettingsKey("FakeVim", settingsKey);
    aspect->setDefaultValue(value);
#ifndef FAKEVIM_STANDALONE
    aspect->setLabelText(labelText);
    aspect->setAutoApply(false);
    registerAspect(aspect);

    if (auto boolAspect = dynamic_cast<FvBoolAspect *>(aspect))
        boolAspect->setLabelPlacement(FvBoolAspect::LabelPlacement::AtCheckBoxWithoutDummyLabel);
#else
    Q_UNUSED(labelText)
#endif

    const QString longName = settingsKey.toLower();
    if (!longName.isEmpty()) {
        m_nameToAspect[longName] = aspect;
        m_aspectToName[aspect] = longName;
    }
    if (!shortName.isEmpty())
        m_nameToAspect[shortName] = aspect;
}

FakeVimSettings *fakeVimSettings()
{
    static FakeVimSettings s;
    return &s;
}

} // namespace Internal
} // namespace FakeVim
