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

#include <rewind/rewind.h>

struct test_state {
  float value;
};

struct test_event_incr {
  int amount;
};

void test_event_incr_apply(const struct test_event_incr* e,
                           struct test_state* s) {
  s->value += (float)e->amount;
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

  RwnEventHandle* eh0 = rwn_history_schedule(
      h, 0, e0, NULL, (RwnEventDestroyFunc)test_event_alive_destroy);
  RwnEventHandle* eh1 = rwn_history_schedule(
      h, 1, e1, NULL, (RwnEventDestroyFunc)test_event_alive_destroy);

  rwn_history_destroy(h);

  ck_assert(e0->alive == false);
  ck_assert(e1->alive == false);

  free(e0);
  free(e1);
}
END_TEST

START_TEST(start_with_invalid_timepoint) {
  RwnHistory* h = rwn_history_create();

  int tp = rwn_history_current_timepoint(h);

  ck_assert(rwn_history_is_timepoint_valid(tp) == false);

  rwn_history_destroy(h);
}
END_TEST

START_TEST(forwards_increments_timepoint) {
  RwnHistory* h = rwn_history_create();

  int tp = rwn_history_current_timepoint(h);

  rwn_history_forwards(h, NULL);

  ck_assert_int_eq(rwn_history_current_timepoint(h), tp + 1);
  rwn_history_destroy(h);
}
END_TEST

START_TEST(backwards_decrements_timepoint) {
  RwnHistory* h = rwn_history_create();

  int tp = rwn_history_current_timepoint(h);

  rwn_history_backwards(h);

  ck_assert_int_eq(rwn_history_current_timepoint(h), tp - 1);
  rwn_history_destroy(h);
}
END_TEST

START_TEST(state_unchanged_after_no_events) {
  const size_t state_size = sizeof(int);

  int* initial_s = malloc(state_size);
  int* result_s = malloc(state_size);

  *initial_s = 0xdeadbeef;
  *result_s = *initial_s;

  RwnHistory* h = rwn_history_create();
  rwn_history_forwards(h, result_s);

  ck_assert_int_eq(*result_s, *initial_s);

  free(initial_s);
  free(result_s);

  rwn_history_destroy(h);
}
END_TEST

START_TEST(events_added_and_removed_from_scheduler) {
  RwnHistory* h = rwn_history_create();

  int* e0 = malloc(sizeof(int));
  int* e1 = malloc(sizeof(int));
  int* e11 = malloc(sizeof(int));

  RwnEventHandle* h0 = rwn_history_schedule(h, 0, e0, NULL, NULL);
  RwnEventHandle* h1 = rwn_history_schedule(h, 1, e1, NULL, NULL);
  RwnEventHandle* h11 = rwn_history_schedule(h, 1, e11, NULL, NULL);

  ck_assert_int_eq(rwn_history_count_events_at(h, 0), 1);
  ck_assert_int_eq(rwn_history_count_events_at(h, 1), 2);

  rwn_history_unschedule(h, h0);
  rwn_history_unschedule(h, h1);
  rwn_history_unschedule(h, h11);

  ck_assert_int_eq(rwn_history_count_events_at(h, 0), 0);
  ck_assert_int_eq(rwn_history_count_events_at(h, 1), 0);

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
      h, 0, e0, NULL, (RwnEventDestroyFunc)test_event_alive_destroy);
  RwnEventHandle* eh1 = rwn_history_schedule(
      h, 1, e1, NULL, (RwnEventDestroyFunc)test_event_alive_destroy);

  rwn_history_unschedule(h, eh0);
  rwn_history_unschedule(h, eh1);

  ck_assert(e0->alive == false);
  ck_assert(e1->alive == false);

  rwn_history_destroy(h);

  free(e0);
  free(e1);
}
END_TEST

START_TEST(destruction_destroys_events_with_callback) {
  RwnHistory* h = rwn_history_create();

  struct test_event_alive *e0, *e1;
  e0 = malloc(sizeof(struct test_event_alive));
  e0->alive = true;
  e1 = malloc(sizeof(struct test_event_alive));
  e1->alive = true;

  RwnEventHandle* eh0 = rwn_history_schedule(
      h, 0, e0, NULL, (RwnEventDestroyFunc)test_event_alive_destroy);
  RwnEventHandle* eh1 = rwn_history_schedule(
      h, 1, e1, NULL, (RwnEventDestroyFunc)test_event_alive_destroy);

  rwn_history_unschedule(h, eh0);
  rwn_history_unschedule(h, eh1);

  ck_assert(e0->alive == false);
  ck_assert(e1->alive == false);

  rwn_history_destroy(h);

  free(e0);
  free(e1);
}
END_TEST

START_TEST(forwards_applies_events) {
  struct test_event_incr* e0 = malloc(sizeof *e0);
  e0->amount = 1;

  struct test_event_incr* e00 = malloc(sizeof *e00);
  e00->amount = 2;

  struct test_state* s = malloc(sizeof(*s));
  s->value = 100;

  RwnHistory* h = rwn_history_create();

  rwn_history_schedule(h, 0, e0, (RwnEventApplyFunc)test_event_incr_apply,
                       (RwnEventDestroyFunc)free);
  rwn_history_schedule(h, 0, e00, (RwnEventApplyFunc)test_event_incr_apply,
                       (RwnEventDestroyFunc)free);

  rwn_history_forwards(h, s);

  ck_assert_int_eq(s->value, 100 + 1 + 2);

  rwn_history_destroy(h);
}
END_TEST

Suite* make_suite(void) {
  Suite* s;
  TCase* tc_core;

  s = suite_create("History");

  tc_core = tcase_create("Initialization");
  tcase_add_test(tc_core, create_allocates_memory);
  tcase_add_test_raise_signal(tc_core, destroy_deallocates_memory, 11);
  tcase_add_test(tc_core, destroy_destroys_events_with_callback);
  suite_add_tcase(s, tc_core);

  tc_core = tcase_create("Timepoints");
  tcase_add_test(tc_core, start_with_invalid_timepoint);
  tcase_add_test(tc_core, forwards_increments_timepoint);
  tcase_add_test(tc_core, backwards_decrements_timepoint);
  suite_add_tcase(s, tc_core);

  tc_core = tcase_create("Scheduling");
  tcase_add_test(tc_core, state_unchanged_after_no_events);
  tcase_add_test(tc_core, events_added_and_removed_from_scheduler);
  tcase_add_test(tc_core, unschedule_destroys_events_with_callback);
  tcase_add_test(tc_core, forwards_applies_events);
  suite_add_tcase(s, tc_core);

  return s;
}

int main(void) {
  int number_failed;
  Suite* s;
  SRunner* sr;

  s = make_suite();
  sr = srunner_create(s);

  srunner_run_all(sr, CK_VERBOSE);
  //  srunner_run_all(sr, CK_NOFORK);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
