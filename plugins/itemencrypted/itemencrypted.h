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

#ifndef ITEMENCRYPTED_H
#define ITEMENCRYPTED_H

#include "item/itemwidget.h"
#include "gui/icons.h"

#include <QProcess>
#include <QWidget>

#include <memory>

namespace Ui {
class ItemEncryptedSettings;
}

class QIODevice;

class ItemEncrypted : public QWidget, public ItemWidget
{
    Q_OBJECT

public:
    explicit ItemEncrypted(QWidget *parent);

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                              const QModelIndex &index) const override;
};

class ItemEncryptedSaver : public QObject, public ItemSaverInterface
{
    Q_OBJECT

public:
    bool saveItems(const QString &tabName, const QAbstractItemModel &model, QIODevice *file) override;

signals:
    void error(const QString &);

private:
    void emitEncryptFailed();
};

class ItemEncryptedScriptable : public ItemScriptable
{
    Q_OBJECT
public slots:
    bool isEncrypted();
    QByteArray encrypt();
    QByteArray decrypt();

    void encryptItem();
    void decryptItem();

    void encryptItems();
    void decryptItems();

    void copyEncryptedItems();
    void pasteEncryptedItems();

    QString generateTestKeys();
    bool isGpgInstalled();

private:
    QByteArray encrypt(const QByteArray &bytes);
    QByteArray decrypt(const QByteArray &bytes);
};

class ItemEncryptedLoader : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID COPYQ_PLUGIN_ITEM_LOADER_ID)
    Q_INTERFACES(ItemLoaderInterface)

public:
    ItemEncryptedLoader();

    ~ItemEncryptedLoader();

    ItemWidget *create(const QVariantMap &data, QWidget *parent, bool) const override;

    QString id() const override { return "itemencrypted"; }
    QString name() const override { return tr("Encryption"); }
    QString author() const override { return QString(); }
    QString description() const override { return tr("Encrypt items and tabs."); }
    QVariant icon() const override { return QVariant(IconLock); }

    QStringList formatsToSave() const override;

    QVariantMap applySettings() override;

    void loadSettings(const QVariantMap &settings) override { m_settings = settings; }

    QWidget *createSettingsWidget(QWidget *parent) override;

    bool canLoadItems(QIODevice *file) const override;

    bool canSaveItems(const QString &tabName) const override;

    ItemSaverPtr loadItems(const QString &tabName, QAbstractItemModel *model, QIODevice *file, int maxItems) override;

    ItemSaverPtr initializeTab(const QString &, QAbstractItemModel *model, int maxItems) override;

    QObject *tests(const TestInterfacePtr &test) const override;

    const QObject *signaler() const override { return this; }

    ItemScriptable *scriptableObject() override;

    QVector<Command> commands() const override;

signals:
    void error(const QString &);

private slots:
    void setPassword();
    void terminateGpgProcess();
    void onGpgProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    enum GpgProcessStatus {
        GpgNotInstalled,
        GpgNotRunning,
        GpgGeneratingKeys,
        GpgChangingPassword
    };

    void updateUi();

    void emitDecryptFailed();

    ItemSaverPtr createSaver();

    std::unique_ptr<Ui::ItemEncryptedSettings> ui;
    QVariantMap m_settings;

    GpgProcessStatus m_gpgProcessStatus;
    QProcess *m_gpgProcess;
};

#endif // ITEMENCRYPTED_H
