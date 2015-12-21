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
#include "ai_cache.h"

#include <pthread.h>
#include <stdint.h>

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <string>
#include <set>
#include <map>
#include <list>
#include <queue> // for std::priority_queue

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
// TODO Remove -enable with build flag...
#define ASYNC_DNS_WITH_UDNS 1
#ifdef ASYNC_DNS_WITH_UDNS
  #define ASYNC_DNS_SUPPORT 1
#endif

#define NRESOLVER_DEFAULT_AI_CACHE_FILE "/tmp/addr_info_cache.json"

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_WITH_UDNS
struct dns_ctx;
struct dns_rr_a4;
struct dns_query;
#endif

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class nconn;
struct host_info;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class nresolver
{
public:
        //: ------------------------------------------------
        //: Types
        //: ------------------------------------------------
        typedef std::list <std::string> resolver_host_list_t;

#ifdef ASYNC_DNS_SUPPORT
        // Async resolver callback
        typedef int32_t (*resolved_cb)(const host_info *, void *);

        struct lookup_job {
                void *m_data;
                resolved_cb m_cb;
                std::string m_host;
                uint16_t m_port;
#ifdef ASYNC_DNS_WITH_UDNS
                struct dns_query *m_dns_query;
                struct dns_ctx *m_dns_ctx;
#endif
                nresolver *m_nresolver;
                lookup_job(void):
                        m_data(NULL),
                        m_cb(NULL),
                        m_host(),
                        m_port(0),
#ifdef ASYNC_DNS_WITH_UDNS
                        m_dns_query(NULL),
                        m_dns_ctx(NULL),
#endif
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
        static const uint64_t S_RESOLVER_ID         = 0xFFFFDEADBEEF0001UL;
        static const uint32_t S_TIMEOUT_S = 4;
        static const uint32_t S_RETRIES = 3;

#endif
        static const uint32_t S_MAX_PARALLEL_LOOKUPS = 10;
        static const uint32_t S_MIN_TTL_S = 10;

        //: ------------------------------------------------
        //: Public methods
        //: ------------------------------------------------
        nresolver();
        ~nresolver();

        int32_t init(std::string addr_info_cache_file = NRESOLVER_DEFAULT_AI_CACHE_FILE,
                     bool a_use_cache = true);
        void add_resolver_host(const std::string a_server);
        void set_port(uint16_t a_port) {m_port = a_port;}
        host_info *lookup_tryfast(const std::string &a_host, uint16_t a_port);
        host_info *lookup_sync(const std::string &a_host, uint16_t a_port);
        bool get_use_cache(void) { return m_use_cache;}
        ai_cache *get_ai_cache(void) {return m_ai_cache;}
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
        void set_timeout_s(uint32_t a_val) { m_timeout_s = a_val;}
        void set_retries(uint32_t a_val) { m_retries = a_val;}
        void set_max_parllel(uint32_t a_val) { m_max_parallel = a_val;}
#endif

private:
        //: ------------------------------------------------
        //: Private methods
        //: ------------------------------------------------
        // Disallow copy/assign
        nresolver& operator=(const nresolver &);
        nresolver(const nresolver &);

        host_info *lookup_inline(const std::string &a_host, uint16_t a_port);

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
        resolver_host_list_t m_resolver_host_list;
        uint16_t m_port;
#ifdef ASYNC_DNS_SUPPORT
        uint32_t m_timeout_s;
        uint32_t m_retries;
        uint32_t m_max_parallel;
#endif
        uint32_t m_use_cache;
        pthread_mutex_t m_cache_mutex;
        ai_cache *m_ai_cache;
};

//: ----------------------------------------------------------------------------
//: cache key helper
//: ----------------------------------------------------------------------------
std::string get_cache_key(const std::string &a_host, uint16_t a_port);

} //namespace ns_hlx {

#endif
