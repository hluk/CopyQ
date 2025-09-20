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

    #define VERIFY_TEMPORARY_TAB_EXIST(tabName) \
    do { \
        RUN("tab", QStringLiteral(tabName) + "\n" + QString(clipboardTabName) + "\n"); \
        RUN("read(0)", "A"); \
        RUN("size()", "1\n"); \
        Settings settings; \
        const QStringList expectedTabs{tabName, clipboardTabName}; \
        QCOMPARE(settings.value("Options/tabs"), expectedTabs); \
        QCOMPARE(settings.value("Options/tray_tab"), tabName); \
        settings.beginReadArray("Tabs"); \
        settings.setArrayIndex(0); \
        QCOMPARE(settings.value("name"), tabName); \
        QCOMPARE(settings.value("icon"), "x"); \
        QCOMPARE(settings.value("store_items").toBool(), false); \
        settings.endArray(); \
    } while(false)

    VERIFY_TEMPORARY_TAB_EXIST("temp1");

    RUN("renameTab('temp1', 'temp2')", "");

    VERIFY_TEMPORARY_TAB_EXIST("temp2");

    // Restart server.
    TEST( m_test->stopServer() );
    TEST( m_test->startServer() );

    RUN("tab", "temp2\n" + QString(clipboardTabName) + "\n");
    RUN("size()", "0\n");
}
