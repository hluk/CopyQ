/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include "itemloaderscript.h"

#include "common/commandstatus.h"
#include "common/common.h"
#include "common/contenttype.h"
#include "common/log.h"
#include "gui/icons.h"
#include "gui/iconfactory.h"
#include "scriptable/scriptable.h"
#include "scriptable/scriptableproxy.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QRegExp>
#include <QScriptEngine>
#include <QScriptValueIterator>
#include <QHBoxLayout>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace {

const int scriptTimeoutMs = 1000;
const char scriptFunctionName[] = "CopyQScript";

QWidget *label(Qt::Orientation orientation, const QString &name, QWidget *w)
{
    QWidget *parent = w->parentWidget();

    QBoxLayout *layout;
    if (orientation == Qt::Horizontal)
        layout = new QHBoxLayout;
    else
        layout = new QVBoxLayout;

    parent->layout()->addItem(layout);

    auto label = new QLabel(name + ":", parent);
    label->setBuddy(w);
    layout->addWidget(label);
    layout->addWidget(w, 1);

    return w;
}

QWidget *settingsWidgetForValue(
        const QVariant &value, const QString &name, QWidget *parent,
        const QScriptValue &defaultValue)
{
    if ( defaultValue.isBool() ) {
        auto checkBox = new QCheckBox(parent);
        checkBox->setChecked( value.toBool() );
        checkBox->setText(name);
        parent->layout()->addWidget(checkBox);
        return checkBox;
    }

    if ( defaultValue.isString() ) {
        auto lineEdit = new QLineEdit(parent);
        lineEdit->setText(value.toString());
        return label(Qt::Horizontal, name, lineEdit);
    }

    if ( defaultValue.isArray() ) {
        auto items = defaultValue.toVariant().toStringList();
        auto w = new QComboBox(parent);
        w->addItems(items);
        const auto index = w->findText(value.toString());
        if (index != -1)
            w->setCurrentIndex(index);
        return label(Qt::Horizontal, name, w);
    }

    return nullptr;
}

QVariant valueForSettingsWidget(QWidget *w)
{
    if ( auto checkBox = qobject_cast<QCheckBox*>(w) )
        return checkBox->isChecked();

    if ( auto lineEdit = qobject_cast<QLineEdit*>(w) )
        return lineEdit->text();

    if ( auto comboBox = qobject_cast<QComboBox*>(w) )
        return comboBox->currentText();

    Q_ASSERT(false);
    return QVariant();
}

QString variableNameToLabel(const QString &name)
{
    QString label;
    label.reserve(name.size());

    for (const auto c : name) {
        if ( label.isEmpty() )
            label += c.toUpper();
        else if (c.isUpper())
            label += ' ' + c.toUpper();
        else if (c == '_')
            label += ' ';
        else
            label += c;
    }

    return label;
}

bool processUncaughtException(const Scriptable &scriptable, const QString &id, const QString &name)
{
    const auto engine = scriptable.engine();

    if ( !engine->hasUncaughtException() )
        return false;

    const auto exceptionText = engine->uncaughtException().toString().trimmed();
    const auto message = QString("Script exception at %1::%2: %3").arg(id, name, exceptionText);
    log(message, LogWarning);

    engine->clearExceptions();

    return true;
}

class ItemScriptableScript : public ItemScriptable
{
    Q_OBJECT
public:
    explicit ItemScriptableScript(const QString &script)
        : m_script(script)
    {
    }

    void start() override
    {
        eval(m_script);
    }

private:
    QString m_script;
};

class ItemScriptableScriptFactory : public ItemScriptableFactoryInterface
{
public:
    explicit ItemScriptableScriptFactory(const QString &script)
        : m_script(script)
    {
    }

    ItemScriptable *create() const override
    {
        return new ItemScriptableScript(m_script);
    }

private:
    QString m_script;
};

