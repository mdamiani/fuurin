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
#include "fuurin/logger.h"
#include "log.h"

#include <cstdio>
#include <string>
#include <vector>
#include <tuple>
#include <functional>
#include <iostream>
#include <sstream>
#include <utility>
#include <iterator>


using namespace fuurin;
using namespace std::string_literals;
namespace utf = boost::unit_test;
namespace bdata = utf::data;


BOOST_AUTO_TEST_CASE(libVersion)
{
    char buf[16];

    std::snprintf(buf, sizeof(buf), "%d.%d.%d",
        VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

    BOOST_TEST(std::string(version()) == std::string(buf));
}


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

        log::Logger::installContentHandler(new log::StandardHandler);

        const log::Loc loc{"test_file", 1};
        const log::Arg u[] = {
            log::Arg{"u0", 10},
            log::Arg{20},
        };
        const log::Arg v[] = {
            log::Arg{"v0", "1234"},
            log::Arg{},
            log::Arg{u},
        };
        const log::Arg a[] = {
            log::Arg{"a1", 1},
            log::Arg{"a2", 2.2},
            log::Arg{"a3", "three"},
            log::Arg{"a4", v},
            log::Arg{4},
        };

        const auto& lvl1 = log::format("%s: %d, %d",
            u[0].key(), u[0].toInt(), u[1].toInt());

        const auto& lvl2 = log::format("%s: %s, <>, %s",
            v[0].key(), v[0].toCString(), lvl1.c_str());

        expected = log::format("%s: %d, %s: %0.1f, %s: %s, %s: %s, %d",
            a[0].key(), a[0].toInt(),
            a[1].key(), a[1].toDouble(),
            a[2].key(), a[2].toCString(),
            a[3].key(), lvl2.c_str(),
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
        const log::Arg args[] = {log::Arg{"test_fun", "test_msg"}};

        logfn(loc, args, std::size(args));
    }
    BOOST_TEST(buf.str().empty());
}


BOOST_AUTO_TEST_CASE(formatLog)
{
    BOOST_TEST("test_fun: test_msg1" == log::format("test_fun: %s%d", "test_msg", 1));
    BOOST_TEST("" == log::format("", "aaa", 1));
}


static void testArg(const log::Arg& arg, log::Arg::Type type, const char* key, int iv, double dv,
    const char* cv, size_t cnt, size_t ref)
{
    BOOST_TEST(arg.type() == type);
    BOOST_TEST(arg.key() == key);
    BOOST_TEST(arg.toInt() == iv);
    BOOST_TEST(arg.toDouble() == dv);
    BOOST_TEST(arg.toCString() == cv);
    BOOST_TEST(arg.count() == cnt);
    BOOST_TEST(arg.refCount() == ref);
    if (type == log::Arg::Type::Array)
        BOOST_TEST(arg.toArray() != nullptr);
    else
        BOOST_TEST(arg.toArray() == nullptr);
}


