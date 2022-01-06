#include <windows.h>
#include <winhttp.h>

#define WIN32_LEAN_AND_MEAN

#pragma comment(lib, "winhttp.lib")
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

#include <array>
#include <bit>
#include <span>
#include <string>
#include <string_view>

namespace registry
{
	bool create_registry_values(const std::span<const std::string_view>&, std::string_view);
	bool delete_registry_values(const std::span<const std::string_view>&);

	namespace environment_variables
	{
		constexpr std::string_view path = "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";
		constexpr auto values = std::to_array<std::string_view>({ "FTP_PROXY", "HTTP_PROXY", "HTTPS_PROXY" });
	}
}

namespace proxies
{
	std::string retrieve_proxy_address();

	constexpr std::string_view ping_command = "ping -n 1 -w 50 wpad";
	constexpr std::wstring_view website_to_ping = L"https://cloudflare.com";

	WINHTTP_AUTOPROXY_OPTIONS autoproxy_options
	{
		.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT,
		.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A
	};
	WINHTTP_PROXY_INFO proxy_info{};
}

std::uint32_t exec(std::string_view args)
{
	std::string cmd = "cmd /c " + std::string{ args };

	STARTUPINFO si{ .cb = sizeof(si) };
	PROCESS_INFORMATION pi{};

	if (!CreateProcess(nullptr, std::data(cmd), nullptr, nullptr, false, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
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
	const std::string proxy_address = proxies::retrieve_proxy_address();

	if (std::empty(proxy_address))
	{
		registry::delete_registry_values(registry::environment_variables::values);
	}
	else
	{
		registry::create_registry_values(registry::environment_variables::values, proxy_address);
	}
}

bool registry::create_registry_values(const std::span<const std::string_view>& values, std::string_view data)
{
	if (std::empty(values) || std::empty(data))
		return false;

	HKEY hkey = nullptr;
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, std::data(registry::environment_variables::path), 0, KEY_SET_VALUE, &hkey) != ERROR_SUCCESS)
		return false;

	for (const auto& value : values)
		RegSetValueEx(hkey, std::data(value), 0, REG_SZ, std::bit_cast<const BYTE*>(std::data(data)), static_cast<DWORD>(std::size(data)));

	RegCloseKey(hkey);
	return true;
}

bool registry::delete_registry_values(const std::span<const std::string_view>& values)
{
	if (std::empty(values))
		return false;

	HKEY hkey = nullptr;
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, std::data(registry::environment_variables::path), 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &hkey) != ERROR_SUCCESS)
		return false;

	for (const auto& value : values)
		if (RegQueryValueEx(hkey, std::data(value), nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
			RegDeleteValue(hkey, std::data(value));

	RegCloseKey(hkey);
	return true;
}

std::string proxies::retrieve_proxy_address()
{
	const HINTERNET winhttp_session = WinHttpOpen(nullptr, WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (winhttp_session == nullptr)
		return {};

	if (!WinHttpGetProxyForUrl(winhttp_session, std::data(proxies::website_to_ping), &autoproxy_options, &proxy_info))
		return {};

	if (proxy_info.lpszProxyBypass != nullptr)
		GlobalFree(proxy_info.lpszProxyBypass);

	if (proxy_info.lpszProxy == nullptr)
		return {};

	std::wstring wide_proxy_address{ proxy_info.lpszProxy };
	GlobalFree(proxy_info.lpszProxy);

	const auto size = WideCharToMultiByte(CP_UTF8, 0, &wide_proxy_address.at(0), static_cast<int>(std::size(wide_proxy_address)), nullptr, 0, nullptr, nullptr);
	if (size <= 0)
		return {};

	std::string proxy_address(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wide_proxy_address.at(0), static_cast<int>(std::size(wide_proxy_address)), &proxy_address.at(0), size, nullptr, nullptr);

	return std::string{ "http://" + proxy_address };
}
