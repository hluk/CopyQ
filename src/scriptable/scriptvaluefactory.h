// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/command.h"
#include "scriptable/scriptablebytearray.h"
#include "scriptable/scriptablefile.h"

#include <QFileInfo>
#include <QJSEngine>
#include <QJSValueIterator>
#include <QRegularExpression>
#include <QUrl>

static QFile *getFile(const QJSValue &value, QJSEngine *engine)
{
    auto obj = engine->fromScriptValue<ScriptableFile*>(value);
    return obj ? obj->self() : nullptr;
}

static QJSValue newByteArray(ScriptableByteArray *ba, QJSEngine *engine)
{
    const QJSValue globalObject = engine->globalObject();
    const QJSValue byteArrayPrototype = globalObject.property("ByteArray").property(QStringLiteral("prototype"));
    auto value = engine->newQObject(ba);
    value.setPrototype(byteArrayPrototype);
    return value;
}

static QVariant toVariant(const QJSValue &value)
{
    const auto variant = value.toVariant();
    Q_ASSERT(value.isUndefined() || value.isNull() || variant.isValid());
    return variant;
}

template <typename T>
struct ScriptValueFactory {
    static QJSValue toScriptValue(const T &value, QJSEngine *)
    {
        return QJSValue(value);
    }

    static T fromScriptValue(const QJSValue &value, QJSEngine *)
    {
        const auto variant = toVariant(value);
        Q_ASSERT( variant.canConvert<T>() );
        return variant.value<T>();
    }
};

template <typename T>
QJSValue toScriptValue(const T &value, QJSEngine *engine)
{
    return ScriptValueFactory<T>::toScriptValue(value, engine);
}

template <typename T>
T fromScriptValue(const QJSValue &value, QJSEngine *engine)
{
    return ScriptValueFactory<T>::fromScriptValue(value, engine);
}

template <typename T>
void fromScriptValueIfValid(const QJSValue &value, QJSEngine *engine, T *outputValue)
{
    if (!value.isUndefined())
        *outputValue = ScriptValueFactory<T>::fromScriptValue(value, engine);
}

template <typename List, typename T>
struct ScriptValueListFactory {
    static QJSValue toScriptValue(const List &list, QJSEngine *engine)
    {
        QJSValue array = engine->newArray();
        for ( int i = 0; i < list.size(); ++i ) {
            const auto value = ScriptValueFactory<T>::toScriptValue(list[i], engine);
            array.setProperty( static_cast<quint32>(i), value );
        }
        return array;
    }

    static List fromScriptValue(const QJSValue &value, QJSEngine *engine)
    {
        if ( !value.isArray() )
            return List();

        const quint32 length = value.property("length").toUInt();
        List list;
        for ( quint32 i = 0; i < length; ++i ) {
            const auto item = value.property(i);
            list.append( ScriptValueFactory<T>::fromScriptValue(item, engine) );
        }
        return list;
    }
};

template <typename T>
struct ScriptValueFactory< QList<T> > : ScriptValueListFactory< QList<T>, T > {};

// QVector is alias for a QList in Qt 6.
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
template <typename T>
struct ScriptValueFactory< QVector<T> > : ScriptValueListFactory< QVector<T>, T > {};
#endif

template <>
struct ScriptValueFactory<QVariantMap> {
    static QJSValue toScriptValue(const QVariantMap &dataMap, QJSEngine *engine)
    {
        QJSValue value = engine->newObject();

        for (auto it = dataMap.constBegin(); it != dataMap.constEnd(); ++it)
            value.setProperty( it.key(), ::toScriptValue(it.value(), engine) );

        return value;
    }

    static QVariantMap fromScriptValue(const QJSValue &value, QJSEngine *engine)
    {
        QVariantMap result;
        QJSValueIterator it(value);
        while ( it.hasNext() ) {
            it.next();
            auto itemValue = ::fromScriptValue<QVariant>( it.value(), engine );
            if (itemValue.type() == QVariant::String)
                itemValue = itemValue.toString().toUtf8();
            result.insert(it.name(), itemValue);
        }
        return result;
    }
};

