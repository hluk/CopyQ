// SPDX-License-Identifier: GPL-3.0-or-later
//
// Console-subsystem wrapper for CopyQ (copyq.com).
//
// Windows distinguishes GUI and console applications at the PE binary level.
// CopyQ's main binary (copyq.exe) is a GUI-subsystem application to avoid
// spawning a console window when launched from a shortcut or at startup.
//
// This wrapper is a console-subsystem binary.  Windows command resolution
// prefers .com over .exe, so typing "copyq" in a terminal runs copyq.com.
// Because it is a console app, the shell waits for it, and stdout/stderr
// are connected to the terminal.
//
// The wrapper launches copyq.exe with the same arguments and inherited
// standard handles.  No pipes, no buffering, no I/O relay — the child
// writes directly to the caller's console or pipe.  This avoids the
// deadlock and argument-mangling problems that plagued CopyQ's earlier
// QProcess-based .com wrapper (2013–2015).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HANDLE s_childProcess = NULL;

static BOOL WINAPI consoleCtrlHandler(DWORD event)
{
    (void)event;
    // The child is a GUI-subsystem process — it does not receive console
    // control events.  Terminate it so the wrapper can exit cleanly.
    if (s_childProcess)
        TerminateProcess(s_childProcess, STATUS_CONTROL_C_EXIT);
    return TRUE;
}

// Replace the file extension in `path` (length `len`) with `newExt`.
// Returns FALSE if no extension is found.
static BOOL replaceExtension(wchar_t *path, DWORD len, const wchar_t *newExt)
{
    for (wchar_t *p = path + len - 1; p > path; --p) {
        if (*p == L'\\' || *p == L'/')
            break;
        if (*p == L'.') {
            // Overwrite from the dot onward.
            for (++p; *newExt; ++p, ++newExt)
                *p = *newExt;
            *p = L'\0';
            return TRUE;
        }
    }
    return FALSE;
}

int main(void)
{
    // Resolve copyq.exe: same directory, swap .com -> .exe.
    wchar_t exePath[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, exePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
        return 1;

    if (!replaceExtension(exePath, len, L"exe"))
        return 1;

    // Pass the original command line through, replacing only argv[0].
    // Skip past the program name in the raw command line.
    const wchar_t *cmdLine = GetCommandLineW();
    const wchar_t *args = cmdLine;
    if (*args == L'"') {
        for (++args; *args && *args != L'"'; ++args) {}
        if (*args == L'"') ++args;
    } else {
        while (*args && *args != L' ' && *args != L'\t') ++args;
    }
    // `args` now points at the space before the first argument (or end).

    // Build: "C:\...\copyq.exe" <original args>
    // Max command line on Windows is 32767 characters.
    wchar_t newCmdLine[32768];
    newCmdLine[0] = L'"';
    DWORD pos = 1;

    for (DWORD i = 0; exePath[i] && pos < 32766; ++i)
        newCmdLine[pos++] = exePath[i];
    if (pos >= 32766)
        return 1;
    newCmdLine[pos++] = L'"';

    // Append the rest of the original command line (includes leading space).
    for (const wchar_t *p = args; *p && pos < 32767; ++p)
        newCmdLine[pos++] = *p;
    newCmdLine[pos] = L'\0';

    // Launch copyq.exe, inheriting our console handles directly.
    // No pipes, no relay — the child writes to the caller's terminal.
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(
            exePath,
            newCmdLine,
            NULL, NULL,
            TRUE,             // Inherit handles
            0,                // No special flags; GUI-subsystem exe won't spawn a console
            NULL, NULL,
            &si, &pi))
    {
        return 1;
    }

    CloseHandle(pi.hThread);
    s_childProcess = pi.hProcess;

    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);

    return (int)exitCode;
}
