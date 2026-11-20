#pragma once
// Tiny hidden-window process helper (Windows only).
#include <string>
#include <cstdint>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace lr {

class Proc {
public:
    bool start(std::string cmd, bool pipeStdin) {
        SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
        HANDLE rd = nullptr, wr = nullptr;
        if (pipeStdin) {
            if (!CreatePipe(&rd, &wr, &sa, 1 << 22)) return false;
            SetHandleInformation(wr, HANDLE_FLAG_INHERIT, 0);
        }
        HANDLE nul = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdInput = pipeStdin ? rd : GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = nul;
        si.hStdError = nul;

        PROCESS_INFORMATION pi{};
        BOOL ok = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                                 CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        if (rd) CloseHandle(rd);
        if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
        if (!ok) {
            if (wr) CloseHandle(wr);
            return false;
        }
        CloseHandle(pi.hThread);
        m_proc = pi.hProcess;
        m_in = wr;
        return true;
    }

    bool write(const void* data, size_t len) {
        auto p = static_cast<const uint8_t*>(data);
        while (len) {
            DWORD n = 0;
            if (!WriteFile(m_in, p, static_cast<DWORD>(len > (1u << 20) ? (1u << 20) : len), &n, nullptr) || !n) return false;
            p += n; len -= n;
        }
        return true;
    }

    // Closes stdin, waits for exit, returns exit code (-1 on failure).
    int finish() {
        if (m_in) { CloseHandle(m_in); m_in = nullptr; }
        int code = -1;
        if (m_proc) {
            WaitForSingleObject(m_proc, INFINITE);
            DWORD c = 0;
            if (GetExitCodeProcess(m_proc, &c)) code = static_cast<int>(c);
            CloseHandle(m_proc);
            m_proc = nullptr;
        }
        return code;
    }

    void kill() {
        if (m_in) { CloseHandle(m_in); m_in = nullptr; }
        if (m_proc) { TerminateProcess(m_proc, 1); CloseHandle(m_proc); m_proc = nullptr; }
    }

    bool running() const { return m_proc != nullptr; }

private:
    HANDLE m_proc = nullptr;
    HANDLE m_in = nullptr;
};

// Run a command to completion with no window. Returns exit code.
inline int runHidden(std::string const& cmd) {
    Proc p;
    if (!p.start(cmd, false)) return -1;
    return p.finish();
}

}
