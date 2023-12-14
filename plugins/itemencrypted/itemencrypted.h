// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ITEMENCRYPTED_H
#define ITEMENCRYPTED_H

#include "item/itemwidget.h"
#include "gui/icons.h"

#include <QProcess>
#include <QVariant>
#include <QWidget>

#include <memory>

namespace Ui {
class ItemEncryptedSettings;
}

class QIODevice;

class ItemEncrypted final : public QWidget, public ItemWidget
{
    Q_OBJECT

public:
    explicit ItemEncrypted(QWidget *parent);
};

class ItemEncryptedSaver final : public QObject, public ItemSaverInterface
{
    Q_OBJECT

public:
    bool saveItems(const QString &tabName, const QAbstractItemModel &model, QIODevice *file) override;

signals:
    void error(const QString &);

private:
    void emitEncryptFailed();
};

class ItemEncryptedScriptable final : public ItemScriptable
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

class ItemEncryptedLoader final : public QObject, public ItemLoaderInterface
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

    void applySettings(QSettings &settings) override;

    void loadSettings(const QSettings &settings) override;

    QWidget *createSettingsWidget(QWidget *parent) override;

    bool canLoadItems(QIODevice *file) const override;

    bool canSaveItems(const QString &tabName) const override;

    ItemSaverPtr loadItems(const QString &tabName, QAbstractItemModel *model, QIODevice *file, int maxItems) override;

    ItemSaverPtr initializeTab(const QString &, QAbstractItemModel *model, int maxItems) override;

    QObject *tests(const TestInterfacePtr &test) const override;

    const QObject *signaler() const override { return this; }

    ItemScriptable *scriptableObject() override;

    QVector<Command> commands() const override;

    bool data(QVariantMap *data, const QModelIndex &) const override;

    bool setData(const QVariantMap &data, const QModelIndex &index, QAbstractItemModel *model) const override;

signals:
    void error(const QString &);

private:
    void setPassword();
    void terminateGpgProcess();
    void onGpgProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    enum GpgProcessStatus {
        GpgCheckIfInstalled,
        GpgNotInstalled,
        GpgNotRunning,
        GpgGeneratingKeys,
        GpgChangingPassword
    };

    void updateUi();

    void emitDecryptFailed();

    ItemSaverPtr createSaver();

    GpgProcessStatus status() const;

    std::unique_ptr<Ui::ItemEncryptedSettings> ui;
    QStringList m_encryptTabs;

    mutable GpgProcessStatus m_gpgProcessStatus;
    QProcess *m_gpgProcess;
};

#endif // ITEMENCRYPTED_H
