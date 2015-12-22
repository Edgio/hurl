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
#include "time_util.h"
#include "host_info.h"

#ifdef ASYNC_DNS_WITH_UDNS
#include <udns.h>
#endif

#include <unistd.h>
#include <netdb.h>
#include <string.h>

namespace ns_hlx {

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
        m_use_cache(false),
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
int32_t nresolver::init(std::string ai_cache_file, bool a_use_cache)
{
        if(m_is_initd)
        {
                return STATUS_OK;
        }
        m_use_cache = a_use_cache;
        if(m_use_cache)
        {
                m_ai_cache = new ai_cache(ai_cache_file);
        }

#ifdef ASYNC_DNS_WITH_UDNS
        if (dns_init(NULL, 0) < 0)
        {
                NDBG_PRINT("Error unable to initialize dns library\n");
                return STATUS_ERROR;
        }
#endif

        m_is_initd = true;
        return STATUS_OK;
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
int32_t nresolver::destroy_async(void* a_ctx,
                                 int &a_fd,
                                 lookup_job_q_t &ao_lookup_job_q,
                                 lookup_job_pq_t &ao_lookup_job_pq)
{
#ifdef ASYNC_DNS_WITH_UDNS
        if(a_fd > 0)
        {
                close(a_fd);
                a_fd = -1;
        }
        dns_ctx *l_ctx = static_cast<dns_ctx *>(a_ctx);
        if(l_ctx)
        {
                dns_free(l_ctx);
                l_ctx = NULL;
        }
#endif
        while(ao_lookup_job_pq.size())
        {
                lookup_job *l_j = ao_lookup_job_pq.top();
                if(l_j)
                {
                        delete l_j;
                }
                ao_lookup_job_pq.pop();
        }
        while(ao_lookup_job_q.size())
        {
                lookup_job *l_j = ao_lookup_job_q.front();
                if(l_j)
                {
                        delete l_j;
                }
                ao_lookup_job_q.pop();
        }
        return STATUS_OK;
}
#endif

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
int32_t nresolver::init_async(void** ao_ctx, int &ao_fd)
{
        int32_t l_status;
        if(!m_is_initd)
        {
                l_status = init();
                if(l_status != STATUS_OK)
                {
                        return STATUS_ERROR;
                }
        }
#ifdef ASYNC_DNS_WITH_UDNS
        pthread_mutex_lock(&m_cache_mutex);
        dns_ctx *l_ctx = NULL;
        l_ctx = dns_new(NULL);
        if(!l_ctx)
        {
                NDBG_PRINT("Error performing dns_new\n");
                pthread_mutex_unlock(&m_cache_mutex);
                return STATUS_ERROR;
        }
        l_status = dns_init(l_ctx, 0);
        if(l_status < 0)
        {
                NDBG_PRINT("Error performing dns_init\n");
                pthread_mutex_unlock(&m_cache_mutex);
                return STATUS_ERROR;
        }

        // Specified Name servers
        if(!m_resolver_host_list.empty())
        {
                // reset nameserver list
                l_status = dns_add_serv(l_ctx, NULL);
                if(l_status < 0)
                {
                        NDBG_PRINT("Error performing dns_add_serv\n");
                        return STATUS_ERROR;
                }
                for(resolver_host_list_t::iterator i_s = m_resolver_host_list.begin();
                    i_s != m_resolver_host_list.end();
                    ++i_s)
                {
                        l_status = dns_add_serv(l_ctx, i_s->c_str());
                        if(l_status < 0)
                        {
                                NDBG_PRINT("Error performing dns_add_serv\n");
                                return STATUS_ERROR;
                        }
                }
        }


        // set dns options
        // Note: PORT MUST be set before setting up m_sock
        // TODO make configurable
        dns_set_opt(l_ctx, DNS_OPT_TIMEOUT, m_timeout_s);
        dns_set_opt(l_ctx, DNS_OPT_NTRIES,  m_retries);
        dns_set_opt(l_ctx, DNS_OPT_PORT,    m_port);

        ao_fd = dns_open(l_ctx);
        if (ao_fd < 0)
        {
                NDBG_PRINT("Error performing dns_open\n");
                pthread_mutex_unlock(&m_cache_mutex);
                return STATUS_ERROR;
        }

        *ao_ctx = l_ctx;
        pthread_mutex_unlock(&m_cache_mutex);
#endif
        return STATUS_OK;
}
#endif

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
host_info *nresolver::lookup_tryfast(const std::string &a_host, uint16_t a_port)
{
        int32_t l_status;
        if(!m_is_initd)
        {
                l_status = init();
                if(l_status != STATUS_OK)
                {
                        return NULL;
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
        pthread_mutex_lock(&m_cache_mutex);
        l_host_info = m_ai_cache->lookup(l_cache_key);
        pthread_mutex_unlock(&m_cache_mutex);

        return l_host_info;
}

//: ----------------------------------------------------------------------------
//: \details: slow resolution
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
host_info *nresolver::lookup_inline(const std::string &a_host, uint16_t a_port)
{
        // Initialize...
        host_info *l_host_info = NULL;
        l_host_info = new host_info();
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
                //NDBG_PRINT("Error getaddrinfo '%s': %s\n",
                //           a_host.c_str(), gai_strerror(gaierr));
                delete l_host_info;
                return NULL;
        }

        //NDBG_PRINT("%sRESOLVE%s: a_host: %s a_port: %d\n",
        //           ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
        //           a_host.c_str(), a_port);

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
                        NDBG_PRINT("Error %s - sockaddr too small (%lu < %lu)\n",
                                   a_host.c_str(),
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
                //ns_hlx::mem_display((const uint8_t *)l_addrinfo_v4->ai_addr,
                //                   l_addrinfo_v4->ai_addrlen);
                //show_host_info();

                memmove(&(l_host_info->m_sa),
                        l_addrinfo_v4->ai_addr,
                        l_addrinfo_v4->ai_addrlen);

                // Set the port
                ((sockaddr_in *)(&(l_host_info->m_sa)))->sin_port = htons(a_port);

                freeaddrinfo(l_addrinfo);
        }
        else if (l_addrinfo_v6 != NULL)
        {
                if (sizeof(l_host_info->m_sa) < l_addrinfo_v6->ai_addrlen)
                {
                        NDBG_PRINT("Error %s - sockaddr too small (%lu < %lu)\n",
                                   a_host.c_str(),
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
                //ns_hlx::mem_display((const uint8_t *)l_addrinfo_v6->ai_addr,
                //                    l_addrinfo_v6->ai_addrlen);
                //show_host_info();
                memmove(&l_host_info->m_sa,
                        l_addrinfo_v6->ai_addr,
                        l_addrinfo_v6->ai_addrlen);

                // Set the port
                ((sockaddr_in6 *)(&(l_host_info->m_sa)))->sin6_port = htons(a_port);

                freeaddrinfo(l_addrinfo);
        }
        else
        {
                NDBG_PRINT("Error no valid address found for host %s\n",
                           a_host.c_str());
                delete l_host_info;
                return NULL;
        }

        // Set to 5min -cuz getaddr-info stinks...
        l_host_info->m_expires_s = get_time_s() + 300;

        //show_host_info();
        if(m_ai_cache)
        {
                l_host_info = m_ai_cache->lookup(get_cache_key(a_host, a_port), l_host_info);
        }
        return l_host_info;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
host_info *nresolver::lookup_sync(const std::string &a_host, uint16_t a_port)
{
        //NDBG_PRINT("%sRESOLVE%s: a_host: %s a_port: %d\n",
        //           ANSI_COLOR_FG_RED, ANSI_COLOR_OFF,
        //           a_host.c_str(), a_port);
        int32_t l_status;
        if(!m_is_initd)
        {
                l_status = init();
                if(l_status != STATUS_OK)
                {
                        return NULL;
                }
        }

        // tryfast lookup
        host_info *l_host_info = NULL;
        l_host_info = lookup_tryfast(a_host, a_port);
        if(l_host_info)
        {
                return l_host_info;
        }
        return lookup_inline(a_host, a_port);
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
        lookup_job *l_job = static_cast<lookup_job *>(a_data);
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
        ai_cache *l_ai_cache = l_job->m_nresolver->get_ai_cache();
        if(!l_ai_cache)
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
                        if(l_status != STATUS_OK)
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

        std::string l_cache_key = get_cache_key(l_job->m_host, l_job->m_port);
        pthread_mutex_lock(&(l_job->m_nresolver->m_cache_mutex));
        l_host_info = l_ai_cache->lookup(l_cache_key, l_host_info);
        pthread_mutex_unlock(&(l_job->m_nresolver->m_cache_mutex));

        // Add to cache
        if(l_job->m_cb)
        {
                int32_t l_status = 0;
                l_status = l_job->m_cb(l_host_info, l_job->m_data);
                if(l_status != STATUS_OK)
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
int32_t nresolver::lookup_async(void* a_ctx,
                                const std::string &a_host,
                                uint16_t a_port,
                                resolved_cb a_cb,
                                void *a_data,
                                uint64_t &a_active,
                                lookup_job_q_t &ao_lookup_job_q,
                                lookup_job_pq_t &ao_lookup_job_pq)
{
        //NDBG_PRINT("%sLOOKUP_ASYNC%s: a_host: %s a_ctx: %p\n",
        //           ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF,
        //           a_host.c_str(),
        //           a_ctx);
        int32_t l_status;
        if(!m_is_initd)
        {
                l_status = init();
                if(l_status != STATUS_OK)
                {
                        return STATUS_ERROR;
                }
        }
#ifdef ASYNC_DNS_WITH_UDNS
        // TODO create lookup job...
        dns_ctx *l_ctx = static_cast<dns_ctx *>(a_ctx);
        if(!l_ctx)
        {
                return STATUS_ERROR;
        }

        if(!a_host.empty())
        {
                //NDBG_PRINT("l_ctx: %p\n", l_ctx);
                lookup_job *l_job = new lookup_job();
                l_job->m_host = a_host;
                l_job->m_port = a_port;
                l_job->m_cb = a_cb;
                l_job->m_nresolver = this;
                l_job->m_data = a_data;
                //NDBG_PRINT("%sADD    LOOKUP%s: HOST: %s\n",
                //           ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF,
                //           l_job->m_host.c_str());
                ao_lookup_job_q.push(l_job);
        }

        a_active = dns_active(l_ctx);
        uint32_t l_submit = m_max_parallel - a_active;
        //NDBG_PRINT("a_active: %lu\n", a_active);
        while((l_submit) && !(ao_lookup_job_q.empty()))
        {
                //NDBG_PRINT("l_submit: %u\n", l_submit);
                lookup_job *l_job = ao_lookup_job_q.front();
                ao_lookup_job_q.pop();
                //NDBG_PRINT("%sSUBMIT LOOKUP%s: HOST: %s\n",
                //           ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF,
                //           l_job->m_host.c_str());
                l_job->m_dns_query = dns_submit_a4(l_ctx, l_job->m_host.c_str(), 0, dns_a4_cb, l_job);
                if (!l_job->m_dns_query)
                {
                        NDBG_PRINT("Error performing dns_submit_a4.  Last status: %d\n",
                                   dns_status(NULL));
                        return STATUS_ERROR;
                }
                l_job->m_dns_ctx = l_ctx;
                l_job->m_start_time = get_time_s();
                ao_lookup_job_pq.push(l_job);
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
        while(!ao_lookup_job_pq.size() && (ao_lookup_job_pq.top()->m_start_time + l_expire_s) < l_now_s)
        {
                lookup_job *l_j = ao_lookup_job_pq.top();
                if(l_j)
                {
                        delete l_j;
                }
                ao_lookup_job_pq.pop();
        }

        return STATUS_OK;
}
#endif

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
int32_t nresolver::handle_io(void* a_ctx)
{
#ifdef ASYNC_DNS_WITH_UDNS
        dns_ctx *l_ctx = static_cast<dns_ctx *>(a_ctx);
        if(!l_ctx)
        {
                return STATUS_ERROR;
        }
        dns_ioevent(l_ctx, 0);

        const int l_delay = dns_timeouts(l_ctx, 0, 0);
        (void) l_delay;
#endif
        return STATUS_OK;
}
#endif

}
