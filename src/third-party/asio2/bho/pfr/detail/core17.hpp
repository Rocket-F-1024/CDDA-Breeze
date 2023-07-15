// Copyright (c) 2016-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#ifndef BHO_PFR_DETAIL_CORE17_HPP
#define BHO_PFR_DETAIL_CORE17_HPP

#include <asio2/bho/pfr/detail/core17_generated.hpp>
#include <asio2/bho/pfr/detail/fields_count.hpp>
#include <asio2/bho/pfr/detail/for_each_field_impl.hpp>
#include <asio2/bho/pfr/detail/rvalue_t.hpp>

namespace bho { namespace pfr { namespace detail {

#ifndef _MSC_VER // MSVC fails to compile the following code, but compiles the structured bindings in core17_generated.hpp
struct do_not_define_std_tuple_size_for_me {
    bool test1 = true;
};

template <class T>
constexpr bool do_structured_bindings_work() noexcept { // ******************************************* IN CASE OF ERROR READ THE FOLLOWING LINES IN bho/pfr/detail/core17.hpp FILE:
    T val{};
    const auto& [a] = val; // ******************************************* IN CASE OF ERROR READ THE FOLLOWING LINES IN bho/pfr/detail/core17.hpp FILE:

    /****************************************************************************
    *
    * It looks like your compiler or Standard Library can not handle C++17
    * structured bindings.
    *
    * Workaround: Define BHO_PFR_USE_CPP17 to 0
    * It will disable the C++17 features for BHO.PFR library.
    *
    * Sorry for the inconvenience caused.
    *
    ****************************************************************************/

    return a;
}

static_assert(
    do_structured_bindings_work<do_not_define_std_tuple_size_for_me>(),
    "====================> BHO.PFR: Your compiler can not handle C++17 structured bindings. Read the above comments for workarounds."
);
#endif // #ifndef _MSC_VER

template <class T>
constexpr auto tie_as_tuple(T& val) noexcept {
  static_assert(
    !std::is_union<T>::value,
    "====================> BHO.PFR: For safety reasons it is forbidden to reflect unions. See `Reflection of unions` section in the docs for more info."
  );
  typedef size_t_<bho::pfr::detail::fields_count<T>()> fields_count_tag;
  return bho::pfr::detail::tie_as_tuple(val, fields_count_tag{});
}

template <class T, class F, std::size_t... I>
void for_each_field_dispatcher(T& t, F&& f, std::index_sequence<I...>) {
    static_assert(
        !std::is_union<T>::value,
        "====================> BHO.PFR: For safety reasons it is forbidden to reflect unions. See `Reflection of unions` section in the docs for more info."
    );
    std::forward<F>(f)(
        detail::tie_as_tuple(t)
    );
}

}}} // namespace bho::pfr::detail

#endif // BHO_PFR_DETAIL_CORE17_HPP
