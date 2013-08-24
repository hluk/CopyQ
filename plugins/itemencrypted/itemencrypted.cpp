/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "itemencrypted.h"
#include "ui_itemencryptedsettings.h"

#include "common/contenttype.h"
#include "item/encrypt.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSettings>
#include <QtPlugin>

namespace {

const QString defaultFormat = QString("application/x-copyq-encrypted-text");

void startGpgProcess(QProcess *p, const QStringList &args)
{
    p->start("gpg", getDefaultEncryptCommandArguments() + args);
}

QByteArray readGpgOutput(const QStringList &args, const QByteArray &input = QByteArray())
{
    QProcess p;
    startGpgProcess( &p, args );
    p.write(input);
    p.closeWriteChannel();
    p.waitForFinished();
    return p.readAllStandardOutput();
}

bool keysExist()
{
    return !readGpgOutput( QStringList("--list-keys") ).isEmpty();
}

} // namespace

ItemEncrypted::ItemEncrypted(const QModelIndex &index, QWidget *parent)
    : QWidget(parent)
    , ItemWidget(this)
    , m_iconLabel(new QLabel(this))
    , m_notesLabel(NULL)
{
    QHBoxLayout *layout = new QHBoxLayout(this);

    // Show small icon.
    m_iconLabel->setObjectName("item_child");
    m_iconLabel->setFont( QFont("FontAwesome") );
    m_iconLabel->setText( QChar(0xf023) );
    layout->addWidget(m_iconLabel);

    // Show item notes if available.
    const QString notes = index.data(contentType::notes).toString();
    if ( !notes.isEmpty() ) {
        m_notesLabel = new QLabel(this);
        m_notesLabel->setObjectName("item_child");
        m_notesLabel->setTextFormat(Qt::PlainText);
        m_notesLabel->setWordWrap(true);
        m_notesLabel->setText(notes);
        layout->addWidget(m_notesLabel);
        layout->setStretchFactor(m_notesLabel, 1);
    }

    updateSize();
}

void ItemEncrypted::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    // Decrypt before editing.
    QPlainTextEdit *textEdit = qobject_cast<QPlainTextEdit *>(editor);
    if (textEdit != NULL) {
        const QStringList formats = index.data(contentType::formats).toStringList();
        const int i = formats.indexOf(defaultFormat);
        if (i != -1) {
            const QByteArray data = index.data(contentType::firstFormat + i).toByteArray();
            const QString text = QString::fromUtf8( readGpgOutput(QStringList("--decrypt"), data) );
            textEdit->setPlainText(text);
            textEdit->selectAll();
        }
    }
}

void ItemEncrypted::setModelData(QWidget *editor, QAbstractItemModel *model,
                                 const QModelIndex &index) const
{
    // Encrypt after editing.
    QPlainTextEdit *textEdit = qobject_cast<QPlainTextEdit*>(editor);
    if (textEdit != NULL) {
        const QStringList formats = index.data(contentType::formats).toStringList();
        const int i = formats.indexOf(defaultFormat);
        if (i != -1) {
            const QString text = textEdit->toPlainText();
            const QByteArray data = readGpgOutput( QStringList("--encrypt"), text.toUtf8() );
            model->setData( index, data, contentType::firstFormat + i );
        }
    }
}

void ItemEncrypted::updateSize()
{
    setMinimumWidth(maximumWidth());
    adjustSize();
}

ItemEncryptedLoader::ItemEncryptedLoader()
    : ui(NULL)
    , m_settings()
    , m_gpgProcessStatus(GpgNotRunning)
    , m_gpgProcess(NULL)
{
}

ItemEncryptedLoader::~ItemEncryptedLoader()
{
    terminateGpgProcess();
    delete ui;
}

ItemWidget *ItemEncryptedLoader::create(const QModelIndex &index, QWidget *parent) const
{
    const QStringList formats = index.data(contentType::formats).toStringList();
    return formats.contains(defaultFormat) ? new ItemEncrypted(index, parent) : NULL;
}

QStringList ItemEncryptedLoader::formatsToSave() const
{
    return QStringList(defaultFormat);
}

