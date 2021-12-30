#include <windows.h>
#define WIN32_LEAN_AND_MEAN

#include <iostream>
#include <string>
#include <string_view>

std::uint32_t exec(std::string_view)
{
	return 0;
}

int main()
{
	std::cout << "hello, friend." << std::endl;
}
