/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
/*======
This file is part of Percona TokuBackup.

Copyright (c) 2006, 2015, Percona and/or its affiliates. All rights reserved.

    Percona TokuBackup is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2,
    as published by the Free Software Foundation.

     Percona TokuBackup is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Percona TokuBackup.  If not, see <http://www.gnu.org/licenses/>.

----------------------------------------

    Percona TokuBackup is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License, version 3,
    as published by the Free Software Foundation.

    Percona TokuBackup is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with Percona TokuBackup.  If not, see <http://www.gnu.org/licenses/>.
======= */

#ident "$Id$"

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
    source_file *first_file  = new source_file();
    source_file *second_file = new source_file();
    first_file->init(FIRST_NAME);
    second_file->init(SECOND_NAME);

    file_hash_table table;
    table.put(first_file);
    table.put(second_file);
    
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

    //    delete second_file;
    //delete first_file;
}

static void test_duplicates(void) {
    file_hash_table table;
    source_file * first_file = new source_file;
    source_file * second_file = new source_file;
    first_file->init(FIRST_NAME);
    second_file->init(SECOND_NAME);

    table.put(first_file);
    check(table.size() == 1);

    table.put(first_file);
    check(table.size() == 1);

    table.put(second_file);
    check(table.size() == 2);

    table.put(first_file);
    check(table.size() == 2);

    table.put(second_file);
    check(table.size() == 2);

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

    assert(table.get(SECOND_NAME)!=NULL);
    table.remove(first_file);    check(table.size()==1);
    temp = table.get(FIRST_NAME);
    assert(table.get(SECOND_NAME)!=NULL);
    if (temp != NULL) {
        fail();
        printf("Should not have returned a file after removing it once: %s\n", temp->name());
        abort();
    }

    assert(table.get(SECOND_NAME)!=NULL);
    table.remove(first_file);    check(table.size()==1);
    assert(table.get(SECOND_NAME)!=NULL);
    table.remove(first_file);    check(table.size()==1);
    table.remove(first_file);    check(table.size()==1);
    temp = table.get(SECOND_NAME);
    if (temp == NULL) {
        fail();
        printf("Should have gotten second file back, even after multiple remove() calls.\n");
        abort();
    }

    table.remove(second_file);    check(table.size()==0);
    temp = table.get(SECOND_NAME);
    if (temp != NULL) {
        fail();
        printf("Should have gotten NULL back, but second file still in hash table.\n");
        abort();
    }
    delete first_file;
    delete second_file;
}

static void seriously_test_duplicates(void) {
    const int N = 100;
    char *fnames[N];
    source_file *files[N];
    file_hash_table table;
    for (int i=0; i<N; i++) {
        char str[100];
        snprintf(str, sizeof(str), "foo%d", i);
        fnames[i] = strdup(str);
        files[i] = new source_file();
        files[i]->init(fnames[i]);
        table.put(files[i]);
        check(table.size()==i+1);
    }
    for (int i=0; i<N; i++) {
        table.put(files[i]);
        check(table.size()==N);
    }
    for (int i=0; i<N; i++) {
        free(fnames[i]);
    }
}

static void test_empty_hash_and_remove(void) {
    file_hash_table table;
    source_file *temp = table.get(FIRST_NAME);
    if (temp != NULL) {
        fprintf(stderr, "Returned pointer that should have been null.");
        abort();
    }

    source_file * first_file = new source_file();
    source_file * second_file = new source_file();
    first_file->init(FIRST_NAME);
    second_file->init(SECOND_NAME);

    table.put(first_file);
    table.put(second_file);

    table.remove(second_file);
    temp = table.get(SECOND_NAME);
    if (temp != NULL) {
        fail();
        fprintf(stderr, "Returned pointer that should have been null.");
        abort();
    }

    table.remove(first_file);
    temp = table.get(FIRST_NAME);
    if (temp != NULL) {
        fail();
        fprintf(stderr, "Returned pointer that should have been null.");
        abort();
    }
    delete first_file;
    delete second_file;
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
