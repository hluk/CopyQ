#include "arguments.h"
#include <QDataStream>
#include <QFile>

Arguments::Arguments()
{
    reset();
}

Arguments::Arguments(const QByteArray &msg)
{
    QByteArray bytes = qUncompress(msg);
    QDataStream in(&bytes, QIODevice::ReadOnly);
    in >> m_args;

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

QByteArray Arguments::message() const
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out << m_args;
    return qCompress(bytes);
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
