
#ifndef LAZYJSON_CONCEPT_HPP
#define LAZYJSON_CONCEPT_HPP

#include <concepts>
#include <string_view>

namespace lazy {

template<class T, class CharT>
concept string_view_convertible = std::convertible_to<T, std::basic_string_view<CharT>> && !std::convertible_to<T, const CharT*>;

} // namespace lazy



#endif // LAZYJSON_CONCEPT_HPP