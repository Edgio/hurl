//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nresolver.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    11/20/2015
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
#include "nresolver.h"
#include "ndebug.h"

#include "base64.h"

// json support
//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Werror"
#include "rapidjson/document.h"
//#pragma GCC diagnostic pop

#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <netdb.h>

//#include <pcre.h>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
//#define IPV4_ADDR_REGEX "\\b\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\b"
// Note matches 999.999.999.999 -check inet_pton result...

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
//static void resolve_cb(void* mydata, int err, struct ub_result* result);

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nresolver::nresolver():
        m_is_initd(false),
        m_use_cache(false),
        m_cache_mutex(),
        m_ai_cache_map(),
        m_ai_cache_file(NRESOLVER_DEFAULT_AI_CACHE_FILE)
{
        pthread_mutex_init(&m_cache_mutex, NULL);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nresolver::~nresolver()
{
#if 0
        pcre_free(m_ip_address_re_compiled);
        if(m_ip_address_pcre_extra != NULL)
        {
                pcre_free(m_ip_address_pcre_extra);
        }
#endif

        // Sync back to disk
        sync_ai_cache();
        pthread_mutex_destroy(&m_cache_mutex);
        for(ai_cache_map_t::iterator i_h = m_ai_cache_map.begin();
            i_h != m_ai_cache_map.end();
            ++i_h)
        {
                if(i_h->second)
                {
                        delete i_h->second;
                        i_h->second = NULL;
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nresolver::init(std::string addr_info_cache_file, bool a_use_cache)
{
        if(m_is_initd)
        {
                return STATUS_OK;
        }
        m_use_cache = a_use_cache;
        if(m_use_cache)
        {
                m_ai_cache_file = addr_info_cache_file;
                if(m_ai_cache_file.empty())
                {
                        //NDBG_PRINT("Using: %s\n", l_db_name.c_str());
                        m_ai_cache_file = NRESOLVER_DEFAULT_AI_CACHE_FILE;
                }
                int32_t l_status = read_ai_cache(m_ai_cache_file);
                if(l_status != STATUS_OK)
                {
                        return STATUS_ERROR;
                }
        }
        m_is_initd = true;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
host_info_s *nresolver::lookup_sync(const std::string &a_host, uint16_t a_port)
{
        //NDBG_PRINT("%sRESOLVE%s: a_host: %s a_port: %d\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, a_host.c_str(), a_port);
        if(!m_is_initd)
        {
                init();
        }

        // ---------------------------------------
        // Cache lookup
        // ---------------------------------------
        host_info_s *l_host_info = NULL;
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
                        l_host_info = m_ai_cache_map[l_cache_key];
                        pthread_mutex_unlock(&m_cache_mutex);
                        return l_host_info;
                }
                pthread_mutex_unlock(&m_cache_mutex);
        }

        // ---------------------------------------
        // Else proceed with slow resolution
        // ---------------------------------------
        // Initialize...
        l_host_info = new host_info_s();
        l_host_info->m_sa_len = sizeof(l_host_info->m_sa);
        memset((void*) &(l_host_info->m_sa), 0, l_host_info->m_sa_len);

        // ---------------------------------------
        // get address...
        // ---------------------------------------
        struct addrinfo l_hints;
        memset(&l_hints, 0, sizeof(l_hints));
        l_hints.ai_family = PF_UNSPEC;
        l_hints.ai_socktype = SOCK_STREAM;
        char portstr[10];
        snprintf(portstr, sizeof(portstr), "%d", (int) a_port);
        struct addrinfo* l_addrinfo;

        int gaierr;
        gaierr = getaddrinfo(a_host.c_str(), portstr, &l_hints, &l_addrinfo);
        if (gaierr != 0)
        {
                //NDBG_PRINT("Error getaddrinfo '%s': %s\n", a_host.c_str(), gai_strerror(gaierr));
                delete l_host_info;
                return NULL;
        }

        //NDBG_PRINT("%sRESOLVE%s: a_host: %s a_port: %d\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, a_host.c_str(), a_port);

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
                {
                        if (l_addrinfo_v4 == (struct addrinfo*) 0)
                                l_addrinfo_v4 = i_addrinfo;
                        break;
                }
                case AF_INET6:
                {
                        if (l_addrinfo_v6 == (struct addrinfo*) 0)
                                l_addrinfo_v6 = i_addrinfo;
                        break;
                }
                }
        }

        //NDBG_PRINT("RESOLVE:\n");

        // If there's an IPv4 address, use that, otherwise try IPv6.
        if (l_addrinfo_v4 != NULL)
        {
                if (sizeof(l_host_info->m_sa) < l_addrinfo_v4->ai_addrlen)
                {
                        NDBG_PRINT("Error %s - sockaddr too small (%lu < %lu)\n", a_host.c_str(),
                              (unsigned long) sizeof(l_host_info->m_sa),
                              (unsigned long) l_addrinfo_v4->ai_addrlen);
                        delete l_host_info;
                        return NULL;
                }
                l_host_info->m_sock_family = l_addrinfo_v4->ai_family;
                l_host_info->m_sock_type = l_addrinfo_v4->ai_socktype;
                l_host_info->m_sock_protocol = l_addrinfo_v4->ai_protocol;
                l_host_info->m_sa_len = l_addrinfo_v4->ai_addrlen;

                //NDBG_PRINT("memmove: addrlen: %d\n", l_addrinfo_v4->ai_addrlen);
                //ns_hlx::mem_display((const uint8_t *)l_addrinfo_v4->ai_addr, l_addrinfo_v4->ai_addrlen);
                //show_host_info();

                memmove(&(l_host_info->m_sa), l_addrinfo_v4->ai_addr, l_addrinfo_v4->ai_addrlen);

                // Set the port
                ((sockaddr_in *)(&(l_host_info->m_sa)))->sin_port = htons(a_port);

                freeaddrinfo(l_addrinfo);
        }
        else if (l_addrinfo_v6 != NULL)
        {
                if (sizeof(l_host_info->m_sa) < l_addrinfo_v6->ai_addrlen)
                {
                        NDBG_PRINT("Error %s - sockaddr too small (%lu < %lu)\n", a_host.c_str(),
                              (unsigned long) sizeof(l_host_info->m_sa),
                              (unsigned long) l_addrinfo_v6->ai_addrlen);
                        delete l_host_info;
                        return NULL;
                }
                l_host_info->m_sock_family = l_addrinfo_v6->ai_family;
                l_host_info->m_sock_type = l_addrinfo_v6->ai_socktype;
                l_host_info->m_sock_protocol = l_addrinfo_v6->ai_protocol;
                l_host_info->m_sa_len = l_addrinfo_v6->ai_addrlen;
                //NDBG_PRINT("memmove: addrlen: %d\n", l_addrinfo_v6->ai_addrlen);
                //ns_hlx::mem_display((const uint8_t *)l_addrinfo_v6->ai_addr, l_addrinfo_v6->ai_addrlen);
                //show_host_info();
                memmove(&l_host_info->m_sa, l_addrinfo_v6->ai_addr, l_addrinfo_v6->ai_addrlen);

                // Set the port
                ((sockaddr_in6 *)(&(l_host_info->m_sa)))->sin6_port = htons(a_port);

                freeaddrinfo(l_addrinfo);
        }
        else
        {
                NDBG_PRINT("Error no valid address found for host %s\n", a_host.c_str());
                delete l_host_info;
                return NULL;
        }

        //show_host_info();
        if(m_use_cache)
        {
                pthread_mutex_lock(&m_cache_mutex);
                m_ai_cache_map[l_cache_key] = l_host_info;
                pthread_mutex_unlock(&m_cache_mutex);
        }
        return l_host_info;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#if 0
nresolver::sc_lookup_status_t nresolver::try_fast_lookup(reqlet *a_reqlet,
                                                         resolve_cb_t a_resolve_cb)
{

        // TODO Move to constructor...
        int l_pcre_exec_ret;
        int l_pcre_sub_str_vec[30];

        // Try to find the regex in aLineToMatch, and report results.
        l_pcre_exec_ret = pcre_exec(m_ip_address_re_compiled,
                                    m_ip_address_pcre_extra,
                                    a_reqlet->m_url.m_host.c_str(),
                                    a_reqlet->m_url.m_host.length(), // length of string
                                    0,                               // Start looking at this point
                                    0,                               // OPTIONS
                                    l_pcre_sub_str_vec,              //
                                    30);                             // Length of subStrVec


        // Report what happened in the pcre_exec call..
        //NDBG_OUTPUT("pcre_exec return: %d\n", l_pcre_exec_ret);
        if((PCRE_ERROR_NOMATCH != l_pcre_exec_ret) &&
           (l_pcre_exec_ret < 0))
        {
                // Something bad happened..
                switch(l_pcre_exec_ret)
                {
                        //case PCRE_ERROR_NOMATCH      : NDBG_OUTPUT("String did not match the pattern\n");        break;
                        case PCRE_ERROR_NULL         : NDBG_PRINT("STATUS_ERROR: Something was null\n");                      break;
                        case PCRE_ERROR_BADOPTION    : NDBG_PRINT("STATUS_ERROR: A bad option was passed\n");                 break;
                        case PCRE_ERROR_BADMAGIC     : NDBG_PRINT("STATUS_ERROR: Magic number bad (compiled re corrupt?)\n"); break;
                        case PCRE_ERROR_UNKNOWN_NODE : NDBG_PRINT("STATUS_ERROR: Something kooky in the compiled re\n");      break;
                        case PCRE_ERROR_NOMEMORY     : NDBG_PRINT("STATUS_ERROR: Ran out of memory\n");                       break;
                        default                      : NDBG_PRINT("STATUS_ERROR: Unknown error\n");                           break;
                } // end switch
                return LOSTATUS_OKUP_STATUS_ERROR;
        }
        else if(PCRE_ERROR_NOMATCH != l_pcre_exec_ret)
        {
        //NDBG_PRINT("Result: We have a match for string: %s\n", a_reqlet->m_url.m_host.c_str());

                // store this IP address in sa:
                int l_status = 0;
                l_status = a_reqlet->slow_resolve();
                if(STATUS_OK != l_status)
                {
                        NDBG_PRINT("STATUS_ERROR: performing slow_resolve.\n");
                        return LOSTATUS_OKUP_STATUS_ERROR;
                }
                else
                {
                        if(NULL != a_resolve_cb)
                        {
                                // Callback...
                                int l_status = 0;
                                l_status = a_resolve_cb(a_reqlet);
                                if(STATUS_OK != l_status)
                                {
                                        NDBG_PRINT("STATUS_ERROR: invoking callback\n");
                                        return LOSTATUS_OKUP_STATUS_ERROR;
                                }

                                // We're done...
                                //NDBG_PRINT("MATCH for a_reqlet: %p.\n", a_reqlet);
                                //a_reqlet->show_host_info();
                                return LOSTATUS_OKUP_STATUS_EARLY;
                        }
                }
        }
        // Try a cache db lookup
        // TODO
        return LOSTATUS_OKUP_STATUS_NONE;
}
#endif

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nresolver::sync_ai_cache(void)
{
        int32_t l_status = 0;
        FILE *l_file_ptr = fopen(m_ai_cache_file.c_str(), "w+");
        if(l_file_ptr == NULL)
        {
                NDBG_PRINT("Error performing fopen. Reason: %s\n", strerror(errno));
                return STATUS_ERROR;
        }
        fprintf(l_file_ptr, "[");
        uint32_t l_len = m_ai_cache_map.size();
        uint32_t i_len = 0;
        for(ai_cache_map_t::const_iterator i_entry = m_ai_cache_map.begin();
            i_entry != m_ai_cache_map.end();
            ++i_entry)
        {
                std::string l_ai_val64 = base64_encode((const unsigned char *)(i_entry->second), sizeof(host_info_s));
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
int32_t nresolver::read_ai_cache(const std::string &a_ai_cache_file)
{
        struct stat l_stat;
        int32_t l_status = STATUS_OK;
        l_status = stat(a_ai_cache_file.c_str(), &l_stat);
        if(l_status != 0)
        {
                //NDBG_PRINT("Error performing stat on file: %s.  Reason: %s\n", a_ai_cache_file.c_str(), strerror(errno));
                return STATUS_OK;
        }

        if(!(l_stat.st_mode & S_IFREG))
        {
                //NDBG_PRINT("Error opening file: %s.  Reason: is NOT a regular file\n", a_ai_cache_file.c_str());
                return STATUS_OK;
        }

        FILE * l_file;
        l_file = fopen(a_ai_cache_file.c_str(),"r");
        if (NULL == l_file)
        {
                //NDBG_PRINT("Error opening file: %s.  Reason: %s\n", a_ai_cache_file.c_str(), strerror(errno));
                return STATUS_OK;
        }

        // Read in file...
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
                                host_info_s *l_host_info = new host_info_s();
                                memcpy(l_host_info, l_ai_decoded.data(), l_ai_decoded.length());
                                m_ai_cache_map[l_host] = l_host_info;
                        }
                }
        }
        l_status = fclose(l_file);
        if (STATUS_OK != l_status)
        {
                NDBG_PRINT("Error performing fclose.  Reason: %s\n", strerror(errno));
                return STATUS_ERROR;
        }
        if(l_buf)
        {
                free(l_buf);
                l_buf = NULL;
        }
        return STATUS_OK;
}

}
