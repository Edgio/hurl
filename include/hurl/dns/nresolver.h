//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
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
#include <pthread.h>
#include <stdint.h>
#include <list>
#include <string>
#include <map>
#include <queue> // for std::priority_queue
#ifdef ASYNC_DNS_WITH_UDNS
  #define ASYNC_DNS_SUPPORT 1
#endif
#ifdef ASYNC_DNS_SUPPORT
#include "hurl/evr/evr.h"
#endif
//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define NRESOLVER_DEFAULT_AI_CACHE_FILE "/tmp/addr_info_cache.json"
//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_WITH_UDNS
struct dns_ctx;
struct dns_rr_a4;
struct dns_query;
#endif
namespace ns_hurl {
//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class nconn;
struct host_info;
class ai_cache;
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
        //: ------------------------------------------------
        //: lookup job
        //: ------------------------------------------------
        struct job {
                void *m_data;
                resolved_cb m_cb;
                std::string m_host;
                uint16_t m_port;
                bool m_complete;
                uint64_t m_start_time;
#ifdef ASYNC_DNS_WITH_UDNS
                struct dns_query *m_dns_query;
                struct dns_ctx *m_dns_ctx;
#endif
                nresolver *m_nresolver;
                job(void):
                        m_data(NULL),
                        m_cb(NULL),
                        m_host(),
                        m_port(0),
                        m_complete(false),
                        m_start_time(0),
#ifdef ASYNC_DNS_WITH_UDNS
                        m_dns_query(NULL),
                        m_dns_ctx(NULL),
#endif
                        m_nresolver(NULL)
                {}
        private:
                // Disallow copy/assign
                job& operator=(const job &);
                job(const job &);
        };
        typedef std::queue<job *>job_q_t;
        //: ------------------------------------------------
        //: Priority queue sorting
        //: ------------------------------------------------
        class lj_compare_start_times {
        public:
                bool operator()(job* t1, job* t2)
                {
                        return (t1->m_start_time > t2->m_start_time);
                }
        };
        typedef std::priority_queue<job *, std::vector<job *>, lj_compare_start_times> job_pq_t;
        //: ------------------------------------------------
        //: async context object
        //: ------------------------------------------------
        struct adns_ctx {
                nresolver *m_ctx;
                resolved_cb m_cb;
#ifdef ASYNC_DNS_WITH_UDNS
                int m_fd;
                dns_ctx *m_udns_ctx;
#endif
                evr_fd m_evr_fd;
                evr_loop *m_evr_loop;
                evr_event *m_timer_obj;
                job_q_t m_job_q;
                job_pq_t m_job_pq;
                adns_ctx():
                        m_ctx(NULL),
                        m_cb(NULL),
#ifdef ASYNC_DNS_WITH_UDNS
                        m_fd(-1),
                        m_udns_ctx(NULL),
#endif
                        m_evr_fd(),
                        m_evr_loop(NULL),
                        m_timer_obj(NULL),
                        m_job_q(),
                        m_job_pq()
                {}
                ~adns_ctx()
                {
                        // empty job queues
                        // TODO
                }
        private:
                // Disallow copy/assign
                adns_ctx& operator=(const adns_ctx &);
                adns_ctx(const adns_ctx &);
        };
#endif
        //: ------------------------------------------------
        //: Const
        //: ------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
        static const uint32_t S_TIMEOUT_S = 4;
        static const uint32_t S_RETRIES = 3;
        static const uint32_t S_MAX_PARALLEL_LOOKUPS = 100;
#endif
        static const uint32_t S_MIN_TTL_S = 10;
        //: ------------------------------------------------
        //: Public methods
        //: ------------------------------------------------
        nresolver();
        ~nresolver();
        int32_t init(bool a_use_cache = true,
                     const std::string &a_ai_cache_file = NRESOLVER_DEFAULT_AI_CACHE_FILE);
        void add_resolver_host(const std::string a_server);
        void set_port(uint16_t a_port) {m_port = a_port;}
        bool get_use_cache(void) { return m_use_cache;}
        ai_cache *get_ai_cache(void) {return m_ai_cache;}
        // Lookups
        int32_t lookup_tryfast(const std::string &a_host, uint16_t a_port, host_info &ao_host_info);
        int32_t lookup_sync(const std::string &a_host, uint16_t a_port, host_info &ao_host_info);
#ifdef ASYNC_DNS_SUPPORT
        adns_ctx* get_new_adns_ctx(evr_loop *a_evr_loop, resolved_cb a_cb);
        static int32_t get_active(adns_ctx* a_adns_ctx);
        int32_t destroy_async(adns_ctx* a_adns_ctx);
        int32_t lookup_async(adns_ctx* a_adns_ctx,
                             const std::string &a_host,
                             uint16_t a_port,
                             void *a_data,
                             void **ao_job_handle);
        void set_timeout_s(uint32_t a_val) { m_timeout_s = a_val;}
        void set_retries(uint32_t a_val) { m_retries = a_val;}
        void set_max_parallel(uint32_t a_val) { m_max_parallel = a_val;}
        // -------------------------------------------------
        // Public Static (class) methods
        // -------------------------------------------------
        static int32_t evr_fd_writeable_cb(void *a_data);
        static int32_t evr_fd_readable_cb(void *a_data);
        static int32_t evr_fd_error_cb(void *a_data);
        static int32_t evr_fd_timeout_cb(void *a_data);
#endif
private:
        //: ------------------------------------------------
        //: Private methods
        //: ------------------------------------------------
        // Disallow copy/assign
        nresolver& operator=(const nresolver &);
        nresolver(const nresolver &);
        int32_t lookup_inline(const std::string &a_host, uint16_t a_port, host_info &ao_host_info);
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
} //namespace ns_hurl {
#endif
