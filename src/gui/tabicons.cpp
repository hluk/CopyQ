// SPDX-License-Identifier: GPL-3.0-or-later

#include "tabicons.h"

#include "common/appconfig.h"
#include "common/config.h"
#include "common/settings.h"
#include "common/textdata.h"
#include "gui/iconfactory.h"

#include <QComboBox>
#include <QDir>
#include <QHash>
#include <QIcon>

namespace {

const QString tabsGroup = QStringLiteral("Tabs");

QByteArray tabNameFromFileSuffix(QByteArray base64Suffix)
{
    return QByteArray::fromBase64(base64Suffix.replace('-', '/'));
}

} // namespace

QList<QString> savedTabs()
{
    QList<QString> tabs = AppConfig().option<Config::tabs>();

    const QString configPath = settingsDirectoryPath();

    QDir configDir(configPath);
    QList<QString> files = configDir.entryList({QStringLiteral("*_tab_*.dat")});
    files.append(configDir.entryList({QStringLiteral("*_tab_*.dat.tmp")}));

    QRegularExpression re("_tab_([^.]*)");

    for (const auto &fileName : files) {
        const auto m = re.match(fileName);
        if (m.hasMatch()) {
            const QString tabName =
                    getTextData(tabNameFromFileSuffix(m.captured(1).toUtf8()));
            if ( !tabName.isEmpty() && !tabs.contains(tabName) )
                tabs.append(tabName);
        }
    }

    if ( tabs.isEmpty() )
        tabs.append( AppConfig().option<Config::clipboard_tab>() );

    return tabs;
}

QString getIconNameForTabName(const QString &tabName)
{
    Settings settings;
    const int size = settings.beginReadArray(tabsGroup);
    for(int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        if (settings.value("name").toString() == tabName)
            return settings.value("icon").toString();
    }

    return QString();
}

void setIconNameForTabName(const QString &tabName, const QString &icon)
{
    Settings settings;
    const int size = settings.beginReadArray(tabsGroup);
    for(int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        if (settings.value("name").toString() == tabName) {
            settings.setValue("icon", icon);
            return;
        }
    }
    settings.endArray();

    settings.beginWriteArray(tabsGroup, size + 1);
    settings.setArrayIndex(size);
    settings.setValue("name", tabName);
    settings.setValue("icon", icon);
    settings.endArray();
}

QIcon getIconForTabName(const QString &tabName)
{
    const QString fileName = getIconNameForTabName(tabName);
    return fileName.isEmpty() ? QIcon() : iconFromFile(fileName);
}

void initTabComboBox(QComboBox *comboBox)
{
    setComboBoxItems(comboBox, AppConfig().option<Config::tabs>());

    for (int i = 1; i < comboBox->count(); ++i) {
        const QString tabName = comboBox->itemText(i);
        const QIcon icon = getIconForTabName(tabName);
        comboBox->setItemIcon(i, icon);
    }
}

void setDefaultTabItemCounterStyle(QWidget *widget)
{
    QFont font = widget->font();
    const qreal pointSize = font.pointSizeF();
    if (pointSize > 0.0)
        font.setPointSizeF(pointSize * 0.7);
    else
        font.setPixelSize( static_cast<int>(font.pixelSize() * 0.7) );
    widget->setFont(font);
}

void setComboBoxItems(QComboBox *comboBox, const QList<QString> &items)
{
    const QString text = comboBox->currentText();
    comboBox->clear();
    comboBox->addItem(QString());
    comboBox->addItems(items);
    comboBox->setEditText(text);

    const int currentIndex = comboBox->findText(text);
    if (currentIndex != -1)
        comboBox->setCurrentIndex(currentIndex);
}
