/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/* Simple test of backup. */
#include <assert.h>
#include <backup.h>
#include <db.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void CKERR(int r) {
    if (r!=0) abort();
}

static int simple_poll(float progress, const char *progress_string, void *poll_extra __attribute__((__unused__))) {
    fprintf(stderr, "backup progress %.2f%%: %s\n", progress*100, progress_string);
    return 0;
}
static void simple_error(int errnum, const char *error_string, void *ignore __attribute__((__unused__))) {
    fprintf(stderr, "backup error: errnum=%d (%s)\n", errnum, error_string);
    abort();
}

#define BASEDIR "backup1"
#define ENVDIR    BASEDIR  ".source"
#define BACKUPDIR BASEDIR ".backup"

static void * start_backup_thread_fun(void* v) {
    assert(v==NULL);
    const char *srcs[1] = {ENVDIR};
    const char *dsts[1] = {BACKUPDIR};
    int r = tokubackup_create_backup(srcs, dsts, 1, simple_poll, NULL, simple_error, NULL);
    assert(r==0);
    return v;
}

DB_ENV *env;
const int envflags = DB_INIT_LOCK| DB_INIT_LOG| DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE |DB_PRIVATE | DB_RECOVER;

int main (int argc __attribute__((__unused__)), char * const argv[] __attribute__((__unused__))) {
    { int r = system("rm -rf " ENVDIR);                                    CKERR(r); }
    { int r = system("rm -rf " BACKUPDIR);                                 CKERR(r); }
    { int r = mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r); }
    { int r = mkdir(BACKUPDIR, S_IRWXU+S_IRWXG+S_IRWXO);                   CKERR(r); }

    { int r = db_env_create(&env, 0);                                      CKERR(r); }
    env->set_errfile(env, stderr);

    { int r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);   CKERR(r); }

    pthread_t thread;
    { int r = pthread_create(&thread, NULL, start_backup_thread_fun, NULL); CKERR(r); }

    DB *db;
    { int r = db_create(&db, env, 0);                                            CKERR(r); }
    { int r = db->open(db, NULL, "hello.data", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(r); }

    {
        DBT key = { .data = (void*)"hello", .size = 6, .ulen = 0, .flags = 0 };
        DBT val = { .data = (void*)"kitty", .size = 6, .ulen = 0, .flags = 0 };
        int r = db->put(db, 0, &key, &val, 0);
        CKERR(r);
    }

    { int r = db->close(db, 0);                                                  CKERR(r); }

    { int r = env->close(env, 0);                                                CKERR(r); }

    {
        void *v;
        int r = pthread_join(thread, &v);
        assert(r==0 && v==NULL);
    }

    // Now make sure that the database is good.
    { int r = db_env_create(&env, 0);                                            CKERR(r); }
    env->set_errfile(env, stderr);
    { int r = env->open(env, BACKUPDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);      CKERR(r); }
    { int r = db_create(&db, env, 0);                                            CKERR(r); }
    { int r = db->open(db, NULL, "hello.data", NULL, DB_BTREE, 0, 0666);         CKERR(r); }
    {
        DBT key = { .data = (void*)"hello", .size = 6, .ulen = 0, .flags = 0 };
        char buf[10];
        DBT val = { .data = buf,            .size = 0, .ulen = 10, .flags = DB_DBT_USERMEM };
        int r = db->get(db, 0, &key, &val, 0);
        CKERR(r);
        assert(val.size==6 && memcmp("kitty", buf, 6)==0);
    }
    { int r = db->close(db, 0);                                                  CKERR(r); }
    { int r = env->close(env, 0);                                                CKERR(r); }

    return 0;
}
