#include <windows.h>
#define WIN32_LEAN_AND_MEAN

#include <iostream>
#include <array>
#include <string>
#include <string_view>

namespace registry
{
	bool create_registry_values();
	bool delete_registry_values();

	namespace environment_variables
	{
		constexpr std::string_view path = "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";
		constexpr auto values = std::to_array<std::string_view>({ "FTP_PROXY", "HTTP_PROXY", "HTTPS_PROXY" });
	}
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
