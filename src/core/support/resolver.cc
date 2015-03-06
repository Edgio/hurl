//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    resolver.cc
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

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
//#include "ndebug.h"
#include "resolver.h"

#pragma GCC diagnostic push
// ignore pedantic warnings
#pragma GCC diagnostic ignored "-Weffc++"
#include "../../leveldb/db.h"
// back to default behaviour
#pragma GCC diagnostic pop

// For getaddrinfo
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <algorithm>
#include <string.h>
#include <unistd.h>


//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define RESOLVER_ERROR(...)\
        do { \
                char _buf[1024];\
                sprintf(_buf, __VA_ARGS__);\
                ao_error = _buf;\
                if(m_verbose)\
                {\
                        fprintf(stdout, "%s:%s.%d: ", __FILE__, __FUNCTION__, __LINE__); \
                        fprintf(stdout, __VA_ARGS__);               \
                        fflush(stdout); \
                }\
        }while(0)

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t resolver::cached_resolve(std::string &a_host,
                                 uint16_t a_port,
                                 host_info_t &a_host_info,
                                 std::string &ao_error)
{

        if(!m_is_initd)
        {
                init();
        }

        // ---------------------------------------
        // Cache lookup
        // ---------------------------------------
        std::string l_ai_result;
        std::string l_cache_key;
        leveldb::Status l_ldb_status;
        if(m_use_cache)
        {
                // Create a cache key
                char l_port_str[8];
                snprintf(l_port_str, 8, "%d", a_port);
                l_cache_key = a_host + ":" + l_port_str;

                //NDBG_PRINT("CACHE_KEY: %s\n", l_cache_key.c_str());

                l_ldb_status = m_ai_db->Get(leveldb::ReadOptions(), l_cache_key, &l_ai_result);
                // If we found it...
                if(l_ldb_status.ok())
                {
                        // Copy the data into host info
                        //NDBG_PRINT("FOUND CACHE_KEY: %s\n", l_cache_key.c_str());
                        memcpy(&a_host_info, l_ai_result.data(),l_ai_result.length());
                        return STATUS_OK;
                }

                // Check for errors
                if (!l_ldb_status.IsNotFound())
                {
                        RESOLVER_ERROR("Error: performing get on address info db\n");
                        return STATUS_ERROR;
                }
        }

        // Else proceed with slow resolution

        // Initialize...
        a_host_info.m_sa_len = sizeof(a_host_info.m_sa);
        memset((void*) &a_host_info.m_sa, 0, a_host_info.m_sa_len);

        //NDBG_PRINT("RESOLVE:\n");

        // ---------------------------------------
        // get address...
        // ---------------------------------------
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        char portstr[10];
        snprintf(portstr, sizeof(portstr), "%d", (int) a_port);
        struct addrinfo* l_addrinfo;

        int gaierr;
        gaierr = getaddrinfo(a_host.c_str(), portstr, &hints, &l_addrinfo);
        if (gaierr != 0)
        {
                RESOLVER_ERROR("Error getaddrinfo '%s': %s\n", a_host.c_str(), gai_strerror(gaierr));
                return STATUS_ERROR;
        }

        // Find the first IPv4 and IPv6 entries.
        struct addrinfo* l_addrinfo_v4 = NULL;
        struct addrinfo* l_addrinfo_v6 = NULL;
        for (struct addrinfo* i_addrinfo = l_addrinfo;
             i_addrinfo != (struct addrinfo*) 0;
             i_addrinfo = i_addrinfo->ai_next)
        {
                switch (i_addrinfo->ai_family)
                {
                case AF_INET:
                        if (l_addrinfo_v4 == (struct addrinfo*) 0)
                                l_addrinfo_v4 = i_addrinfo;
                        break;
                case AF_INET6:
                        if (l_addrinfo_v6 == (struct addrinfo*) 0)
                                l_addrinfo_v6 = i_addrinfo;
                        break;
                }
        }

        //NDBG_PRINT("RESOLVE:\n");

        // If there's an IPv4 address, use that, otherwise try IPv6.
        if (l_addrinfo_v4 != NULL)
        {
                if (sizeof(a_host_info.m_sa) < l_addrinfo_v4->ai_addrlen)
                {
                        RESOLVER_ERROR("Error %s - sockaddr too small (%lu < %lu)\n", a_host.c_str(),
                              (unsigned long) sizeof(a_host_info.m_sa),
                              (unsigned long) l_addrinfo_v4->ai_addrlen);
                        return STATUS_ERROR;

                }
                a_host_info.m_sock_family = l_addrinfo_v4->ai_family;
                a_host_info.m_sock_type = l_addrinfo_v4->ai_socktype;
                a_host_info.m_sock_protocol = l_addrinfo_v4->ai_protocol;
                a_host_info.m_sa_len = l_addrinfo_v4->ai_addrlen;

                //NDBG_PRINT("memmove: addrlen: %d\n", l_addrinfo_v4->ai_addrlen);
                //ns_hlo::mem_display((const uint8_t *)l_addrinfo_v4->ai_addr, l_addrinfo_v4->ai_addrlen);
                //show_host_info();

                memmove(&a_host_info.m_sa, l_addrinfo_v4->ai_addr, l_addrinfo_v4->ai_addrlen);
                freeaddrinfo(l_addrinfo);
        }
        else if (l_addrinfo_v6 != NULL)
        {
                if (sizeof(a_host_info.m_sa) < l_addrinfo_v6->ai_addrlen)
                {
                        RESOLVER_ERROR("Error %s - sockaddr too small (%lu < %lu)\n", a_host.c_str(),
                              (unsigned long) sizeof(a_host_info.m_sa),
                              (unsigned long) l_addrinfo_v6->ai_addrlen);
                        return STATUS_ERROR;
                }
                a_host_info.m_sock_family = l_addrinfo_v6->ai_family;
                a_host_info.m_sock_type = l_addrinfo_v6->ai_socktype;
                a_host_info.m_sock_protocol = l_addrinfo_v6->ai_protocol;
                a_host_info.m_sa_len = l_addrinfo_v6->ai_addrlen;

                //NDBG_PRINT("memmove: addrlen: %d\n", l_addrinfo_v6->ai_addrlen);
                //ns_hlo::mem_display((const uint8_t *)l_addrinfo_v6->ai_addr, l_addrinfo_v6->ai_addrlen);
                //show_host_info();

                memmove(&a_host_info.m_sa, l_addrinfo_v6->ai_addr, l_addrinfo_v6->ai_addrlen);
                freeaddrinfo(l_addrinfo);
        }
        else
        {
                RESOLVER_ERROR("Error no valid address found for host %s\n", a_host.c_str());
                return STATUS_ERROR;

        }

        // Set the port
        a_host_info.m_sa.sin_port = htons(a_port);

        //show_host_info();

        if(m_use_cache)
        {
                // Add the host info to the database
                l_ai_result.clear();
                l_ai_result.append((char *)&a_host_info, sizeof(a_host_info));
                l_ldb_status = m_ai_db->Put(leveldb::WriteOptions(), l_cache_key, l_ai_result);
                // If we found it...
                if(l_ldb_status.ok())
                {
                        // Copy the data into host info
                        memcpy(&a_host_info, l_ai_result.data(),l_ai_result.length());
                        return STATUS_OK;
                }
        }

        return STATUS_OK;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t resolver::init(std::string addr_info_cache_db, bool a_use_cache)
{
        // Check if already is initd
        if(m_is_initd)
                return STATUS_OK;

        m_use_cache = a_use_cache;
        if(m_use_cache)
        {
                // Init with cache file if exists
                std::string l_db_name = addr_info_cache_db;
                if(l_db_name.empty())
                {
                        NDBG_PRINT("Using: %s\n", l_db_name.c_str());
                        l_db_name = RESOLVER_DEFAULT_LDB_PATH;
                }

                // TODO Options???
                leveldb::Options l_ldb_options;
                l_ldb_options.create_if_missing = true;

                leveldb::Status l_ldb_status;
                l_ldb_status = leveldb::DB::Open(l_ldb_options, l_db_name.c_str(), &m_ai_db);
                if (false == l_ldb_status.ok())
                {
                        printf("Error: Unable to open/create test database: %s\n", l_db_name.c_str());
                        return STATUS_ERROR;
                }
        }

        // Mark as initialized
        m_is_initd = true;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
resolver::resolver(void):
        m_is_initd(false),
        m_verbose(false),
        m_color(false),
        m_timeout_s(5),
        m_use_cache(false),
        m_ai_db(NULL)
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
resolver::~resolver()
{

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
resolver *resolver::get(void)
{
        if (m_singleton_ptr == NULL) {
                //If not yet created, create the singleton instance
                m_singleton_ptr = new resolver();

                // Initialize

        }
        return m_singleton_ptr;
}

//: ----------------------------------------------------------------------------
//: Class variables
//: ----------------------------------------------------------------------------
// the pointer to the singleton for the instance 
resolver *resolver::m_singleton_ptr;




