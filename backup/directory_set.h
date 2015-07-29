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

#ifndef DIRECTORY_SET_H
#define DIRECTORY_SET_H

#include <dirent.h>

    class directory_set {
    public:
        //----------------------------------------------------------
        // Allocates memory for storing given directories.
        directory_set(const int count,
                      const char **sources,
                      const char **destinations);
        
        //----------------------------------------------------------
        // De-allocates memory allocated in constructor and realpath()
        // calls.
        ~directory_set();

        //----------------------------------------------------------
        // Creates realpath() versions of currently set directory
        // paths.  Returns an error if realpath() fails.
        int update_to_full_path(void);

        //----------------------------------------------------------
        // Returns an error if any of the following criteria are NOT
        // met:
        int validate(void) const;

        //----------------------------------------------------------
        // Returns index of matching source dir, or -1 if given file
        // not in directory set.
        int find_index_matching_prefix(const char *file) const;

        //----------------------------------------------------------
        // Accessors for number of, source, and destination
        // directories.
        const char *source_directory_at(const int index) const;
        const char *destination_directory_at(const int index) const;
        int number_of_directories() const;

    private:
        const char **m_sources;
        const char **m_destinations;
        const int m_count;
        bool m_real_path_successful;
        directory_set();
        int verify_destination_is_empty(const int index, DIR *dir) const;
        void handle_realpath_results(const int r, const int allocated_pairs);
        int update_to_real_path_on_index(const int i);
        int verify_no_two_directories_are_the_same(void);
    };

#endif // End of header guardian.
