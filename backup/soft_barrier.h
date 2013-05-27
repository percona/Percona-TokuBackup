/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef SOFT_BARRIER_H
#define SOFT_BARRIER_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: fmap.h 55798 2013-04-20 12:33:47Z christianrober $"

/* The soft barrier provides a way to go from capture mode to no-capture mode.
 * Capture threads perform operations (such as open, pwrite, etc.)  Each operation must be performed in capture mode or in no-capture mode.
 * In no-capture mode, the capture threads simply perform the underlying open, or pwrite or whatever.  They may modify the backup data strutures too.
 * In capture mode, the capture threads also modify the backup.
 *
 * Initially we are in no-capture mode.
 *
 * A backup thread sets the system to capture mode and then will start backing up the data.  Before the backup starts, we want to make sure
 *  that any operation that has started in no-capture mode finishes.  (Meanwhile new capture operations run in capture mode).
 *
 * When the backup finishes, we turn off capture mode, and then we want to make sure that any operation that has started in capture mode finishes.
 * 
 * Everthing is static, there is only one soft barrier.
 */
#include <pthread.h>
#include <stdint.h>

// The backup system employs "soft barriers" to separate episodes of capturing from noncapturing.
// Every client operation arranges to call
//   b = enter_operation();
//   do operation assuming that we are capturing if b is true, and not capturing if b is false.
//   finish_operation(b);
// The backup thread does
//    do operations assuming that all client operations that are in flight know that capturing is false.
//    set_capture_mode_and_wait();
//    do operations assuming that all client operations that are in flight know that capturing is true.
//    clear_capture_mode_and_wait();
//    do operations assuming that all client operations that are in flight know that capturing is false.

class barrier_location {
  private:
    const char *file;
    int   line;
    int   count;
  public:
    barrier_location(const char *f, int l) throw() : file(f), line(l), count(0) {}
    void increment(int inc) { __sync_fetch_and_add(&count, inc); }
};
        

class soft_barrier {
public:
    static void set_capture_mode_and_wait(void);
    // Effect: Set capture mode to true, and wait until every thread that entered while capture mode was false to call finish.
    // Requires: When calling this, capture mode is false.

    static void clear_capture_mode_and_wait(void);
    // Effect: Set capture mode to true, and wait until every thread that entered while capture mode was true to call finish.
    // Requires: When calling this, capture mode is true

    static bool enter_operation(void); // enter, and return true if we are in capture mode, false if we are not.
    static void finish_operation(bool); // exit, pass in which mode we were in.

    static bool enter_operation(barrier_location *bloc);
    static void finish_operation(bool, barrier_location *bloc);
private:
    static pthread_mutex_t   mutex;
    static pthread_cond_t    cond;
    static bool              capture_mode;
    static uint64_t          counts[2];
};

#endif
