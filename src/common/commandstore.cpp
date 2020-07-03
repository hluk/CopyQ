/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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

void loadCommand(const QSettings &settings, Commands *commands)
{
    Command c;
    c.enable = settings.value("Enable", true).toBool();

    c.name = settings.value("Name").toString();
    c.re   = QRegularExpression( settings.value("Match").toString() );
    c.wndre = QRegularExpression( settings.value("Window").toString() );
    c.matchCmd = settings.value("MatchCommand").toString();
    c.cmd = settings.value("Command").toString();
    c.sep = settings.value("Separator").toString();

    c.input = settings.value("Input").toString();
    if ( c.input == "false" || c.input == "true" )
        c.input = c.input == "true" ? QString(mimeText) : QString();

    c.output = settings.value("Output").toString();
    if ( c.output == "false" || c.output == "true" )
        c.output = c.output == "true" ? QString(mimeText) : QString();

    c.wait = settings.value("Wait").toBool();
    c.automatic = settings.value("Automatic").toBool();
    c.display = settings.value("Display").toBool();
    c.transform = settings.value("Transform").toBool();
    c.hideWindow = settings.value("HideWindow").toBool();
    c.icon = settings.value("Icon").toString();
    c.shortcuts = settings.value("Shortcut").toStringList();
    c.globalShortcuts = settings.value("GlobalShortcut").toStringList();
    c.tab = settings.value("Tab").toString();
    c.outputTab = settings.value("OutputTab").toString();
    c.inMenu = settings.value("InMenu").toBool();
    c.isScript = settings.value("IsScript").toBool();

    const auto globalShortcutsOption = settings.value("IsGlobalShortcut");
    if ( globalShortcutsOption.isValid() ) {
        c.isGlobalShortcut = settings.value("IsGlobalShortcut").toBool();
    } else {
        // Backwards compatibility with v3.1.2 and below.
        if ( c.globalShortcuts.contains("DISABLED") )
            c.globalShortcuts.clear();
        c.isGlobalShortcut = !c.globalShortcuts.isEmpty();
    }

    if (settings.value("Ignore").toBool())
        c.remove = c.automatic = true;
    else
        c.remove = settings.value("Remove").toBool();

    commands->append(c);
}

void saveValue(const char *key, const QRegularExpression &re, QSettings *settings)
{
    settings->setValue(key, re.pattern());
}

void saveValue(const char *key, const QVariant &value, QSettings *settings)
{
    settings->setValue(key, value);
}

/// Save only modified command properties.
template <typename Member>
void saveNewValue(const char *key, const Command &command, const Member &member, QSettings *settings)
{
    if (command.*member != Command().*member)
        saveValue(key, command.*member, settings);
}

void saveCommand(const Command &c, QSettings *settings)
{
    saveNewValue("Name", c, &Command::name, settings);
    saveNewValue("Match", c, &Command::re, settings);
    saveNewValue("Window", c, &Command::wndre, settings);
    saveNewValue("MatchCommand", c, &Command::matchCmd, settings);
    saveNewValue("Command", c, &Command::cmd, settings);
    saveNewValue("Input", c, &Command::input, settings);
    saveNewValue("Output", c, &Command::output, settings);

    // Separator for new command is set to '\n' for convenience.
    // But this value shouldn't be saved if output format is not set.
    if ( c.sep != "\\n" || !c.output.isEmpty() )
        saveNewValue("Separator", c, &Command::sep, settings);

    saveNewValue("Wait", c, &Command::wait, settings);
    saveNewValue("Automatic", c, &Command::automatic, settings);
    saveNewValue("Display", c, &Command::display, settings);
    saveNewValue("InMenu", c, &Command::inMenu, settings);
    saveNewValue("IsGlobalShortcut", c, &Command::isGlobalShortcut, settings);
    saveNewValue("IsScript", c, &Command::isScript, settings);
    saveNewValue("Transform", c, &Command::transform, settings);
    saveNewValue("Remove", c, &Command::remove, settings);
    saveNewValue("HideWindow", c, &Command::hideWindow, settings);
    saveNewValue("Enable", c, &Command::enable, settings);
    saveNewValue("Icon", c, &Command::icon, settings);
    saveNewValue("Shortcut", c, &Command::shortcuts, settings);
    saveNewValue("GlobalShortcut", c, &Command::globalShortcuts, settings);
    saveNewValue("Tab", c, &Command::tab, settings);
    saveNewValue("OutputTab", c, &Command::outputTab, settings);
}

Commands importCommands(QSettings *settings)
{
    auto commands = loadCommands(settings);

    for (auto &command : commands) {
        if (command.cmd.startsWith("\n    ")) {
            command.cmd.remove(0, 5);
            command.cmd.replace("\n    ", "\n");
        } else if (command.cmd.startsWith("\r\n    ")) {
            command.cmd.remove(0, 6);
            command.cmd.replace("\r\n    ", "\n");
        }
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
    saveCommands(commands, settings.settingsData());
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
    QRegularExpression re(R"(^(\d+\\)?Command="?)");

    for (const auto &line : data.split('\n')) {
        const auto m = re.match(line);
        if (m.hasMatch()) {
            int i = m.capturedLength();
            commandData.append(line.leftRef(i));

            const bool addQuotes = !commandData.endsWith('"');
            if (addQuotes)
                commandData.append('"');

            commandData.append("\n    ");
            bool escape = false;

            for (; i < line.size(); ++i) {
                const QChar c = line[i];

                if (escape) {
                    escape = false;

                    if (c == 'n') {
                        commandData.append("\n    ");
                    } else {
                        commandData.append('\\');
                        commandData.append(c);
                    }
                } else if (c == '\\') {
                    escape = !escape;
                } else {
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
