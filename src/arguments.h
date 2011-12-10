#ifndef ARGUMENTS_H
#define ARGUMENTS_H

#include <QList>
#include <QByteArray>
#include <QVariant>

class Arguments
{
public:
    Arguments();
    Arguments(int &argc, char **argv);

    QByteArray message() const;

    void removeFirst()
    {
        if ( m_args.isEmpty() )
            m_args.removeFirst();
    }

    void back();

    int toInt();
    QString toString();
    QByteArray toByteArray();
    QVariant toVariant();

    void setDefault(const QVariant &default_value);

    int length() const { return m_args.length(); }
    bool atEnd() const { return m_current == length(); }
    bool finished() const { return !error() && atEnd(); }

    void setError() { m_error = true; }
    bool error() const { return m_error; }
    void reset();

    friend QDataStream &operator <<(QDataStream &stream, const Arguments &args);
    friend QDataStream &operator >>(QDataStream &stream, Arguments &args);

private:
    QList<QByteArray> m_args;
    int m_current;
    bool m_error;
    QVariant m_default_value;
};

// get argument (no arguments left OR argument cannot convert to type T -> error)
// defaul value can be set when the rvalue is const
// example:
//   args >> cmd >> 0 >> row >> "text/plain" >> mime
//   i.e. read cmd, row (default value is 0) and mime (defaul value is "text/plain")
Arguments &operator >>(Arguments &args, int &dest);
Arguments &operator >>(Arguments &args, const int &dest);
Arguments &operator >>(Arguments &args, QString &dest);
Arguments &operator >>(Arguments &args, const QString &dest);
Arguments &operator >>(Arguments &args, QByteArray &dest);
Arguments &operator >>(Arguments &args, const QByteArray &dest);

#endif // MESSAGE_H
