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
    explicit ConsoleReader(QProcess *process)
        : m_process(process)
    {
    }

    void run()
    {
        QFile fin;
        fin.open(stdin, QIODevice::ReadOnly);
        m_process->write( fin.readAll() );
        m_process->closeWriteChannel();
    }

private:
    QProcess *m_process;
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
    QString cmd = QCoreApplication::applicationFilePath();

    QFile ferr;
    ferr.open(stderr, QIODevice::WriteOnly);

    // *.com -> *.exe
    if ( !cmd.endsWith(".com", Qt::CaseInsensitive) ) {
        ferr.write( QObject::tr("ERROR: Unexpected binary file name!\n").toLocal8Bit() );
        return 1;
    }
    cmd.replace(cmd.size() - 3, 3, "exe");

    // Pass arguments and execute the *.exe file.
    QProcess p;
    args.removeFirst();
    p.setProcessChannelMode(QProcess::ForwardedChannels);
    p.start(cmd, args);
    if ( !p.waitForStarted(-1) ) {
        ferr.write( QObject::tr("ERROR: Failed to start \"%1\"!\n")
                    .arg(cmd).toLocal8Bit() );
        return 1;
    }

    // Redirect stdin.
    ConsoleReader reader(&p);
    reader.start();

    // Read stderr and stdout.
    QFile fout;
    fout.open(stdout, QIODevice::WriteOnly);

    p.waitForFinished(-1);
    if ( p.error() != QProcess::UnknownError ) {
        ferr.write( p.errorString().toLocal8Bit() );
        ferr.write("\n");
    }

    reader.terminate();

    return p.exitCode();
}
