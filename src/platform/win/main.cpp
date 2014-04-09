/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QThread>

namespace {

/**
 * Redirect console input to other process.
 */
class ConsoleReader : public QThread {
public:
    explicit ConsoleReader(const QStringList &arguments)
        : m_arguments(arguments)
        , m_cmd(QCoreApplication::applicationFilePath())
        , m_exitCode(0)
    {
    }

    void run()
    {
        QProcess p;
        p.setProcessChannelMode(QProcess::ForwardedChannels);

        QFile ferr;
        ferr.open(stderr, QIODevice::WriteOnly);
        QFile fout;
        fout.open(stdout, QIODevice::WriteOnly);
        QFile fin;
        fin.open(stdin, QIODevice::ReadOnly);

        // *.com -> *.exe
        if ( !m_cmd.endsWith(".com", Qt::CaseInsensitive) ) {
            ferr.write( QObject::tr("ERROR: Unexpected binary file name!\n").toUtf8() );
            m_exitCode = -1;
            return;
        }
        m_cmd.replace(m_cmd.size() - 3, 3, "exe");

        p.start(m_cmd, m_arguments);
        if ( !p.waitForStarted(-1) ) {
            ferr.write( QObject::tr("ERROR: Failed to start \"%1\"!\n").toUtf8() );
            m_exitCode = -2;
            return;
        }

        while ( !p.waitForFinished(100) ) {
            fout.write( p.readAllStandardOutput() );
            ferr.write( p.readAllStandardError() );

            if ( p.isWritable() ) {
                if ( fin.atEnd() )
                    p.closeWriteChannel();
                if ( fin.waitForReadyRead(0) )
                    p.write( fin.readAll() );
            }
        }

        fout.write( p.readAllStandardOutput() );
        ferr.write( p.readAllStandardError() );

        if ( p.error() != QProcess::Timedout && p.error() != QProcess::UnknownError ) {
            ferr.write( p.errorString().toUtf8() );
            ferr.write("\n");
            m_exitCode = -3;
            return;
        }

        m_exitCode = p.exitCode();
    }

    int exitCode() const { return m_exitCode; }

private:
    QStringList m_arguments;
    QString m_cmd;
    int m_exitCode;
};

} // namespace

int main(int argc, char *argv[])
{
    /* Execute .exe file instead of this .com file.
     * Redirect stdin from this .com process to .exe process.
     * Redirect stdout and stderr from .exe process to this .com process (e.g. current console).
     */
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments();
    args.removeFirst();

    // Pass arguments and execute the *.exe file.
    // Redirect stdin, stdou and stderr.
    ConsoleReader reader(args);
    reader.start();

    reader.wait();

    return reader.exitCode();
}