template <>
struct ScriptValueFactory<QByteArray> {
    static QJSValue toScriptValue(const QByteArray &bytes, QJSEngine *engine)
    {
        return newByteArray(new ScriptableByteArray(bytes), engine);
    }
};

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
template <>
struct ScriptValueFactory<QStringList> {
    static QJSValue toScriptValue(const QStringList &list, QJSEngine *engine)
    {
        return ScriptValueFactory< QList<QString> >::toScriptValue(list, engine);
    }

    static QStringList fromScriptValue(const QJSValue &value, QJSEngine *engine)
    {
        return ScriptValueFactory< QList<QString> >::fromScriptValue(value, engine);
    }
};
#endif

template <>
struct ScriptValueFactory<QString> {
    static QJSValue toScriptValue(const QString &text, QJSEngine *)
    {
        return QJSValue(text);
    }

    static QString fromScriptValue(const QJSValue &value, QJSEngine *)
    {
        return toString(value);
    }
};

template <>
struct ScriptValueFactory<QRegularExpression> {
    static QJSValue toScriptValue(const QRegularExpression &re, QJSEngine *engine)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5,13,0)
        return engine->toScriptValue(re);
#else
        const auto caseSensitivity =
            re.patternOptions().testFlag(QRegularExpression::CaseInsensitiveOption)
            ? Qt::CaseInsensitive
            : Qt::CaseSensitive;
        const QRegExp re2(re.pattern(), caseSensitivity);
        return engine->toScriptValue(re2);
#endif
    }

    static QRegularExpression fromScriptValue(const QJSValue &value, QJSEngine *)
    {
        if (value.isRegExp()) {
            const auto variant = value.toVariant();
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
            return variant.toRegularExpression();
#else
            // Support for Qt 5.12.z and below.
            if ( variant.canConvert<QRegularExpression>() )
                return variant.toRegularExpression();

            const QRegExp reOld = variant.toRegExp();
            const auto caseSensitivity =
                reOld.caseSensitivity() == Qt::CaseInsensitive
                ? QRegularExpression::CaseInsensitiveOption
                : QRegularExpression::NoPatternOption;
            return QRegularExpression(reOld.pattern(), caseSensitivity);
#endif
        }

        if (value.isVariant()) {
            const auto variant = value.toVariant();
            return variant.toRegularExpression();
        }

        return QRegularExpression( toString(value) );
    }
};

template <>
struct ScriptValueFactory<Command> {
    static QJSValue toScriptValue(const Command &command, QJSEngine *engine)
    {
        QJSValue value = engine->newObject();

        value.setProperty(QStringLiteral("name"), command.name);
        value.setProperty(QStringLiteral("re"), ::toScriptValue(command.re, engine));
        value.setProperty(QStringLiteral("wndre"), ::toScriptValue(command.wndre, engine));
        value.setProperty(QStringLiteral("matchCmd"), command.matchCmd);
        value.setProperty(QStringLiteral("cmd"), command.cmd);
        value.setProperty(QStringLiteral("sep"), command.sep);
        value.setProperty(QStringLiteral("input"), command.input);
        value.setProperty(QStringLiteral("output"), command.output);
        value.setProperty(QStringLiteral("wait"), command.wait);
        value.setProperty(QStringLiteral("automatic"), command.automatic);
        value.setProperty(QStringLiteral("display"), command.display);
        value.setProperty(QStringLiteral("inMenu"), command.inMenu);
        value.setProperty(QStringLiteral("isGlobalShortcut"), command.isGlobalShortcut);
        value.setProperty(QStringLiteral("isScript"), command.isScript);
        value.setProperty(QStringLiteral("transform"), command.transform);
        value.setProperty(QStringLiteral("remove"), command.remove);
        value.setProperty(QStringLiteral("hideWindow"), command.hideWindow);
        value.setProperty(QStringLiteral("enable"), command.enable);
        value.setProperty(QStringLiteral("icon"), command.icon);
        value.setProperty(QStringLiteral("shortcuts"), ::toScriptValue(command.shortcuts, engine));
        value.setProperty(QStringLiteral("globalShortcuts"), ::toScriptValue(command.globalShortcuts, engine));
        value.setProperty(QStringLiteral("tab"), command.tab);
        value.setProperty(QStringLiteral("outputTab"), command.outputTab);
        value.setProperty(QStringLiteral("internalId"), command.internalId);

        return value;
    }

