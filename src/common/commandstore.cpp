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
const QRegularExpression nameLocalizationRe(R"(^Name(?:_(.*))?$)");

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

    for (auto &&key : settings.childKeys()) {
        if (key == QLatin1String("Enable")) {
            c.enable = settings.value(key).toBool();
        } else if (auto match = nameLocalizationRe.match(key); match.hasMatch()) {
            const QString languageCode = match.captured(1);
            const QString value = settings.value(key).toString();
            if (languageCode.isEmpty())
                c.name = value;
            else
                c.nameLocalization[languageCode] = value;
        } else if (key == QLatin1String("Match")) {
            c.re = QRegularExpression( settings.value(key).toString() );
        } else if (key == QLatin1String("Window")) {
            c.wndre = QRegularExpression( settings.value(key).toString() );
        } else if (key == QLatin1String("MatchCommand")) {
            c.matchCmd = settings.value(key).toString();
        } else if (key == QLatin1String("Command")) {
            c.cmd = settings.value(key).toString();
        } else if (key == QLatin1String("Separator")) {
            c.sep = settings.value(key).toString();
        } else if (key == QLatin1String("Input")) {
            c.input = settings.value(key).toString();
            if (c.input == falseString || c.input == trueString)
                c.input = c.input == trueString ? mimeText : QLatin1String();
        } else if (key == QLatin1String("Output")) {
            c.output = settings.value(key).toString();
            if (c.output == falseString || c.output == trueString)
                c.output = c.output == trueString ? mimeText : QLatin1String();
        } else if (key == QLatin1String("Wait")) {
            c.wait = settings.value(key).toBool();
        } else if (key == QLatin1String("Automatic")) {
            c.automatic = settings.value(key).toBool();
        } else if (key == QLatin1String("Display")) {
            c.display = settings.value(key).toBool();
        } else if (key == QLatin1String("Transform")) {
            c.transform = settings.value(key).toBool();
        } else if (key == QLatin1String("HideWindow")) {
            c.hideWindow = settings.value(key).toBool();
        } else if (key == QLatin1String("Icon")) {
            c.icon = settings.value(key).toString();
        } else if (key == QLatin1String("Shortcut")) {
            c.shortcuts = settings.value(key).toStringList();
        } else if (key == QLatin1String("GlobalShortcut")) {
            c.globalShortcuts = settings.value(key).toStringList();
        } else if (key == QLatin1String("Tab")) {
            c.tab = settings.value(key).toString();
        } else if (key == QLatin1String("OutputTab")) {
            c.outputTab = settings.value(key).toString();
        } else if (key == QLatin1String("InternalId")) {
            c.internalId = settings.value(key).toString();
        } else if (key == QLatin1String("InMenu")) {
            c.inMenu = settings.value(key).toBool();
        } else if (key == QLatin1String("IsScript")) {
            c.isScript = settings.value(key).toBool();
        } else if (key == QLatin1String("IsGlobalShortcut")) {
            c.isGlobalShortcut = settings.value(key).toBool();
        } else if (key == QLatin1String("Remove")) {
            c.remove = settings.value(key).toBool();
        } else if (key == QLatin1String("Ignore")) {
            if (settings.value(key).toBool())
                c.remove = c.automatic = true;
        }
    }

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

    for (auto it = c.nameLocalization.constBegin(); it != c.nameLocalization.constEnd(); ++it) {
        const auto key = QStringLiteral("Name_%1").arg(it.key());
        saveValue(key, it.value(), settings);
    }
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
    TemporarySettings temporarySettings({});
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
