// SPDX-License-Identifier: GPL-3.0-or-later

#include "itemeditor.h"

#include "common/action.h"
#include "common/mimetypes.h"
#include "common/log.h"
#include "common/processsignals.h"
#include "common/temporaryfile.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QTemporaryFile>
#include <QTimer>

#include <cstdio>

namespace {

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
        if ( !QFile::remove(tmpPath) )
            log( QString("Failed to remove temporary file (%1)").arg(tmpPath), LogError );
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
        log("Failed to create temporary file for external editor", LogError);
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
    COPYQ_LOG( QString("Starting editor command: %1").arg(m_editor->commandLine()) );
    m_editor->start();

    return true;
}

void ItemEditor::close()
{
    if (m_editor && (m_editor->actionFailed() || m_editor->exitCode() != 0) ) {
        const QString errorString = m_editor->errorString();
        if ( !errorString.isEmpty() )
            log( QString("Editor command error: %1").arg(errorString), LogWarning );

        const int exitCode = m_editor->exitCode();
        if (exitCode != 0)
            log( QString("Editor command exit code: %1").arg(exitCode), LogWarning );

        const QString errorOutput = QString::fromUtf8(m_editor->errorOutput());
        if ( !errorOutput.isEmpty() )
            log( QString("Editor command stderr: %1").arg(errorOutput), LogWarning );

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
            log( QString("Failed to read temporary file (%1)!").arg(m_info.fileName()),
                 LogError );
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

