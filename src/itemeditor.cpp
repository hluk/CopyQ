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

#include "itemeditor.h"
#include "client_server.h"

#include <QDir>
#include <QTextStream>
#include <QProcess>
#include <QTimer>

ItemEditor::ItemEditor(const QString &txt, const QString &editor, QObject *parent)
    : QObject(parent)
    , m_txt(txt)
    , m_hash( qHash(m_txt) )
    , m_editorcmd(editor)
    , m_editor(NULL)
    , m_timer( new QTimer(this) )
    , m_tmpfile()
    , m_info()
    , m_lastmodified()
{
}

ItemEditor::~ItemEditor()
{
    if (m_editor && m_editor->isOpen())
        m_editor->close();
}

bool ItemEditor::start()
{
    // create temp file
    QString tmpPath = QDir( QDir::tempPath() ).absoluteFilePath("CopyQ.XXXXXX");
    m_tmpfile.setFileTemplate(tmpPath);
    m_tmpfile.setAutoRemove(true);
    m_tmpfile.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
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
    m_timer->start(500);
    connect( m_timer, SIGNAL(timeout()),
             this, SLOT(onTimer()) );

    // exec editor
    m_editor = new QProcess(this);
    connect( m_editor, SIGNAL(finished(int, QProcess::ExitStatus)),
            this, SLOT(close()) );
    m_editor->start(m_editorcmd.arg(m_tmpfile.fileName()));

    return true;
}

bool ItemEditor::fileModified()
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

void ItemEditor::onTimer()
{
    if ( fileModified() ) {
        emit fileModified(m_txt);
        m_hash = qHash(m_txt);
    }
}

