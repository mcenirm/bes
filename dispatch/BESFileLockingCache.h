// BESFileLockingCache.h

// This file was originally part of bes, A C++ back-end server
// implementation framework for the OPeNDAP Data Access Protocol.
// Copied to libdap. This is used to cache responses built from
// functional CE expressions.

// Copyright (c) 2012 OPeNDAP, Inc
// Author: James Gallagher <jgallagher@opendap.org>,
// Patrick West <pwest@ucar.edu> and Jose Garcia <jgarcia@ucar.edu>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//
// You can contact University Corporation for Atmospheric Research at
// 3080 Center Green Drive, Boulder, CO 80301

#ifndef BESFileLockingCache_h_
#define BESFileLockingCache_h_ 1

#include <unistd.h>

#include <map>
#include <string>
#include <list>

#include "BESObj.h"

// These typedefs are used to record information about the files in the cache.
// See BESFileLockingCache.cc and look at the purge() method.
typedef struct {
    string name;
    unsigned long long size;
    time_t time;
} cache_entry;

typedef std::list<cache_entry> CacheFiles;

/** @brief Implementation of a caching mechanism for compressed data.
 * This cache uses simple advisory locking found on most modern unix file systems.
 * Compressed files are uncompressed and stored in a cache where they can be
 * used over and over until removed from the cache. Several processes can
 * share the cache with each reading from files. At the same time, new files
 * can be added and the cache can be purged, without disrupting the existing
 * read operations.
 *
 * How it works. When a file is added to the cache, the cache is locked - no
 * other processes can add, read or remove files. Once a file has been added,
 * the cache size is examined and, if needed, the cache is purged so that its
 * size is 80% of the maximum size. Then the cache is unlocked. When a process
 * looks to see if a file is already in the cache, the entire cache is locked.
 * If the file is present, a shared read lock is obtained and the cache is unlocked.
 *
 * Methods: create_and_lock() and get_read_lock() open and lock files; the former
 * creates the file and locks it exclusively iff it does not exist, while the
 * latter obtains a shared lock iff the file already exists. The unlock()
 * methods unlock a file. The lock_cache_info() and unlock_cache_info() are
 * used to control access to the whole cache - with the open + lock and
 * close + unlock operations performed atomically. Other methods that operate
 * on the cache info file must only be called when the lock has been obtained.
 */
class BESFileLockingCache: public BESObj {
private:
    static const char DAP_CACHE_CHAR = '#';

protected:

    // pathname of the cache directory
    string d_cache_dir;

    // tack this on the front of each cache file name
    string d_prefix;

    /// How many bytes can the cache hold before we have to purge
    unsigned long long d_max_cache_size_in_bytes;

    // When we purge, how much should we throw away. Set in the ctor to 80% of the max size.
    unsigned long long d_target_size;

private:

    // Suppress the assignment operator and default copy ctor, ...
    BESFileLockingCache(const BESFileLockingCache &);
    BESFileLockingCache &operator=(const BESFileLockingCache &rhs);

    void m_check_ctor_params();
    void m_initialize_cache_info();

    unsigned long long m_collect_cache_dir_info(CacheFiles &contents);

    /// Name of the file that tracks the size of the cache
    string d_cache_info;
    int d_cache_info_fd;

    void m_record_descriptor(const string &file, int fd);
    int m_get_descriptor(const string &file);

    // map that relates files to the descriptor used to obtain a lock
    typedef std::multimap<string, int> FilesAndLockDescriptors;
    FilesAndLockDescriptors d_locks;

    // Removed; see comment in .cc file. void unlock_and_close(int fd);

protected:

    BESFileLockingCache(): d_cache_dir(""), d_prefix(""), d_max_cache_size_in_bytes(0), d_target_size(0),  d_cache_info(""), d_cache_info_fd(-1){};
    void initialize(const string &cache_dir, const string &prefix, unsigned long long size);
    BESFileLockingCache(const string &cache_dir, const string &prefix, unsigned long long size);
    virtual ~BESFileLockingCache() { if (d_cache_info_fd != -1){ close(d_cache_info_fd); d_cache_info_fd=-1;} }

public:

    virtual string get_cache_file_name(const string &src, bool mangle = true);

    virtual bool create_and_lock(const string &target, int &fd);
    virtual bool get_read_lock(const string &target, int &fd);
    virtual void exclusive_to_shared_lock(int fd);
    virtual void unlock_and_close(const string &target);
    // Removed. See comments in the .cc file virtual void unlock_and_close(int fd);

    virtual bool getExclusiveLock(string file_name, int &ref_fd);

    virtual void lock_cache_write();
    virtual void lock_cache_read();
    virtual void unlock_cache();

    virtual unsigned long long update_cache_info(const string &target);
    virtual bool cache_too_big(unsigned long long current_size) const;
    virtual unsigned long long get_cache_size();
    virtual void update_and_purge(const string &new_file);
    virtual void purge_file(const string &file);

    const string getCacheFilePrefix();
    const string getCacheDirectory();

    static bool dir_exists(const string &dir);
    // static string assemblePath(const string &firstPart, const string &secondPart, bool addLeadingSlash =  false);


    virtual void dump(ostream &strm) const ;
};

#endif // BESFileLockingCache_h_
