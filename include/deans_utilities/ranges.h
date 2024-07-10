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
template <typename DestinationT, typename SourceT>
inline void append_range(DestinationT &&dest, const SourceT &src) requires
	std::indirectly_copyable<std::ranges::iterator_t<SourceT>, std::ranges::iterator_t<DestinationT>> &&
	std::ranges::sized_range<DestinationT> && std::ranges::random_access_range<SourceT> &&
	std::ranges::sized_range<SourceT> && std::ranges::input_range<SourceT>
{
	assert(src.size() != 0 && "Attempt to append empty range");

	const uint32_t originalSize = dest.size();
	dest.resize(dest.size() + src.size());

	std::copy(src.begin(), src.end(), dest.begin() + originalSize);
}

} // namespace ranges
