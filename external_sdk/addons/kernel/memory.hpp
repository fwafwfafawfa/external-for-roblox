#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include <cstdint>
#include <string>
#include <vector>
#include <Psapi.h>

// Global safe mode flag
inline bool g_safe_mode = false;

class c_memory {
public:
    c_memory(const char* process_name);
    ~c_memory();

    uintptr_t find_image();
    int32_t find_process(const char* process_name);

    DWORD get_pid() const { return process_id; }
    bool is_valid() const { return process_handle != NULL && process_handle != INVALID_HANDLE_VALUE; }


    template <typename T>
    T read(uintptr_t address) {
        // Safety checks
        if (g_safe_mode) return T{};
        if (!address || address < 0x10000) return T{};
        if (!process_handle || process_handle == INVALID_HANDLE_VALUE) return T{};

        T buffer{};
        if (!ReadProcessMemory(process_handle, reinterpret_cast<LPCVOID>(address), &buffer, sizeof(T), nullptr)) {
            return T{};
        }
        return buffer;
    }

    template <typename T>
    bool write(uintptr_t address, const T& buffer) {
        // Safety checks
        if (g_safe_mode) return false;
        if (!address || address < 0x10000) return false;
        if (!process_handle || process_handle == INVALID_HANDLE_VALUE) return false;

        return WriteProcessMemory(process_handle, reinterpret_cast<LPVOID>(address), &buffer, sizeof(T), nullptr);
    }

    // Try read with explicit success result and bytes-read check
    template <typename T>
    bool try_read(uintptr_t address, T& out) {
        if (g_safe_mode) return false;
        if (!address || address < 0x10000) return false;
        if (!process_handle || process_handle == INVALID_HANDLE_VALUE) return false;

        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(process_handle, reinterpret_cast<LPCVOID>(address), &out, sizeof(T), &bytesRead)) {
            return false;
        }
        return bytesRead == sizeof(T);
    }

private:
    HANDLE process_handle = NULL;
    int32_t process_id = 0;

};

inline c_memory* memory = nullptr;