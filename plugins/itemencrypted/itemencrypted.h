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

#ifndef ITEMENCRYPTED_H
#define ITEMENCRYPTED_H

#include "item/itemwidget.h"

#include <QProcess>
#include <QWidget>

namespace Ui {
class ItemEncryptedSettings;
}

class QLabel;

class ItemEncrypted : public QWidget, public ItemWidget
{
    Q_OBJECT

public:
    ItemEncrypted(const QModelIndex &index, QWidget *parent);

    virtual void setEditorData(QWidget *editor, const QModelIndex &index) const;

    virtual void setModelData(QWidget *editor, QAbstractItemModel *model,
                              const QModelIndex &index) const;

protected:
    virtual void updateSize();

private:
    QLabel *m_iconLabel;
    QLabel *m_notesLabel;
};

class ItemEncryptedLoader : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID COPYQ_PLUGIN_ITEM_LOADER_ID)
    Q_INTERFACES(ItemLoaderInterface)

public:
    ItemEncryptedLoader();

    ~ItemEncryptedLoader();

    virtual ItemWidget *create(const QModelIndex &index, QWidget *parent) const;

    virtual QString id() const { return "itemencrypted"; }
    virtual QString name() const { return tr("Encrypted"); }
    virtual QString author() const { return QString(); }
    virtual QString description() const { return tr("Encrypted items"); }
    virtual QVariant icon() const { return QVariant(0xf023); }

    virtual QStringList formatsToSave() const;

    virtual QVariantMap applySettings();

    virtual void loadSettings(const QVariantMap &settings) { m_settings = settings; }

    virtual QWidget *createSettingsWidget(QWidget *parent);

private slots:
    void setPassword();
    void terminateGpgProcess();
    void onGpgProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    enum GpgProcessStatus {
        GpgNotRunning,
        GpgGeneratingKeys,
        GpgChangingPassword
    };

    void updateUi();

    Ui::ItemEncryptedSettings *ui;
    QVariantMap m_settings;

    GpgProcessStatus m_gpgProcessStatus;
    QProcess *m_gpgProcess;
};

#endif // ITEMENCRYPTED_H
