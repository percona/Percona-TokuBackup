/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."

#ifndef DIRECTORY_SET_H
#define DIRECTORY_SET_H

#include <dirent.h>

    class Directory_Set {
    public:
        //----------------------------------------------------------
        // Allocates memory for storing given directories.
        Directory_Set(const int count,
                      const char **sources,
                      const char **destinations);
        
        //----------------------------------------------------------
        // De-allocates memory allocated in constructor and realpath()
        // calls.
        ~Directory_Set();

        //----------------------------------------------------------
        // Creates realpath() versions of currently set directory
        // paths.  Returns an error if realpath() fails.
        int Update_To_Full_Path(void);

        //----------------------------------------------------------
        // Returns an error if any of the following criteria are NOT
        // met:
        int Validate(void) const;

        //----------------------------------------------------------
        // Returns index of matching source dir, or -1 if given file
        // not in directory set.
        int Find_Index_Matching_Prefix(const char *file) const;

        //----------------------------------------------------------
        // Accessors for number of, source, and destination
        // directories.
        const char *Source_Directory_At(const int index) const;
        const char *Destination_Directory_At(const int index) const;
        int Number_Of_Directories() const;

    private:
        const char **m_sources;
        const char **m_destinations;
        const int m_count;
        bool m_real_path_successful;
        Directory_Set();
        int verify_destination_is_empty(const int index, DIR *dir) const;
        void handle_realpath_results(const int r, const int allocated_pairs);
        int update_to_real_path_on_index(const int i);
        int verify_no_two_directories_are_the_same(void);
    };

#endif // End of header guardian.
