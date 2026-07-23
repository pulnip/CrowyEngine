// for std::views::enumerate
#pragma once

#include <ranges>

#if !defined(__cpp_lib_ranges_enumerate)

#include <concepts>
#include <iterator>
#include <tuple>
#include <type_traits>
#include <utility>
#include <version>

namespace std
{
    template <std::ranges::input_range R>
        requires std::ranges::view<R>
    class enumerate_view: public std::ranges::view_interface<enumerate_view<R>>{
        R range = R{};
        using Diff = std::ranges::range_difference_t<R>;

        struct iterator{
            std::ranges::iterator_t<R> cur{};
            Diff idx = 0;

            using difference_type = Diff;
            using value_type = std::tuple<Diff, std::ranges::range_value_t<R>>;
            using iterator_concept = std::conditional_t<
                std::ranges::forward_range<R>,
                std::forward_iterator_tag,
                std::input_iterator_tag
            >;

            constexpr auto operator*() const{
                return std::tuple<Diff, std::ranges::range_reference_t<R>>(idx, *cur);
            }

            constexpr iterator& operator++(){
                ++cur;
                ++idx;

                return *this;
            }

            constexpr void operator++(int)
                requires (!std::ranges::forward_range<R>)
            {
                ++*this;
            }

            constexpr iterator operator++(int)
                requires std::ranges::forward_range<R>
            {
                auto tmp = *this;
                ++*this;

                return tmp;
            }

            friend constexpr bool operator==(const iterator& lhs, const iterator& rhs)
                requires std::equality_comparable<std::ranges::iterator_t<R>>
            {
                return lhs.cur == rhs.cur;
            }
        };

        struct sentinel{
            std::ranges::sentinel_t<R> end_{};

            friend constexpr bool operator==(const iterator& it, const sentinel& s){
                return it.cur == s.end_;
            }
        };

    public:
        enumerate_view()
            requires std::default_initializable<R>
        = default;
        constexpr explicit enumerate_view(R base)
            : range(std::move(base)){}

        constexpr R base() const&
            requires std::copy_constructible<R>
        {
            return range;
        }

        constexpr R base() &&{
            return std::move(range);
        }

        constexpr iterator begin(){
            return iterator{std::ranges::begin(range), Diff(0)};
        }
        constexpr sentinel end(){
            return sentinel{std::ranges::end(range)};
        }

        constexpr auto size()
            requires std::ranges::sized_range<R>
        {
            return std::ranges::size(range);
        }
    };

    template <class R>
    enumerate_view(R&&) -> enumerate_view<std::views::all_t<R>>;

    namespace detail {
        struct enumerate_fn: public std::ranges::range_adaptor_closure<enumerate_fn>{
            template <std::ranges::viewable_range R>
            constexpr auto operator()(R&& r) const{
                return enumerate_view<std::views::all_t<R>>(
                    std::views::all(std::forward<R>(r))
                );
            }
        };
    }

    namespace ranges::views{
        inline constexpr detail::enumerate_fn enumerate{};
    }
}

#endif
