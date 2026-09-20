#pragma once

#include <iostream>
#include <format>
#include <string_view>

namespace Revert
{
	template <typename... Args>
	inline void Note(std::format_string<Args...> fmt, Args&&... args)
	{
		std::cout << "[revert] " << std::format(fmt, std::forward<Args>(args)...) << '\n';
	}

	inline void Note(std::string_view text)
	{
		std::cout << "[revert] " << text << '\n';
	}
}
