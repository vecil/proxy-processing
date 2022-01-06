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

using namespace std::literals;

namespace registry
{
	auto create_registry_values(const std::span<const std::string_view>&, std::string_view) -> bool;
	auto delete_registry_values(const std::span<const std::string_view>&) -> bool;

	namespace environment_variables
	{
		constexpr auto path{ "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"sv };
		constexpr auto values{ std::to_array({ "FTP_PROXY"sv, "HTTP_PROXY"sv, "HTTPS_PROXY"sv }) };
	}
}

namespace proxies
{
	auto retrieve_proxy_address() -> std::string;
	constexpr auto website_to_ping{ L"https://cloudflare.com"sv };

	WINHTTP_AUTOPROXY_OPTIONS autoproxy_options
	{
		.dwFlags{ WINHTTP_AUTOPROXY_AUTO_DETECT },
		.dwAutoDetectFlags{ WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A }
	};
	WINHTTP_PROXY_INFO proxy_info{};
}

auto main() -> int
{
	const auto proxy_address{ proxies::retrieve_proxy_address() };

	if (std::empty(proxy_address))
		registry::delete_registry_values(registry::environment_variables::values);
	else
		registry::create_registry_values(registry::environment_variables::values, proxy_address);
}

auto registry::create_registry_values(const std::span<const std::string_view>& values, std::string_view data) -> bool
{
	if (std::empty(values) || std::empty(data))
		return false;

	auto hkey{ HKEY{} };
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

	auto hkey{ HKEY{} };
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
	const auto winhttp_session{ WinHttpOpen(nullptr, WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0) };
	if (winhttp_session == nullptr)
		return {};

	if (!WinHttpGetProxyForUrl(winhttp_session, std::data(proxies::website_to_ping), &autoproxy_options, &proxy_info))
		return {};
	WinHttpCloseHandle(winhttp_session);

	if (proxy_info.lpszProxyBypass != nullptr)
		GlobalFree(proxy_info.lpszProxyBypass);

	if (proxy_info.lpszProxy == nullptr)
		return {};

	const auto wide_proxy_address{ std::wstring{ proxy_info.lpszProxy } };
	GlobalFree(proxy_info.lpszProxy);

	const auto size{ WideCharToMultiByte(CP_UTF8, 0, &wide_proxy_address.at(0), static_cast<int>(std::size(wide_proxy_address)), nullptr, 0, nullptr, nullptr) };
	if (size <= 0)
		return {};

	auto proxy_address{ std::string(size, 0) };
	WideCharToMultiByte(CP_UTF8, 0, &wide_proxy_address.at(0), static_cast<int>(std::size(wide_proxy_address)), &proxy_address.at(0), size, nullptr, nullptr);

	return std::string{ "http://" + proxy_address };
}
