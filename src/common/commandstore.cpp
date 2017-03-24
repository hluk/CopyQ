/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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
#include "common/mimetypes.h"
#include "common/settings.h"
#include "common/temporarysettings.h"
#include "common/textdata.h"

#include <QSettings>
#include <QString>

namespace {

void loadCommand(const QSettings &settings, CommandFilter filter, Commands *commands)
{
    Command c;
    c.enable = settings.value("Enable", true).toBool();

    if (filter == EnabledCommands && !c.enable)
        return;

    c.name = settings.value("Name").toString();
    c.re   = QRegExp( settings.value("Match").toString() );
    c.wndre = QRegExp( settings.value("Window").toString() );
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
    c.transform = settings.value("Transform").toBool();
    c.hideWindow = settings.value("HideWindow").toBool();
    c.icon = settings.value("Icon").toString();
    c.shortcuts = settings.value("Shortcut").toStringList();
    c.globalShortcuts = settings.value("GlobalShortcut").toStringList();
    c.tab = settings.value("Tab").toString();
    c.outputTab = settings.value("OutputTab").toString();
    c.inMenu = settings.value("InMenu").toBool();

    if (c.globalShortcuts.size() == 1 && c.globalShortcuts[0] == "DISABLED")
        c.globalShortcuts = QStringList();

    if (settings.value("Ignore").toBool())
        c.remove = c.automatic = true;
    else
        c.remove = settings.value("Remove").toBool();

    commands->append(c);
}

void saveValue(const char *key, const QRegExp &re, QSettings *settings)
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
    saveNewValue("InMenu", c, &Command::inMenu, settings);
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
    auto commands = loadCommands(settings, AllCommands);
    for (auto &command : commands) {
        if (command.cmd.startsWith("\n    ")) {
            command.cmd.remove(0, 5);
            command.cmd.replace("\n    ", "\n");
        }
    }

    return commands;
}

} // namespace

Commands loadEnabledCommands()
{
    QSettings settings;
    return loadCommands(&settings, EnabledCommands);
}

Commands loadAllCommands()
{
    QSettings settings;
    return loadCommands(&settings, AllCommands);
}

void saveCommands(const Commands &commands)
{
    Settings settings;
    saveCommands(commands, settings.settingsData());
}

Commands loadCommands(QSettings *settings, CommandFilter filter)
{
    Commands commands;

    const QStringList groups = settings->childGroups();

    if ( groups.contains("Command") ) {
        settings->beginGroup("Command");
        loadCommand(*settings, filter, &commands);
        settings->endGroup();
    }

    int size = settings->beginReadArray("Commands");

    for(int i=0; i<size; ++i) {
        settings->setArrayIndex(i);
        loadCommand(*settings, filter, &commands);
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
    QRegExp re(R"(^(\d+\\)?Command="?)");

    for (const auto &line : data.split('\n')) {
        if (line.contains(re)) {
            int i = re.matchedLength();
            commandData.append(line.left(i));

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

    return commandData;
}
