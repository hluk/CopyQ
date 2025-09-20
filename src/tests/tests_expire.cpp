#include "test_utils.h"
#include "tests.h"

#include "common/settings.h"

void Tests::expireTabs()
{
    {
        Settings settings;
        settings.setValue("Options/tabs", QStringList{"temp1", clipboardTabName});
        settings.setValue("Options/tray_tab", "temp1");
        settings.beginWriteArray("Tabs");
        settings.setArrayIndex(0);
        settings.setValue("name", "temp1");
        settings.setValue("icon", "x");
        settings.setValue("store_items", false);
        settings.endArray();
    }

    // Trigger configuration reload.
    RUN("config('maxitems', 10)", "10\n");

    RUN("tab", "temp1\n" + QString(clipboardTabName) + "\n");

    RUN("add" << "A", "");

    // Trigger configuration reload.
    RUN("config('maxitems', 11)", "11\n");

    RUN("read(0)", "A");
    RUN("size()", "1\n");

    {
        Settings settings;
        const QStringList expectedTabs{"temp1", clipboardTabName};
        QCOMPARE(settings.value("Options/tabs"), expectedTabs);
        QCOMPARE(settings.value("Options/tray_tab"), "temp1");
        settings.beginReadArray("Tabs");
        settings.setArrayIndex(0);
        QCOMPARE(settings.value("name"), QString("temp1"));
        QCOMPARE(settings.value("icon"), QString("x"));
        QCOMPARE(settings.value("store_items").toBool(), false);
        settings.endArray();
    }

    RUN("renameTab('temp1', 'temp2')", "");
    RUN("tab", "temp2\n" + QString(clipboardTabName) + "\n");
    RUN("read(0)", "A");
    RUN("size()", "1\n");

    {
        Settings settings;
        const QStringList expectedTabs{"temp2", clipboardTabName};
        QCOMPARE(settings.value("Options/tabs"), expectedTabs);
        QCOMPARE(settings.value("Options/tray_tab"), "temp2");
        settings.beginReadArray("Tabs");
        settings.setArrayIndex(0);
        QCOMPARE(settings.value("name"), QString("temp2"));
        QCOMPARE(settings.value("icon"), QString("x"));
        QCOMPARE(settings.value("store_items").toBool(), false);
        settings.endArray();
    }

    // Restart server.
    TEST( m_test->stopServer() );
    TEST( m_test->startServer() );

    RUN("tab", "temp2\n" + QString(clipboardTabName) + "\n");
    RUN("size()", "0\n");
}
