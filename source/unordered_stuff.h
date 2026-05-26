#pragma once

// We have this so that anyone who doesn't use it can fallback to std's stuff.
#include <ankerl/unordered_dense.h>
#if defined(ANKERL_UNORDERED_DENSE_H)
template <typename Key, typename T, typename Hash = ankerl::unordered_dense::hash<Key>,
          typename KeyEqual = std::equal_to<Key>,
          typename Allocator = std::allocator<std::pair<Key, T>>>
using unordered_map = ankerl::unordered_dense::map<Key, T, Hash, KeyEqual, Allocator>;

template <typename Key, typename Hash = ankerl::unordered_dense::hash<Key>,
          typename KeyEqual = std::equal_to<Key>,
          typename Allocator = std::allocator<Key>>
using unordered_set = ankerl::unordered_dense::set<Key, Hash, KeyEqual, Allocator>;
#else
#include "unordered_map"
#include "unordered_set"
template <typename Key, typename T, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>,
          typename Allocator = std::allocator<std::pair<Key const, T>>>
using unordered_map = std::unordered_map<Key, T, Hash, KeyEqual, Allocator>;

template <typename Key, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>,
          typename Allocator = std::allocator<Key>>
using unordered_set = std::unordered_set<Key, Hash, KeyEqual, Allocator>;
#endif