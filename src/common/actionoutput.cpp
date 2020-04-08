/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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

#include "actionoutput.h"

#include "common/action.h"
#include "common/contenttype.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "gui/clipboardbrowser.h"
#include "gui/mainwindow.h"
#include "item/serialize.h"

#include <QObject>
#include <QPersistentModelIndex>

namespace {

template <typename ActionOutput>
void connectActionOutput(Action *action, ActionOutput *actionOutput)
{
    QObject::connect( action, &Action::actionOutput,
                      actionOutput, &ActionOutput::onActionOutput );
    QObject::connect( action, &Action::actionFinished,
                      actionOutput, &ActionOutput::onActionFinished );

    action->setReadOutput(true);
}

class ActionOutputItems final : public QObject
{
public:
    ActionOutputItems(
            MainWindow *wnd,
            Action *action,
            const QString &outputItemFormat,
            const QString &outputTabName,
            const QRegularExpression &itemSeparator)
        : QObject(action)
        , m_wnd(wnd)
        , m_outputFormat(outputItemFormat)
        , m_tab(outputTabName)
        , m_sep(itemSeparator)
    {
        connectActionOutput(action, this);
    }

    void onActionOutput(const QByteArray &output)
    {
        m_lastOutput.append( getTextData(output) );
        auto items = m_lastOutput.split(m_sep);
        m_lastOutput = items.takeLast();
        if ( !items.isEmpty() )
            addItems(items);
    }

    void onActionFinished(Action *)
    {
        if ( !m_lastOutput.isEmpty() )
            addItems(QStringList() << m_lastOutput);
    }

private:
    void addItems(const QStringList &items)
    {
        ClipboardBrowser *c = m_tab.isEmpty() ? m_wnd->browser() : m_wnd->tab(m_tab);
        for (const auto &item : items)
            c->add( createDataMap(m_outputFormat, item) );
    }

    MainWindow *m_wnd;
    QString m_outputFormat;
    QString m_tab;
    QRegularExpression m_sep;
    QString m_lastOutput;
};

class ActionOutputItem final : public QObject
{
public:
    ActionOutputItem(
            MainWindow *wnd,
            Action *action,
            const QString &outputItemFormat,
            const QString &outputTabName)
        : QObject(action)
        , m_wnd(wnd)
        , m_outputFormat(outputItemFormat)
        , m_tab(outputTabName)
    {
        connectActionOutput(action, this);
    }

    void setOutputFormat(const QString &outputItemFormat) { m_outputFormat = outputItemFormat; }
    void setOutputTab(const QString &outputTabName) { m_tab = outputTabName; }

    void onActionOutput(const QByteArray &output)
    {
        m_output.append(output);
    }

    void onActionFinished(Action *)
    {
        if ( m_output.isEmpty() )
            return;

        ClipboardBrowser *c = m_tab.isEmpty() ? m_wnd->browser() : m_wnd->tab(m_tab);
        c->add( createDataMap(m_outputFormat, m_output) );
    }

private:
    MainWindow *m_wnd;
    QString m_outputFormat;
    QString m_tab;
    QByteArray m_output;
};

class ActionOutputIndex final : public QObject
{
public:
    ActionOutputIndex(
            MainWindow *wnd,
            Action *action,
            const QString &outputItemFormat,
            const QModelIndex &index)
        : QObject(action)
        , m_wnd(wnd)
        , m_outputFormat(outputItemFormat)
        , m_index(index)
    {
        connectActionOutput(action, this);
    }

    void onActionOutput(const QByteArray &output)
    {
        m_output.append(output);
        changeItem();
    }

    void onActionFinished(Action *action)
    {
        changeItem();

        auto removeFormats = action->inputFormats();
        if ( !removeFormats.isEmpty() ) {
            ClipboardBrowser *c = m_wnd->browserForItem(m_index);
            if (c) {
                removeFormats.removeAll(m_outputFormat);
                if ( !removeFormats.isEmpty() )
                    c->model()->setData(m_index, removeFormats, contentType::removeFormats);
            }
        }
    }

private:
    void changeItem()
    {
        ClipboardBrowser *c = m_wnd->browserForItem(m_index);
        if (c == nullptr)
            return;

        QVariantMap dataMap;
        if (m_outputFormat == mimeItems)
            deserializeData(&dataMap, m_output);
        else
            dataMap.insert(m_outputFormat, m_output);

        c->model()->setData(m_index, dataMap, contentType::updateData);
    }

    MainWindow *m_wnd;
    QString m_outputFormat;
    QPersistentModelIndex m_index;
    QByteArray m_output;
};

} // namespace

void actionOutput(
        MainWindow *wnd,
        Action *action,
        const QString &outputItemFormat,
        const QString &outputTabName,
        const QRegularExpression &itemSeparator
        )
{
    new ActionOutputItems(wnd, action, outputItemFormat, outputTabName, itemSeparator);
}

void actionOutput(
        MainWindow *wnd,
        Action *action,
        const QString &outputItemFormat,
        const QString &outputTabName
        )
{
    new ActionOutputItem(wnd, action, outputItemFormat, outputTabName);
}

void actionOutput(
        MainWindow *wnd,
        Action *action,
        const QString &outputItemFormat,
        const QModelIndex &index
        )
{
    new ActionOutputIndex(wnd, action, outputItemFormat, index);
}
