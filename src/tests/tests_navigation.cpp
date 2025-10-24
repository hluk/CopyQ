// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"

namespace {

const QString navigate = QStringLiteral(R"(
    (function(key){
        plugins.itemtests.keys(str(key), '%1');
        print(testSelected());
    })
)").arg(clipboardBrowserId);

} // namespace

void Tests::navigationTestInit()
{
    RUN("tab" << "tab1" << "add(1,2,3)", "");
    RUN("setCurrentTab('tab1')", "");
}

void Tests::navigationTestDownUp(const QString &down, const QString &up)
{
    RUN("selectItems(0)", "true\n");
    KEYS(clipboardBrowserId);

    RUN(navigate << down, "tab1 1 1");
    RUN(navigate << down, "tab1 2 2");
    RUN(navigate << down, "tab1 2 2");

    RUN(navigate << up, "tab1 1 1");
    RUN(navigate << up, "tab1 0 0");
    RUN(navigate << up, "tab1 0 0");

    RUN(navigate << "SHIFT+" + down, "tab1 1 0 1");
    RUN(navigate << "SHIFT+" + down, "tab1 2 0 1 2");
    RUN(navigate << "SHIFT+" + up, "tab1 1 0 1");
    RUN(navigate << "SHIFT+" + up, "tab1 0 0");
}

void Tests::navigationTestEndHome(const QString &end, const QString &home)
{
    RUN("selectItems(0)", "true\n");
    KEYS(clipboardBrowserId);

    RUN(navigate << end, "tab1 2 2");
    RUN(navigate << end, "tab1 2 2");

    RUN(navigate << home, "tab1 0 0");
    RUN(navigate << home, "tab1 0 0");
}

void Tests::navigationTestEscapeEditor(const QString &esc, const QString &editor)
{
    KEYS(editor << editorId);
    KEYS(esc << clipboardBrowserId);
}

void Tests::navigationTestEscapeSearch(const QString &esc, const QString &search)
{
    KEYS(search << filterEditId << ":test");
    RUN("filter", "test\n");
    KEYS(esc << clipboardBrowserId);
    RUN("filter", "\n");
}

void Tests::navigationDefault()
{
    RUN("config" << "navigation_style", "0\n");
    navigationTestInit();
    navigationTestDownUp("DOWN", "UP");
    navigationTestEndHome("END", "HOME");
    navigationTestEscapeSearch("ESC", "F3");
    navigationTestEscapeEditor("ESC", "F2");
}

void Tests::navigationVi()
{
    RUN("config" << "navigation_style" << "1", "1\n");
    navigationTestInit();
    navigationTestDownUp("J", "K");
    navigationTestEndHome("SHIFT+G", "G");
    navigationTestEscapeSearch("CTRL+[", "/");
    navigationTestEscapeEditor("CTRL+[", "F2");
}

void Tests::navigationEmacs()
{
    RUN("config" << "navigation_style" << "2", "2\n");
    navigationTestInit();
    navigationTestDownUp("CTRL+N", "CTRL+P");
    navigationTestEndHome("ALT+>", "ALT+<");
    navigationTestEscapeSearch("CTRL+G", "CTRL+F");
    navigationTestEscapeEditor("CTRL+G", "F2");
}
