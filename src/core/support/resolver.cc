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
#include "base64.h"

// json support
#include "rapidjson/document.h"

// For getaddrinfo
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <algorithm>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

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
        if(m_use_cache)
        {
                // Create a cache key
                char l_port_str[8];
                snprintf(l_port_str, 8, "%d", a_port);
                l_cache_key = a_host + ":" + l_port_str;

                // Lookup in map
                pthread_mutex_lock(&m_cache_mutex);
                if(m_ai_cache_map.find(l_cache_key) != m_ai_cache_map.end())
                {
                        l_ai_result = m_ai_cache_map[l_cache_key];
                        memcpy(&a_host_info, l_ai_result.data(),l_ai_result.length());
                        pthread_mutex_unlock(&m_cache_mutex);
                        return STATUS_OK;
                }
                pthread_mutex_unlock(&m_cache_mutex);

        }

        // Else proceed with slow resolution

        // Initialize...
        a_host_info.m_sa_len = sizeof(a_host_info.m_sa);
        memset((void*) &a_host_info.m_sa, 0, a_host_info.m_sa_len);

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

                // Add to map
                pthread_mutex_lock(&m_cache_mutex);
                m_ai_cache_map[l_cache_key] = l_ai_result;
                pthread_mutex_unlock(&m_cache_mutex);

        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t resolver::sync_ai_cache(void)
{

        int32_t l_status = 0;
        // Open
        FILE *l_file_ptr = fopen(m_ai_cache_file.c_str(), "w+");
        if(l_file_ptr == NULL)
        {
                NDBG_PRINT("Error performing fopen. Reason: %s\n", strerror(errno));
                return STATUS_ERROR;
        }

        // Write

        fprintf(l_file_ptr, "[");

        uint32_t l_len = m_ai_cache_map.size();
        uint32_t i_len = 0;
        for(ai_cache_map_t::const_iterator i_entry = m_ai_cache_map.begin();
            i_entry != m_ai_cache_map.end();
            ++i_entry)
        {
                std::string l_ai_val64 = base64_encode((const unsigned char *)i_entry->second.c_str(), i_entry->second.length());

                fprintf(l_file_ptr, "{");
                fprintf(l_file_ptr, "\"host\": \"%s\",", i_entry->first.c_str());
                fprintf(l_file_ptr, "\"ai\": \"%s\"", l_ai_val64.c_str());

                ++i_len;
                if(i_len == l_len)
                fprintf(l_file_ptr, "}");
                else
                fprintf(l_file_ptr, "},");
        }

        fprintf(l_file_ptr, "]");

        // Close
        l_status = fclose(l_file_ptr);
        if(l_status != 0)
        {
                NDBG_PRINT("Error performing fclose. Reason: %s\n", strerror(errno));
                return STATUS_ERROR;
        }

        return STATUS_OK;

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t resolver::read_ai_cache(const std::string &a_ai_cache_file)
{
        // ---------------------------------------
        // Check is a file
        // TODO
        // ---------------------------------------
        struct stat l_stat;
        int32_t l_status = STATUS_OK;
        l_status = stat(a_ai_cache_file.c_str(), &l_stat);
        if(l_status != 0)
        {
                //NDBG_PRINT("Error performing stat on file: %s.  Reason: %s\n", a_ai_cache_file.c_str(), strerror(errno));
                return STATUS_OK;
        }

        // Check if is regular file
        if(!(l_stat.st_mode & S_IFREG))
        {
                //NDBG_PRINT("Error opening file: %s.  Reason: is NOT a regular file\n", a_ai_cache_file.c_str());
                return STATUS_OK;
        }

        // ---------------------------------------
        // Open file...
        // ---------------------------------------
        FILE * l_file;
        l_file = fopen(a_ai_cache_file.c_str(),"r");
        if (NULL == l_file)
        {
                //NDBG_PRINT("Error opening file: %s.  Reason: %s\n", a_ai_cache_file.c_str(), strerror(errno));
                return STATUS_OK;
        }

        // ---------------------------------------
        // Read in file...
        // ---------------------------------------
        int32_t l_size = l_stat.st_size;
        int32_t l_read_size;
        char *l_buf = (char *)malloc(sizeof(char)*l_size);
        l_read_size = fread(l_buf, 1, l_size, l_file);
        if(l_read_size != l_size)
        {
                //NDBG_PRINT("Error performing fread.  Reason: %s [%d:%d]\n",
                //                strerror(errno), l_read_size, l_size);
                return STATUS_OK;
        }
        std::string l_buf_str;
        l_buf_str.assign(l_buf, l_size);


        // NOTE: rapidjson assert's on errors -interestingly
        rapidjson::Document l_doc;
        l_doc.Parse(l_buf_str.c_str());
        if(!l_doc.IsArray())
        {
                //NDBG_PRINT("Error reading json from file: %s.  Reason: data is not an array\n",
                //                a_ai_cache_file.c_str());
                return STATUS_OK;
        }

        // rapidjson uses SizeType instead of size_t.
        for(rapidjson::SizeType i_record = 0; i_record < l_doc.Size(); ++i_record)
        {
                if(!l_doc[i_record].IsObject())
                {
                        //NDBG_PRINT("Error reading json from file: %s.  Reason: array membe not an object\n",
                        //                a_ai_cache_file.c_str());
                        return STATUS_OK;
                }

                std::string l_host;
                std::string l_ai;
                if(l_doc[i_record].HasMember("host"))
                {
                        l_host = l_doc[i_record]["host"].GetString();

                        if(l_doc[i_record].HasMember("ai"))
                        {
                                l_ai = l_doc[i_record]["ai"].GetString();

                                std::string l_ai_decoded;
                                l_ai_decoded = base64_decode(l_ai);

                                m_ai_cache_map[l_host] = l_ai_decoded;
                        }
                }
        }

        // ---------------------------------------
        // Close file...
        // ---------------------------------------
        l_status = fclose(l_file);
        if (STATUS_OK != l_status)
        {
                NDBG_PRINT("Error performing fclose.  Reason: %s\n", strerror(errno));
                return STATUS_ERROR;
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t resolver::init(std::string addr_info_cache_file, bool a_use_cache)
{
        // Check if already is initd
        if(m_is_initd)
        {
                return STATUS_OK;
        }
        // Mark as initialized
        m_is_initd = true;

        m_use_cache = a_use_cache;
        if(m_use_cache)
        {
                // Init with cache file if exists
                m_ai_cache_file = addr_info_cache_file;
                if(m_ai_cache_file.empty())
                {
                        //NDBG_PRINT("Using: %s\n", l_db_name.c_str());
                        m_ai_cache_file = RESOLVER_DEFAULT_AI_CACHE_FILE;
                }

                // TODO Read from json file into map
                int32_t l_status = read_ai_cache(m_ai_cache_file);
                if(l_status != STATUS_OK)
                {
                        return STATUS_ERROR;
                }
                // Base64 decode the entries

        }


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
        m_cache_mutex(),
        m_ai_cache_map(),
        m_ai_cache_file(RESOLVER_DEFAULT_AI_CACHE_FILE)
{
        // Init mutex
        pthread_mutex_init(&m_cache_mutex, NULL);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
resolver::~resolver()
{
        // Sync back to disk
        sync_ai_cache();

        // Init mutex
        pthread_mutex_destroy(&m_cache_mutex);

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
pthread_mutex_t g_singleton_mutex = PTHREAD_MUTEX_INITIALIZER;
resolver *resolver::get(void)
{
        if (m_singleton_ptr == NULL) {
                pthread_mutex_lock(&g_singleton_mutex);
                //If not yet created, create the singleton instance
                if (m_singleton_ptr == NULL) {
                        m_singleton_ptr = new resolver();
                }
                pthread_mutex_unlock(&g_singleton_mutex);
        }
        return m_singleton_ptr;
}

//: ----------------------------------------------------------------------------
//: Class variables
//: ----------------------------------------------------------------------------
// the pointer to the singleton for the instance 
resolver *resolver::m_singleton_ptr;




