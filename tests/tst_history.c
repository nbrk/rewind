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
#include <check.h>
#include <stdlib.h>

#include <pthread.h>

#include <rewind/rewind.h>

struct test_state {
  float value;
};

struct test_event_incr {
  int amount;
};

struct test_event_decr {
  int amount;
};

struct test_event_mult {
  int by;
};

void test_event_incr_apply(const struct test_event_incr* e,
                           struct test_state* s) {
  s->value += (float)e->amount;
}

void test_event_decr_apply(const struct test_event_decr* e,
                           struct test_state* s) {
  s->value -= (float)e->amount;
}

void test_event_mult_apply(const struct test_event_mult* e,
                           struct test_state* s) {
  s->value *= (float)e->by;
}

struct test_state_mt {
  float value;
  pthread_mutex_t mutex;
};

struct test_event_incr_mt {
  int amount;
};

void test_event_incr_apply_mt(const struct test_event_incr_mt* e,
                              struct test_state_mt* s) {
  // some CPU, IO work, etc
  int i;
  for (i = 0; i < 1000; ++i)
    ;

  pthread_mutex_lock(&s->mutex);
  s->value += (float)e->amount;
  pthread_mutex_unlock(&s->mutex);
}

struct test_event_alive {
  bool alive;
};

void test_event_alive_destroy(struct test_event_alive* e) {
  e->alive = false;
}

START_TEST(create_allocates_memory) {
  RwnHistory* h = NULL;

  h = rwn_history_create();

  ck_assert_ptr_nonnull(h);

  rwn_history_destroy(h);
}
END_TEST

START_TEST(destroy_deallocates_memory) {
  RwnHistory* h = (RwnHistory*)0xdeadbeef;

  rwn_history_destroy(h);
}
END_TEST

START_TEST(destroy_destroys_events_with_callback) {
  RwnHistory* h = rwn_history_create();

  struct test_event_alive *e0, *e1;
  e0 = malloc(sizeof(struct test_event_alive));
  e0->alive = true;
  e1 = malloc(sizeof(struct test_event_alive));
  e1->alive = true;

  rwn_history_schedule(h, 0, 0, e0, NULL,
                       (RwnEventDestroyFunc)test_event_alive_destroy);
  rwn_history_schedule(h, 1, 0, e1, NULL,
                       (RwnEventDestroyFunc)test_event_alive_destroy);

  rwn_history_destroy(h);

  ck_assert(e0->alive == false);
  ck_assert(e1->alive == false);

  free(e0);
  free(e1);
}
END_TEST

START_TEST(events_added_and_removed_from_scheduler) {
  RwnHistory* h = rwn_history_create();

  int* e0 = malloc(sizeof(int));
  int* e1 = malloc(sizeof(int));
  int* e11 = malloc(sizeof(int));

  RwnEventHandle* h0 = rwn_history_schedule(h, 0, 0, e0, NULL, NULL);
  RwnEventHandle* h1 = rwn_history_schedule(h, 1, 0, e1, NULL, NULL);
  RwnEventHandle* h11 = rwn_history_schedule(h, 1, 0, e11, NULL, NULL);

  ck_assert_int_eq(rwn_history_count_events(h, 0), 1);
  ck_assert_int_eq(rwn_history_count_events(h, 1), 2);

  rwn_history_unschedule(h, h0);
  rwn_history_unschedule(h, h1);
  rwn_history_unschedule(h, h11);

  ck_assert_int_eq(rwn_history_count_events(h, 0), 0);
  ck_assert_int_eq(rwn_history_count_events(h, 1), 0);

  rwn_history_destroy(h);

  free(e0);
  free(e1);
  free(e11);
}
END_TEST

