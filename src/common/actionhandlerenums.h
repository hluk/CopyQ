#ifndef ACTIONHANDLERENUMS_H
#define ACTIONHANDLERENUMS_H

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
    sort,
    status
};
}

#endif // ACTIONHANDLERENUMS_H
