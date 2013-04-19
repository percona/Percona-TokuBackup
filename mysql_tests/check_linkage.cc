/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:  
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id: manager.cc 55673 2013-04-18 06:15:08Z bkuszmaul $"

/* Check that the symbols are resolving properly in mysqld. */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <vector>

const char *progname;
const char *exename;

void usage(int exitcode) {
    printf("Usage: %s [--help | path_to_mysqld]\n", progname);
    exit(exitcode);
}

int main (int argc, const char *argv[]) {
    progname = argv[0];
    if (argc!=2) usage(1);
    if (strcmp(argv[1], "--help")==0) usage(0);
    exename = argv[1];

    {
        struct stat buf;
        int r = stat(exename, &buf);
        assert(r==0);
        assert(S_ISREG(buf.st_mode));
    }
    {
        size_t len = strlen(exename) + strlen(progname) + 100;
        char string[len];
        {
            size_t actual_len = snprintf(string, len, "ldd %s > %s.data", exename, progname);
            assert(actual_len<len);
        }
        int r = system(string);
        assert(r==0);
    }
    std::vector<char*> libs;
    {
        size_t len = strlen(progname)+100;
        char fname[len];
        {
            size_t actual_len = snprintf(fname, len, "%s.data", progname);
            assert(actual_len<len);
        }
        FILE *f = fopen(fname, "r");
        assert(f!=NULL);
        char *line = (char*)malloc(1);
        size_t linesize=1;
        while (1) {
            ssize_t r = getline(&line, &linesize, f);
            if(r==-1) break;
            char *filestart;
            {
                char *location = strstr(line, " => ");
                if (location==NULL) {
                    continue; // sometimes there is no =>
                } else if (location[4]==' ' || location[4]=='(') {
                    continue; // sometimes it doesn't find anything.
                } else {
                    filestart=location+4;
                }
            }
            {
                char *end = strchr(filestart, ' ');
                assert(end);
                size_t len = end-filestart;
                char *filename = (char*)malloc(len+1);
                strncpy(filename, filestart, len);
                filename[len]=0;
                libs.push_back(filename);
            }
        }
        free(line);
        {
            int r = fclose(f);
            assert(r==0);
        }
    }

    const char *symbols[] = {"open64", "write", "pwrite64", "read", "lseek64", "ftruncate64", "truncate64", "unlink" , "rename", "mkdir", NULL};
    int result_status = 0;
    for (int symnum=0; symbols[symnum]; symnum++) {
        const char *sym = symbols[symnum];
        //printf("checking symbol %s\n", sym);
        for (size_t libnum=0; libnum<libs.size(); libnum++) {
            size_t len = strlen(progname);
            len += strlen(libs[libnum]);
            len += strlen(sym);
            len += 100;
            char cmd[len];
            size_t actual_len = snprintf(cmd, len, "nm -D %s | egrep \"[WT] %s\\$\" > %s.searched", libs[libnum], sym, progname);
            assert(actual_len<len);
            //printf("about to do %s\n", cmd);
            int r = system(cmd);
            assert(WIFEXITED(r));
            if (WEXITSTATUS(r)==0) {
                //printf("yes\n");
                if (strstr(libs[libnum], "libHotBackup.so")==NULL) {
                    fprintf(stderr, "Symbol %s is found in %s ahead of libHotBackup\n", sym, libs[libnum]);
                }
                break;
            } else if (WEXITSTATUS(r)==1) {
                if (strstr(libs[libnum], "libHotBackup.so")!=NULL) {
                    fprintf(stderr, "Symbol %s is not in libHotBackup.so\n", sym);
                    result_status = 1;
                }
                break;
            } else {
                abort();
            }
        }
    }
    

    // Free everything
    while (libs.size()>0) {
        char *n = libs[libs.size()-1];
        libs.pop_back();
        free(n);
    }

    // Should also check that the versions of open, pwrite, lseek, ftruncate, and truncate aren't unresolved in the original source.
    for (int symnum=0; symbols[symnum]; symnum++) {
        const char *sym = symbols[symnum];
        if (strstr(sym, "64")!=NULL) {
            size_t slen = strlen(sym);
            size_t len =  strlen(exename) + slen + 100;
            char cmd[len];
            snprintf(cmd, len, "nm %s | egrep \" %.*s($|@)\" > /dev/null", exename, (int)slen-2, sym);
            if (1) printf("cmd=%s\n", cmd);
            int r = system(cmd);
            assert(WIFEXITED(r));
            if (WEXITSTATUS(r)==0) {
                fprintf(stderr, "I expected %.*s not to be used (in favor of of %s).  But the executable uses the short version.\n",
                        (int)slen-2, sym, sym);
                result_status = 1;
            } else {
                assert(WEXITSTATUS(r)==1); // I don't want that function to be there.
            }
        }
    }

    return result_status;
}
    
