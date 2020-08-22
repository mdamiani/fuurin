/**
 * Copyright (c) Contributors as noted in the AUTHORS file.
 *
 * This Source Code Form is part of *fuurin* library.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define BOOST_TEST_MODULE logarg
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <benchmark/benchmark.h>

#include "fuurin/logger.h"
#include "log.h"
#include "types.h"

#include <cstdio>
#include <string>
#include <vector>
#include <tuple>
#include <functional>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <utility>
#include <iterator>
#include <string_view>
#include <errno.h>
#include <thread>
#include <ostream>


using namespace std::literals;

using namespace fuurin;
namespace utf = boost::unit_test;
namespace bdata = utf::data;


struct redirect
{
    redirect(std::ostream& oss, std::streambuf* buf)
        : _oss(oss)
        , _old(oss.rdbuf(buf))
    {
    }

    ~redirect()
    {
        _oss.rdbuf(_old);
    }

private:
    std::ostream& _oss;
    std::streambuf* _old;
};


#define psv(sv) static_cast<int>(sv.size()), sv.data()

BOOST_DATA_TEST_CASE(standardLogContentHandler,
    bdata::make({
        log::Logger::debug,
        log::Logger::info,
        log::Logger::warn,
        log::Logger::error,
    }),
    logfn)
{
    std::string expected, out;
    {
        std::stringstream buf;
        const auto& ro = redirect(std::cout, buf.rdbuf());
        const auto& re = redirect(std::cerr, buf.rdbuf());
        UNUSED(ro);
        UNUSED(re);

        log::Logger::installContentHandler(new log::StandardHandler);

        const log::Loc loc{"test_file", 1};
        const log::Arg u[] = {
            log::Arg{"u0"sv, 10},
            log::Arg{20},
        };
        const log::Arg v[] = {
            log::Arg{"v0"sv, "1234"sv},
            log::Arg{},
            log::Arg{u},
        };
        const log::Arg a[] = {
            log::Arg{"a1"sv, 1},
            log::Arg{"a2"sv, 2.2},
            log::Arg{"a3"sv, "three"sv},
            log::Arg{"a4"sv, v},
            log::Arg{4},
        };

        const auto& lvl1 = log::format("%*s: %d, %d",
            psv(u[0].key()), u[0].toInt(), u[1].toInt());

        const auto& lvl2 = log::format("%*s: %*s, <>, %s",
            psv(v[0].key()), psv(v[0].toString()), lvl1.c_str());

        expected = log::format("%*s: %d, %*s: %0.1f, %*s: %*s, %*s: %s, %d",
            psv(a[0].key()), a[0].toInt(),
            psv(a[1].key()), a[1].toDouble(),
            psv(a[2].key()), psv(a[2].toString()),
            psv(a[3].key()), lvl2.c_str(),
            a[4].toInt());

        logfn(loc, a, std::size(a));

        out = buf.str();
        for (const char& c : {'\r', '\n'})
            out.erase(std::remove(out.begin(), out.end(), c), out.end());
    }
    BOOST_TEST(out == expected);
}


BOOST_DATA_TEST_CASE(silentLogContentHandler,
    bdata::make({
        log::Logger::debug,
        log::Logger::info,
        log::Logger::warn,
        log::Logger::error,
    }),
    logfn)
{
    std::stringstream buf;
    {
        const auto ro = redirect(std::cout, buf.rdbuf());
        const auto re = redirect(std::cerr, buf.rdbuf());

        log::Logger::installContentHandler(new log::SilentHandler);

        const log::Loc loc{"test_file", 1};
        const log::Arg args[] = {log::Arg{"test_fun"sv, "test_msg"sv}};

        logfn(loc, args, std::size(args));
    }
    BOOST_TEST(buf.str().empty());
}


BOOST_AUTO_TEST_CASE(formatLog)
{
    BOOST_TEST("test_fun: test_msg1" == log::format("test_fun: %s%d", "test_msg", 1));
    BOOST_TEST("" == log::format("", "aaa", 1));
}


static void testArg(const log::Arg& arg, log::Arg::Type type, std::string_view key, int iv, double dv,
    std::string_view cv, size_t cnt, size_t ref)
{
    BOOST_TEST(arg.type() == type);
    BOOST_TEST(arg.key() == key);
    BOOST_TEST(arg.toInt() == iv);
    BOOST_TEST(arg.toDouble() == dv);
    BOOST_TEST(arg.toString() == cv);
    BOOST_TEST(arg.count() == cnt);
    BOOST_TEST(arg.refCount() == ref);
    if (type == log::Arg::Type::Array)
        BOOST_TEST(arg.toArray() != nullptr);
    else
        BOOST_TEST(arg.toArray() == nullptr);
}


BOOST_DATA_TEST_CASE(logArgType,
    bdata::make({
        std::make_tuple(log::Arg::Type::Invalid, "key1"sv, 0, 0.0, "0"sv, ""s, false, 0),
        std::make_tuple(log::Arg::Type::Int, "key2"sv, 10, 0.0, "0"sv, ""s, false, 0),
        std::make_tuple(log::Arg::Type::Errno, "key2.1"sv, ENOENT, 0.0, "0"sv, ""s, false, 0),
        std::make_tuple(log::Arg::Type::Double, "key3"sv, 0, 10.0, "0"sv, ""s, false, 0),
        std::make_tuple(log::Arg::Type::String, "key4"sv, 0, 0.0, "charval"sv, ""s, false, 0),
        std::make_tuple(log::Arg::Type::String, "key5"sv, 0, 0.0, ""sv, "strval"s, true, 0),
        std::make_tuple(log::Arg::Type::String, "key6"sv, 0, 0.0, ""sv, ""s, true, 0),
        std::make_tuple(log::Arg::Type::String, "key7"sv, 0, 0.0, ""sv, std::string(), true, 0),
        std::make_tuple(log::Arg::Type::String, "key8"sv, 0, 0.0, ""sv, std::string(log::Arg::MaxStringStackSize + 1, 'a'), true, 1),
        std::make_tuple(log::Arg::Type::Array, "key9"sv, 1, 2.2, "char*"sv, "1234567"s, bool(), 0),
        std::make_tuple(log::Arg::Type::Array, "key10"sv, 3, 3.3, "char3*"sv, std::string(log::Arg::MaxStringStackSize + 1, 'a'), bool(), 1),
    }),
    argType, argKey, argInt, argDouble, argChar, argString, useString, refCnt)
{
    switch (argType) {
    case log::Arg::Type::Invalid: {
        log::Arg a;
        testArg(a, argType, std::string_view(), 0, 0, std::string_view(), 0, 0);
        break;
    }
    case log::Arg::Type::Int: {
        log::Arg a{argKey, argInt};
        testArg(a, argType, argKey, argInt, 0, std::string_view(), 1, 0);
        break;
    }
    case log::Arg::Type::Errno: {
        log::Arg a{argKey, log::ec_t{argInt}};
        testArg(a, argType, argKey, argInt, 0, "No such file or directory"sv, 1, 0);
        break;
    }
    case log::Arg::Type::Double: {
        log::Arg a{argKey, argDouble};
        testArg(a, argType, argKey, 0, argDouble, std::string_view(), 1, 0);
        break;
    }
    case log::Arg::Type::String: {
        if (!useString) {
            log::Arg a{argKey, argChar};
            testArg(a, argType, argKey, 0, 0, argChar, 1, 0);
        } else {
            log::Arg a{argKey, argString};
            testArg(a, argType, argKey, 0, 0, argString, 1, refCnt);
        }
        break;
    }
    case log::Arg::Type::Array: {
        const auto& longstr = std::string(log::Arg::MaxStringStackSize + 1, 'a');

        const log::Arg& a = log::Arg{
            argKey,
            {
                log::Arg{"k1"sv, argInt},
                log::Arg{"k2"sv, argDouble},
                log::Arg{"k3"sv, argChar},
                log::Arg{"k4"sv, argString},
                log::Arg{
                    "k5"sv,
                    {
                        log::Arg{"p0"sv, argInt},
                        log::Arg{"p1"sv, argDouble},
                        log::Arg{
                            "p2"sv,
                            {
                                log::Arg{"u0"sv, longstr},
                            },
                        },
                    },
                },
                log::Arg{"k6"sv, {}},
            },
        };

        testArg(a, argType, argKey, 0, 0, std::string_view(), 6, 1);
        const auto& arr1 = a.toArray();
        testArg(arr1[0], log::Arg::Type::Int, "k1"sv, argInt, 0, std::string_view(), 1, 0);
        testArg(arr1[1], log::Arg::Type::Double, "k2"sv, 0, argDouble, std::string_view(), 1, 0);
        testArg(arr1[2], log::Arg::Type::String, "k3"sv, 0, 0, argChar, 1, 0);
        testArg(arr1[3], log::Arg::Type::String, "k4"sv, 0, 0, argString, 1, refCnt);
        testArg(arr1[4], log::Arg::Type::Array, "k5"sv, 0, 0, std::string_view(), 3, 1);
        testArg(arr1[5], log::Arg::Type::Array, "k6"sv, 0, 0, std::string_view(), 0, 1);
        const auto& arr2 = arr1[4].toArray();
        testArg(arr2[0], log::Arg::Type::Int, "p0"sv, argInt, 0, std::string_view(), 1, 0);
        testArg(arr2[1], log::Arg::Type::Double, "p1"sv, 0, argDouble, std::string_view(), 1, 0);
        testArg(arr2[2], log::Arg::Type::Array, "p2"sv, 0, 0, std::string_view(), 1, 1);
        const auto& arr3 = arr2[2].toArray();
        testArg(arr3[0], log::Arg::Type::String, "u0"sv, 0, 0, longstr, 1, 1);
        break;
    }
    }
}


BOOST_DATA_TEST_CASE(logArgRef,
    bdata::make({
        log::Arg::Type::String,
        log::Arg::Type::Array,
    }),
    argType)
{
    log::Arg a;
    std::function<void(const log::Arg&, unsigned)> test;

    if (argType == log::Arg::Type::String) {
        const std::string_view key("key");
        const std::string val(log::Arg::MaxStringStackSize + 1, 'a');

        test = [key, val](const log::Arg& arg, unsigned cnt) {
            testArg(arg, log::Arg::Type::String, key, 0, 0, val, 1, cnt);
        };

        a = log::Arg{key, val};

    } else if (argType == log::Arg::Type::Array) {
        const std::string_view key("key");
        const log::Arg val[] = {
            log::Arg{"k0"sv, 1},
            log::Arg{"k1"sv, 2},
        };

        test = [key, val](const log::Arg& arg, unsigned cnt) {
            testArg(arg, log::Arg::Type::Array, key, 0, 0, std::string_view(), 2, cnt);

            const auto* arr = arg.toArray();
            for (size_t i = 0; i < std::size(val); ++i)
                testArg(arr[i], log::Arg::Type::Int, log::format("k%d", i).c_str(),
                    i + 1, 0, std::string_view(), 1, 0);
        };

        a = log::Arg{key, val};

    } else {
        BOOST_FAIL("unsupported type for reference count");
    }

    {
        log::Arg b(a);
        test(b, 2);
        test(a, 2);
    }

    test(a, 1);

    log::Arg c;
    c = a;
    test(c, 2);
    test(a, 2);

    {
        log::Arg d(std::move(c));
        test(d, 2);
        test(a, 2);
    }

    test(a, 1);

    log::Arg e = a;
    log::Arg f = e;
    test(e, 3);
    test(f, 3);

    a = log::Arg{};
    test(e, 2);
    test(f, 2);

    testArg(a, log::Arg::Type::Invalid, std::string_view(), 0, 0, std::string_view(), 0, 0);
}


BOOST_AUTO_TEST_CASE(logArgCopyStringStack)
{
    log::Arg a{"key"sv, "value"sv};
    testArg(a, log::Arg::Type::String, "key"sv, 0, 0, "value"sv, 1, 0);

    log::Arg b = a;
    testArg(b, log::Arg::Type::String, "key"sv, 0, 0, "value"sv, 1, 0);
}


BOOST_DATA_TEST_CASE(logArgShareAtomic,
    bdata::make({
        log::Arg::Type::String,
        log::Arg::Type::Array,
    }),
    argType)
{
    const auto& longstr = std::string(log::Arg::MaxStringStackSize + 1, 'a');
    log::Arg a;

    if (argType == log::Arg::Type::String) {
        a = log::Arg{"key"sv, longstr};

    } else if (argType == log::Arg::Type::Array) {
        a = log::Arg{
            "m0"sv,
            {
                log::Arg{"k1"sv, 1},
                log::Arg{"k2"sv, 1.0},
                log::Arg{"k3"sv, "hey"sv},
                log::Arg{
                    "k5"sv,
                    {
                        log::Arg{"p0"sv, 2},
                        log::Arg{"p1"sv, 2.0},
                        log::Arg{
                            "p2"sv,
                            {
                                log::Arg{"u0"sv, longstr},
                            },
                        },
                    },
                },
                log::Arg{"k6"sv, {}},
            },
        };
    }

    auto f = [&a]() {
        log::Arg b(a);
        b.refCount();
    };

    std::thread t1(f);
    std::thread t2(f);

    t1.join();
    t2.join();
}


static void BM_LogArgNoAlloc(benchmark::State& state)
{
    const auto key = "some key"sv;
    const auto val = "very long const char *"sv;

    for (auto _ : state)
        log::Arg a{key, val};
}
BENCHMARK(BM_LogArgNoAlloc);

static void BM_LogArgStack(benchmark::State& state)
{
    const auto key = "some key"sv;
    const auto val = "1234567"s;

    for (auto _ : state)
        log::Arg a{key, val};
}
BENCHMARK(BM_LogArgStack);

static void BM_LogArgHeap(benchmark::State& state)
{
    const auto key = "some key"sv;
    const auto val = "12345678 very long string"s;

    for (auto _ : state)
        log::Arg a{key, val};
}
BENCHMARK(BM_LogArgHeap);

static void BM_LogArgShare(benchmark::State& state)
{
    const auto key = "some key"sv;
    const auto val = "12345678 very long string"s;
    log::Arg a{key, val};

    for (auto _ : state)
        log::Arg b(a);
}
BENCHMARK(BM_LogArgShare);

static void BM_LogArgArray(benchmark::State& state)
{
    const auto key = "some key"sv;
    const log::Arg val[] = {log::Arg{"k1", 5.5}};

    for (auto _ : state)
        log::Arg a{key, val};
}
BENCHMARK(BM_LogArgArray);

BOOST_AUTO_TEST_CASE(bench, *utf::disabled())
{
    ::benchmark::RunSpecifiedBenchmarks();
}
