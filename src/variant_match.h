#pragma once

#include <experimental/variant.hpp>
#include <type_traits>

// Add missing variant_size and variant_size_v (Submitted a PR for this)
//namespace std {
//	namespace experimental {
//		template <class VariantType>
//		struct variant_size
//		{
//			static constexpr auto value = std::experimental::fundamentals_v3::tuple_size<VariantType>::value;
//		};
//		template <class VariantType>
//		constexpr std::size_t variant_size_v = variant_size<VariantType>::value;
//	}
//}

namespace match_detail
{
	using namespace std::experimental;
	using namespace std;

	template <int variant_index, typename VariantType, typename OneFunc>
	constexpr void match(VariantType&& variant, OneFunc func)
	{
		assert(variant.index() == variant_index);
		func(get<variant_index>(std::forward<VariantType>(variant)));
	}

	template <int variant_index, typename VariantType, typename FirstFunc, typename SecondFunc, typename... Rest>
	constexpr void match(VariantType&& variant, FirstFunc func1, SecondFunc func2, Rest... funcs)
	{
		if (variant.index() == variant_index)
		{
			match<variant_index>(variant, func1); // Call single func version
			return;
		}
		return match<variant_index + 1>(std::forward<VariantType>(variant), func2, funcs...);
	}
}

template <typename VariantType, typename... Funcs>
void match(VariantType&& variant, Funcs... funcs)
{
	using VT = std::remove_reference_t<VariantType>;
	static_assert(sizeof...(funcs) == std::experimental::variant_size_v<VT>, "Number of functions must match number of variant types");
	match_detail::match<0>(std::forward<VariantType>(variant), funcs...);
}
