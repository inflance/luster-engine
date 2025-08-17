#pragma once

#include <cstdint>
#include <bitset>

namespace luster::ecs
{
	using Entity = std::uint32_t; // simple incremental id

	constexpr std::size_t MaxComponents = 64; // can raise if needed
	using ComponentMask = std::bitset<MaxComponents>;

	inline std::size_t nextComponentTypeId()
	{
		static std::size_t counter = 0;
		return counter++;
	}

	template <typename T>
	inline std::size_t componentTypeId()
	{
		static std::size_t id = nextComponentTypeId();
		return id;
	}
}


