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
#include "item/serialize.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSettings>
#include <QtPlugin>

namespace {

const QString defaultFormat = QString("application/x-copyq-encrypted");

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
{
    QHBoxLayout *layout = new QHBoxLayout(this);

    // Show small icon.
    QLabel *iconLabel = new QLabel(this);
    iconLabel->setObjectName("item_child");
    iconLabel->setTextFormat(Qt::RichText);
    iconLabel->setText("<span style=\"font-family:FontAwesome\">&#xf023;</span>");
    layout->addWidget(iconLabel);

    // Show item notes if available.
    const QString notes = index.data(contentType::notes).toString();
    if ( !notes.isEmpty() ) {
        QLabel *notesLabel = new QLabel(this);
        notesLabel = new QLabel(this);
        notesLabel->setObjectName("item_child");
        notesLabel->setTextFormat(Qt::PlainText);
        notesLabel->setWordWrap(true);
        notesLabel->setText(notes);
        layout->addWidget(notesLabel);
        layout->setStretchFactor(notesLabel, 1);
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
            const QByteArray encryptedBytes = index.data(contentType::firstFormat + i).toByteArray();
            const QByteArray bytes = readGpgOutput(QStringList("--decrypt"), encryptedBytes);
            QMimeData data;
            deserializeData(&data, bytes);
            textEdit->setPlainText(data.text());
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
            QMimeData data;
            data.setText( textEdit->toPlainText() );
            QByteArray bytes = serializeData(data);
            const QByteArray encryptedBytes = readGpgOutput( QStringList("--encrypt"), bytes );
            model->setData( index, encryptedBytes, contentType::firstFormat + i );
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

    // Check if gpg application is available.
    QProcess p;
    startGpgProcess(&p, QStringList("--version"));
    p.closeWriteChannel();
    p.waitForFinished();
    if ( p.error() != QProcess::UnknownError )
        m_gpgProcessStatus = GpgNotInstalled;

    updateUi();

    connect( ui->pushButtonPassword, SIGNAL(clicked()),
             this, SLOT(setPassword()) );

    return w;
}

void ItemEncryptedLoader::setPassword()
{
    if (m_gpgProcessStatus == GpgGeneratingKeys)
        return;

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
    } else {
        // Change password.
        m_gpgProcessStatus = GpgChangingPassword;
        m_gpgProcess = new QProcess(this);
        startGpgProcess( m_gpgProcess, QStringList() << "--edit-key" << "copyq" << "passwd" << "save");
    }

    m_gpgProcess->waitForStarted();
    if ( m_gpgProcess->state() == QProcess::NotRunning ) {
        onGpgProcessFinished( m_gpgProcess->exitCode(), m_gpgProcess->exitStatus() );
    } else {
        connect( m_gpgProcess, SIGNAL(finished(int,QProcess::ExitStatus)),
                 this, SLOT(onGpgProcessFinished(int,QProcess::ExitStatus)) );
        updateUi();
    }
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
    QString error;

    if (m_gpgProcess != NULL) {
        if (ui != NULL) {
            error = tr("Error: %1");
            if (exitStatus != QProcess::NormalExit)
                error = error.arg(m_gpgProcess->errorString());
            else if (exitCode != 0)
                error = error.arg(QString::fromUtf8(m_gpgProcess->readAllStandardError()));
            else if ( m_gpgProcess->error() != QProcess::UnknownError )
                error = error.arg(m_gpgProcess->errorString());
            else if ( !keysExist() )
                error = error.arg( tr("Failed to generate keys.") );
            else
                error.clear();
        }

        m_gpgProcess->deleteLater();
        m_gpgProcess = NULL;
    }

    GpgProcessStatus oldStatus = m_gpgProcessStatus;
    m_gpgProcessStatus = GpgNotRunning;

    if ( oldStatus == GpgGeneratingKeys && error.isEmpty() ) {
        setPassword();
    } else {
        updateUi();
        ui->labelInfo->setText( error.isEmpty() ? tr("Done") : error );
    }
}

void ItemEncryptedLoader::updateUi()
{
    if (ui == NULL)
        return;

    if (m_gpgProcessStatus == GpgNotInstalled) {
        ui->labelInfo->setText("To use item encryption, install <a href=\"http://www.gnupg.org/\">GnuPG</a> application and restart CopyQ.");
        ui->pushButtonPassword->hide();
    } else if (m_gpgProcessStatus == GpgGeneratingKeys) {
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
