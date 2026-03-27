#pragma once
#include <Windows.h>
#include <winioctl.h>
#include <TlHelp32.h>
#include <cstdint>
#include <string>
#include <memory>
#include <vector>

#define code_rw CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1442, METHOD_BUFFERED, FILE_SPECIAL_ACCESS) 
#define code_ba CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1443, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define code_get_guarded_region CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1444, METHOD_BUFFERED, FILE_SPECIAL_ACCESS) 
#define code_GetDirBase CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1445, METHOD_BUFFERED, FILE_SPECIAL_ACCESS) 
#define code_security 0x7327c6cA 

typedef struct _rw {
	INT32 security;
	INT32 process_id;
	ULONGLONG address;
	ULONGLONG buffer;
	ULONGLONG size;
	BOOLEAN write;
} rw, * prw;

typedef struct _ba {
	INT32 security;
	INT32 process_id;
	ULONGLONG* address;
} ba, * pba;

typedef struct _ga {
	INT32 security;
	ULONGLONG* address;
} ga, * pga;

typedef struct _MEMORY_OPERATION_DATA {
	uint32_t pid;
	ULONGLONG* cr3;
} MEMORY_OPERATION_DATA, * PMEMORY_OPERATION_DATA;

namespace km {
	inline HANDLE driver_handle;
	inline INT32 process_id;
	inline uintptr_t virtualaddy;
	inline uintptr_t cr3;

	inline bool find_driver() {
		driver_handle = CreateFileW(L"\\\\.\\eSIGNuTILITES1", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

		if (!driver_handle || (driver_handle == INVALID_HANDLE_VALUE))
			return false;

		return true;
	}

	inline void read_physical(PVOID address, PVOID buffer, DWORD size) {
		_rw arguments = { 0 };

		arguments.security = code_security;
		arguments.address = (ULONGLONG)address;
		arguments.buffer = (ULONGLONG)buffer;
		arguments.size = size;
		arguments.process_id = process_id;
		arguments.write = FALSE;

		DeviceIoControl(driver_handle, code_rw, &arguments, sizeof(arguments), nullptr, NULL, NULL, NULL);
	}

	inline void write_physical(PVOID address, PVOID buffer, DWORD size) {
		_rw arguments = { 0 };

		arguments.security = code_security;
		arguments.address = (ULONGLONG)address;
		arguments.buffer = (ULONGLONG)buffer;
		arguments.size = size;
		arguments.process_id = process_id;
		arguments.write = TRUE;

		DeviceIoControl(driver_handle, code_rw, &arguments, sizeof(arguments), nullptr, NULL, NULL, NULL);
	}

	inline uintptr_t fetch_cr3() {
		uintptr_t cr3_val = NULL;
		_MEMORY_OPERATION_DATA arguments = { 0 };

		arguments.pid = process_id;
		arguments.cr3 = (ULONGLONG*)&cr3_val;

		DeviceIoControl(driver_handle, code_GetDirBase, &arguments, sizeof(arguments), nullptr, NULL, NULL, NULL);

		return cr3_val;
	}

	inline uintptr_t find_image() {
		uintptr_t image_address = { NULL };
		_ba arguments = { NULL };

		arguments.security = code_security;
		arguments.process_id = process_id;
		arguments.address = (ULONGLONG*)&image_address;

		DeviceIoControl(driver_handle, code_ba, &arguments, sizeof(arguments), nullptr, NULL, NULL, NULL);

		return image_address;
	}

	inline uintptr_t get_guarded_region() {
		uintptr_t guarded_region_address = { NULL };
		_ga arguments = { NULL };

		arguments.security = code_security;
		arguments.address = (ULONGLONG*)&guarded_region_address;

		DeviceIoControl(driver_handle, code_get_guarded_region, &arguments, sizeof(arguments), nullptr, NULL, NULL, NULL);

		return guarded_region_address;
	}

	inline INT32 find_process(LPCTSTR process_name) {
		PROCESSENTRY32 pt;
		HANDLE hsnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		pt.dwSize = sizeof(PROCESSENTRY32);
		if (Process32First(hsnap, &pt)) {
			do {
				if (!lstrcmpi(pt.szExeFile, process_name)) {
					CloseHandle(hsnap);
					process_id = pt.th32ProcessID;
					return pt.th32ProcessID;
				}
			} while (Process32Next(hsnap, &pt));
		}
		CloseHandle(hsnap);
		return { NULL };
	}
}

template <typename T>
inline T Read(uint64_t address) {
	T buffer{ };
	if (address == NULL || address < 10000) {
		return buffer;
	}
	km::read_physical((PVOID)address, &buffer, sizeof(T));
	return buffer;
}

template <typename T>
inline bool Write(uint64_t address, T value) {
	km::write_physical((PVOID)address, &value, sizeof(T));
	return true;
}

// Lowercase aliases for compatibility
template <typename T>
inline T read(uint64_t address) {
	return Read<T>(address);
}

template <typename T>
inline bool write(uint64_t address, T value) {
	return Write<T>(address, value);
}

inline std::string ReadString(uint64_t Address) {
	std::unique_ptr<char[]> buffer(new char[64]);
	km::read_physical((PVOID)Address, buffer.get(), 64);
	return buffer.get();
}

inline std::string read_wstr(uintptr_t address) {
	wchar_t buffer[1024 * sizeof(wchar_t)];
	km::read_physical((PVOID)address, &buffer, 1024 * sizeof(wchar_t));

	std::wstring new_buffer = std::wstring(buffer);

	return std::string(new_buffer.begin(), new_buffer.end());
}

template <typename T>
inline T Read_chain(std::uint64_t address, std::vector<std::uint64_t> chain) {
	uint64_t cur_read = address;

	for (size_t r = 0; r < chain.size() - 1; ++r)
		cur_read = Read<std::uint64_t>(cur_read + chain[r]);

	return Read<T>(cur_read + chain[chain.size() - 1]);
}

inline int PID(std::wstring name) {
	const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	PROCESSENTRY32 entry{ };
	entry.dwSize = sizeof(PROCESSENTRY32);

	Process32First(snapshot, &entry);
	do {
		if (!name.compare(entry.szExeFile)) {
			CloseHandle(snapshot);
			return entry.th32ProcessID;
		}
	} while (Process32Next(snapshot, &entry));

	CloseHandle(snapshot);
	return 0;
}
