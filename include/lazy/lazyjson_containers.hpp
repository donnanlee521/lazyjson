#ifndef LAZYJSON_CONTAINERS_HPP
#define LAZYJSON_CONTAINERS_HPP

#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <format>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "lazy/lazyjson_concept.hpp"

namespace lazy {

template <typename KT, typename VT>
class json_ordered_dict_iterator;

template <typename KT, typename VT>
  requires(string_view_convertible<KT, char>)
class json_ordered_dict {
 public:
  using key_type = KT;
  using mapped_type = VT;

  using key_pointer_type = const KT*;
  using mapped_pointer_type = VT*;
  using mapped_const_pointer_type = const VT*;
  using value_type = std::pair<key_type, mapped_type>;
  using key_container_type = std::vector<key_type>;
  using value_container_type = std::vector<mapped_type>;
  using size_type = std::size_t;
  using iterator_type = json_ordered_dict_iterator<key_type, mapped_type>;
  using const_iterator_type =
      json_ordered_dict_iterator<key_type, const mapped_type>;

 private:
  key_container_type k_;
  value_container_type v_;

  decltype(auto) lower_bound(std::string_view key) {
    return std::ranges::lower_bound(this->k_, key,
                                    std::less<std::string_view>{});
  }

  decltype(auto) lower_bound(std::string_view key) const {
    return std::ranges::lower_bound(this->k_, key,
                                    std::less<std::string_view>{});
  }

  mapped_type& get_val(std::string_view key) {
    auto iter = this->lower_bound(key);
    if (iter == this->k_.end() || *iter != key) {
      throw std::invalid_argument{std::format("invalid key {}", key)};
    }
    return *std::next(this->v_.begin(), std::distance(this->k_.begin(), iter));
  }

  mapped_type const& get_val(std::string_view key) const {
    auto iter = this->lower_bound(key);
    if (iter == this->k_.end() || *iter != key) {
      throw std::invalid_argument{std::format("invalid key {}", key)};
    }
    return *std::next(this->v_.begin(), std::distance(this->k_.begin(), iter));
  }

  static iterator_type make_dict_iter(
      typename key_container_type::iterator kiter,
      typename value_container_type::iterator viter) noexcept {
    return iterator_type{kiter.base(), viter.base()};
  }

  static const_iterator_type make_dict_iter(
      typename key_container_type::const_iterator kiter,
      typename value_container_type::const_iterator viter) noexcept {
    return const_iterator_type{kiter.base(), viter.base()};
  }

 public:
  json_ordered_dict() noexcept = default;
  json_ordered_dict(std::initializer_list<value_type> _li) : k_{}, v_{} {}

  explicit json_ordered_dict(std::vector<value_type>&& li) : k_{}, v_{} {
    const std::size_t sz{li.size()};

    std::vector<std::size_t> indices(sz);
    std::iota(indices.begin(), indices.end(), 0lu);

    std::ranges::sort(indices, [&](std::size_t a, std::size_t b) noexcept {
      return li[a].first < li[b].first;
    });

    k_.reserve(sz);
    v_.reserve(sz);
    for (auto idx : indices) {
      auto&& [k, v] = li[idx];
      k_.push_back(std::move(k));
      v_.push_back(std::move(v));
    }
  }

  template <string_view_convertible<char> U, typename... Args>
  std::pair<iterator_type, bool> emplace(U&& key, Args&&... args) {
    auto key_iter = this->lower_bound(key);
    auto val_iter = std::next(v_.begin(), std::distance(k_.begin(), key_iter));
    if (key_iter != this->k_.end() && *key_iter == key) {
      return {make_dict_iter(key_iter, val_iter), false};
    }

    key_iter = this->k_.emplace(key_iter, std::forward<U>(key));
    val_iter = this->v_.emplace(val_iter, std::forward<Args>(args)...);

    return {make_dict_iter(key_iter, val_iter), true};
  }

  iterator_type find(std::string_view key) {
    auto key_iter = this->lower_bound(key);
    auto val_iter = std::next(v_.begin(), std::distance(k_.begin(), key_iter));
    if (key_iter == this->k_.end() || *key_iter != key) {
      return make_dict_iter(this->k_.end(), this->v_.end());
    }
    return make_dict_iter(key_iter, val_iter);
  }

