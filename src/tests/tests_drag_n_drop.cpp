#include "test_utils.h"
#include "tests.h"

#include "common/sleeptimer.h"

#include <QStyleHints>

// Simulate move event to the target. A single move event does not seem to be
// enough to update the drag target.
#define MOVE_EVENT_COUNT 4
#define MOVE_TO(TARGET) do { \
    for (int i = 0; i < MOVE_EVENT_COUNT; ++i) { \
        RUN("keys" << "mouse|MOVE|" TARGET, ""); \
        waitFor(dragDelay() / MOVE_EVENT_COUNT); \
    } \
} while(false)
#define DROP_TO(TARGET) do { \
    MOVE_TO(TARGET); \
    RUN("keys" << "mouse|RELEASE|" TARGET, ""); \
} while(false)
#define DRAG_FROM_TO(SOURCE, TARGET) do { \
    RUN("keys" << "mouse|PRESS|" SOURCE, ""); \
    waitFor(dragDelay()); \
    DROP_TO(TARGET); \
} while(false)

#define DRAG_TAB(SOURCE, TARGET) \
    DRAG_FROM_TO("tab_tree_item|text=" SOURCE, "tab_tree_item|text=" TARGET)

namespace {

int dragDelay()
{
    static const int delay = qMax(300, qApp->styleHints()->startDragTime() * 2);
    return delay;
}

} // namespace


void Tests::dragNDropTreeTab()
{
    SKIP_ON_ENV("COPYQ_TESTS_SKIP_DRAG_AND_DROP");

    RUN("config" << "tab_tree" << "true", "true\n");
    RUN("config('tabs', ['TAB1','TAB2'])", "TAB1\nTAB2\n");
    WAIT_ON_OUTPUT("tab", "TAB1\nTAB2\nCLIPBOARD\n");

    DRAG_TAB("TAB2", "TAB1");
    RUN("keys" << "mouse|RELEASE|tab_tree_item|text=TAB1", "");

    WAIT_ON_OUTPUT("tab", "TAB1\nTAB1/TAB2\nCLIPBOARD\n");
}

void Tests::dragNDropTreeTabNested()
{
    SKIP_ON_ENV("COPYQ_TESTS_SKIP_DRAG_AND_DROP");

    RUN("config" << "tab_tree" << "true", "true\n");
    RUN("config('tabs', ['a/b/c/d','a/b/c'])", "a/b/c/d\na/b/c\n");
    WAIT_ON_OUTPUT("tab", "a/b/c/d\na/b/c\nCLIPBOARD\n");

    DRAG_TAB("c", "a");

    WAIT_ON_OUTPUT("tab", "a/c\na/c/d\nCLIPBOARD\n");
}
