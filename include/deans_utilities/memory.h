#pragma once
#include "stdint.h"
#include "util/ranges.h"
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace memory
{
/*
	A memory range is an arbitrary range of contigious memory
	A memory region is a allocated memory range
*/

/**
 * @brief Get size of a buffer if you align the end
 * 
 * @param size size if buffer
 * @param alignment alignment of object
 * @return std::size_t aligned size
 */
inline std::size_t aligned_size(std::size_t size, std::size_t alignment)
{
	return ((size + (alignment - 1)) & ~(alignment - 1));
}

// Common class for memory operations
struct region
{
	region()
		: startPtr(nullptr), endPtr(nullptr) {}

	region(uint8_t *start, uint8_t *end)
		: startPtr(start), endPtr(end)
	{
	}

	region(uint8_t *start, uint32_t size)
		: region(start, start + size)
	{
	}

	region(const region &other)
		: region(other.startPtr, other.endPtr)
	{
	}

	region(region &&other)
		: region(other.startPtr, other.endPtr)
	{
		other.startPtr = nullptr;
		other.endPtr = nullptr;
	}

	region &operator=(const memory::region &other)
	{
		startPtr = other.startPtr;
		endPtr = other.endPtr;
		return *this;
	}

	region &operator=(memory::region &&other)
	{
		startPtr = other.startPtr;
		endPtr = other.endPtr;

		other.startPtr = nullptr;
		other.endPtr = nullptr;

		return *this;
	}

	uint8_t *startPtr;
	uint8_t *endPtr;

	inline uint32_t size() const
	{
		return endPtr - startPtr;
	}
};

// This object allows for writing to a memory region
struct writer : public region
{
	writer()
		: region(), writeEnd(nullptr) {}

	writer(region destination, uint32_t amount)
		: region(destination), writeEnd(destination.startPtr + amount)
	{
		assert(writeEnd <= region::endPtr && "First write in memory::writer exceeds memory region size");
	}

	bool has_space(uint32_t amount) const
	{
		writeEnd = region::startPtr + amount;
		return writeEnd <= region::endPtr;
	}

	void write(const void *src, uint32_t amount)
	{
		memcpy(region::startPtr, src, amount);
		region::startPtr = writeEnd;
	}

  private:
	mutable uint8_t *writeEnd = nullptr;
};

namespace unsafe
{
	/**
	 * @brief This is an unsafe memory writer, please avoid using it
	 * Internally it is just a wrapper for a pointer,
	 */
	struct writer
	{
		writer()
			: writeDest(nullptr) {}

		writer(uint8_t *start)
			: writeDest(start)
		{
		}

		/*! @brief Check if writer is valid*/
		explicit operator bool() const { return writeDest; }

		/*! @brief write to the pointer and advance it*/
		void write(const void *src, uint32_t amount)
		{
			memcpy(writeDest, src, amount);
			writeDest += amount;
		}
		
		/*! @brief Write a string to the pointer and advance it*/
		void write_str(const char *src, uint32_t amount)
		{
			std::strncpy((char*)writeDest, src, amount);
			writeDest += amount;
		}
		/**
		 * @brief Get a pointer to where this writer is pointing to
		 * 
		 */
		uint8_t *get_pointer() const
		{
			return writeDest;
		}

	  private:
		uint8_t *writeDest = nullptr;
	};

} // namespace unsafe

/**
 * @brief A memory block
 *
 */
struct resource
{
	/*! @brief unsigned integer type*/
	using size_type = std::size_t;

	/*! @brief Non-allocating default constructor*/
	resource()
		: resource(nullptr, 0)
	{
	}

	/**
	 * @brief Allocate a region of specified size
	 *
	 * @param size Size in bytes of the allocated region
	 */
	resource(size_type size)
		: resource(new uint8_t[size], size)
	{
		assert(size != 0);
	}

	/*! @brief Move constructor */
	resource(resource &&other)
		: resource(other.ptr, other.size)
	{
		other.ptr = nullptr;
		other.size = 0;
	}

	/*! @brief Deleted copy constructor, delete to avoid double free */
	resource(const resource &other) = delete;

	/*! @brief Copy assignment operator, delete to avoid double free*/
	resource &operator=(const resource &other) = delete;

	/*! @brief Move Assignment, deletes previous contents if necessary */
	resource &operator=(resource &&other)
	{
		if (ptr)
			delete ptr;

		ptr = other.ptr;
		size = other.size;

		other.ptr = nullptr;
		other.size = 0;

		return *this;
	}

	/*! @brief Get a handle for the region*/
	inline operator region() const
	{
		return region(ptr, ptr + size);
	}

	/**
	 * @brief Get a handle for an unsafe memory writer, avoid using this
	 *
	 * This should only really be used with fixed size containers, like string_array
	 * @warning This is a memory unsafe function and probably shouldn't be used
	 */
	inline operator unsafe::writer() const
	{
		return unsafe::writer(ptr);
	}

	/*! @brief Deconstructor*/
	virtual ~resource()
	{
		if (ptr)
			delete ptr;
	}

  private:
	/*! @brief Internal Constructor to help with member assignment */
	resource(uint8_t *ptr, size_type size)
		: ptr(ptr), size(size)
	{
	}

	uint8_t *ptr;
	size_type size;
};

struct resource_allocator
{
	using value_type = resource;

	/**
	 * @brief Bulk allocate memory resources
	 *
	 * @param resourceSize Resource Size in bytes
	 * @param amount Number of resource to allocate
	 * @param dest Destination container for those resources
	 */
	void alloc_objects(uint32_t resourceSize, uint32_t amount, auto &&dest)
	{
		const uint32_t oldRegionSize = regions.size();
		regions.resize(regions.size() + amount);

		auto newRegions = std::ranges::subrange(regions.begin() + oldRegionSize, regions.end());

		for (auto &element : newRegions)
			element = std::move(resource(resourceSize));

		ranges::append_range(dest, newRegions);
	}

  private:
	std::vector<resource> regions;
};

/**
 * @brief A pool of memory allocations
 *
 * It's primary purpose it to allow for quick access to cache memory to then copy to and from gpu
 *
 * @warning Releases all allocated regions on deconstruction
 */
struct resource_pool
{
	/**
	 * @brief Constructor
	 * @param resourceSize The size of allocated regions, this should not be zero
	 * @param allocationAmount Number of regions to allocate when empty, this should not be zero
	 *
	 * @warning Neither parameter should be zero
	 */
	resource_pool(uint32_t resourceSize, uint32_t allocationAmount = 1)
		: resourceSize(resourceSize),
		  allocationAmount(allocationAmount)
	{
		assert(resourceSize != 0 && "Resource size cannot be zero");
		assert(allocationAmount != 0 && "Allocation amount cannot be zero");

		alloc_regions(allocationAmount);
	}

	/*! @brief Deconstructor, releases all allocated resources */
	~resource_pool() {}

	/*! @brief Acquire a region from the pool*/
	region acquire()
	{
		if (available.empty())
			alloc_regions(allocationAmount);

		region output = available.back();
		available.pop_back();
		return output;
	}

	/**
	 * @brief Bulk acquire a number of regions
	 *
	 * @param count Number of regions needed
	 * @return std::vector<region> acquired regions
	 *
	 * @todo Determine if copy to member available and then to regions has a performance impact
	 */
	std::vector<region> acquire(uint32_t count)
	{
		assert(count != 0 && "Cannot bulk acquire zero regions");

		std::vector<region> regions(count);

		if (available.size() < count)
			alloc_regions(allocationAmount + count - available.size());

		const uint32_t copyStart = available.size() - count;

		std::copy(available.begin() + copyStart, available.end(), regions.begin());

		return std::move(regions);
	}

	/**
	 * @brief Release a reserved region
	 *
	 * @param target region to be released
	 *
	 * @warning this does not check if region is acquired
	 */
	void release(region target)
	{
		available.push_back(target);
	}

	/**
	 * @brief Bulk release regions
	 *
	 * @param regionContainer A container of regions to be released
	 */
	void release(auto &regionContainer)
	{
		assert(regionContainer.size() != 0 && "Region Container size cannot be zero");
		ranges::append_range(available, regionContainer);
	}

  private:
	/**
	 * @brief internal helper function for bulk allocating regions
	 *
	 * @param allocationAmount number of regions to allocate
	 */
	void alloc_regions(uint32_t allocationAmount)
	{
		uint32_t availableSize = available.size();
		allocator.alloc_objects(resourceSize, allocationAmount, available);
	}

	uint32_t resourceSize;
	uint32_t allocationAmount;
	std::vector<region> available;

	resource_allocator allocator;
};

// TODO implement memory::ranges::writer
namespace ranges
{
	struct writer
	{
		resource_pool &resourcePool;
	};
} // namespace ranges

} // namespace memory