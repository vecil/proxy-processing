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
	using namespace std::literals;

	auto create_registry_values(const std::span<const std::wstring_view>, const std::wstring_view) -> bool;
	auto delete_registry_values(const std::span<const std::wstring_view>) -> bool;

	namespace environment_variables
	{
		constexpr auto path{ L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"sv };
		constexpr auto values{ std::array{ L"FTP_PROXY"sv, L"HTTP_PROXY"sv, L"HTTPS_PROXY"sv } };
	}
}

namespace proxy
{
	using namespace std::literals;

	auto retrieve_proxy_address() -> std::wstring;
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
	const auto proxy_address{ proxy::retrieve_proxy_address() };

	if (std::empty(proxy_address))
		registry::delete_registry_values(registry::environment_variables::values);
	else
		registry::create_registry_values(registry::environment_variables::values, proxy_address);
}

auto registry::create_registry_values(const std::span<const std::wstring_view> values, const std::wstring_view data) -> bool
{
	if (std::empty(values) || std::empty(data))
		return false;

	HKEY hkey{};
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, std::data(registry::environment_variables::path), 0, KEY_SET_VALUE, &hkey) != ERROR_SUCCESS)
		return false;

	// Include the null character and compute the size, as a wchar_t's size is implementation-defined (16 bits on Windows).
	const auto data_size{ static_cast<DWORD>((std::size(data) + 1) * sizeof(data[0])) };

	for (const auto value : values)
		RegSetKeyValue(hkey, nullptr, std::data(value), REG_SZ, std::data(data), data_size);

	RegCloseKey(hkey);
	return true;
}

auto registry::delete_registry_values(const std::span<const std::wstring_view> values) -> bool
{
	if (std::empty(values))
		return false;

	HKEY hkey{};
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, std::data(registry::environment_variables::path), 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &hkey) != ERROR_SUCCESS)
		return false;

	for (const auto value : values)
		if (RegGetValue(hkey, nullptr, std::data(value), RRF_RT_REG_SZ, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
			RegDeleteKeyValue(hkey, nullptr, std::data(value));

	RegCloseKey(hkey);
	return true;
}

auto proxy::retrieve_proxy_address() -> std::wstring
{
	const auto winhttp_session{ WinHttpOpen(nullptr, WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0) };
	if (winhttp_session == nullptr)
		return {};

	const auto did_get_proxy{ WinHttpGetProxyForUrl(winhttp_session, std::data(proxy::website_to_ping), &autoproxy_options, &proxy_info) };
	WinHttpCloseHandle(winhttp_session);

	if (!did_get_proxy)
		return {};

	if (proxy_info.lpszProxyBypass != nullptr)
		GlobalFree(proxy_info.lpszProxyBypass);

	if (proxy_info.lpszProxy == nullptr)
		return {};

	const auto proxy_address{ L"http://" + std::wstring{ proxy_info.lpszProxy } };
	GlobalFree(proxy_info.lpszProxy);

	return proxy_address;
}
