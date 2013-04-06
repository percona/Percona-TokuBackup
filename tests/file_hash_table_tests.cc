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

static int test_hash_collisions(void)
{
    int result = 0;    
    return result;
}

static int test_add(void)
{
    int result = 0;
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
        result = -1;
    }

    temp = table.get(SECOND_NAME);
    r = strcmp(temp->name(), SECOND_NAME);
    if (r != 0) {
        fail();
        printf("Returned file in hash does not match SECOND input file.");
        result = -2;
    }
    
    return result;
}

static int test_duplicates(void)
{
    int result = 0;
    file_hash_table table;
    source_file first_file(FIRST_NAME);
    source_file second_file(SECOND_NAME);

    printf("hash table size = %d\n", table.size());
    table.put(&first_file);
    printf("hash table size = %d\n", table.size());
    table.put(&first_file);
    printf("hash table size = %d\n", table.size());
    table.put(&second_file);
    printf("hash table size = %d\n", table.size());
    table.put(&first_file);
    printf("hash table size = %d\n", table.size());
    source_file * temp = table.get(FIRST_NAME);
    if (temp == NULL) {
        result = -1;
        fail();
        printf("Should NOT have gotten NULL  after duplicate entries added.\n");
        return result;
    }

    if (strcmp(temp->name(), FIRST_NAME) != 0) {
        result = -2;
        fail();
        printf("Should have returned first directory after duplicate entries added.\n");
        return result;
    }

    table.remove(&first_file);
    printf("hash table size = %d\n", table.size());
    temp = table.get(FIRST_NAME);
    if (temp != NULL) {
        result = -3;
        fail();
        printf("Should not have returned a file after removing it once: %s\n", temp->name());
        return result;
    }

    table.remove(&first_file);
    printf("hash table size = %d\n", table.size());
    table.remove(&first_file);
    printf("hash table size = %d\n", table.size());
    table.remove(&first_file);
    printf("hash table size = %d\n", table.size());
    temp = table.get(SECOND_NAME);
    if (temp == NULL) {
        result = -4;
        fail();
        printf("Should have gotten second file back, even after multiple remove() calls.\n");
        return result;
    }

    table.remove(&second_file);
    printf("hash table size = %d\n", table.size());
    temp = table.get(SECOND_NAME);
    if (temp != NULL) {
        result = -5;
        fail();
        printf("Should have gotten NULL back, but second file still in hash table.\n");
        return result;
    }

    return result;
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


static int file_hash_table_tests(void)
{
    int result = 0;
    result = test_hash_collisions();
    result = test_add();
    if (result != 0) {
        return result;
    }

    result = test_duplicates();
    if (result != 0) {
        return result;
    }

    result = test_empty_hash_and_remove();
    if (result != 0) {
        return result;
    }

    result = test_concurrent_access();
    if (result != 0) {
        return result;
    }

    pass();
    return result;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    int result = file_hash_table_tests();
    return result;
}
