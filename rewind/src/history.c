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
#include <rewind/history.h>

#include "uthash.h"
#include "utlist.h"

#include <stdlib.h>
#include <string.h>

struct EventListEntry {
  void* user_event;
  RwnEventApplyFunc user_event_apply_func;
  RwnEventDestroyFunc user_event_destroy_func;
  struct EventListEntry* next;
};

struct TimepointHashMapEntry {
  int timepoint; /* key */
  struct EventListEntry* event_list;
  UT_hash_handle hh;
};

struct RwnEventHandle {
  int timepoint;
  struct EventListEntry* event_list_elem;
  RwnEventHandle* next;
};

struct RwnHistory {
  struct TimepointHashMapEntry* timepoint_hash_map;
  RwnEventHandle* event_handle_list;
};

RwnHistory* rwn_history_create(void) {
  RwnHistory* h;

  h = malloc(sizeof(*h));

  h->timepoint_hash_map = NULL;
  h->event_handle_list = NULL;

  return h;
}

void rwn_history_destroy(RwnHistory* h) {
  // free the tp map
  struct TimepointHashMapEntry *entry, *entry_tmp;
  HASH_ITER(hh, h->timepoint_hash_map, entry, entry_tmp) {
    // free events of the tp map entry
    struct EventListEntry *evtentry, *evtentry_tmp;
    LL_FOREACH_SAFE(entry->event_list, evtentry, evtentry_tmp) {
      LL_DELETE(entry->event_list, evtentry);
      // free the user structure if needed
      if (evtentry->user_event_destroy_func != NULL)
        evtentry->user_event_destroy_func(evtentry->user_event);
      free(evtentry);
    }
    HASH_DEL(h->timepoint_hash_map, entry);
    free(entry);
  }

  // free/invalidate all issued event handles
  RwnEventHandle *handle, *handle_tmp;
  LL_FOREACH_SAFE(h->event_handle_list, handle, handle_tmp) {
    LL_DELETE(h->event_handle_list, handle);
    free(handle);
  }

  // free the main struct
  free(h);
}

RwnEventHandle* rwn_history_schedule(RwnHistory* h,
                                     int at_timepoint,
                                     const void* evt,
                                     RwnEventApplyFunc evt_apply_func,
                                     RwnEventDestroyFunc evt_destroy_func) {
  if (at_timepoint < 0)
    return NULL;

  struct TimepointHashMapEntry* mapentry;
  HASH_FIND_INT(h->timepoint_hash_map, &at_timepoint, mapentry);
  if (mapentry == NULL) {
    // new map entry
    mapentry = malloc(sizeof(*mapentry));
    mapentry->timepoint = at_timepoint;
    mapentry->event_list = NULL;
    HASH_ADD_INT(h->timepoint_hash_map, timepoint, mapentry);
  }

  // start or continue event list in the entry
  struct EventListEntry* evtentry = malloc(sizeof(*evtentry));
  evtentry->user_event = (void*)evt;
  evtentry->user_event_apply_func = evt_apply_func;
  evtentry->user_event_destroy_func = evt_destroy_func;
  evtentry->next = NULL;
  LL_APPEND(mapentry->event_list, evtentry);

  // create, save and return opaque handle describing how to locate the event
  RwnEventHandle* handle = malloc(sizeof(*handle));
  handle->timepoint = at_timepoint;
  handle->event_list_elem = evtentry;
  handle->next = NULL;
  LL_APPEND(h->event_handle_list, handle);

  return handle;
}

int rwn_history_count_events(const RwnHistory* h, int at_timepoint) {
  if (at_timepoint < 0)
    return 0;

  struct TimepointHashMapEntry* mapentry;
  HASH_FIND_INT(h->timepoint_hash_map, &at_timepoint, mapentry);
  if (mapentry != NULL) {
    struct EventListEntry* evtentry;
    int count;
    LL_COUNT(mapentry->event_list, evtentry, count);
    return count;
  }
  return 0;
}

