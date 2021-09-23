/*
 * Copyright 2021 Nikolay Burkov <nbrk@linklevel.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#pragma once

#include <stdbool.h>

typedef struct RwnHistory RwnHistory;
typedef struct RwnEventHandle RwnEventHandle;
typedef void (*RwnEventApplyFunc)(const void* e, void* s);
typedef void (*RwnEventDestroyFunc)(void* e);

extern RwnHistory* rwn_history_create(void);
extern void rwn_history_destroy(RwnHistory* h);
extern int rwn_history_count_events(const RwnHistory* h, int at_timepoint);
extern int rwn_history_get_events(const RwnHistory* h,
                                  int at_timepoint,
                                  void** user_eventv);
extern int rwn_history_state_delta(const RwnHistory* h,
                                   int start_timepoint,
                                   int finish_timepoint,
                                   void* delta_state);
extern RwnEventHandle* rwn_history_schedule(
    RwnHistory* h,
    int at_timepoint,
    int at_phase,
    const void* evt,
    RwnEventApplyFunc evt_apply_func,
    RwnEventDestroyFunc evt_destroy_func);
extern void rwn_history_unschedule(RwnHistory* h, RwnEventHandle* eh);
extern int rwn_history_unschedule_all(RwnHistory* h,
                                      int from_timepoint,
                                      int to_timepoint);