START_TEST(unschedule_destroys_events_with_callback) {
  RwnHistory* h = rwn_history_create();

  struct test_event_alive *e0, *e1;
  e0 = malloc(sizeof(struct test_event_alive));
  e0->alive = true;
  e1 = malloc(sizeof(struct test_event_alive));
  e1->alive = true;

  RwnEventHandle* eh0 = rwn_history_schedule(
      h, 0, 0, e0, NULL, (RwnEventDestroyFunc)test_event_alive_destroy);
  RwnEventHandle* eh1 = rwn_history_schedule(
      h, 1, 0, e1, NULL, (RwnEventDestroyFunc)test_event_alive_destroy);

  rwn_history_unschedule(h, eh0);
  rwn_history_unschedule(h, eh1);

  ck_assert(e0->alive == false);
  ck_assert(e1->alive == false);

  rwn_history_destroy(h);

  free(e0);
  free(e1);
}
END_TEST

START_TEST(no_state_delta_after_no_events) {
  struct test_state* state = malloc(sizeof(*state));
  state->value = 123;

  RwnHistory* h = rwn_history_create();

  // ne events scheduled
  int evtcnt = rwn_history_state_delta(h, 0, 100, state, 0);
  ck_assert_int_eq(evtcnt, 0);
  ck_assert_int_eq(state->value, 123);

  rwn_history_destroy(h);

  free(state);
}
END_TEST

START_TEST(state_delta_after_events) {
  struct test_state* state = malloc(sizeof(*state));
  state->value = 123;

  RwnHistory* h = rwn_history_create();

  struct test_event_incr* e_incr;
  struct test_event_decr* e_decr;

  /*
   * Schedule different events at different tps
   */
  e_incr = malloc(sizeof(*e_incr));
  e_incr->amount = 70;
  rwn_history_schedule(h, 0, 0, e_incr,
                       (RwnEventApplyFunc)test_event_incr_apply, free);

  e_incr = malloc(sizeof(*e_incr));
  e_incr->amount = 7;
  rwn_history_schedule(h, 2, 0, e_incr,
                       (RwnEventApplyFunc)test_event_incr_apply, free);

  e_decr = malloc(sizeof(*e_decr));
  e_decr->amount = 100;
  rwn_history_schedule(h, 3, 0, e_decr,
                       (RwnEventApplyFunc)test_event_decr_apply, free);

  int evtcnt = rwn_history_state_delta(h, 0, 10, state, 0);
  ck_assert_int_eq(evtcnt, 3);
  ck_assert_int_eq(state->value, 100);

  rwn_history_destroy(h);

  free(state);
}
END_TEST

START_TEST(scheduled_event_count_and_ptrs_returned) {
  RwnHistory* h = rwn_history_create();

  struct test_event_incr* ev[10];
  int i;
  for (i = 0; i < 10; ++i) {
    ev[i] = malloc(sizeof **ev);
    rwn_history_schedule(h, 100, 0, ev[i], NULL, free);
  }

  struct test_event_incr* retev[10];
  ck_assert_int_eq(rwn_history_count_events(h, 100), 10);
  int evtcnt = rwn_history_get_events(h, 100, (void**)retev);
  ck_assert_int_eq(evtcnt, 10);
  for (i = 0; i < 10; ++i) {
    ck_assert_ptr_eq(retev[i], ev[i]);
  }

  rwn_history_destroy(h);
}
END_TEST

START_TEST(unschedule_all_destroys_events) {
  RwnHistory* h = rwn_history_create();

  struct test_event_alive* ev[10];
  int i;
  for (i = 0; i < 10; ++i) {
    ev[i] = malloc(sizeof **ev);
    ev[i]->alive = true;
    rwn_history_schedule(
        h, rand() % 10, 0, ev[i], NULL,
        (RwnEventDestroyFunc)test_event_alive_destroy);  // special destroy cb
  }

  int evtcnt = rwn_history_unschedule_all(h, 0, 10);
  ck_assert_int_eq(evtcnt, 10);

  for (i = 0; i < 10; ++i) {
    ck_assert(!ev[i]->alive);
    free(ev[i]);
  }

  rwn_history_destroy(h);
}
END_TEST

