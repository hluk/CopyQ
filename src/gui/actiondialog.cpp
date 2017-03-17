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

#include "actiondialog.h"
#include "ui_actiondialog.h"

#include "common/appconfig.h"
#include "common/action.h"
#include "common/command.h"
#include "common/config.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "item/serialize.h"
#include "gui/windowgeometryguard.h"

#include <QFile>
#include <QMessageBox>

#include <memory>

namespace {

void initFormatComboBox(QComboBox *combo, const QStringList &additionalFormats = QStringList())
{
    QStringList formats = QStringList() << QString() << QString(mimeText) << additionalFormats;
    formats.removeDuplicates();
    combo->clear();
    combo->addItems(formats);
}

bool wasChangedByUser(QObject *object)
{
    return object->property("UserChanged").toBool();
}

void setChangedByUser(QWidget *object)
{
    object->setProperty("UserChanged", object->hasFocus());
}

QString commandToLabel(const QString &command)
{
    QString label = command.size() > 48 ? command.left(48) + "..." : command;
    label.replace('\n', " ");
    label.replace(QRegExp("\\s\\+"), " ");
    return label;
}

} // namespace

ActionDialog::ActionDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ActionDialog)
    , m_data()
    , m_capturedTexts()
    , m_currentCommandIndex(-1)
{
    ui->setupUi(this);
    ui->comboBoxCommands->setFont( ui->commandEdit->commandFont() );

    on_comboBoxInputFormat_currentIndexChanged(QString());
    on_comboBoxOutputFormat_editTextChanged(QString());
    loadSettings();

    WindowGeometryGuard::create(this);
}

ActionDialog::~ActionDialog()
{
    delete ui;
}

void ActionDialog::setInputData(const QVariantMap &data)
{
    m_data = data;

    QString defaultFormat = ui->comboBoxInputFormat->currentText();
    initFormatComboBox(ui->comboBoxInputFormat, data.keys());
    const int index = qMax(0, ui->comboBoxInputFormat->findText(defaultFormat));
    ui->comboBoxInputFormat->setCurrentIndex(index);
}

void ActionDialog::restoreHistory()
{
    const int maxCount = AppConfig().option<Config::command_history_size>();
    ui->comboBoxCommands->setMaxCount(maxCount + 1);

    QFile file( dataFilename() );
    file.open(QIODevice::ReadOnly);
    QDataStream in(&file);
    QVariant v;

    ui->comboBoxCommands->clear();
    ui->comboBoxCommands->addItem(QString());
    while( !in.atEnd() && ui->comboBoxCommands->count() <= maxCount ) {
        in >> v;
        const QVariantMap values = v.value<QVariantMap>();
        const QString cmd = values.value("cmd").toString();
        ui->comboBoxCommands->addItem( commandToLabel(cmd), v );
    }
    ui->comboBoxCommands->setCurrentIndex(0);
}

const QString ActionDialog::dataFilename() const
{
    return getConfigurationFilePath( QString("_cmds.dat") );
}

void ActionDialog::saveHistory()
{
    QFile file( dataFilename() );
    file.open(QIODevice::WriteOnly);
    QDataStream out(&file);

    for (int i = 1; i < ui->comboBoxCommands->count(); ++i) {
        const QVariant itemData = ui->comboBoxCommands->itemData(i);
        out << itemData;
    }
}

void ActionDialog::createAction()
{
    const QString cmd = ui->commandEdit->command();

    if ( cmd.isEmpty() )
        return;

    const QString inputFormat = ui->comboBoxInputFormat->currentText();
    const QString input =
            ui->inputText->isVisible() ? ui->inputText->toPlainText() : QString();

    // Expression %1 in command is always replaced with item text.
    if ( m_capturedTexts.isEmpty() )
        m_capturedTexts.append(QString());
    m_capturedTexts[0] = getTextData(m_data);

    std::unique_ptr<Action> act( new Action() );
    act->setCommand(cmd, m_capturedTexts);
    if (input.isEmpty() && !inputFormat.isEmpty())
        act->setInput(m_data, inputFormat);
    else
        act->setInput(input.toUtf8());
    act->setOutputFormat(ui->comboBoxOutputFormat->currentText());
    act->setItemSeparator(QRegExp(ui->separatorEdit->text()));
    act->setOutputTab(ui->comboBoxOutputTab->currentText());
    act->setName(m_actionName);
    act->setData(m_data);
    emit accepted(act.release());
}

void ActionDialog::setCommand(const Command &cmd)
{
    ui->comboBoxCommands->setCurrentIndex(0);
    ui->commandEdit->setCommand(cmd.cmd);
    ui->separatorEdit->setText(cmd.sep);

    int index = ui->comboBoxInputFormat->findText(cmd.input);
    if (index == -1) {
        ui->comboBoxInputFormat->insertItem(0, cmd.input);
        index = 0;
    }
    ui->comboBoxInputFormat->setCurrentIndex(index);

    ui->comboBoxOutputFormat->setEditText(cmd.output);

    m_capturedTexts = cmd.re.isEmpty() ? QStringList() : cmd.re.capturedTexts();
    m_actionName = cmd.name;
    m_actionName.remove('&');
}

void ActionDialog::setOutputTabs(const QStringList &tabs,
                                 const QString &currentTabName)
{
    QComboBox *w = ui->comboBoxOutputTab;
    w->clear();
    w->addItem("");
    w->addItems(tabs);
    w->setEditText(currentTabName);
}

