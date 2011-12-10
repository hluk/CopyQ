#include "arguments.h"
#include <QDataStream>
#include <QFile>

Arguments::Arguments()
{
    reset();
}

Arguments::Arguments(int &argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
        m_args.append( QByteArray(argv[i]) );

    // dash character means 'read from stdin'
    int len = m_args.length();
    if ( (len == 1 || len == 2) && m_args.first() == QString("-") ) {
        QByteArray mime;
        if ( len == 2 ) {
            mime = m_args.last();
        } else {
            mime = QByteArray("text/plain");
        }

        // read text from stdin
        QFile in;
        in.open(stdin, QIODevice::ReadOnly);
        m_args.clear();
        m_args << QByteArray("write") << mime << in.readAll();
    }

    reset();
}

QVariant Arguments::toVariant()
{
    QVariant default_value = m_default_value;
    m_default_value.clear();

    if (m_error)
        return QVariant();

    if ( m_current >= m_args.length() ) {
        if ( default_value.isValid() ) {
            return default_value;
        }
        m_error = true;
        return QVariant();
    }

    return m_args.at(m_current++);
}

QByteArray Arguments::toByteArray()
{
    QVariant res = toVariant();
    return res.toByteArray();
}

QString Arguments::toString()
{
    QVariant res = toVariant();
    return res.toString();
}

int Arguments::toInt()
{
    QVariant default_value = m_default_value;
    m_default_value.clear();

    if (m_error)
        return 0;

    if ( m_current >= m_args.length() ) {
        if ( default_value.isValid() ) {
            return default_value.toInt();
        }
        m_error = true;
        return 0;
    }

    QVariant res( m_args.at(m_current++) );
    bool ok;
    int n = res.toInt(&ok);
    if (ok) {
        return n;
    } else if ( default_value.isValid() ) {
        back();
        return default_value.toInt();
    } else {
        m_error = true;
        return 0;
    }
}

void Arguments::reset()
{
    m_current = 0;
    m_error = false;
}

void Arguments::back()
{
    m_error = false;
    if(m_current > 0)
        --m_current;
}

void Arguments::setDefault(const QVariant &default_value)
{
    m_default_value = default_value;
}

QDataStream &operator <<(QDataStream &stream, const Arguments &args)
{
    int len = args.length();

    stream << len;
    for( int i = 0; i<len; ++i ) {
        const QByteArray &arg = args.m_args[i];
        stream << (uint)(arg.length()) << arg.constData();
    }

    return stream;
}

QDataStream &operator>>(QDataStream &stream, Arguments &args)
{
    int len;
    uint arg_len;

    stream >> len;
    for( int i = 0; i<len; ++i ) {
        char *buffer;
        stream >> arg_len;
        stream.readBytes(buffer, arg_len);
        if (buffer) {
            QByteArray arg(buffer, arg_len);
            delete buffer;
            args.m_args.append(arg);
        }
    }

    return stream;
}

Arguments &operator >>(Arguments &args, int &dest)
{
    dest = args.toInt();
    return args;
}

Arguments &operator >>(Arguments &args, const int &dest)
{
    args.setDefault( QVariant(dest) );
    return args;
}

Arguments &operator >>(Arguments &args, QString &dest)
{
    dest = args.toString();
    return args;
}

Arguments &operator >>(Arguments &args, const QString &dest)
{
    args.setDefault( QVariant(dest) );
    return args;
}

Arguments &operator >>(Arguments &args, QByteArray &dest)
{
    dest = args.toByteArray();
    return args;
}

Arguments &operator >>(Arguments &args, const QByteArray &dest)
{
    args.setDefault( QVariant(dest) );
    return args;
}
