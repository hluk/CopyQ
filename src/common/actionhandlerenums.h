#pragma once


#include <QtGlobal>

enum class ActionState {
    Starting,
    Running,
    Finished,
    Error,
};

namespace ActionHandlerColumn {
enum {
    id,
    name,
    status,
    started,
    finished,
    error,
    count
};
}

namespace ActionHandlerRole {
enum {
    sort = Qt::UserRole,
    status,
    id
};
}
