//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    resolver.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    02/07/2014
//:
//:   Licensed under the Apache License, Version 2.0 (the "License");
//:   you may not use this file except in compliance with the License.
//:   You may obtain a copy of the License at
//:
//:       http://www.apache.org/licenses/LICENSE-2.0
//:
//:   Unless required by applicable law or agreed to in writing, software
//:   distributed under the License is distributed on an "AS IS" BASIS,
//:   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//:   See the License for the specific language governing permissions and
//:   limitations under the License.
//:
//: ----------------------------------------------------------------------------
#ifndef _RESOLVER_H
#define _RESOLVER_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "ndebug.h"
#include "host_info.h"

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define RESOLVER_DEFAULT_LDB_PATH "/tmp/addr_info_cache.db"

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
namespace leveldb {
        class DB;
}

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class resolver
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        int32_t init(std::string addr_info_cache_db = "", bool a_use_cache = false);

        ~resolver();

        // Settings...
        void set_verbose(bool a_val) { m_verbose = a_val;}
        void set_color(bool a_val) { m_color = a_val;}
        void set_timeout_s(int32_t a_val) {m_timeout_s = a_val;}
        int32_t cached_resolve(std::string &a_host, uint16_t a_port, host_info_t &a_host_info);

        // -------------------------------------------------
        // Class methods
        // -------------------------------------------------
        // Get the singleton instance
        static resolver *get(void);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(resolver)
        resolver();

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        bool m_is_initd;

        // -------------------------------------------------
        // Settings
        // -------------------------------------------------
        bool m_verbose;
        bool m_color;
        uint32_t m_timeout_s;
        uint32_t m_use_cache;

        leveldb::DB* m_ai_db;

        // -------------------------------------------------
        // Class members
        // -------------------------------------------------
        // the pointer to the singleton for the instance 
        static resolver *m_singleton_ptr;

};


#endif