    static Command fromScriptValue(const QJSValue &value, QJSEngine *engine)
    {
        Command command;

        ::fromScriptValueIfValid( value.property("name"), engine, &command.name );
        ::fromScriptValueIfValid( value.property("re"), engine, &command.re );
        ::fromScriptValueIfValid( value.property("wndre"), engine, &command.wndre );
        ::fromScriptValueIfValid( value.property("matchCmd"), engine, &command.matchCmd );
        ::fromScriptValueIfValid( value.property("cmd"), engine, &command.cmd );
        ::fromScriptValueIfValid( value.property("sep"), engine, &command.sep );
        ::fromScriptValueIfValid( value.property("input"), engine, &command.input );
        ::fromScriptValueIfValid( value.property("output"), engine, &command.output );
        ::fromScriptValueIfValid( value.property("wait"), engine, &command.wait );
        ::fromScriptValueIfValid( value.property("automatic"), engine, &command.automatic );
        ::fromScriptValueIfValid( value.property("display"), engine, &command.display );
        ::fromScriptValueIfValid( value.property("inMenu"), engine, &command.inMenu );
        ::fromScriptValueIfValid( value.property("isGlobalShortcut"), engine, &command.isGlobalShortcut );
        ::fromScriptValueIfValid( value.property("isScript"), engine, &command.isScript );
        ::fromScriptValueIfValid( value.property("transform"), engine, &command.transform );
        ::fromScriptValueIfValid( value.property("remove"), engine, &command.remove );
        ::fromScriptValueIfValid( value.property("hideWindow"), engine, &command.hideWindow );
        ::fromScriptValueIfValid( value.property("enable"), engine, &command.enable );
        ::fromScriptValueIfValid( value.property("icon"), engine, &command.icon );
        ::fromScriptValueIfValid( value.property("shortcuts"), engine, &command.shortcuts );
        ::fromScriptValueIfValid( value.property("globalShortcuts"), engine, &command.globalShortcuts );
        ::fromScriptValueIfValid( value.property("tab"), engine, &command.tab );
        ::fromScriptValueIfValid( value.property("outputTab"), engine, &command.outputTab );
        ::fromScriptValueIfValid( value.property("internalId"), engine, &command.internalId );

        return command;
    }
};

template <>
struct ScriptValueFactory<QVariant> {
    static QJSValue toScriptValue(const QVariant &variant, QJSEngine *engine)
    {
        if ( !variant.isValid() )
            return QJSValue(QJSValue::UndefinedValue);

        if (variant.type() == QVariant::Bool)
            return ::toScriptValue(variant.toBool(), engine);

        if (variant.type() == QVariant::ByteArray)
            return ::toScriptValue(variant.toByteArray(), engine);

        if (variant.type() == QVariant::String)
            return ::toScriptValue(variant.toString(), engine);

        if (variant.type() == QVariant::Char)
            return ::toScriptValue(variant.toString(), engine);

        if (variant.type() == QVariant::RegularExpression)
            return ::toScriptValue(variant.toRegularExpression(), engine);

        if (variant.canConvert<QVariantList>())
            return ::toScriptValue(variant.value<QVariantList>(), engine);

        if (variant.canConvert<QVariantMap>())
            return ::toScriptValue(variant.value<QVariantMap>(), engine);

        if (variant.canConvert<QByteArray>())
            return newByteArray(new ScriptableByteArray(variant), engine);

        return engine->toScriptValue(variant);
    }

    static QVariant fromScriptValue(const QJSValue &value, QJSEngine *engine)
    {
        const auto bytes = getByteArray(value);
        if (bytes)
            return QVariant(*bytes);

        auto file = getFile(value, engine);
        if (file) {
            const QFileInfo fileInfo(*file);
            const auto path = fileInfo.absoluteFilePath();
            return QVariant::fromValue( QUrl::fromLocalFile(path) );
        }

        if (value.isArray())
            return ScriptValueFactory<QVariantList>::fromScriptValue(value, engine);

        const auto variant = toVariant(value);
        if (variant.type() == QVariant::ByteArray)
            return variant;

        if (value.isObject())
            return ScriptValueFactory<QVariantMap>::fromScriptValue(value, engine);

        Q_ASSERT(value.isUndefined() || value.isNull() || variant.isValid());
        return variant;
    }
};

