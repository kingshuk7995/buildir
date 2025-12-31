#pragma once

#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utils.hpp>
#include <vector>

namespace serde {

// concepts

template <typename T>
concept IsMap =
    requires {
      typename T::key_type;
      typename T::mapped_type;
      typename T::value_type;
    } &&
    std::same_as<typename T::value_type, std::pair<const typename T::key_type,
                                                   typename T::mapped_type>>;

// primitives

template <typename T>
  requires std::is_trivially_copyable_v<T>
inline std::array<std::byte, sizeof(T)> serialize_value(T value) noexcept {
  if constexpr (std::is_integral_v<T> && sizeof(T) > 1) {
    if constexpr (std::endian::native == std::endian::big) {
      value = std::byteswap(value);
    }
  }
  return std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
}

inline void serialize_string(const std::string &s,
                             std::vector<std::byte> &dest) {
  if (s.size() > std::numeric_limits<uint32_t>::max())
    fatal("string too large");

  auto sz = serialize_value<uint32_t>(static_cast<uint32_t>(s.size()));
  dest.insert(dest.end(), sz.begin(), sz.end());

  const auto *p = reinterpret_cast<const std::byte *>(s.data());
  dest.insert(dest.end(), p, p + s.size());
}

// vector<T>

template <typename T>
  requires(std::is_trivially_copyable_v<T> || std::same_as<T, std::string>)
void serialize_vec(const std::vector<T> &v, std::vector<std::byte> &dest) {
  if (v.size() > std::numeric_limits<uint32_t>::max())
    fatal("vector too large");

  auto sz = serialize_value<uint32_t>(static_cast<uint32_t>(v.size()));
  dest.insert(dest.end(), sz.begin(), sz.end());

  for (const T &x : v) {
    if constexpr (std::same_as<T, std::string>) {
      serialize_string(x, dest);
    } else {
      auto bytes = serialize_value(x);
      dest.insert(dest.end(), bytes.begin(), bytes.end());
    }
  }
}

// vector<vector<T>>...

template <typename T>
void serialize_vec(const std::vector<std::vector<T>> &v,
                   std::vector<std::byte> &dest) {
  if (v.size() > std::numeric_limits<uint32_t>::max())
    fatal("outer vector too large");

  auto sz = serialize_value<uint32_t>(static_cast<uint32_t>(v.size()));
  dest.insert(dest.end(), sz.begin(), sz.end());

  for (const auto &inner : v) {
    serialize_vec(inner, dest);
  }
}

// map<K,V>

template <typename T>
  requires IsMap<T>
void serialize_map(const T &m, std::vector<std::byte> &dest) {
  using K = typename T::key_type;
  using V = typename T::mapped_type;

  if (m.size() > std::numeric_limits<uint32_t>::max())
    fatal("map too large");

  auto sz = serialize_value<uint32_t>(static_cast<uint32_t>(m.size()));
  dest.insert(dest.end(), sz.begin(), sz.end());

  for (const auto &[k, v] : m) {
    if constexpr (std::same_as<K, std::string>) {
      serialize_string(k, dest);
    } else {
      auto kb = serialize_value(k);
      dest.insert(dest.end(), kb.begin(), kb.end());
    }

    if constexpr (std::same_as<V, std::string>) {
      serialize_string(v, dest);
    } else {
      auto vb = serialize_value(v);
      dest.insert(dest.end(), vb.begin(), vb.end());
    }
  }
}

} // namespace serde

namespace serde {

// primitives

template <typename T>
  requires std::is_trivially_copyable_v<T>
inline T deserialize_value(const std::byte *&ptr) {
  auto arr = std::bit_cast<T>(
      *reinterpret_cast<const std::array<std::byte, sizeof(T)> *>(ptr));
  ptr += sizeof(T);

  if constexpr (std::is_integral_v<T> && sizeof(T) > 1) {
    if constexpr (std::endian::native == std::endian::big) {
      arr = std::byteswap(arr);
    }
  }
  return arr;
}

inline std::string deserialize_string(const std::byte *&ptr) {
  uint32_t sz = deserialize_value<uint32_t>(ptr);
  std::string s(reinterpret_cast<const char *>(ptr), sz);
  ptr += sz;
  return s;
}

// vector<T> and nested ones

template <typename T> struct is_std_vector : std::false_type {};

template <typename T, typename Alloc>
struct is_std_vector<std::vector<T, Alloc>> : std::true_type {};

template <typename T>
inline constexpr bool is_std_vector_v = is_std_vector<T>::value;

template <typename T> struct vector_inner;

template <typename T, typename Alloc>
struct vector_inner<std::vector<T, Alloc>> {
  using type = T;
};

template <typename T> T deserialize_vec(const std::byte *&ptr);

template <typename T>
  requires std::is_trivially_copyable_v<T>
T deserialize_vec(const std::byte *&ptr) {
  return deserialize_value<T>(ptr);
}

template <>
inline std::string deserialize_vec<std::string>(const std::byte *&ptr) {
  return deserialize_string(ptr);
}

template <typename T>
  requires is_std_vector_v<T>
T deserialize_vec(const std::byte *&ptr) {
  using Inner = typename vector_inner<T>::type;

  uint32_t sz = deserialize_value<uint32_t>(ptr);
  T vec;
  vec.reserve(sz);

  for (uint32_t i = 0; i < sz; ++i) {
    vec.emplace_back(deserialize_vec<Inner>(ptr));
  }
  return vec;
}

// map<K,V>

template <typename T>
  requires serde::IsMap<T>
T deserialize_map(const std::byte *&ptr) {
  using K = typename T::key_type;
  using V = typename T::mapped_type;

  uint32_t sz = deserialize_value<uint32_t>(ptr);
  T m;
  m.reserve(sz);

  for (uint32_t i = 0; i < sz; ++i) {
    K k = [&] {
      if constexpr (std::same_as<K, std::string>) {
        return deserialize_string(ptr);
      } else {
        return deserialize_value<K>(ptr);
      }
    }();

    V v = [&] {
      if constexpr (std::same_as<V, std::string>) {
        return deserialize_string(ptr);
      } else {
        return deserialize_value<V>(ptr);
      }
    }();

    m.emplace(std::move(k), std::move(v));
  }
  return m;
}

} // namespace serde
