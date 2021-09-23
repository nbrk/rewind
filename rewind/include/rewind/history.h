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

/**
 * @brief Opaque object holding crucial information about all scheduled
 * (planned) events of past and future
 */
typedef struct RwnHistory RwnHistory;

/**
 * @brief Opaque handle issued upon event's scheduling and used to unschedule
 * (remove) the event afterwards
 */
typedef struct RwnEventHandle RwnEventHandle;

/**
 * @brief Type of concrete event apply function provided by the user; takes the
 * event and the state pointers
 */
typedef void (*RwnEventApplyFunc)(const void* e, void* s);

/**
 * @brief Type of concrete event destroy function provided by the user (for
 * example, free()); takes the event pointer
 */
typedef void (*RwnEventDestroyFunc)(void* e);

/**
 * @brief Create history object
 * @return
 */
extern RwnHistory* rwn_history_create(void);

/**
 * @brief Destroy the history object and free any of its data structure,
 * including scheduled user's events (if such a function was provided upon
 * `rwn_history_schedule()`
 *
 * @param h
 */
extern void rwn_history_destroy(RwnHistory* h);

/**
 * @brief Count number of events planned to occur at the given timepoint
 * (including events without apply func, which will not be executed really)
 * @param h
 * @param at_timepoint
 * @return number of events
 */
extern int rwn_history_count_events(const RwnHistory* h, int at_timepoint);

/**
 * @brief Get pointers to all events (the datastructures pointed to by the user)
 * planned for execution at the given time point.
 * @param h
 * @param at_timepoint
 * @param user_eventv Vector of pointers of at least
 * `rwn_history_count_events()` length. Caller allocs and owns the vector.
 * @return number of events written to the vector
 */
extern int rwn_history_get_events(const RwnHistory* h,
                                  int at_timepoint,
                                  void** user_eventv);

/**
 * @brief Apply scheduled events to the state, progressing sequentially through
 * the history time.
 *
 * If two or more events within a timepoint share the same
 * `phase` number, they are considered to happen in parallel, in which case the
 * last parameter `max_threads` can be used to set maximum number of concurrent
 * threads per phase. Value of zero disables multithreading.
 *
 * NOTE: if `max_threads` is greater than zero, then all user events' `apply`
 * functions are considered to be THREADSAFE with respect to any modifications
 * of the state.
 *
 * @param h
 * @param start_timepoint first timepoint to apply planned events at
 * @param finish_timepoint last timepoint to apply planned events at
 * @param state the datastructure that will be successively modified by the
 * events
 * @param max_threads phase-multithreaded execution parameter, see above
 * description. Value of zero means no multithreading.
 * @return
 */
extern int rwn_history_state_delta(const RwnHistory* h,
                                   int start_timepoint,
                                   int finish_timepoint,
                                   void* state,
                                   const int max_threads);

/**
 * @brief Plan an event occurence at the given time point.
 * @param h
 * @param at_timepoint
 * @param at_phase phase the event belongs to (there could be several sequential
 * phases within a timepoint)
 * @param evt pointer to the user's event datastructure
 * @param evt_apply_func pointer to user's `apply` function for this event, or
 * NULL if no use (then why?..)
 * @param evt_destroy_func pointer to user's `destroy` function for this event,
 * or NULL if no use
 * @return handle to the newly scheduled event that can be used later to
 * unschedule it
 */
extern RwnEventHandle* rwn_history_schedule(
    RwnHistory* h,
    int at_timepoint,
    int at_phase,
    const void* evt,
    RwnEventApplyFunc evt_apply_func,
    RwnEventDestroyFunc evt_destroy_func);

/**
 * @brief Delete event occurence from the calendar
 * @param h
 * @param eh event handle previously acquired via `rwn_history_schedule()`
 */
extern void rwn_history_unschedule(RwnHistory* h, RwnEventHandle* eh);

/**
 * @brief Delete all event occurences at the given time point
 * @param h
 * @param start_timepoint first timepoint to delete all events at
 * @param finish_timepoint last timepoint to delete all events at
 * @return number of events that were actually unscheduled in the process
 */
extern int rwn_history_unschedule_all(RwnHistory* h,
                                      int start_timepoint,
                                      int finish_timepoint);
