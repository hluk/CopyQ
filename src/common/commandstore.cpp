// SPDX-License-Identifier: GPL-3.0-or-later

#include "commandstore.h"

#include "common/command.h"
#include "common/common.h"
#include "common/config.h"
#include "common/mimetypes.h"
#include "common/settings.h"
#include "common/temporarysettings.h"
#include "common/textdata.h"

#include <QSettings>
#include <QString>

namespace {

const QLatin1String falseString("false");
const QLatin1String trueString("true");

void normalizeLineBreaks(QString &cmd)
{
    if (cmd.startsWith("\n    ")) {
        cmd.remove(0, 5);
        cmd.replace("\n    ", "\n");
    } else if (cmd.startsWith("\r\n    ")) {
        cmd.remove(0, 6);
        cmd.replace("\r\n    ", "\n");
    }
}

void loadCommand(const QSettings &settings, Commands *commands)
{
    Command c;
    c.enable = settings.value(QStringLiteral("Enable"), true).toBool();

    c.name = settings.value(QStringLiteral("Name")).toString();
    c.re   = QRegularExpression( settings.value(QStringLiteral("Match")).toString() );
    c.wndre = QRegularExpression( settings.value(QStringLiteral("Window")).toString() );
    c.matchCmd = settings.value(QStringLiteral("MatchCommand")).toString();
    c.cmd = settings.value(QStringLiteral("Command")).toString();
    c.sep = settings.value(QStringLiteral("Separator")).toString();

    c.input = settings.value(QStringLiteral("Input")).toString();
    if (c.input == falseString || c.input == trueString)
        c.input = c.input == trueString ? mimeText : QLatin1String();

    c.output = settings.value(QStringLiteral("Output")).toString();
    if (c.output == falseString || c.output == trueString)
        c.output = c.output == trueString ? mimeText : QLatin1String();

    c.wait = settings.value(QStringLiteral("Wait")).toBool();
    c.automatic = settings.value(QStringLiteral("Automatic")).toBool();
    c.display = settings.value(QStringLiteral("Display")).toBool();
    c.transform = settings.value(QStringLiteral("Transform")).toBool();
    c.hideWindow = settings.value(QStringLiteral("HideWindow")).toBool();
    c.icon = settings.value(QStringLiteral("Icon")).toString();
    c.shortcuts = settings.value(QStringLiteral("Shortcut")).toStringList();
    c.globalShortcuts = settings.value(QStringLiteral("GlobalShortcut")).toStringList();
    c.tab = settings.value(QStringLiteral("Tab")).toString();
    c.outputTab = settings.value(QStringLiteral("OutputTab")).toString();
    c.internalId = settings.value(QStringLiteral("InternalId")).toString();
    c.inMenu = settings.value(QStringLiteral("InMenu")).toBool();
    c.isScript = settings.value(QStringLiteral("IsScript")).toBool();

    const auto globalShortcutsOption = settings.value(QStringLiteral("IsGlobalShortcut"));
    if ( globalShortcutsOption.isValid() ) {
        c.isGlobalShortcut = globalShortcutsOption.toBool();
    } else {
        // Backwards compatibility with v3.1.2 and below.
        if ( c.globalShortcuts.contains(QLatin1String("DISABLED")) )
            c.globalShortcuts.clear();
        c.isGlobalShortcut = !c.globalShortcuts.isEmpty();
    }

    if (settings.value(QStringLiteral("Ignore")).toBool())
        c.remove = c.automatic = true;
    else
        c.remove = settings.value(QStringLiteral("Remove")).toBool();

    commands->append(c);
}

void saveValue(const QString &key, const QRegularExpression &re, QSettings *settings)
{
    settings->setValue(key, re.pattern());
}

void saveValue(const QString &key, const QVariant &value, QSettings *settings)
{
    settings->setValue(key, value);
}

/// Save only modified command properties.
template <typename Member>
void saveNewValue(const QString &key, const Command &command, const Member &member, QSettings *settings)
{
    if (command.*member != Command().*member)
        saveValue(key, command.*member, settings);
}

void saveCommand(const Command &c, QSettings *settings)
{
    saveNewValue(QStringLiteral("Name"), c, &Command::name, settings);
    saveNewValue(QStringLiteral("Match"), c, &Command::re, settings);
    saveNewValue(QStringLiteral("Window"), c, &Command::wndre, settings);
    saveNewValue(QStringLiteral("MatchCommand"), c, &Command::matchCmd, settings);
    saveNewValue(QStringLiteral("Command"), c, &Command::cmd, settings);
    saveNewValue(QStringLiteral("Input"), c, &Command::input, settings);
    saveNewValue(QStringLiteral("Output"), c, &Command::output, settings);

    // Separator for new command is set to '\n' for convenience.
    // But this value shouldn't be saved if output format is not set.
    if ( c.sep != QLatin1String("\\n") || !c.output.isEmpty() )
        saveNewValue(QStringLiteral("Separator"), c, &Command::sep, settings);

    saveNewValue(QStringLiteral("Wait"), c, &Command::wait, settings);
    saveNewValue(QStringLiteral("Automatic"), c, &Command::automatic, settings);
    saveNewValue(QStringLiteral("Display"), c, &Command::display, settings);
    saveNewValue(QStringLiteral("InMenu"), c, &Command::inMenu, settings);
    saveNewValue(QStringLiteral("IsGlobalShortcut"), c, &Command::isGlobalShortcut, settings);
    saveNewValue(QStringLiteral("IsScript"), c, &Command::isScript, settings);
    saveNewValue(QStringLiteral("Transform"), c, &Command::transform, settings);
    saveNewValue(QStringLiteral("Remove"), c, &Command::remove, settings);
    saveNewValue(QStringLiteral("HideWindow"), c, &Command::hideWindow, settings);
    saveNewValue(QStringLiteral("Enable"), c, &Command::enable, settings);
    saveNewValue(QStringLiteral("Icon"), c, &Command::icon, settings);
    saveNewValue(QStringLiteral("Shortcut"), c, &Command::shortcuts, settings);
    saveNewValue(QStringLiteral("GlobalShortcut"), c, &Command::globalShortcuts, settings);
    saveNewValue(QStringLiteral("Tab"), c, &Command::tab, settings);
    saveNewValue(QStringLiteral("OutputTab"), c, &Command::outputTab, settings);
    saveNewValue(QStringLiteral("InternalId"), c, &Command::internalId, settings);
}

Commands importCommands(QSettings *settings)
{
    auto commands = loadCommands(settings);

    for (auto &command : commands) {
        normalizeLineBreaks(command.cmd);
        normalizeLineBreaks(command.matchCmd);
    }

    return commands;
}

} // namespace

