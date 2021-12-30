#include <windows.h>
#define WIN32_LEAN_AND_MEAN

#include <iostream>
#include <string>
#include <string_view>

namespace registry
{
	bool create_registry_values();
	bool delete_registry_values();
}

std::uint32_t exec(std::string_view args)
{
	std::string cmd = "cmd /c " + std::string{ args };

	STARTUPINFO si{ .cb = sizeof(si) };
	PROCESS_INFORMATION pi{};

	if (!CreateProcess(nullptr, std::data(cmd), nullptr, nullptr, false, 0, nullptr, nullptr, &si, &pi))
		return GetLastError();

	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD ret{};
	GetExitCodeProcess(pi.hProcess, &ret);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return ret;
}

int main()
{
	std::atexit([]()
	{
		std::cin.get();
	});

	exec("echo hello, friend.");
}
