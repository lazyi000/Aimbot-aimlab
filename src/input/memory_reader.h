#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <cstdint>
#include <string>
#include <vector>

namespace aimlab {

struct ProcessInfo {
    DWORD         pid{};
    std::wstring  name;
    std::wstring  title;
};

class MemoryReader {
public:
    MemoryReader() = default;
    ~MemoryReader();

    MemoryReader(const MemoryReader&) = delete;
    MemoryReader& operator=(const MemoryReader&) = delete;
    MemoryReader(MemoryReader&&) noexcept;
    MemoryReader& operator=(MemoryReader&&) noexcept;

    bool Open(DWORD pid);
    void Close();
    bool IsOpen() const { return m_handle != nullptr; }
    DWORD GetPid() const { return m_pid; }

    template<typename T>
    T Read(uintptr_t address) const {
        T value{};
        if (!m_handle) return value;
        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(m_handle, reinterpret_cast<LPCVOID>(address),
                               &value, sizeof(T), &bytesRead) ||
            bytesRead != sizeof(T)) {
            return T{};
        }
        return value;
    }

    bool ReadRaw(uintptr_t address, void* buffer, SIZE_T size) const {
        if (!m_handle) return false;
        SIZE_T bytesRead = 0;
        return ReadProcessMemory(m_handle, reinterpret_cast<LPCVOID>(address),
                                 buffer, size, &bytesRead) && bytesRead == size;
    }

    uintptr_t GetModuleBase(const wchar_t* moduleName) const;

    static std::vector<ProcessInfo> EnumerateProcesses(const wchar_t* nameFilter = nullptr);

private:
    HANDLE m_handle = nullptr;
    DWORD  m_pid    = 0;
};

}  // namespace aimlab