Commands loadAllCommands()
{
    const QString commandConfigPath = getConfigurationFilePath("-commands.ini");
    return importCommandsFromFile(commandConfigPath);
}

void saveCommands(const Commands &commands)
{
    Settings settings(getConfigurationFilePath("-commands.ini"));
    saveCommands(commands, &settings);
}

Commands loadCommands(QSettings *settings)
{
    Commands commands;

    const QStringList groups = settings->childGroups();

    if ( groups.contains("Command") ) {
        settings->beginGroup("Command");
        loadCommand(*settings, &commands);
        settings->endGroup();
    }

    const int size = settings->beginReadArray("Commands");
    // Allow undefined "size" for the array in settings.
    if (size == 0) {
        for (int i = 0; ; ++i) {
            settings->setArrayIndex(i);
            if ( settings->childKeys().isEmpty() )
                break;
            loadCommand(*settings, &commands);
        }
    } else {
        for (int i = 0; i < size; ++i) {
            settings->setArrayIndex(i);
            loadCommand(*settings, &commands);
        }
    }

    settings->endArray();

    return commands;
}

void saveCommands(const Commands &commands, QSettings *settings)
{
    settings->remove("Commands");
    settings->remove("Command");

    if (commands.size() == 1) {
        settings->beginGroup("Command");
        saveCommand(commands[0], settings);
        settings->endGroup();
    } else {
        settings->beginWriteArray("Commands");
        int i = 0;
        for (const auto &c : commands) {
            settings->setArrayIndex(i++);
            saveCommand(c, settings);
        }
        settings->endArray();
    }
}

Commands importCommandsFromFile(const QString &filePath)
{
    QSettings commandsSettings(filePath, QSettings::IniFormat);
    return importCommands(&commandsSettings);
}

Commands importCommandsFromText(const QString &commands)
{
    TemporarySettings temporarySettings(commands.toUtf8());
    return importCommands( temporarySettings.settings() );
}

QString exportCommands(const Commands &commands)
{
    TemporarySettings temporarySettings;
    saveCommands( commands, temporarySettings.settings() );

    // Replace ugly '\n' with indented lines.
    const QString data = getTextData( temporarySettings.content() );
    QString commandData;
    QRegularExpression re(R"(^(\d+\\)?(Command|MatchCommand)="?)");

    for (const auto &line : data.split('\n')) {
        const auto m = re.match(line);
        if (m.hasMatch()) {
            int i = m.capturedLength();
            commandData.append(line.left(i));

            const bool addQuotes = !commandData.endsWith('"');
            if (addQuotes)
                commandData.append('"');

            commandData.append('\n');
            const QLatin1String indent("    ");
            bool escape = false;

            for (; i < line.size(); ++i) {
                const QChar c = line[i];

                if (escape) {
                    escape = false;

                    if (c == 'n') {
                        commandData.append('\n');
                    } else {
                        if ( commandData.endsWith('\n') )
                            commandData.append(indent);
                        commandData.append('\\');
                        commandData.append(c);
                    }
                } else if (c == '\\') {
                    escape = !escape;
                } else {
                    if ( commandData.endsWith('\n') )
                        commandData.append(indent);
                    commandData.append(c);
                }
            }

            if (addQuotes)
                commandData.append('"');
        } else {
            commandData.append(line);
        }

        commandData.append('\n');
    }

    return commandData.trimmed();
}