START_TEST(scheduled_events_applied_by_phases) {
  RwnHistory* h = rwn_history_create();
  struct test_state* state = malloc(sizeof(*state));

  struct test_event_mult* e_mult1 = malloc(sizeof(*e_mult1));
  e_mult1->by = 1;

  struct test_event_mult* e_mult2 = malloc(sizeof(*e_mult2));
  e_mult2->by = 2;

  struct test_event_incr* e_incr1 = malloc(sizeof(*e_incr1));
  e_incr1->amount = 1;

  struct test_event_incr* e_incr2 = malloc(sizeof(*e_incr2));
  e_incr2->amount = 2;

  const int MULT_PHASE = 0;
  const int INCR_PHASE = 1;
  rwn_history_schedule(h, 0, INCR_PHASE, e_incr1,
                       (RwnEventApplyFunc)test_event_incr_apply, free);
  rwn_history_schedule(h, 0, MULT_PHASE, e_mult1,
                       (RwnEventApplyFunc)test_event_mult_apply, free);
  rwn_history_schedule(h, 0, INCR_PHASE, e_incr2,
                       (RwnEventApplyFunc)test_event_incr_apply, free);
  rwn_history_schedule(h, 0, MULT_PHASE, e_mult2,
                       (RwnEventApplyFunc)test_event_mult_apply, free);
  /*
   * s + 1, s * 1, s + 2, s * 2
   *
   * Gives s is 0:
   * Correct order gives result: 3
   * Incorrect order gives result: 6
   * Gives s is 1:
   * Correct order gives result: 5
   * Incorrect order gives result: 8
   */
  state->value = 0;
  rwn_history_state_delta(h, 0, 0, state, 0);
  ck_assert_int_eq(state->value, 3);

  state->value = 1;
  rwn_history_state_delta(h, 0, 0, state, 0);
  ck_assert_int_eq(state->value, 5);

  rwn_history_destroy(h);
}
END_TEST

START_TEST(state_delta_after_events_with_multithreaded_phases) {
  struct test_state_mt* state = malloc(sizeof(*state));
  state->value = 0;

  // will sync state writes on the following mutex (kept in the state)
  pthread_mutex_init(&state->mutex, NULL);

  struct test_event_incr_mt* ev[100];
  int i;

  RwnHistory* h = rwn_history_create();

  for (i = 0; i < 100; ++i) {
    ev[i] = malloc(sizeof(**ev));
    ev[i]->amount = 1;
    rwn_history_schedule(h, 0, 0, ev[i],
                         (RwnEventApplyFunc)test_event_incr_apply_mt, free);
  }

  rwn_history_state_delta(h, 0, 1, state, 10);

  ck_assert_int_eq(state->value, 100);

  rwn_history_destroy(h);

  free(state);
}
END_TEST

/*
 * TEST DRIVER CODE
 */
Suite* make_suite(void) {
  Suite* s;
  TCase* tc_core;

  s = suite_create("History");

  tc_core = tcase_create("Initialization");
  tcase_add_test(tc_core, create_allocates_memory);
  tcase_add_test_raise_signal(tc_core, destroy_deallocates_memory, 11);
  tcase_add_test(tc_core, destroy_destroys_events_with_callback);
  suite_add_tcase(s, tc_core);

  tc_core = tcase_create("Scheduling");
  tcase_add_test(tc_core, no_state_delta_after_no_events);
  tcase_add_test(tc_core, events_added_and_removed_from_scheduler);
  tcase_add_test(tc_core, unschedule_destroys_events_with_callback);
  tcase_add_test(tc_core, state_delta_after_events);
  tcase_add_test(tc_core, scheduled_event_count_and_ptrs_returned);
  tcase_add_test(tc_core, unschedule_all_destroys_events);
  tcase_add_test(tc_core, scheduled_events_applied_by_phases);
  tcase_add_test(tc_core, state_delta_after_events_with_multithreaded_phases);
  suite_add_tcase(s, tc_core);

  return s;
}

int main(void) {
  int number_failed;
  Suite* s;
  SRunner* sr;

  s = make_suite();
  sr = srunner_create(s);

  //  srunner_set_fork_status(sr, CK_NOFORK);
  srunner_run_all(sr, CK_VERBOSE);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