class ItemSaverScript : public ItemSaverInterface
{
public:
    ItemSaverScript(
            const QString &id,
            const ItemSaverPtr &saver,
            const QScriptValue &obj,
            Scriptable *scriptable)
        : m_id(id)
        , m_saver(saver)
        , m_obj(obj)
        , m_scriptable(scriptable)
    {
    }

    bool saveItems(const QString &tabName, const QAbstractItemModel &model, QIODevice *file) override
    {
        return m_saver->saveItems(tabName, model, file);
    }

    bool canRemoveItems(const QList<QModelIndex> &indexList, QString *error) override
    {
        return m_saver->canRemoveItems(indexList, error);
    }

    bool canMoveItems(const QList<QModelIndex> &indexList) override
    {
        return m_saver->canMoveItems(indexList);
    }

    void itemsRemovedByUser(const QList<QModelIndex> &indexList) override
    {
        m_saver->itemsRemovedByUser(indexList);
    }

    QVariantMap copyItem(const QAbstractItemModel &model, const QVariantMap &itemData) override
    {
        const auto itemData2 = m_saver->copyItem(model, itemData);
        return transformData("copyItem", itemData2);
    }

    QVariantMap displayItem(const QAbstractItemModel &model, const QVariantMap &itemData) override
    {
        const auto itemData2 = m_saver->displayItem(model, itemData);
        return transformData("displayItem", itemData2);
    }

private:
    QVariantMap transformData(const QString &fnName, const QVariantMap &itemData)
    {
        auto fn = m_obj.property(fnName);
        if ( !fn.isFunction() )
            return itemData;

        QTimer timer;
        initSingleShotTimer( &timer, scriptTimeoutMs, m_scriptable, SLOT(abort()) );
        timer.start();

        const auto args = QScriptValueList() << m_scriptable->fromDataMap(itemData);
        const auto result = fn.call(m_obj, args);

        const auto takesTooLong = !timer.isActive();
        timer.stop();

        if (takesTooLong) {
            const auto message = QString("Script %1: %2 timed out").arg(m_id, fnName);
            log(message, LogWarning);
            m_obj = QScriptValue();
            return itemData;
        }

        if ( processUncaughtException(*m_scriptable, m_id, fnName) )
            return itemData;

        if ( result.isUndefined() || result.isNull() )
            return itemData;

        return m_scriptable->toDataMap(result);
    }

    QString m_id;
    ItemSaverPtr m_saver;
    QScriptValue m_obj;
    Scriptable *m_scriptable;
};

class ItemLoaderScript : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_INTERFACES(ItemLoaderInterface)

