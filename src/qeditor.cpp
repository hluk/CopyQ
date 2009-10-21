/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

    This file is part of Copyq.

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

#include <QDebug>
#include <QDir>
#include <QTextStream>
#include "qeditor.h"

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
    timerEvent(NULL);

    if (m_editor && m_editor->isOpen())
        m_editor->close();
}

bool QEditor::start()
{
    // create temp file
    QString tmpPath = QDir( QDir::tempPath() ).absolutePath();
    m_tmpfile.setFileTemplate(tmpPath + "/copyq.XXXXXX");
    m_tmpfile.setAutoRemove(true);
    if ( !m_tmpfile.open() ) {
        qDebug() << "ERROR: Temporary file" << m_tmpfile.fileName() << "open failed!";
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

void QEditor::timerEvent(QTimerEvent *)
{
    m_info.refresh();
    if (m_lastmodified != m_info.lastModified() ) {
        m_lastmodified = m_info.lastModified();

        // read text
        m_txt = m_tmpfile.seek(0);
        m_txt = m_tmpfile.readAll().data();

        // new hash
        uint newhash = qHash(m_txt);

        if( newhash != m_hash ) {
            emit fileModified(m_hash, m_txt);
            m_hash = newhash;
        }
    }
}