QVariantMap ItemEncryptedLoader::applySettings()
{
    Q_ASSERT(ui != NULL);
    return  m_settings;
}

QWidget *ItemEncryptedLoader::createSettingsWidget(QWidget *parent)
{
    delete ui;
    ui = new Ui::ItemEncryptedSettings;
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);
    updateUi();

    connect( ui->pushButtonPassword, SIGNAL(clicked()),
             this, SLOT(setPassword()) );

    return w;
}

void ItemEncryptedLoader::setPassword()
{
    if (m_gpgProcess != NULL) {
        terminateGpgProcess();
        return;
    }

    if ( !keysExist() ) {
        // Generate keys if they don't exist.
        const KeyPairPaths keys;
        m_gpgProcessStatus = GpgGeneratingKeys;
        m_gpgProcess = new QProcess(this);
        startGpgProcess( m_gpgProcess, QStringList() << "--batch" << "--gen-key" );
        m_gpgProcess->write( "\nKey-Type: RSA"
                 "\nKey-Usage: encrypt"
                 "\nKey-Length: 2048"
                 "\nName-Real: copyq"
                 "\n%secring " + keys.sec.toUtf8() +
                 "\n%pubring " + keys.pub.toUtf8() +
                 "\n%commit"
                 "\n" );
        m_gpgProcess->closeWriteChannel();

        connect( m_gpgProcess, SIGNAL(finished(int,QProcess::ExitStatus)),
                 this, SLOT(onGpgProcessFinished(int,QProcess::ExitStatus)) );
    } else {
        // Change password.
        m_gpgProcessStatus = GpgChangingPassword;
        m_gpgProcess = new QProcess(this);
        startGpgProcess( m_gpgProcess, QStringList() << "--edit-key" << "copyq" << "passwd" << "save");

        connect( m_gpgProcess, SIGNAL(finished(int,QProcess::ExitStatus)),
                 this, SLOT(onGpgProcessFinished(int,QProcess::ExitStatus)) );
    }

    updateUi();
}

void ItemEncryptedLoader::terminateGpgProcess()
{
    if (m_gpgProcess == NULL)
        return;
    QProcess *p = m_gpgProcess;
    m_gpgProcess = NULL;
    p->terminate();
    p->waitForFinished();
    p->deleteLater();
    m_gpgProcessStatus = GpgNotRunning;
    updateUi();
}

void ItemEncryptedLoader::onGpgProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_gpgProcess != NULL) {
        if (ui != NULL) {
            if (exitStatus != QProcess::NormalExit)
                ui->labelInfo->setText( tr("Error: %1").arg(m_gpgProcess->errorString()) );
            else if (exitCode != 0)
                ui->labelInfo->setText( tr("Error: %1").arg(QString::fromUtf8(m_gpgProcess->readAllStandardError())) );
            else
                ui->labelInfo->setText( tr("Done") );
        }

        m_gpgProcess->deleteLater();
        m_gpgProcess = NULL;
    }

    GpgProcessStatus oldStatus = m_gpgProcessStatus;
    m_gpgProcessStatus = GpgNotRunning;

    updateUi();

    if (oldStatus == GpgGeneratingKeys)
        setPassword();
}

void ItemEncryptedLoader::updateUi()
{
    if (ui == NULL)
        return;

    if (m_gpgProcessStatus == GpgGeneratingKeys) {
        ui->labelInfo->setText( tr("Creating new keys (this may take a few minutes)...") );
        ui->pushButtonPassword->setText( tr("Cancel") );
    } else if (m_gpgProcessStatus == GpgChangingPassword) {
        ui->labelInfo->setText( tr("Setting new password...") );
        ui->pushButtonPassword->setText( tr("Cancel") );
    } else if ( !keysExist() ) {
        ui->labelInfo->setText( tr("Encryption keys <strong>must be generated</strong> before item encryption can be used.") );
        ui->pushButtonPassword->setText( tr("Generate New Keys...") );
    } else {
        ui->pushButtonPassword->setText( tr("Change Password...") );
    }
}

Q_EXPORT_PLUGIN2(itemdata, ItemEncryptedLoader)
