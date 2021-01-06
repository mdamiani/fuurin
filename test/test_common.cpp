/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define BOOST_TEST_MODULE common
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <benchmark/benchmark.h>

#include "fuurin/fuurin.h"
#include "fuurin/lrucache.h"

#include <ostream>


using namespace std::literals;

using namespace fuurin;
namespace utf = boost::unit_test;
namespace bdata = utf::data;


BOOST_AUTO_TEST_CASE(libVersion)
{
    char buf[16];

    std::snprintf(buf, sizeof(buf), "%d.%d.%d",
        VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

    BOOST_TEST(VERSION_MAJOR + VERSION_MINOR + VERSION_PATCH != 0);
    BOOST_TEST(std::string(version()) == std::string(buf));
}


/**
 * LRUCache
 */
using CacheT = LRUCache<std::string, int>;

namespace std {
std::ostream& operator<<(std::ostream& os, const CacheT::Item& v)
{
    os << v.first << ":" << v.second;
    return os;
}

std::ostream& operator<<(std::ostream& os, const CacheT::List::iterator& v)
{
    os << *v;
    return os;
}

std::ostream& operator<<(std::ostream& os, const CacheT::List::const_iterator& v)
{
    os << *v;
    return os;
}

std::ostream& operator<<(std::ostream& os, const CacheT& c)
{
    os << "[";
    const char* sep = "";
    for (const auto& v : c.list()) {
        os << sep;
        os << v;
        sep = ", ";
    }
    os << "]";
    return os;
}
} // namespace std


BOOST_AUTO_TEST_CASE(testLRUCache)
{
    // init
    size_t sz = 3;
    CacheT c{sz};

    const auto testCache = [&c, sz](size_t len, const CacheT& e) {
        BOOST_TEST(c.capacity() == sz);
        BOOST_TEST(c.size() == len);
        BOOST_TEST(c.empty() == (len == 0));
        BOOST_TEST(c == e);
    };

    testCache(0, {});

    // comparison
    CacheT e1{{"a", 1}};
    CacheT e2{{"a", 1}, {"b", 2}};
    CacheT e3{{"a", 1}, {"b", 2}};
    BOOST_TEST(e1 != e2);
    BOOST_TEST(e2 == e3);

    // put: add item
    std::string k = "a";
    int v = 1;
    BOOST_TEST((c.put(k, v) != c.list().end()));
    testCache(1, {{"a", 1}});

    c.put("b", 2);
    testCache(2, {{"a", 1}, {"b", 2}});

    // put: update item
    c.put("a", 3);
    testCache(2, {{"b", 2}, {"a", 3}});

    // put: add item, below capacity
    c.put("c", 5);
    testCache(3, {{"b", 2}, {"a", 3}, {"c", 5}});

    // put: update item, below capacity
    c.put("b", 3);
    testCache(3, {{"a", 3}, {"c", 5}, {"b", 3}});

    // put: add item, over capacity
    c.put("d", 6);
    testCache(3, {{"c", 5}, {"b", 3}, {"d", 6}});

    // get: item present
    auto el = c.get("b");
    BOOST_TEST(el.has_value());
    BOOST_TEST(el.value() == CacheT::Item("b", 3));
    testCache(2, {{"c", 5}, {"d", 6}});

    // get: item not present
    el = c.get("x");
    BOOST_TEST(!el.has_value());
    testCache(2, {{"c", 5}, {"d", 6}});

    // find: found
    auto it1 = c.find("d");
    BOOST_TEST(it1 != c.list().end());
    BOOST_TEST(*it1 == CacheT::Item("d", 6));

    // find: const: found
    auto it2 = static_cast<const CacheT*>(&c)->find("c");
    BOOST_TEST(it2 != c.list().end());
    BOOST_TEST(*it2 == CacheT::Item("c", 5));

    // find: not found
    BOOST_TEST(c.find("x") == c.list().end());

    // get: make empty
    c.get("c");
    c.get("d");
    testCache(0, {});


    // infinite capacity
    CacheT c2;
    BOOST_TEST((c2.put("a", 1) != c2.list().end()));
    BOOST_TEST(c2.find("a") != c.list().end());
    BOOST_TEST(c2.size() == 1u);


    // clear cache
    CacheT d1{{"a", 1}, {"b", 2}};
    BOOST_TEST(d1.size() == 2u);
    BOOST_TEST(!d1.empty());
    d1.clear();
    BOOST_TEST(d1.size() == 0u);
    BOOST_TEST(d1.empty());
}
