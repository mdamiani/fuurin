/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FUURIN_LRUCACHE_H
#define FUURIN_LRUCACHE_H

#include <initializer_list>
#include <list>
#include <optional>
#include <unordered_map>
#include <utility>


namespace fuurin {

/**
 * \brief Class implementing a Least Recently Used Cache.
 *
 * Items are stored in a list, ordered from the least recently used item
 * (front), to the most recently used one (back).
 *
 * The cache has a capacity parameter. When the size of the cache reaches the
 * capacity limit, then upon insertion of a new item, the least recently used
 * one will be removed.
 */
template<typename K, typename V>
class LRUCache
{
public:
    ///< List item alias.
    using Item = std::pair<K, V>;
    ///< List alias.
    using List = std::list<Item>;


public:
    /**
     * \brief Initializes a cache with infinite capacity.
     */
    LRUCache()
        : capacity_{0}
    {
    }


    /**
     * \brief Initializes a cache with limited capacity.
     *
     * Creates a cache object with the passed capacity.
     *
     * \param[in] sz Cache capacity.
     */
    LRUCache(size_t sz)
        : capacity_{sz}
    {
    }


    /**
     * \brief Constructor with initializer.
     *
     * Sets capacity equal to the provided number of items
     * and puts them into the cache.
     *
     * \param[in] l List of items.
     */
    LRUCache(const std::initializer_list<Item>& l)
        : capacity_{l.size()}
    {
        for (const auto& v : l)
            put(v.first, v.second);
    }


    /**
     * \brief Destructor.
     */
    virtual ~LRUCache() noexcept = default;


    /**
     * \return Cache capacity.
     */
    size_t capacity() const noexcept
    {
        return capacity_;
    }


    /**
     * \return Cache size.
     */
    size_t size() const
    {
        return list_.size();
    }


    /**
     * \return Whether cache size is zero.
     */
    bool empty() const
    {
        return size() == 0;
    }


    /**
     * \brief Clears this cache.
     */
    void clear()
    {
        list_.clear();
        map_.clear();
    }


    /**
     * \brief Puts or updates an item into the cache.
     *
     * The passed item is set as the most recently used.
     *
     * In case the same key is already present in cache,
     * then its usage is updated and moved to the end
     * of the list.
     *
     * In case the key needs to be added and the capacity
     * was already reached, then the least recently used
     * item will be removed.
     *
     * \param[in] k Item's key.
     * \param[in] v Item's value.
     *
     * \return An iterator to the new inserted item,
     *         that is the last element of the \ref list().
     */
    typename List::iterator put(const K& k, const V& v)
    {
        if (auto it = map_.find(k); it == map_.end()) {
            if (capacity_ > 0 && map_.size() >= capacity_) {
                const auto& el = list_.front();
                map_.erase(el.first);
                list_.pop_front();
            }
        } else {
            list_.erase(it->second);
        }

        list_.emplace_back(k, v);
        const auto it = --list_.end();
        map_[k] = it;
        return it;
    }


    /**
     * \brief Gets an item from the cache.
     *
     * If the passed key is contained in the cache,
     * then it is removed and returned.
     *
     * \param[in] k Item's key.
     *
     * \return Optionally the removed item.
     */
    std::optional<Item> get(const K& k)
    {
        const auto it = map_.find(k);

        if (it == map_.end())
            return {};

        const auto vit = it->second;
        const Item ret{*vit};

        map_.erase(k);
        list_.erase(vit);

        return {ret};
    }


    /**
     * \brief Finds an item by its key.
     *
     * The cache list is not modified.
     *
     * \param[in] k Item's key.
     *
     * \return An iterator to the cache list.
     */
    ///@{
    typename List::iterator find(const K& k)
    {
        const auto it = map_.find(k);
        if (it == map_.end())
            return list_.end();

        return it->second;
    }

    typename List::const_iterator find(const K& k) const
    {
        const auto it = map_.find(k);
        if (it == map_.end())
            return list_.end();

        return it->second;
    }
    ///@}


    /**
     * \return The cache list.
     */
    ///@{
    List& list()
    {
        return list_;
    }

    const List& list() const
    {
        return list_;
    }
    ///@}


    /**
     * \brief Comparison operator.
     *
     * \param[in] rhs Other cache to compare.
     *
     * \return \c true in case the cache list has the same items and the same ordering.
     */
    ///@{
    bool operator==(const LRUCache<K, V>& rhs) const
    {
        return list_ == rhs.list_;
    }

    bool operator!=(const LRUCache<K, V>& rhs) const
    {
        return !(*this == rhs);
    }
    ///@}


private:
    const size_t capacity_;                              ///< Cache capacity, i.e. max size.
    List list_;                                          ///< List of cache items.
    std::unordered_map<K, typename List::iterator> map_; ///< Items position in cache list.
};

} // namespace fuurin

#endif // FUURIN_LRUCACHE_H