static bool is_event_handle_valid(const RwnHistory* h,
                                  const RwnEventHandle* eh) {
  struct TimepointHashMapEntry* mapentry;
  HASH_FIND_INT(h->timepoint_hash_map, &eh->timepoint, mapentry);
  if (mapentry == NULL)
    return false;

  if (mapentry->event_list == NULL)
    return false;

  bool event_info_found = false;
  struct EventListEntry* evtentry;
  LL_FOREACH(mapentry->event_list, evtentry) {
    if (eh->event_list_elem == evtentry) {
      event_info_found = true;
      break;
    }
  }

  return event_info_found;
}

void rwn_history_unschedule(RwnHistory* h, RwnEventHandle* eh) {
  struct TimepointHashMapEntry* mapentry;
  HASH_FIND_INT(h->timepoint_hash_map, &eh->timepoint, mapentry);

  assert(is_event_handle_valid(h, eh));

  if (mapentry != NULL) {
    // free user data, if destroy_func is provided
    struct EventListEntry* evtentry = eh->event_list_elem;
    if (evtentry->user_event_destroy_func != NULL)
      evtentry->user_event_destroy_func(evtentry->user_event);

    // delete the evtentry from the list of event infos
    LL_DELETE(mapentry->event_list, eh->event_list_elem);
    free(eh->event_list_elem);

    // free the whole timepoint hashmap entry, if there are no events left
    int count;
    LL_COUNT(mapentry->event_list, evtentry, count);
    if (count == 0) {
      HASH_DEL(h->timepoint_hash_map, mapentry);
      free(mapentry);
    }
  }

  // free/invalidate the handle
  LL_DELETE(h->event_handle_list, eh);
  free(eh);
}

int rwn_history_state_delta(const RwnHistory* h,
                            int from_timepoint,
                            int to_timepoint,
                            void* state) {
  if (from_timepoint < 0 || to_timepoint < 0)
    return 0;

  if (to_timepoint < from_timepoint)
    return 0;

  int evtcount = 0;
  int i;
  for (i = from_timepoint; i < to_timepoint; ++i) {
    struct TimepointHashMapEntry* mapentry;
    HASH_FIND_INT(h->timepoint_hash_map, &i, mapentry);
    if (mapentry != NULL) {
      struct EventListEntry* evtentry;
      LL_FOREACH(mapentry->event_list, evtentry) {
        if (state != NULL && evtentry->user_event != NULL &&
            evtentry->user_event_apply_func != NULL) {
          evtentry->user_event_apply_func(evtentry->user_event, state);
          evtcount += 1;
        }
      }
    }
  }

  return evtcount;
}

int rwn_history_unschedule_all(RwnHistory* h,
                               int from_timepoint,
                               int to_timepoint) {
  if (from_timepoint < 0 || to_timepoint < 0)
    return 0;

  if (to_timepoint < from_timepoint)
    return 0;

  int evtcount = 0;

  // Iterate over issued event handles to find and unsched such events
  RwnEventHandle *eh, *eh_tmp;
  LL_FOREACH_SAFE(h->event_handle_list, eh, eh_tmp) {
    if (eh->timepoint >= from_timepoint && eh->timepoint < to_timepoint) {
      rwn_history_unschedule(h, eh);
      evtcount += 1;
    }
  }

  return evtcount;
}

int rwn_history_get_events(const RwnHistory* h,
                           int at_timepoint,
                           void** user_eventv) {
  if (rwn_history_count_events(h, at_timepoint) <= 0)
    return 0;

  int evtcount = 0;

  struct TimepointHashMapEntry* mapentry;
  HASH_FIND_INT(h->timepoint_hash_map, &at_timepoint, mapentry);
  if (mapentry != NULL) {
    struct EventListEntry* evtentry;
    LL_FOREACH(mapentry->event_list, evtentry) {
      user_eventv[evtcount] = evtentry->user_event;
      evtcount += 1;
    }
  }

  return evtcount;
}
