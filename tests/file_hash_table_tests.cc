/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: copy_files.cc 55013 2013-04-03 01:41:38Z bkuszmaul $"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "backup_test_helpers.h"
#include "file_hash_table.h"
#include "source_file.h"

const char *FIRST_NAME = "I/Am/Long/File/Path/HearMeRoar";
const char *SECOND_NAME = "YetAnotherFileNameThatIsLong";

static void test_hash_collisions(void) {
    // Todo: There is not test here.
}

static void test_add(void) {
    source_file first_file(FIRST_NAME);
    source_file second_file(SECOND_NAME);

    file_hash_table table;
    table.put(&first_file);
    table.put(&second_file);
    
    source_file *temp = table.get(FIRST_NAME);
    int r = strcmp(temp->name(), FIRST_NAME);
    if (r != 0) {
        fail();
        printf("Returned file in hash does not match SECOND input file.");
        abort();
    }

    temp = table.get(SECOND_NAME);
    r = strcmp(temp->name(), SECOND_NAME);
    if (r != 0) {
        fail();
        printf("Returned file in hash does not match SECOND input file.");
        abort();
    }
}

static void test_duplicates(void) {
    file_hash_table table;
    source_file first_file(FIRST_NAME);
    source_file second_file(SECOND_NAME);

    printf("hash table size = %d\n", table.size());

    table.put(&first_file);
    assert(table.size() == 1);
    printf("hash table size = %d\n", table.size());

    table.put(&first_file);
    assert(table.size() == 1);
    printf("hash table size = %d\n", table.size());

    table.put(&second_file);
    assert(table.size() == 2);
    printf("hash table size = %d\n", table.size());

    table.put(&first_file);
    assert(table.size() == 2);
    printf("hash table size = %d\n", table.size());

    table.put(&second_file);
    assert(table.size() == 2);
    printf("hash table size = %d\n", table.size());

    source_file * temp = table.get(FIRST_NAME);
    if (temp == NULL) {
        fail();
        printf("Should NOT have gotten NULL  after duplicate entries added.\n");
        abort();
    }

    if (strcmp(temp->name(), FIRST_NAME) != 0) {
        fail();
        printf("Should have returned first directory after duplicate entries added.\n");
        abort();
    }

    table.remove(&first_file);
    printf("hash table size = %d\n", table.size());
    temp = table.get(FIRST_NAME);
    if (temp != NULL) {
        fail();
        printf("Should not have returned a file after removing it once: %s\n", temp->name());
        abort();
    }

    table.remove(&first_file);
    printf("hash table size = %d\n", table.size());
    table.remove(&first_file);
    printf("hash table size = %d\n", table.size());
    table.remove(&first_file);
    printf("hash table size = %d\n", table.size());
    temp = table.get(SECOND_NAME);
    if (temp == NULL) {
        fail();
        printf("Should have gotten second file back, even after multiple remove() calls.\n");
        abort();
    }

    table.remove(&second_file);
    printf("hash table size = %d\n", table.size());
    temp = table.get(SECOND_NAME);
    if (temp != NULL) {
        fail();
        printf("Should have gotten NULL back, but second file still in hash table.\n");
        abort();
    }
}

static void seriously_test_duplicates(void) {
    const int N = 2*file_hash_table::BUCKET_MAX;
    char *fnames[N];
    source_file *files[N];
    file_hash_table table;
    for (int i=0; i<N; i++) {
        char str[100];
        snprintf(str, sizeof(str), "foo%d", i);
        fnames[i] = strdup(str);
        files[i] = new source_file(fnames[i]);
        table.put(files[i]);
        assert(table.size()==i+1);
    }
    for (int i=0; i<N; i++) {
        table.put(files[i]);
        assert(table.size()==N);
    }
    for (int i=0; i<N; i++) {
        delete files[i];
    }
}

static int test_empty_hash_and_remove(void)
{
    int result = 0;
    file_hash_table table;
    source_file *temp = table.get(FIRST_NAME);
    if (temp != NULL) {
        result = -1;
        printf("Returned pointer that should have been null.");
        return result;
    }

    source_file first_file(FIRST_NAME);
    source_file second_file(SECOND_NAME);

    table.put(&first_file);
    table.put(&second_file);

    table.remove(&second_file);
    temp = table.get(SECOND_NAME);
    if (temp != NULL) {
        result = -2;
        fail();
        printf("Returned pointer that should have been null.");
        return result;
    }

    table.remove(&first_file);
    temp = table.get(FIRST_NAME);
    if (temp != NULL) {
        result = -3;
        fail();
        printf("Returned pointer that should have been null.");
        return result;
    }

    return result;
}

static int test_concurrent_access(void)
{
    int result = 0;
    return result;
}


int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    test_hash_collisions();
    test_add();
    test_duplicates();
    seriously_test_duplicates();
    test_empty_hash_and_remove();
    test_concurrent_access();
    return 0;
}