public:
    ItemLoaderScript(const Command &command, ScriptableProxy *proxy)
        : m_engine()
        , m_scriptable(&m_engine, proxy)
        , m_name(command.name)
        , m_id( m_name.toLower().replace(QRegExp("[^a-zA-Z0-9_]"), "_") )
        , m_script(command.cmd)
        , m_icon(command.icon)
    {
        QObject::connect( &m_scriptable, SIGNAL(sendMessage(QByteArray,int)),
                          this, SLOT(sendMessage(QByteArray,int)) );
    }

    int priority() const override
    {
        const auto priorityValue = value("priority");
        bool ok;
        const auto priority = priorityValue.toVariant().toInt(&ok);
        return ok ? priority : 20;
    }

    QString id() const override
    {
        return m_id;
    }

    QString name() const override
    {
        return stringValue("name", m_name);
    }

    QString author() const override
    {
        return stringValue("author");
    }

    QString description() const override
    {
        return stringValue("description");
    }

    QVariant icon() const override
    {
        if ( m_icon.isEmpty() )
            return IconCog;

        const auto iconId = toIconId(m_icon);
        if (iconId != 0)
            return iconId;

        return m_icon;
    }

    QStringList formatsToSave() const override
    {
        return value("formatsToSave").toVariant().toStringList();
    }

    QVariantMap applySettings() override
    {
        for (auto it = m_settingsWidgets.constBegin(); it != m_settingsWidgets.constEnd(); ++it) {
            const auto name = it.key();
            const auto widget = it.value();
            m_settings[name] = valueForSettingsWidget(widget);
        }

        return m_settings;
    }

    void loadSettings(const QVariantMap &settings) override
    {
        m_settings = settings;

        auto fn = m_obj.property("loadSettings");
        if ( fn.isFunction() ) {
            const auto args = QScriptValueList() << m_scriptable.fromDataMap(m_settings);
            fn.call(m_obj, args);
            if ( processUncaughtException(m_scriptable, m_id, "loadSettings") )
                m_obj = QScriptValue();
        }
    }

    QWidget *createSettingsWidget(QWidget *parent) override
    {
        m_settingsWidgets.clear();

        auto settingsObject = value("defaultSettings");

        auto settingsWidget = new QWidget(parent);
        auto layout = new QVBoxLayout(settingsWidget);

        QScriptValueIterator it(settingsObject);
        while (it.hasNext()) {
            it.next();
            if ( it.flags() & QScriptValue::SkipInEnumeration )
                continue;

            const auto id = it.name();
            const auto defaultValue = it.value();
            const auto value = m_settings.value(id, defaultValue.toVariant());
            const auto label = variableNameToLabel(id);

            auto ww = settingsWidgetForValue(value, label, settingsWidget, defaultValue);
            if (ww) {
                ww->setObjectName(id);
                m_settingsWidgets.insert(id, ww);
            }
        }

        layout->addStretch(1);

        return settingsWidget;
    }

    ItemSaverPtr transformSaver(const ItemSaverPtr &saver, QAbstractItemModel *) override
    {
        if ( m_obj.property("copyItem").isFunction()
             || m_obj.property("displayItem").isFunction() )
        {
            return std::make_shared<ItemSaverScript>(m_id, saver, m_obj, &m_scriptable);
        }

        return saver;
    }

    ItemScriptableFactoryPtr scriptableFactory() override
    {
        return std::make_shared<ItemScriptableScriptFactory>(m_script);
    }

    bool init()
    {
        if ( m_script.isEmpty() )
            return false;

        m_scriptable.eval(m_script, m_name);
        m_obj = m_engine.globalObject().property(scriptFunctionName);
        if (m_obj.isFunction())
            m_obj = m_obj.call();

        return !processUncaughtException(m_scriptable, m_id, scriptFunctionName);
    }

private slots:
    void sendMessage(const QByteArray &message, int messageCode)
    {
        if ( !message.isEmpty() ) {
            const auto text = QString::fromUtf8(message);
            switch (messageCode) {
            case CommandError:
            case CommandBadSyntax:
            case CommandException:
                log(text, LogWarning);
                break;
            default:
                log(text);
                break;
            }
        }
    }

private:
    QString stringValue(const QString &variableName, const QString &defaultValue = QString()) const
    {
        const auto value = this->value(variableName);

        if ( value.isValid() ) {
            const auto text = value.toString();
            if ( !text.isNull() )
                return text;
        }

        return defaultValue;
    }

    QScriptValue value(const QString &variableName) const
    {
        auto value = m_obj.property(variableName);
        if ( value.isFunction() )
            value = value.call(m_obj, QScriptValueList());

        if ( processUncaughtException(m_scriptable, m_id, variableName) )
            return QScriptValue();

        return value;
    }

    void log(QString text, LogLevel level = LogNote) const
    {
        const auto label = "scripts::" + m_id + ": ";
        text.prepend(label);
        text.replace("\n", label);
        ::log(text, level);
    }

    QScriptEngine m_engine;
    Scriptable m_scriptable;
    QScriptValue m_obj;
    QString m_name;
    QString m_id;
    QString m_script;
    QString m_icon;
    QVariantMap m_settings;
    QMap<QString, QWidget*> m_settingsWidgets;
};

} // namespace

ItemLoaderPtr createItemLoaderScript(const Command &command, ScriptableProxy *proxy)
{
    auto loader = std::make_shared<ItemLoaderScript>(command, proxy);
    if ( loader->init() )
        return loader;
    return nullptr;
}

#include "itemloaderscript.moc"
