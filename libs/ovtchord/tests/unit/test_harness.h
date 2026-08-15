// test_harness.h
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Minimal self-contained test harness (no external dependency).

#pragma once

#include <cstdio>
#include <vector>

namespace ovtchord_test
{
    struct TestCase { const char* name; void (*fn)(); };

    inline std::vector<TestCase>& registry() { static std::vector<TestCase> r; return r; }
    inline int& checks()   { static int c = 0; return c; }
    inline int& failures() { static int f = 0; return f; }
}

#define TEST(name) \
    static void test_##name(); \
    namespace { struct Reg_##name { Reg_##name() { ovtchord_test::registry().push_back ({ #name, test_##name }); } } reg_##name; } \
    static void test_##name()

#define CHECK(cond) \
    do { ++ovtchord_test::checks(); if (! (cond)) { ++ovtchord_test::failures(); std::printf ("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
