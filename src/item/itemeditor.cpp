// SPDX-License-Identifier: GPL-3.0-or-later

#include "itemeditor.h"

#include "common/action.h"
#include "common/mimetypes.h"
#include "common/temporaryfile.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QLoggingCategory>
#include <QTemporaryFile>
#include <QTimer>

namespace {

Q_DECLARE_LOGGING_CATEGORY(editorCategory)
Q_LOGGING_CATEGORY(editorCategory, "copyq.itemeditor")

QString getFileSuffixFromMime(const QString &mime)
{
    if (mime == mimeText)
        return QLatin1String(".txt");
    if (mime == "text/html")
        return QLatin1String(".html");
    if (mime == "text/xml")
        return QLatin1String(".xml");
    if (mime == "image/bmp")
        return QLatin1String(".bmp");
    if (mime == "image/jpeg")
        return QLatin1String(".jpg");
    if (mime == "image/png")
        return QLatin1String(".png");
    if (mime == "image/gif")
        return QLatin1String(".gif");
    if (mime == "image/svg+xml" || mime == "image/x-inkscape-svg-compressed")
        return QLatin1String(".svg");
    if (mime == COPYQ_MIME_PREFIX "theme")
        return QLatin1String(".ini");
    return QString();
}

} // namespace

ItemEditor::ItemEditor(const QByteArray &data, const QString &mime, const QString &editor,
                       QObject *parent)
    : QObject(parent)
    , m_data(data)
    , m_mime(mime)
    , m_hash( qHash(m_data) )
    , m_editorcmd(editor)
    , m_editor(nullptr)
    , m_timer( new QTimer(this) )
    , m_info()
    , m_lastmodified()
    , m_lastSize(0)
    , m_modified(false)
{
    if ( !m_editorcmd.contains("%1") )
        m_editorcmd.append(" %1");
}

ItemEditor::~ItemEditor()
{
    if (m_editor && m_editor->isRunning())
        m_editor->terminate();

    QString tmpPath = m_info.filePath();
    if ( !tmpPath.isEmpty() ) {
        QFile f(tmpPath);
        if ( !f.remove() )
            qCCritical(editorCategory) << "Failed to remove editor temporary file:" << f.errorString();
    }
}

void ItemEditor::setIndex(const QModelIndex &index)
{
    m_index = index;
}

bool ItemEditor::start()
{
    // create temp file
    QTemporaryFile tmpfile;
    const auto suffix = getFileSuffixFromMime(m_mime);
    if ( !openTemporaryFile(&tmpfile, suffix) ) {
        qCCritical(editorCategory) << "Failed to create editor temporary file";
        return false;
    }

    const auto fileName = tmpfile.fileName();

    // write text to temp file
    tmpfile.write(m_data);

    // Close file before launching editor (this is required on Windows).
    tmpfile.setAutoRemove(false);
    tmpfile.close();

    // monitor file
    m_info.setFile(fileName);
    m_lastmodified = m_info.lastModified();
    m_lastSize = m_info.size();
    m_timer->start(500);
    connect( m_timer, &QTimer::timeout,
             this, &ItemEditor::onTimer );

    // create editor process
    m_editor = new Action(this);
    connect(m_editor, &Action::actionFinished, this, &ItemEditor::close);

    // use native path for filename to edit
    const auto nativeFilePath = QDir::toNativeSeparators( m_info.absoluteFilePath() );

    // execute editor
    m_editor->setCommand(m_editorcmd, {nativeFilePath});
    qCDebug(editorCategory) << "Starting editor command:" << m_editor->commandLine();
    m_editor->start();

    return true;
}

void ItemEditor::close()
{
    if (m_editor && (m_editor->actionFailed() || m_editor->exitCode() != 0) ) {
        const QString errorString = m_editor->errorString();
        if ( !errorString.isEmpty() )
            qCWarning(editorCategory) << "Editor command error:" << errorString;

        const int exitCode = m_editor->exitCode();
        if (exitCode != 0)
            qCWarning(editorCategory) << "Editor command exit code:" << exitCode;

        const QString errorOutput = QString::fromUtf8(m_editor->errorOutput());
        if ( !errorOutput.isEmpty() )
            qCWarning(editorCategory) << "Editor command stderr:" << errorOutput;

        if ( m_editor->actionFailed() )
            emit error( tr("Editor command failed (see logs)") );
    }

    if ( m_modified || wasFileModified() )
        emit fileModified(m_data, m_mime, m_index);

    emit closed(this, m_index);
}

bool ItemEditor::wasFileModified()
{
    m_info.refresh();
    if ( m_lastmodified != m_info.lastModified() ||  m_lastSize != m_info.size() ) {
        m_lastmodified = m_info.lastModified();
        m_lastSize = m_info.size();

        // read text
        QFile file( m_info.filePath() );
        if ( file.open(QIODevice::ReadOnly) ) {
            m_data = file.readAll();
            file.close();
        } else {
            qCCritical(editorCategory) << "Failed to read editor temporary file:" << file.errorString();
        }

        // new hash
        uint newhash = qHash(m_data);

        return newhash != m_hash;
    }

    return false;
}

void ItemEditor::onTimer()
{
    if (m_modified) {
        // Wait until file is fully overwritten.
        if ( !wasFileModified() ) {
            m_modified = false;
            emit fileModified(m_data, m_mime, m_index);
            m_hash = qHash(m_data);
        }
    } else {
        m_modified = wasFileModified();
    }
}
