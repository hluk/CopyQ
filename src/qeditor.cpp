/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

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

#include <QDir>
#include <QTextStream>
#include "qeditor.h"
#include "client_server.h"

QEditor::QEditor(const QString &txt, const QString &editor, QObject *parent) : QObject(parent)
{
    m_txt = txt;
    m_hash = qHash(m_txt);
    m_editorcmd = editor;
    m_editor = NULL;
}

QEditor::~QEditor()
{
    timer.stop();
    if (m_editor && m_editor->isOpen())
        m_editor->close();
}

bool QEditor::start()
{
    // create temp file
    QString tmpPath = QDir( QDir::tempPath() ).absoluteFilePath("CopyQ.XXXXXX");
    m_tmpfile.setFileTemplate(tmpPath);
    m_tmpfile.setAutoRemove(true);
    if ( !m_tmpfile.open() ) {
        log( tr("Failed to open temporary file (%1) for editing item in external editor!")
             .arg(m_tmpfile.fileName()), LogError );
        return false;
    }

    // write text to temp file
    QTextStream out(&m_tmpfile);
    out << m_txt;

    // monitor file
    m_info.setFile( m_tmpfile.fileName() );
    m_lastmodified = m_info.lastModified();
    timer.start(500, this);

    // exec editor
    m_editor = new QProcess(this);
    connect( m_editor, SIGNAL(finished(int, QProcess::ExitStatus)),
            this, SLOT(close()) );
    m_editor->start(m_editorcmd.arg(m_tmpfile.fileName()));

    return true;
}

bool QEditor::fileModified()
{
    m_info.refresh();
    if (m_lastmodified != m_info.lastModified() ) {
        m_lastmodified = m_info.lastModified();

        // read text
        m_txt = m_tmpfile.seek(0);
        m_txt = m_tmpfile.readAll().data();

        // new hash
        uint newhash = qHash(m_txt);

        return newhash != m_hash;
    }
    else
        return false;
}

void QEditor::timerEvent(QTimerEvent *)
{
    if ( fileModified() ) {
        emit fileModified(m_txt);
        m_hash = qHash(m_txt);
    }
}