BOOST_DATA_TEST_CASE(logArgType,
    bdata::make({
        std::make_tuple(log::Arg::Invalid, "key1", 0, 0.0, "0", ""s, false),
        std::make_tuple(log::Arg::Int, "key2", 10, 0.0, "0", ""s, false),
        std::make_tuple(log::Arg::Double, "key3", 0, 10.0, "0", ""s, false),
        std::make_tuple(log::Arg::CString, "key4", 0, 0.0, "charval", ""s, false),
        std::make_tuple(log::Arg::CString, "key5", 0, 0.0, "", "strval"s, true),
        std::make_tuple(log::Arg::CString, "key6", 0, 0.0, "", ""s, true),
        std::make_tuple(log::Arg::CString, "key7", 0, 0.0, "", std::string(), true),
        std::make_tuple(log::Arg::CString, "key8", 0, 0.0, "", "12345678"s, true),
        std::make_tuple(log::Arg::Array, "key9", 1, 2.2, "char*", "1234567"s, false),
        std::make_tuple(log::Arg::Array, "key10", 3, 3.3, "char3*", "123456789"s, false),
    }),
    argType, argKey, argInt, argDouble, argChar, argString, argAlloc)
{
    const auto& refCnt = [&argString]() {
        if (argString.size() + 1 > sizeof(double))
            return 1;
        else
            return 0;
    };

    switch (argType) {
    case log::Arg::Type::Invalid: {
        log::Arg a;
        testArg(a, argType, nullptr, 0, 0, nullptr, 0, 0);
        break;
    }
    case log::Arg::Type::Int: {
        log::Arg a{argKey, argInt};
        testArg(a, argType, argKey, argInt, 0, nullptr, 1, 0);
        break;
    }
    case log::Arg::Type::Double: {
        log::Arg a{argKey, argDouble};
        testArg(a, argType, argKey, 0, argDouble, nullptr, 1, 0);
        break;
    }
    case log::Arg::Type::CString: {
        if (!argAlloc) {
            log::Arg a{argKey, argChar};
            testArg(a, argType, argKey, 0, 0, argChar, 1, 0);
        } else {
            log::Arg a{argKey, argString};
            testArg(a, argType, argKey, 0, 0, argString.c_str(), 1, refCnt());
        }
        break;
    }
    case log::Arg::Type::Array: {
        const log::Arg& a = log::Arg{
            argKey,
            {
                log::Arg{"k1", argInt},
                log::Arg{"k2", argDouble},
                log::Arg{"k3", argChar},
                log::Arg{"k4", argString},
                log::Arg{
                    "k5",
                    {
                        log::Arg{"p0", argInt},
                        log::Arg{"p1", argDouble},
                        log::Arg{
                            "p2",
                            {
                                log::Arg{"u0", "123456789"s},
                            },
                        },
                    },
                },
                log::Arg{"k6", {}},
            },
        };

        testArg(a, argType, argKey, 0, 0, nullptr, 6, 1);
        const auto& arr1 = a.toArray();
        testArg(arr1[0], log::Arg::Type::Int, "k1", argInt, 0, nullptr, 1, 0);
        testArg(arr1[1], log::Arg::Type::Double, "k2", 0, argDouble, nullptr, 1, 0);
        testArg(arr1[2], log::Arg::Type::CString, "k3", 0, 0, argChar, 1, 0);
        testArg(arr1[3], log::Arg::Type::CString, "k4", 0, 0, argString.c_str(), 1, refCnt());
        testArg(arr1[4], log::Arg::Type::Array, "k5", 0, 0, nullptr, 3, 1);
        testArg(arr1[5], log::Arg::Type::Array, "k6", 0, 0, nullptr, 0, 1);
        const auto& arr2 = arr1[4].toArray();
        testArg(arr2[0], log::Arg::Type::Int, "p0", argInt, 0, nullptr, 1, 0);
        testArg(arr2[1], log::Arg::Type::Double, "p1", 0, argDouble, nullptr, 1, 0);
        testArg(arr2[2], log::Arg::Type::Array, "p2", 0, 0, nullptr, 1, 1);
        const auto& arr3 = arr2[2].toArray();
        testArg(arr3[0], log::Arg::Type::CString, "u0", 0, 0, "123456789", 1, 1);
        break;
    }
    }
}


BOOST_DATA_TEST_CASE(logArgRef,
    bdata::make({
        log::Arg::Type::CString,
        log::Arg::Type::Array,
    }),
    argType)
{
    log::Arg a;
    std::function<void(const log::Arg&, unsigned)> test;

    if (argType == log::Arg::Type::CString) {
        const char* key = "key";
        const std::string val("very long long value");

        test = [key, val](const log::Arg& arg, unsigned cnt) {
            testArg(arg, log::Arg::Type::CString, key, 0, 0, val.c_str(), 1, cnt);
        };

        a = log::Arg{key, val};

    } else if (argType == log::Arg::Type::Array) {
        const char* key = "key";
        const log::Arg val[] = {
            log::Arg{"k0", 1},
            log::Arg{"k1", 2},
        };

        test = [key, val](const log::Arg& arg, unsigned cnt) {
            testArg(arg, log::Arg::Type::Array, key, 0, 0, nullptr, 2, cnt);

            const auto* arr = arg.toArray();
            for (size_t i = 0; i < std::size(val); ++i)
                testArg(arr[i], log::Arg::Type::Int, log::format("k%d", i).c_str(),
                    i + 1, 0, nullptr, 1, 0);
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

    testArg(a, log::Arg::Type::Invalid, nullptr, 0, 0, nullptr, 0, 0);
}


static void BM_LogArgNoAlloc(benchmark::State& state)
{
    const auto key = "some key";
    const auto val = "very long const char *";

    for (auto _ : state)
        log::Arg a{key, val};
}
BENCHMARK(BM_LogArgNoAlloc);

static void BM_LogArgStack(benchmark::State& state)
{
    const auto key = "some key";
    const auto val = "1234567"s;

    for (auto _ : state)
        log::Arg a{key, val};
}
BENCHMARK(BM_LogArgStack);

static void BM_LogArgHeap(benchmark::State& state)
{
    const auto key = "some key";
    const auto val = "12345678 very long string"s;

    for (auto _ : state)
        log::Arg a{key, val};
}
BENCHMARK(BM_LogArgHeap);

static void BM_LogArgShare(benchmark::State& state)
{
    const auto key = "some key";
    const auto val = "12345678 very long string"s;
    log::Arg a{key, val};

    for (auto _ : state)
        log::Arg b(a);
}
BENCHMARK(BM_LogArgShare);

static void BM_LogArgArray(benchmark::State& state)
{
    const auto key = "some key";
    const log::Arg val[] = {log::Arg{"k1", 5.5}};

    for (auto _ : state)
        log::Arg a{key, val};
}
BENCHMARK(BM_LogArgArray);

BOOST_AUTO_TEST_CASE(bench, *utf::disabled())
{
    ::benchmark::RunSpecifiedBenchmarks();
}
