#pragma once
#include <algorithm>
#include <cassert>
#include <ranges>

namespace ranges
{

/**
 * @brief Copy from src range to the end of dest
 *
 * @param dest destination container
 * @param src range to copy
 *
 * @warning src must not me empty
 */
inline void append_range(auto &&dest, const std::ranges::sized_range auto &src)
{
	assert(src.size() != 0 && "Attempt to append empty range");

	const uint32_t originalSize = dest.size();
	dest.resize(dest.size() + src.size());

	std::copy(src.begin(), src.end(), dest.begin() + originalSize);
}

} // namespace ranges
