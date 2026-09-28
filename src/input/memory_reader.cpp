#include "memory_reader.h"

#include <algorithm>

namespace aimlab {

MemoryReader::~MemoryReader() {
    Close();
}

MemoryReader::MemoryReader(MemoryReader&& other) noexcept
    : m_handle(other.m_handle), m_pid(other.m_pid) {
    other.m_handle = nullptr;
    other.m_pid = 0;
}

MemoryReader& MemoryReader::operator=(MemoryReader&& other) noexcept {
    if (this != &other) {
        Close();
        m_handle = other.m_handle;
        m_pid = other.m_pid;
        other.m_handle = nullptr;
        other.m_pid = 0;
    }
    return *this;
}

bool MemoryReader::Open(DWORD pid) {
    Close();
    m_handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!m_handle) return false;
    m_pid = pid;
    return true;
}

void MemoryReader::Close() {
    if (m_handle) {
        CloseHandle(m_handle);
        m_handle = nullptr;
        m_pid = 0;
    }
}

uintptr_t MemoryReader::GetModuleBase(const wchar_t* moduleName) const {
    if (!m_handle) return 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_pid);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    uintptr_t base = 0;

    if (Module32FirstW(snap, &me)) {
        do {
            if (_wcsicmp(me.szModule, moduleName) == 0) {
                base = reinterpret_cast<uintptr_t>(me.modBaseAddr);
                break;
            }
        } while (Module32NextW(snap, &me));
    }

    CloseHandle(snap);
    return base;
}

std::vector<ProcessInfo> MemoryReader::EnumerateProcesses(const wchar_t* nameFilter) {
    std::vector<ProcessInfo> out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(snap, &pe)) {
        do {
            if (nameFilter && _wcsicmp(pe.szExeFile, nameFilter) != 0)
                continue;

            ProcessInfo info;
            info.pid  = pe.th32ProcessID;
            info.name = pe.szExeFile;

            HWND h = nullptr;
            while ((h = FindWindowExW(nullptr, h, nullptr, nullptr)) != nullptr) {
                DWORD wpid = 0;
                GetWindowThreadProcessId(h, &wpid);
                if (wpid == info.pid && IsWindowVisible(h)) {
                    wchar_t title[256]{};
                    GetWindowTextW(h, title, 256);
                    info.title = title;
                    break;
                }
            }

            out.push_back(std::move(info));
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    std::sort(out.begin(), out.end(),
              [](const ProcessInfo& a, const ProcessInfo& b) { return a.name < b.name; });
    return out;
}

}  // namespace aimlab
