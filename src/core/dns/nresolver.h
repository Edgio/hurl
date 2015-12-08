//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nresolver.h
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
#ifndef _NRESOLVER_H
#define _NRESOLVER_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "evr.h"

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <string>
#include <queue>
#include <map>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define NRESOLVER_DEFAULT_AI_CACHE_FILE "/tmp/addr_info_cache.json"

// TODO Remove -enable with build flag...
//#define ASYNC_DNS_WITH_UDNS 1
#ifdef ASYNC_DNS_WITH_UDNS
#define ASYNC_DNS_SUPPORT 1
#endif

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
//struct real_pcre;
//typedef real_pcre pcre;
//struct pcre_extra;

#ifdef ASYNC_DNS_WITH_UDNS
struct dns_ctx;
struct dns_rr_a4;
#endif

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class nconn;

// Host info
struct host_info_s {
        struct sockaddr_storage m_sa;
        int m_sa_len;
        int m_sock_family;
        int m_sock_type;
        int m_sock_protocol;
        uint64_t m_expires_s;

        host_info_s():
                m_sa(),
                m_sa_len(16),
                m_sock_family(AF_INET),
                m_sock_type(SOCK_STREAM),
                m_sock_protocol(IPPROTO_TCP),
                m_expires_s(0)
        {
                ((struct sockaddr_in *)(&m_sa))->sin_family = AF_INET;
        };

        void show(void)
        {
                printf("+-----------+\n");
                printf("| Host Info |\n");
                printf("+-----------+-------------------------\n");
                printf(": m_sock_family:   %d\n",  m_sock_family);
                printf(": m_sock_type:     %d\n",  m_sock_type);
                printf(": m_sock_protocol: %d\n",  m_sock_protocol);
                printf(": m_sa_len:        %d\n",  m_sa_len);
                printf(": m_expires:       %lu\n", m_expires_s);
                printf("+-------------------------------------\n");
        };

};

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class nresolver
{
public:
        //: ------------------------------------------------
        //: Types
        //: ------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
        // Async resolver callback
        typedef int32_t (*resolved_cb)(const host_info_s *, void *);

        struct lookup_job {
                void *m_data;
                resolved_cb m_cb;
                std::string m_host;
                uint16_t m_port;
                nresolver *m_nresolver;
                lookup_job(void):
                        m_data(NULL),
                        m_cb(NULL),
                        m_host(),
                        m_port(0),
                        m_nresolver(NULL)
                {}
        private:
                // Disallow copy/assign
                lookup_job& operator=(const lookup_job &);
                lookup_job(const lookup_job &);
        };
        typedef std::queue<lookup_job *>lookup_job_q_t;
#endif



        //: ------------------------------------------------
        //: Const
        //: ------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
        static const uint64_t S_RESOLVER_ID = 0xFFFFFFFFFFFFFFFFUL;
#endif
        static const uint32_t S_MAX_PARALLEL_LOOKUPS = 10;
        static const uint32_t S_MIN_TTL_S = 10;

        //: ------------------------------------------------
        //: Public methods
        //: ------------------------------------------------
        nresolver();
        ~nresolver();

        int32_t init(std::string addr_info_cache_file = "", bool a_use_cache = false);
        host_info_s *lookup_tryfast(const std::string &a_host, uint16_t a_port);
        host_info_s *lookup_sync(const std::string &a_host, uint16_t a_port);
#ifdef ASYNC_DNS_SUPPORT
        int32_t init_async(void** ao_ctx, int &ao_fd);
        int32_t destroy_async(void* a_ctx, int &a_fd);
        int32_t lookup_async(void* a_ctx,
                             const std::string &a_host,
                             uint16_t a_port,
                             resolved_cb a_cb,
                             void *a_data,
                             uint64_t &a_active,
                             lookup_job_q_t &ao_lookup_job_q);
        int32_t handle_io(void* a_ctx);
#endif

private:
        //: ------------------------------------------------
        //: Types
        //: ------------------------------------------------
        typedef std::map <std::string, host_info_s *> ai_cache_map_t;

        //: ------------------------------------------------
        //: Private methods
        //: ------------------------------------------------
        // Disallow copy/assign
        nresolver& operator=(const nresolver &);
        nresolver(const nresolver &);
        int32_t sync_ai_cache(void);
        int32_t read_ai_cache(const std::string &a_ai_cache_file);
        host_info_s *lookup_inline(const std::string &a_host, uint16_t a_port);
        void add_host_info_cache(const std::string &a_host, uint16_t a_port, host_info_s *a_host_info);

        //: ------------------------------------------------
        //: Private static methods
        //: ------------------------------------------------
#ifdef ASYNC_DNS_WITH_UDNS
        static void dns_a4_cb(struct dns_ctx *a_ctx, struct dns_rr_a4 *a_result, void *a_data);
#endif

        //: ------------------------------------------------
        //: Private members
        //: ------------------------------------------------
        bool m_is_initd;
        uint32_t m_use_cache;
        pthread_mutex_t m_cache_mutex;
        ai_cache_map_t m_ai_cache_map;
        std::string m_ai_cache_file;
        //pcre *m_ip_address_re_compiled;
        //pcre_extra *m_ip_address_pcre_extra;

};

} //namespace ns_hlx {

#endif
