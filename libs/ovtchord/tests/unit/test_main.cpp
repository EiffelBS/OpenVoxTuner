// test_main.cpp
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#include "test_harness.h"

int main()
{
    for (auto& t : ovtchord_test::registry())
    {
        std::printf ("[ RUN ] %s\n", t.name);
        t.fn();
    }
    std::printf ("\n%d checks, %d failures\n", ovtchord_test::checks(), ovtchord_test::failures());
    return ovtchord_test::failures() == 0 ? 0 : 1;
}
