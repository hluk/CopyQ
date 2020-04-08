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

#include "itemeditor.h"

#include "common/mimetypes.h"
#include "common/log.h"
#include "common/processsignals.h"
#include "common/temporaryfile.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QProcess>
#include <QTemporaryFile>
#include <QTimer>

#include <cstdio>

namespace {

QString getFileSuffixFromMime(const QString &mime)
{
    if (mime == mimeText)
        return QString(".txt");
    if (mime == "text/html")
        return QString(".html");
    if (mime == "text/xml")
        return QString(".xml");
    if (mime == "image/bmp")
        return QString(".bmp");
    if (mime == "image/jpeg")
        return QString(".jpg");
    if (mime == "image/png")
        return QString(".png");
    if (mime == "image/gif")
        return QString(".gif");
    if (mime == "image/svg+xml" || mime == "image/x-inkscape-svg-compressed")
        return QString(".svg");
    if (mime == COPYQ_MIME_PREFIX "theme")
        return QString(".ini");
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
    if (m_editor && m_editor->isOpen())
        m_editor->close();

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
    m_editor = new QProcess(this);
    connectProcessFinished(m_editor, this, &ItemEditor::close);
    connectProcessError(m_editor, this, &ItemEditor::onError);

    // use native path for filename to edit
    const auto nativeFilePath = QDir::toNativeSeparators( m_info.absoluteFilePath() );
    const auto cmd = m_editorcmd.arg('"' + nativeFilePath + '"');

    // execute editor
    m_editor->start(cmd, QIODevice::ReadOnly);
    m_editor->closeWriteChannel();
    m_editor->closeReadChannel(QProcess::StandardOutput);

    return m_editor->waitForStarted(10000);
}

void ItemEditor::close()
{
    // check if file was modified before closing
    if ( m_modified || wasFileModified() )
        emit fileModified(m_data, m_mime, m_index);

    if (m_editor && m_editor->exitCode() != 0 ) {
        emitError( tr("editor exit code is %1").arg(m_editor->exitCode()) );
        const QByteArray errors = m_editor->readAllStandardError();
        if ( !errors.isEmpty() )
            emitError( QString::fromUtf8(errors) );
    }

    emit closed(this, m_index);
}

void ItemEditor::onError()
{
    emitError( m_editor->errorString() );
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

void ItemEditor::emitError(const QString &errorString)
{
    emit error( tr("Editor command: %1").arg(errorString) );
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

