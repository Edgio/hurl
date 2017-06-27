//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
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
#include "hurl/dns/ai_cache.h"
#include "hurl/dns/nresolver.h"
#include "ndebug.h"
#include "hurl/dns/nlookup.h"
#include "hurl/evr/evr.h"
#include "hurl/nconn/host_info.h"
#include "hurl/support/time_util.h"
#include "hurl/status.h"
#include "hurl/support/trace.h"
#ifdef ASYNC_DNS_WITH_UDNS
#include "udns-0.4/udns.h"
#endif
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
// for inet_pton
#include <arpa/inet.h>
namespace ns_hurl {
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string get_cache_key(const std::string &a_host, uint16_t a_port)
{
        char l_port_str[8];
        snprintf(l_port_str, 8, "%d", a_port);
        std::string l_cache_key;
        l_cache_key = a_host + ":" + l_port_str;
        return l_cache_key;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nresolver::nresolver():
        m_is_initd(false),
        m_resolver_host_list(),
        m_port(53),
#ifdef ASYNC_DNS_SUPPORT
        m_timeout_s(S_TIMEOUT_S),
        m_retries(S_RETRIES),
        m_max_parallel(S_MAX_PARALLEL_LOOKUPS),
#endif
        m_use_cache(true),
        m_cache_mutex(),
        m_ai_cache(NULL)
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
        // Sync back to disk
        if(m_use_cache && m_ai_cache)
        {
                delete m_ai_cache;
                m_ai_cache = NULL;
        }
        pthread_mutex_destroy(&m_cache_mutex);
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nresolver::init(bool a_use_cache,
                        const std::string &a_ai_cache_file)
{
        if(m_is_initd)
        {
                return HURL_STATUS_OK;
        }
        m_use_cache = a_use_cache;
        if(m_use_cache)
        {
                m_ai_cache = new ai_cache(a_ai_cache_file);
        }
        else
        {
                m_ai_cache = NULL;
        }
#ifdef ASYNC_DNS_WITH_UDNS
        if (dns_init(NULL, 0) < 0)
        {
                TRC_ERROR("unable to initialize dns library\n");
                return HURL_STATUS_ERROR;
        }
#endif
        m_is_initd = true;
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nresolver::add_resolver_host(const std::string a_server)
{
        m_resolver_host_list.push_back(a_server);
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
int32_t nresolver::destroy_async(adns_ctx* a_adns_ctx)
{
        if(!a_adns_ctx)
        {
                TRC_ERROR("a_adns_ctx == NULL\n");
                return HURL_STATUS_ERROR;
        }
#ifdef ASYNC_DNS_WITH_UDNS
        if(a_adns_ctx->m_fd > 0)
        {
                close(a_adns_ctx->m_fd);
                a_adns_ctx->m_fd = -1;
        }
        dns_ctx *l_ctx = static_cast<dns_ctx *>(a_adns_ctx->m_udns_ctx);
        if(l_ctx)
        {
                dns_free(l_ctx);
                l_ctx = NULL;
        }
#endif
        while(a_adns_ctx->m_job_pq.size())
        {
                job *l_j = a_adns_ctx->m_job_pq.top();
                if(l_j)
                {
                        delete l_j;
                }
                a_adns_ctx->m_job_pq.pop();
        }
        while(a_adns_ctx->m_job_q.size())
        {
                job *l_j = a_adns_ctx->m_job_q.front();
                if(l_j)
                {
                        delete l_j;
                }
                a_adns_ctx->m_job_q.pop();
        }
        return HURL_STATUS_OK;
}
#endif
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
nresolver::adns_ctx *nresolver::get_new_adns_ctx(evr_loop *a_evr_loop, resolved_cb a_cb)
{
        int32_t l_status;
        if(!m_is_initd)
        {
                l_status = init();
                if(l_status != HURL_STATUS_OK)
                {
                        return NULL;
                }
        }
        adns_ctx* l_adns_ctx = new adns_ctx();
        l_adns_ctx->m_ctx = this;
        l_adns_ctx->m_cb = a_cb;
        l_adns_ctx->m_evr_loop = a_evr_loop;
#ifdef ASYNC_DNS_WITH_UDNS
        pthread_mutex_lock(&m_cache_mutex);
        l_adns_ctx->m_udns_ctx = dns_new(NULL);
        if(!l_adns_ctx->m_udns_ctx)
        {
                TRC_ERROR("performing dns_new\n");
                delete l_adns_ctx;
                pthread_mutex_unlock(&m_cache_mutex);
                return NULL;
        }
        l_status = dns_init(l_adns_ctx->m_udns_ctx, 0);
        if(l_status < 0)
        {
                TRC_ERROR("performing dns_init\n");
                delete l_adns_ctx;
                pthread_mutex_unlock(&m_cache_mutex);
                return NULL;
        }
        // Specified Name servers
        if(!m_resolver_host_list.empty())
        {
                // reset nameserver list
                l_status = dns_add_serv(l_adns_ctx->m_udns_ctx, NULL);
                if(l_status < 0)
                {
                        TRC_ERROR("performing dns_add_serv\n");
                        delete l_adns_ctx;
                        pthread_mutex_unlock(&m_cache_mutex);
                        return NULL;
                }
                for(resolver_host_list_t::iterator i_s = m_resolver_host_list.begin();
                    i_s != m_resolver_host_list.end();
                    ++i_s)
                {
                        l_status = dns_add_serv(l_adns_ctx->m_udns_ctx, i_s->c_str());
                        if(l_status < 0)
                        {
                                TRC_ERROR("performing dns_add_serv\n");
                                delete l_adns_ctx;
                                pthread_mutex_unlock(&m_cache_mutex);
                                return NULL;
                        }
                }
        }
        // set dns options
        // Note: PORT MUST be set before setting up m_sock
        // TODO make configurable
        dns_set_opt(l_adns_ctx->m_udns_ctx, DNS_OPT_TIMEOUT, m_timeout_s);
        dns_set_opt(l_adns_ctx->m_udns_ctx, DNS_OPT_NTRIES,  m_retries);
        dns_set_opt(l_adns_ctx->m_udns_ctx, DNS_OPT_PORT,    m_port);
        int l_fd;
        l_fd = dns_open(l_adns_ctx->m_udns_ctx);
        if (l_fd < 0)
        {
                TRC_ERROR("performing dns_open\n");
                delete l_adns_ctx;
                pthread_mutex_unlock(&m_cache_mutex);
                return NULL;
        }
        // Set non-blocking
        l_status = fcntl(l_fd, F_SETFL, O_NONBLOCK | O_RDWR);
        if (l_status == -1)
        {
                TRC_ERROR("fcntl(FD_CLOEXEC) failed: %s\n", strerror(errno));
                delete l_adns_ctx;
                // TODO udns cleanup???
                pthread_mutex_unlock(&m_cache_mutex);
                return NULL;
        }
        // evr fd setup
        l_adns_ctx->m_evr_fd.m_data = l_adns_ctx;
        l_adns_ctx->m_evr_fd.m_magic = EVR_EVENT_FD_MAGIC;
        l_adns_ctx->m_evr_fd.m_read_cb = evr_fd_readable_cb;
        l_adns_ctx->m_evr_fd.m_write_cb = evr_fd_writeable_cb;
        l_adns_ctx->m_evr_fd.m_error_cb = NULL;
        l_status = a_evr_loop->add_fd(l_fd,
                                      EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_RD_HUP|EVR_FILE_ATTR_MASK_ET,
                                      &l_adns_ctx->m_evr_fd);
        if (l_status != HURL_STATUS_OK)
        {
                TRC_ERROR("add_fd failed\n");
                delete l_adns_ctx;
                // TODO udns cleanup???
                pthread_mutex_unlock(&m_cache_mutex);
                return NULL;
        }
        pthread_mutex_unlock(&m_cache_mutex);
#endif
        return l_adns_ctx;
}
#endif
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static bool is_valid_ip_address(const char *a_str)
{
    struct sockaddr_in l_sa;
    return (inet_pton(AF_INET, a_str, &(l_sa.sin_addr)) != 0);
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nresolver::lookup_tryfast(const std::string &a_host,
                                  uint16_t a_port,
                                  host_info &ao_host_info)
{
        //NDBG_PRINT("%sRESOLVE%s: a_host: %s a_port: %d\n",
        //           ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
        //           a_host.c_str(), a_port);
        int32_t l_status;
        if(!m_is_initd)
        {
                l_status = init();
                if(l_status != HURL_STATUS_OK)
                {
                        return HURL_STATUS_ERROR;
                }
        }
        // ---------------------------------------
        // cache lookup
        // ---------------------------------------
        host_info *l_host_info = NULL;

        // Create a cache key
        char l_port_str[8];
        snprintf(l_port_str, 8, "%d", a_port);
        std::string l_cache_key;
        l_cache_key = a_host + ":" + l_port_str;

        // Lookup in map
        if(m_use_cache && m_ai_cache)
        {
                pthread_mutex_lock(&m_cache_mutex);
                l_host_info = m_ai_cache->lookup(l_cache_key);
                pthread_mutex_unlock(&m_cache_mutex);
        }

        if(l_host_info)
        {
                ao_host_info = *l_host_info;
                return HURL_STATUS_OK;
        }

        // Lookup inline
        if(is_valid_ip_address(a_host.c_str()))
        {
                return lookup_inline(a_host, a_port, ao_host_info);
        }
        return HURL_STATUS_ERROR;
}
//: ----------------------------------------------------------------------------
//: \details: slow resolution
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nresolver::lookup_inline(const std::string &a_host, uint16_t a_port, host_info &ao_host_info)
{
        int32_t l_status;
        host_info *l_host_info = new host_info();
        l_status = nlookup(a_host, a_port, *l_host_info);
        if(l_status != HURL_STATUS_OK)
        {
                delete l_host_info;
                return HURL_STATUS_ERROR;
        }
        //show_host_info();
        if(m_use_cache && m_ai_cache)
        {
                l_host_info = m_ai_cache->lookup(get_cache_key(a_host, a_port), l_host_info);
        }
        int32_t l_retval = HURL_STATUS_OK;
        if(l_host_info)
        {
                ao_host_info = *l_host_info;
                l_retval = HURL_STATUS_OK;
        }
        else
        {
                l_retval = HURL_STATUS_ERROR;
        }
        if(l_host_info && (!m_use_cache || !m_ai_cache))
        {
                delete l_host_info;
                l_host_info = NULL;
        }
        return l_retval;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nresolver::lookup_sync(const std::string &a_host, uint16_t a_port, host_info &ao_host_info)
{
        //NDBG_PRINT("%sRESOLVE%s: a_host: %s a_port: %d\n",
        //           ANSI_COLOR_FG_RED, ANSI_COLOR_OFF,
        //           a_host.c_str(), a_port);
        int32_t l_status;
        if(!m_is_initd)
        {
                l_status = init();
                if(l_status != HURL_STATUS_OK)
                {
                        return HURL_STATUS_ERROR;
                }
        }
        // tryfast lookup
        l_status = lookup_tryfast(a_host, a_port, ao_host_info);
        if(l_status == HURL_STATUS_OK)
        {
                return HURL_STATUS_OK;
        }
        return lookup_inline(a_host, a_port, ao_host_info);
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const char *s_bytes_2_ip_str(const unsigned char *c)
{
        static char b[sizeof("255.255.255.255")];
        sprintf(b, "%u.%u.%u.%u", c[0], c[1], c[2], c[3]);
        return b;
}
//: ----------------------------------------------------------------------------
//: \details: A query callback routine
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_WITH_UDNS
void nresolver::dns_a4_cb(struct dns_ctx *a_ctx,
                          struct dns_rr_a4 *a_result,
                          void *a_data)
{
        job *l_job = static_cast<job *>(a_data);
        //NDBG_PRINT("l_job: %p\n", l_job);
        if(!l_job)
        {
                //NDBG_PRINT("job[%p] == NULL\n", l_job);
                return;
        }
        if(!l_job->m_nresolver)
        {
                if (a_result)
                {
                        free(a_result);
                }
                return;
        }
        if(!a_result ||
          (a_result->dnsa4_nrr <= 0))
        {
                //NDBG_PRINT("FAIL job: %s status: %d. Reason: %s\n",
                //                l_job->m_host.c_str(),
                //                dns_status(a_ctx),
                //                dns_strerror(dns_status(a_ctx)));
                if(l_job->m_cb)
                {
                        int32_t l_status = 0;
                        l_status = l_job->m_cb(NULL, l_job->m_data);
                        if(l_status != HURL_STATUS_OK)
                        {
                                //NDBG_PRINT("Error performing callback.\n");
                        }
                }
                if (a_result)
                {
                        free(a_result);
                }
                return;
        }

        if(l_job->m_complete)
        {
                if (a_result)
                {
                        free(a_result);
                }
                return;
        }
        l_job->m_complete = true;
        //NDBG_PRINT("%sokay host%s: %s, size: %d -ip: %s\n",
        //           ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF,
        //           a_result->dnsa4_qname,
        //           a_result->dnsa4_nrr,
        //           s_bytes_2_ip_str((unsigned char *) &(a_result->dnsa4_addr->s_addr)));

        // Create host_info_t
        host_info *l_host_info = NULL;
        l_host_info = new host_info();
        l_host_info->m_sa_len = sizeof(l_host_info->m_sa);
        memset((void*) &(l_host_info->m_sa), 0, l_host_info->m_sa_len);
        sockaddr_in *l_sockaddr_in = (sockaddr_in *)&(l_host_info->m_sa);
        l_sockaddr_in->sin_family = AF_INET;
        l_sockaddr_in->sin_addr = a_result->dnsa4_addr[0];
        l_sockaddr_in->sin_port = htons(l_job->m_port);

        l_host_info->m_sa_len = sizeof(sockaddr_in);

        uint64_t l_ttl_s = a_result->dnsa4_ttl;
        if(l_ttl_s < S_MIN_TTL_S)
        {
                l_ttl_s = 10;
        }
        l_host_info->m_expires_s = get_time_s() + l_ttl_s;

        if(l_job->m_nresolver->m_use_cache && l_job->m_nresolver->get_ai_cache())
        {
                std::string l_cache_key = get_cache_key(l_job->m_host, l_job->m_port);
                pthread_mutex_lock(&(l_job->m_nresolver->m_cache_mutex));
                l_host_info = l_job->m_nresolver->get_ai_cache()->lookup(l_cache_key, l_host_info);
                pthread_mutex_unlock(&(l_job->m_nresolver->m_cache_mutex));
        }

        // Add to cache
        if(l_job->m_cb)
        {
                int32_t l_status = 0;
                l_status = l_job->m_cb(l_host_info, l_job->m_data);
                if(l_status != HURL_STATUS_OK)
                {
                        //NDBG_PRINT("Error performing callback.\n");
                }
        }
        if (a_result)
        {
                free(a_result);
        }
}
#endif
//: ----------------------------------------------------------------------------
//: \details: A query callback routine
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_WITH_UDNS
// TODO ipv6 support???
//static void s_dns_a6_cb(struct dns_ctx *a_ctx,
//                        struct dns_rr_a6 *a_result,
//                        void *a_data)
//{
//        if(!a_result ||
//          (a_result->dnsa6_nrr <= 0))
//        {
//                NDBG_PRINT("fail\n");
//                return;
//        }
//        printf("okay host: %s, size: %d -ip: %s\n",
//                        a_result->dnsa6_qname,
//                        a_result->dnsa6_nrr,
//                        s_bytes_2_ip_str((unsigned char *) &(a_result->dnsa6_addr->__in6_u.__u6_addr32)));
//        return;
//}
#endif
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
int32_t nresolver::lookup_async(adns_ctx* a_adns_ctx,
                                const std::string &a_host,
                                uint16_t a_port,
                                void *a_data,
                                void **ao_job_handle)
{
        //NDBG_PRINT("%sLOOKUP_ASYNC%s: a_host: %s a_adns_ctx: %p\n",
        //           ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF,
        //           a_host.c_str(),
        //           a_adns_ctx);
        int32_t l_status;
        if(!m_is_initd)
        {
                l_status = init();
                if(l_status != HURL_STATUS_OK)
                {
                        return HURL_STATUS_ERROR;
                }
        }
        if(!a_adns_ctx)
        {
                TRC_ERROR("a_adns_ctx == NULL\n");
                return HURL_STATUS_ERROR;
        }
#ifdef ASYNC_DNS_WITH_UDNS
        dns_ctx *l_ctx = a_adns_ctx->m_udns_ctx;
        if(!l_ctx)
        {
                return HURL_STATUS_ERROR;
        }
        if(!a_host.empty())
        {
                //NDBG_PRINT("l_ctx: %p\n", l_ctx);
                job *l_job = new job();
                l_job->m_host = a_host;
                l_job->m_port = a_port;
                l_job->m_cb = a_adns_ctx->m_cb;
                l_job->m_nresolver = this;
                l_job->m_data = a_data;
                if(ao_job_handle)
                {
                        *ao_job_handle = l_job;
                }
                //NDBG_PRINT("%sADD    LOOKUP%s: HOST: %s\n",
                //           ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF,
                //           l_job->m_host.c_str());
                a_adns_ctx->m_job_q.push(l_job);
        }

        int32_t l_active = dns_active(l_ctx);
        uint32_t l_submit = m_max_parallel - l_active;
        //NDBG_PRINT("a_active: %lu\n", a_active);
        while((l_submit) && !(a_adns_ctx->m_job_q.empty()))
        {
                //NDBG_PRINT("l_submit: %u\n", l_submit);
                job *l_job = a_adns_ctx->m_job_q.front();
                a_adns_ctx->m_job_q.pop();
                //NDBG_PRINT("%sSUBMIT LOOKUP%s: HOST: %s\n",
                //           ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF,
                //           l_job->m_host.c_str());
                l_job->m_dns_query = dns_submit_a4(l_ctx, l_job->m_host.c_str(), 0, dns_a4_cb, l_job);
                if (!l_job->m_dns_query)
                {
                        TRC_ERROR("performing dns_submit_a4.  Last status: %d\n", dns_status(NULL));
                        return HURL_STATUS_ERROR;
                }
                l_job->m_dns_ctx = l_ctx;
                l_job->m_start_time = get_time_s();
                a_adns_ctx->m_job_pq.push(l_job);
                //NDBG_PRINT("%sSTATUS%s job: %s status: %d. Reason: %s\n",
                //                ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF,
                //                l_job->m_host.c_str(),
                //                dns_status(l_ctx),
                //                dns_strerror(dns_status(l_ctx)));

                // TODO ipv6 support???
                //l_query = dns_submit_a6(NULL, a_host.c_str(), 0, s_dns_a6_cb, l_data);
                //if (!l_query)
                //{
                //        NDBG_PRINT("submit error %d\n", dns_status(NULL));
                //}
                --l_submit;
        }
        // ???
        time_t l_now;
        l_now = time(NULL);
        const int l_delay = dns_timeouts(l_ctx, 1, l_now);
        (void) l_delay;
#endif
        // Check for expires
        uint32_t l_expire_s = m_timeout_s*m_retries*2;
        uint64_t l_now_s = get_time_s();
        while(!a_adns_ctx->m_job_pq.size() && (a_adns_ctx->m_job_pq.top()->m_start_time + l_expire_s) < l_now_s)
        {
                job *l_j = a_adns_ctx->m_job_pq.top();
                if(l_j)
                {
                        delete l_j;
                }
                a_adns_ctx->m_job_pq.pop();
        }
        // Get active number
        l_active = get_active(a_adns_ctx);
        // Add timer to handle timeouts
        if(l_active && !a_adns_ctx->m_timer_obj)
        {
                evr_loop *l_el = a_adns_ctx->m_evr_loop;
                if(l_el)
                {
                        l_el->add_event(1000,                        // Timeout ms
                                        evr_fd_timeout_cb,           // timeout cb
                                        a_adns_ctx,                  // data *
                                        &(a_adns_ctx->m_timer_obj)); // timer obj
                }
        }
        return HURL_STATUS_OK;
}
#endif
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
int32_t nresolver::get_active(adns_ctx* a_adns_ctx)
{
        if(!a_adns_ctx)
        {
                TRC_ERROR("a_adns_ctx == NULL\n");
                return 0;
        }
#ifdef ASYNC_DNS_WITH_UDNS
        if(!(a_adns_ctx->m_udns_ctx))
        {
                TRC_ERROR("a_adns_ctx->m_udns_ctx == NULL\n");
                return 0;
        }
        int32_t l_active = dns_active(a_adns_ctx->m_udns_ctx);
        if(l_active < 0)
        {
                TRC_ERROR("l_active[%d] < 0\n", l_active);
                return 0;
        }
        return l_active;
#else
        return 0;
#endif
}
#endif
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
int32_t nresolver::evr_fd_writeable_cb(void *a_data)
{
        //NDBG_PRINT("%sWRITEABLE%s\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF);
        TRC_ERROR("writeable cb for adns resolver");
        return HURL_STATUS_OK;
}
#endif
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
int32_t nresolver::evr_fd_readable_cb(void *a_data)
{
        //NDBG_PRINT("%sREADABLE%s\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF);
        adns_ctx *l_adns_ctx = static_cast<adns_ctx *>(a_data);
        if(!l_adns_ctx)
        {
                TRC_ERROR("a_ctx == NULL\n");
                return HURL_STATUS_ERROR;
        }
#ifdef ASYNC_DNS_WITH_UDNS
        if(!l_adns_ctx->m_udns_ctx)
        {
                // TODO REMOVE
                TRC_ERROR("l_adns_ctx->m_udns_ctx == NULL.\n");
                return HURL_STATUS_ERROR;
        }
        dns_ioevent(l_adns_ctx->m_udns_ctx, 0);
#endif
        if(l_adns_ctx->m_ctx)
        {
                // Try dequeue any q'd dns req's
                std::string l_unused;
                int32_t l_s;
                void *l_job;
                l_s = l_adns_ctx->m_ctx->lookup_async(l_adns_ctx,l_unused,0,l_adns_ctx,&l_job);
                if(l_s != HURL_STATUS_OK)
                {
                        TRC_ERROR("lookup_async.\n");
                        return HURL_STATUS_ERROR;
                }
        }
        return HURL_STATUS_OK;
}
#endif
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
int32_t nresolver::evr_fd_error_cb(void *a_data)
{
        //NDBG_PRINT("%sERROR%s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF);
        TRC_ERROR("evr_fd_error_cb\n");
        return HURL_STATUS_OK;
}
#endif
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
int32_t nresolver::evr_fd_timeout_cb(void *a_data)
{
        //NDBG_PRINT("%sTIMEOUT%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
        // timeout cb
        adns_ctx *l_adns_ctx = static_cast<adns_ctx *>(a_data);
        if(!l_adns_ctx)
        {
                TRC_ERROR("a_data == NULL\n");
                return HURL_STATUS_ERROR;
        }
        if(l_adns_ctx->m_ctx)
        {
                // Try dequeue any q'd dns req's
                l_adns_ctx->m_timer_obj = NULL;
                std::string l_unused;
                int32_t l_s;
                void *l_job;
                l_adns_ctx->m_timer_obj = NULL;
                l_s = l_adns_ctx->m_ctx->lookup_async(l_adns_ctx,l_unused,0,l_adns_ctx,&l_job);
                if(l_s != HURL_STATUS_OK)
                {
                        return HURL_STATUS_ERROR;
                }
        }
        return HURL_STATUS_OK;
}
#endif
}