  mapped_type& operator[](std::string_view key) { return this->get_val(key); };
  mapped_type const& operator[](std::string_view key) const {
    return this->get_val(key);
  };
  mapped_type& at(std::string_view key) { return this->get_val(key); };
  mapped_type const& at(std::string_view key) const {
    return this->get_val(key);
  };

  size_type size() const noexcept { return this->k_.size(); }
  void reserve(size_type n) {
    __cplusplus;
    this->k_.reserve(n);
    this->v_.reserve(n);
  }

  iterator_type begin() noexcept {
    return make_dict_iter(this->k_.begin(), this->v_.begin());
  }
  iterator_type end() noexcept {
    return make_dict_iter(this->k_.end(), this->v_.end());
  }
  const_iterator_type begin() const noexcept {
    return make_dict_iter(this->k_.begin(), this->v_.begin());
  }
  const_iterator_type end() const noexcept {
    return make_dict_iter(this->k_.end(), this->v_.end());
  }
};

template <typename KT, typename VT>
class json_ordered_dict_iterator {
  using self_type = json_ordered_dict_iterator<KT, VT>;

 public:
  using iterator_concept = std::random_access_iterator_tag;
  using iterator_category = std::random_access_iterator_tag;
  using key_type = const std::remove_const_t<KT>;
  using key_pointer_type = const key_type*;
  using key_reference_type = const key_type&;

  using mapped_type = VT;
  using mapped_pointer_type = VT*;
  using mapped_reference_type = VT&;

  using value_type = std::pair<key_reference_type, mapped_reference_type>;

  using difference_type = std::ptrdiff_t;

 private:
  key_pointer_type kp_{};
  mapped_pointer_type vp_{};

  struct pair_proxy {
    value_type val_;
    const value_type* operator->() const noexcept { return &val_; }
  };

 public:
  constexpr json_ordered_dict_iterator() noexcept = default;
  constexpr json_ordered_dict_iterator(key_pointer_type kp,
                                       mapped_pointer_type vp) noexcept
      : kp_{kp}, vp_{vp} {}

  value_type operator*() const noexcept { return {*kp_, *vp_}; }
  pair_proxy operator->() const noexcept { return pair_proxy{{*kp_, *vp_}}; }

  self_type& operator++() noexcept {
    ++kp_;
    ++vp_;
    return *this;
  }

  self_type operator++(int) noexcept {
    auto self = *this;
    ++(*this);
    return self;
  }

  self_type& operator+=(difference_type n) noexcept {
    kp_ += n;
    vp_ += n;
    return *this;
  }

  self_type operator+(difference_type n) const noexcept {
    auto cp = *this;
    cp += n;
    return cp;
  }

  self_type& operator--() noexcept {
    --kp_;
    --vp_;
    return *this;
  }

  self_type operator--(int) noexcept {
    auto self = *this;
    --(*this);
    return self;
  }

  self_type& operator-=(difference_type n) noexcept {
    kp_ -= n;
    vp_ -= n;
    return *this;
  }

  self_type operator-(difference_type n) const noexcept {
    auto cp = *this;
    cp -= n;
    return cp;
  }

  bool operator==(self_type const& other) const noexcept {
    return this->kp_ == other.kp_;
  }

  std::strong_ordering operator<=>(self_type const& other) const noexcept {
    return this->kp_ <=> other.kp_;
  }

  value_type operator[](difference_type n) const noexcept {
    return {*kp_, *vp_};
  }

  friend difference_type operator-(self_type const& a,
                                   self_type const& b) noexcept;
  friend self_type operator+(difference_type, self_type) noexcept;
};

template <class KT, class VT>
std::iter_difference_t<json_ordered_dict_iterator<KT, VT>> operator-(
    json_ordered_dict_iterator<KT, VT> const& a,
    json_ordered_dict_iterator<KT, VT> const& b) noexcept {
  return a.kp_ - b.kp_;
}

template <class KT, class VT>
json_ordered_dict_iterator<KT, VT> operator+(
    std::iter_difference_t<json_ordered_dict_iterator<KT, VT>> n,
    json_ordered_dict_iterator<KT, VT> x) noexcept {
  x += n;
  return x;
}

static_assert(
    std::bidirectional_iterator<json_ordered_dict_iterator<std::string, int>>);

};  // namespace lazy

#endif