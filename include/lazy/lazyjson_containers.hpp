#ifndef LAZYJSON_CONTAINERS_HPP
#define LAZYJSON_CONTAINERS_HPP

#include <algorithm>
#include <compare>
#include <format>
#include <functional>
#include <initializer_list>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "lazy/lazyjson_concept.hpp"

namespace lazy {

template <typename KT, typename VT, typename C = std::vector<std::pair<KT, VT>>>
  requires(string_view_convertible<KT, char>)
class json_ordered_dict {
 public:
  using key_type = KT;
  using mapped_type = VT;
  using value_type = std::pair<key_type, mapped_type>;
  using iterator_type = typename C::iterator;
  using const_iterator_type = typename C::const_iterator;

 private:
  C c_;

  iterator_type lower_bound(std::string_view key) {
    return std::ranges::lower_bound(
        this->c_, key, std::less<std::string_view>{}, &value_type::first);
  }

  const_iterator_type lower_bound(std::string_view key) const {
    return std::ranges::lower_bound(
        this->c_, key, std::less<std::string_view>{}, &value_type::first);
  }

  mapped_type& get_val(std::string_view key) {
    auto iter = this->lower_bound(key);
    if (iter == c_.end() || iter->first != key) {
      throw std::invalid_argument{std::format("invalid key {}", key)};
    }
    return iter->second;
  }

  mapped_type const& get_val(std::string_view key) const {
    auto iter = this->lower_bound(key);
    if (iter == c_.end() || iter->first != key) {
      throw std::invalid_argument{std::format("invalid key {}", key)};
    }
    return iter->second;
  }

 public:
  json_ordered_dict() noexcept = default;
  json_ordered_dict(std::initializer_list<value_type> _li) : c_{_li} {
    std::ranges::sort(this->c_, {}, &value_type::first);
  }

  template <string_view_convertible<char> U, typename... Args>
  std::pair<iterator_type, bool> emplace(U&& key, Args&&... args) {
    auto iter = this->lower_bound(key);
    if (iter != this->c_.end() && iter->first == key) {
      return {iter, false};
    }
    iter = this->c_.emplace(iter, std::piecewise_construct,
                            std::forward_as_tuple(std::forward<U>(key)),
                            std::forward_as_tuple(std::forward<Args>(args)...));
    return {iter, true};
  }

  mapped_type& operator[](std::string_view key) { return this->get_val(key); };
  mapped_type const& operator[](std::string_view key) const {
    return this->get_val(key);
  };
  mapped_type& at(std::string_view key) { return this->get_val(key); };
  mapped_type const& at(std::string_view key) const {
    return this->get_val(key);
  };

  std::size_t size() const noexcept { return this->c_.size(); }

  iterator_type begin() noexcept { return this->c_.begin(); }
  iterator_type end() noexcept { return this->c_.end(); }
  const_iterator_type begin() const noexcept { return this->c_.begin(); }
  const_iterator_type end() const noexcept { return this->c_.end(); }
};

};  // namespace lazy

#endif