void ActionDialog::loadSettings()
{
    initFormatComboBox(ui->comboBoxInputFormat);
    initFormatComboBox(ui->comboBoxOutputFormat);
    restoreHistory();
}

Command ActionDialog::command() const
{
    Command cmd;

    cmd.cmd = ui->commandEdit->command();
    cmd.name = commandToLabel(cmd.cmd);
    cmd.input = ui->comboBoxInputFormat->currentText();
    cmd.output = ui->comboBoxOutputFormat->currentText();
    cmd.sep = ui->separatorEdit->text();
    cmd.outputTab = ui->comboBoxOutputTab->currentText();

    return cmd;
}

void ActionDialog::done(int r)
{
    emit closed(this);

    QDialog::done(r);
}

void ActionDialog::on_buttonBox_clicked(QAbstractButton* button)
{
    switch ( ui->buttonBox->standardButton(button) ) {
    case QDialogButtonBox::Ok:
        createAction();
        saveCurrentCommandToHistory();
        close();
        break;

    case QDialogButtonBox::Apply:
        createAction();
        saveCurrentCommandToHistory();
        break;

    case QDialogButtonBox::Save:
        emit saveCommand(command());
        QMessageBox::information(
                    this, tr("Command saved"),
                    tr("Command was saved and can be accessed from item menu.\n"
                       "You can set up the command in preferences.") );
        break;

    case QDialogButtonBox::Cancel:
        close();
        break;

    default:
        break;
    }
}

void ActionDialog::on_comboBoxCommands_currentIndexChanged(int index)
{
    if ( m_currentCommandIndex >= 0 && m_currentCommandIndex < ui->comboBoxCommands->count() ) {
        QVariant itemData = createCurrentItemData();
        if (itemData != ui->comboBoxCommands->itemData(m_currentCommandIndex))
            ui->comboBoxCommands->setItemData(m_currentCommandIndex, itemData);
    }

    m_currentCommandIndex = index;

    // Restore values from history.
    QVariant v = ui->comboBoxCommands->itemData(index);
    QVariantMap values = v.value<QVariantMap>();

    ui->commandEdit->setCommand(values.value("cmd").toString());

    // Don't automatically change values if they were edited by user.
    if ( !wasChangedByUser(ui->comboBoxInputFormat) ) {
        int i = ui->comboBoxInputFormat->findText(values.value("input").toString());
        if (i != -1)
            ui->comboBoxInputFormat->setCurrentIndex(i);
    }

    if ( !wasChangedByUser(ui->comboBoxOutputFormat) )
        ui->comboBoxOutputFormat->setEditText(values.value("output").toString());

    if ( !wasChangedByUser(ui->separatorEdit) )
        ui->separatorEdit->setText(values.value("sep").toString());

    if ( !wasChangedByUser(ui->comboBoxOutputTab) )
        ui->comboBoxOutputTab->setEditText(values.value("outputTab").toString());
}

void ActionDialog::on_comboBoxInputFormat_currentIndexChanged(const QString &format)
{
    setChangedByUser(ui->comboBoxInputFormat);

    bool show = format.toLower().startsWith(QString("text"));
    ui->inputText->setVisible(show);

    QString text;
    if ((show || format.isEmpty()) && !m_data.isEmpty() )
        text = getTextData( m_data, format.isEmpty() ? mimeText : format );
    ui->inputText->setPlainText(text);
}

void ActionDialog::on_comboBoxOutputFormat_editTextChanged(const QString &text)
{
    setChangedByUser(ui->comboBoxOutputFormat);

    bool showSeparator = text.toLower().startsWith(QString("text"));
    ui->separatorLabel->setVisible(showSeparator);
    ui->separatorEdit->setVisible(showSeparator);

    bool showOutputTab = !text.isEmpty();
    ui->labelOutputTab->setVisible(showOutputTab);
    ui->comboBoxOutputTab->setVisible(showOutputTab);
}

void ActionDialog::on_comboBoxOutputTab_editTextChanged(const QString &)
{
    setChangedByUser(ui->comboBoxOutputTab);
}

void ActionDialog::on_separatorEdit_textEdited(const QString &)
{
    setChangedByUser(ui->separatorEdit);
}

QVariant ActionDialog::createCurrentItemData()
{
    QVariantMap values;
    values["cmd"] = ui->commandEdit->command();
    values["input"] = ui->comboBoxInputFormat->currentText();
    values["output"] = ui->comboBoxOutputFormat->currentText();
    values["sep"] = ui->separatorEdit->text();
    values["outputTab"] = ui->comboBoxOutputTab->currentText();

    return values;
}

void ActionDialog::saveCurrentCommandToHistory()
{
    const QString cmd = ui->commandEdit->command();
    if (cmd.isEmpty())
        return;

    const QVariant itemData = createCurrentItemData();
    ui->comboBoxCommands->setCurrentIndex(0);

    for (int i = ui->comboBoxCommands->count() - 1; i >= 1; --i) {
        const QVariant itemData2 = ui->comboBoxCommands->itemData(i);
        const QString cmd2 = itemData2.toMap().value("cmd").toString();
        if (cmd == cmd2)
            ui->comboBoxCommands->removeItem(i);
    }

    ui->comboBoxCommands->insertItem(1, commandToLabel(cmd), itemData);
    ui->comboBoxCommands->setCurrentIndex(1);

    saveHistory();
}
