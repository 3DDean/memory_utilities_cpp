#pragma once
#include "stdint.h"
#include <vector>
#include <stdexcept>
#include <cstring>

namespace memory
{
/*
	A memory range is an arbitrary range of contigious memory
	A memory region is a allocated memory range
*/

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
		std::swap(startPtr, other.startPtr);
		std::swap(endPtr, other.endPtr);
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
		: region() {}

	writer(region destination)
		: region(destination)
	{
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

// This object manages allocation and deallocation of memory regions
struct resource : public region
{
	resource()
		: region{nullptr, nullptr}
	{
	}

	resource(uint32_t size)
		: resource(new uint8_t[size], size)
	{
	}

	resource(resource &&other)
		: region(other.startPtr, other.endPtr)
	{
		other.startPtr = nullptr;
		other.endPtr = nullptr;
	}

	resource(const resource &other) = delete;

	virtual ~resource()
	{
		if (region::startPtr)
			delete region::startPtr;
	}

  private:
	resource(uint8_t *ptr, uint32_t size)
		: region(ptr, ptr + size)
	{
	}
};

struct resource_allocator
{
	void alloc_objects(uint32_t resourceSize, uint32_t amount, auto &&destItt)
	{
		regions.reserve(regions.size() + amount);
		auto newRegionItt = regions.end();
		for (size_t i = 0; i < amount; i++)
			regions.emplace_back(resourceSize);

		std::copy(newRegionItt, regions.end(), destItt);
	}

	std::vector<resource> regions;
};

// This object creates a pool of regions to allow for quick
struct resource_pool
{
	// resourceSize and allocationAmount must not be 0
	resource_pool(uint32_t resourceSize, uint32_t allocationAmount = 1)
		: resourceSize(resourceSize),
		  allocationAmount(allocationAmount)
	{
		if (resourceSize == 0)
			throw std::logic_error("A memory resource pool resource size, must not be zero");
		
		if (allocationAmount == 0)
			throw std::logic_error("A memory resource pool resource size, must not be zero");

		alloc_regions();
	}

	~resource_pool() {}
	region reserve_region()
	{
		if (available.empty())
			alloc_regions();

		region output = available.back();
		available.pop_back();
		return output;
	}

	void release_region(region target)
	{
		available.push_back(target);
	}

  private:
	void alloc_regions()
	{
		uint32_t availableSize = available.size();
		available.resize(available.size() + allocationAmount);
		allocator.alloc_objects(resourceSize, allocationAmount, available.begin() + availableSize);
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