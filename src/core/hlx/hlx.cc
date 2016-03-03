//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hlx.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    05/28/2015
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
#include "hlx/hlx.h"
#include "ndebug.h"
#include "tls_util.h"
#include "time_util.h"
#include "stat_util.h"
#include "t_hlx.h"
#include "nresolver.h"
#include "nconn_tcp.h"

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define JS_ADD_MEMBER(_obj, _key, _val, _l_js_alloc)\
_obj.AddMember(_key,\
                rapidjson::Value(_val, _l_js_alloc).Move(),\
                _l_js_alloc)

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlx::hlx(void):
        m_t_conf(NULL),
        m_stats(false),
        m_num_threads(1),
        m_lsnr_list(),
        m_nresolver(NULL),
        m_dns_use_sync(false),
        m_dns_use_ai_cache(true),
        m_dns_ai_cache_file(NRESOLVER_DEFAULT_AI_CACHE_FILE),
        m_start_time_ms(0),
        m_t_hlx_list(),
        m_is_initd(false),
        m_cur_subr_uid(0),
        m_server_name("hlx")
{
        m_t_conf = new t_conf();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlx::~hlx()
{
        for(t_hlx_list_t::iterator i_t = m_t_hlx_list.begin();
                        i_t != m_t_hlx_list.end();
                        ++i_t)
        {
                if(*i_t)
                {
                        delete *i_t;
                }
        }
        m_t_hlx_list.clear();

        for(lsnr_list_t::iterator i_t = m_lsnr_list.begin();
                        i_t != m_lsnr_list.end();
                        ++i_t)
        {
                if(*i_t)
                {
                        delete *i_t;
                }
        }
        m_lsnr_list.clear();

        // -------------------------------------------------
        // tls cleanup
        // -------------------------------------------------
        if(m_t_conf->m_tls_server_ctx)
        {
                SSL_CTX_free(m_t_conf->m_tls_server_ctx);
                m_t_conf->m_tls_server_ctx = NULL;
        }
        if(m_t_conf->m_tls_client_ctx)
        {
                SSL_CTX_free(m_t_conf->m_tls_client_ctx);
                m_t_conf->m_tls_client_ctx = NULL;
        }

        if(m_nresolver)
        {
                delete m_nresolver;
                m_nresolver = NULL;
        }

        if(m_t_conf)
        {
            delete m_t_conf;
            m_t_conf = NULL;
        }
}

//: ----------------------------------------------------------------------------
//:                               Getters
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::get_thread_stat(t_stat_list_t &ao_stat_list)
{
        // Aggregate
        for(t_hlx_list_t::const_iterator i_client = m_t_hlx_list.begin();
           i_client != m_t_hlx_list.end();
           ++i_client)
        {
                // Get stuff from client...
                ao_stat_list.emplace(ao_stat_list.begin(), (*i_client)->get_stat());
                // TODO
                //t_stat_t l_stat;
                //(*i_client)->get_stats_copy(l_stat);
                //add_to_total_stat_agg(ao_stats, l_stat);
        }
}

//: ----------------------------------------------------------------------------
//: \details: Aggregate thread stats.
//: \return:  NA
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void aggregate_stat(t_stat_t &ao_total, const t_stat_t &a_stat)
{
        add_stat(ao_total.m_ups_stat_us_connect , a_stat.m_ups_stat_us_connect);
        add_stat(ao_total.m_ups_stat_us_first_response , a_stat.m_ups_stat_us_first_response);
        add_stat(ao_total.m_ups_stat_us_end_to_end , a_stat.m_ups_stat_us_end_to_end);

        ao_total.m_dns_resolve_req += a_stat.m_dns_resolve_req;
        ao_total.m_dns_resolve_active += a_stat.m_dns_resolve_active;
        ao_total.m_dns_resolved += a_stat.m_dns_resolved;
        ao_total.m_dns_resolve_ev += a_stat.m_dns_resolve_ev;

        ao_total.m_ups_conn_started += a_stat.m_ups_conn_started;
        ao_total.m_ups_conn_completed += a_stat.m_ups_conn_completed;
        ao_total.m_ups_reqs += a_stat.m_ups_reqs;
        ao_total.m_ups_idle_killed += a_stat.m_ups_idle_killed;
        ao_total.m_ups_subr_queued += a_stat.m_ups_subr_queued;

        for(status_code_count_map_t::const_iterator i_code =
                        a_stat.m_ups_status_code_count_map.begin();
                        i_code != a_stat.m_ups_status_code_count_map.end();
                        ++i_code)
        {
                status_code_count_map_t::iterator i_code2;
                i_code2 = ao_total.m_ups_status_code_count_map.find(i_code->first);
                if(i_code2 == ao_total.m_ups_status_code_count_map.end())
                {
                        ao_total.m_ups_status_code_count_map[i_code->first] = i_code->second;
                }
                else
                {
                        i_code2->second += i_code->second;
                }
        }

        ao_total.m_cln_conn_started += a_stat.m_cln_conn_started;
        ao_total.m_cln_conn_completed += a_stat.m_cln_conn_completed;
        ao_total.m_cln_reqs += a_stat.m_cln_reqs;
        ao_total.m_cln_idle_killed += a_stat.m_cln_idle_killed;
        ao_total.m_pool_conn_active += a_stat.m_pool_conn_active;
        ao_total.m_pool_conn_idle += a_stat.m_pool_conn_idle;
        ao_total.m_pool_proxy_conn_active += a_stat.m_pool_proxy_conn_active;
        ao_total.m_pool_proxy_conn_idle += a_stat.m_pool_proxy_conn_idle;
        ao_total.m_pool_hconn_free += a_stat.m_pool_hconn_free;
        ao_total.m_pool_hconn_used += a_stat.m_pool_hconn_used;
        ao_total.m_pool_resp_free += a_stat.m_pool_resp_free;
        ao_total.m_pool_resp_used += a_stat.m_pool_resp_used;
        ao_total.m_pool_rqst_free += a_stat.m_pool_rqst_free;
        ao_total.m_pool_rqst_used += a_stat.m_pool_rqst_used;
        ao_total.m_pool_nbq_free += a_stat.m_pool_nbq_free;
        ao_total.m_pool_nbq_used += a_stat.m_pool_nbq_used;
        ao_total.m_total_run += a_stat.m_total_run;
        ao_total.m_total_errors += a_stat.m_total_errors;
        ao_total.m_total_bytes_read += a_stat.m_total_bytes_read;
        ao_total.m_total_bytes_written += a_stat.m_total_bytes_written;
}

//: ----------------------------------------------------------------------------
//: \details: Get hlx stats
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::get_stat(const t_stat_list_t &a_stat_list, t_stat_t &ao_stat)
{
        // Aggregate
        for(t_stat_list_t::const_iterator i_s = a_stat_list.begin();
            i_s != a_stat_list.end();
            ++i_s)
        {
                // Get stuff from client...
                // TODO
                t_stat_t l_stat;
                aggregate_stat(ao_stat, *i_s);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::get_stat(t_stat_t &ao_stat)
{
        t_stat_list_t l_stat_list;
        get_thread_stat(l_stat_list);
        get_stat(l_stat_list, ao_stat);
        // TODO Cache stats -up to second???
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define DISPLAY_DNS_STAT(_stat) NDBG_OUTPUT("| %s%24s%s: %12" PRIu64 "\n", ANSI_COLOR_FG_MAGENTA, #_stat, ANSI_COLOR_OFF,(uint64_t)l_stat._stat)
#define DISPLAY_CLN_STAT(_stat) NDBG_OUTPUT("| %s%24s%s: %12" PRIu64 "\n", ANSI_COLOR_FG_GREEN,   #_stat, ANSI_COLOR_OFF,(uint64_t)l_stat._stat)
#define DISPLAY_SRV_STAT(_stat) NDBG_OUTPUT("| %s%24s%s: %12" PRIu64 "\n", ANSI_COLOR_FG_BLUE,    #_stat, ANSI_COLOR_OFF,(uint64_t)l_stat._stat)
#define DISPLAY_GEN_STAT(_stat) NDBG_OUTPUT("| %s%24s%s: %12" PRIu64 "\n", ANSI_COLOR_FG_CYAN,    #_stat, ANSI_COLOR_OFF,(uint64_t)l_stat._stat)
void hlx::display_stat(void)
{
        uint32_t i_t = 0;
        for(t_hlx_list_t::const_iterator i_client = m_t_hlx_list.begin();
           i_client != m_t_hlx_list.end();
           ++i_client, ++i_t)
        {
                // Get stuff from client...
                // TODO
                const t_stat_t &l_stat = (*i_client)->get_stat();
                NDBG_OUTPUT("+-----------------------------------------------------------\n");
                NDBG_OUTPUT("| THREAD [%6d]\n", i_t);
                NDBG_OUTPUT("+-----------------------------------------------------------\n");
                //xstat_t m_stat_us_connect;
                //xstat_t m_stat_us_first_response;
                //xstat_t m_stat_us_end_to_end;
                DISPLAY_DNS_STAT(m_dns_resolve_req);
                DISPLAY_DNS_STAT(m_dns_resolve_active);
                DISPLAY_DNS_STAT(m_dns_resolved);
                DISPLAY_DNS_STAT(m_dns_resolve_ev);
                DISPLAY_CLN_STAT(m_ups_conn_started);
                DISPLAY_CLN_STAT(m_ups_conn_completed);
                DISPLAY_CLN_STAT(m_ups_reqs);
                DISPLAY_CLN_STAT(m_ups_idle_killed);
                DISPLAY_CLN_STAT(m_ups_subr_queued);
                DISPLAY_SRV_STAT(m_cln_conn_started);
                DISPLAY_SRV_STAT(m_cln_conn_completed);
                DISPLAY_SRV_STAT(m_cln_reqs);
                DISPLAY_SRV_STAT(m_cln_idle_killed);
                DISPLAY_SRV_STAT(m_pool_conn_active);
                DISPLAY_SRV_STAT(m_pool_conn_idle);
                DISPLAY_SRV_STAT(m_pool_proxy_conn_active);
                DISPLAY_SRV_STAT(m_pool_proxy_conn_idle);
                DISPLAY_SRV_STAT(m_pool_hconn_free);
                DISPLAY_SRV_STAT(m_pool_hconn_used);
                DISPLAY_SRV_STAT(m_pool_resp_free);
                DISPLAY_SRV_STAT(m_pool_resp_used);
                DISPLAY_SRV_STAT(m_pool_rqst_free);
                DISPLAY_SRV_STAT(m_pool_rqst_used);
                DISPLAY_SRV_STAT(m_pool_nbq_free);
                DISPLAY_SRV_STAT(m_pool_nbq_used);
                DISPLAY_GEN_STAT(m_total_run);
                DISPLAY_GEN_STAT(m_total_errors);
                DISPLAY_GEN_STAT(m_total_bytes_read);
                DISPLAY_GEN_STAT(m_total_bytes_written);
                //NDBG_OUTPUT("| %sPending resolve%s: \n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF);
                //for(subr_pending_resolv_map_t::const_iterator i_r = l_stat.m_subr_pending_resolv_map.begin();
                //    i_r != l_stat.m_subr_pending_resolv_map.end();
                //    ++i_r)
                //{
                //        NDBG_OUTPUT("|     %s\n", i_r->first.c_str());
                //}
                //status_code_count_map_t m_status_code_count_map;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nresolver *hlx::get_nresolver(void)
{
        return m_nresolver;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool hlx::get_dns_use_sync(void)
{
        return m_dns_use_sync;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool hlx::is_running(void)
{
        for (t_hlx_list_t::iterator i_t = m_t_hlx_list.begin();
                        i_t != m_t_hlx_list.end();
                        ++i_t)
        {
                if((*i_t)->is_running())
                {
                        return true;
                }
        }
        return false;
}

//: ----------------------------------------------------------------------------
//:                               Setters
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_verbose(bool a_val)
{
        m_t_conf->m_verbose = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_color(bool a_val)
{
        m_t_conf->m_color = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_stats(bool a_val)
{
        m_t_conf->m_collect_stats = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_num_threads(uint32_t a_num_threads)
{
        m_num_threads = a_num_threads;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_num_parallel(uint32_t a_num_parallel)
{
        m_t_conf->m_num_parallel = a_num_parallel;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_num_reqs_per_conn(int32_t a_num_reqs_per_conn)
{
        m_t_conf->m_num_reqs_per_conn = a_num_reqs_per_conn;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_start_time_ms(uint64_t a_start_time_ms)
{
        m_start_time_ms = a_start_time_ms;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_collect_stats(bool a_val)
{
        m_t_conf->m_collect_stats = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: Global timeout for connect/read/write
//: \return:  NA
//: \param:   a_val: timeout in seconds
//: ----------------------------------------------------------------------------
void hlx::set_timeout_ms(uint32_t a_val)
{
        m_t_conf->m_timeout_ms = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: Set server name -used for requests/response headers
//: \return:  NA
//: \param:   a_name: server name
//: ----------------------------------------------------------------------------
void hlx::set_server_name(const std::string &a_name)
{
        m_server_name = a_name;
}

//: ----------------------------------------------------------------------------
//: \details: Get server name -used for requests/response headers
//: \return:  server name
//: \param:   NA
//: ----------------------------------------------------------------------------
const std::string &hlx::get_server_name(void) const
{
        return m_server_name;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_sock_opt_no_delay(bool a_val)
{
        m_t_conf->m_sock_opt_no_delay = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_sock_opt_send_buf_size(uint32_t a_send_buf_size)
{
        m_t_conf->m_sock_opt_send_buf_size = a_send_buf_size;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_sock_opt_recv_buf_size(uint32_t a_recv_buf_size)
{
        m_t_conf->m_sock_opt_recv_buf_size = a_recv_buf_size;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_dns_use_sync(bool a_val)
{
        m_dns_use_sync = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_dns_use_ai_cache(bool a_val)
{
        m_dns_use_ai_cache = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_dns_ai_cache_file(const std::string &a_file)
{
        m_dns_ai_cache_file = a_file;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_tls_server_ctx_cipher_list(const std::string &a_cipher_list)
{
        m_t_conf->m_tls_server_ctx_cipher_list = a_cipher_list;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx::set_tls_server_ctx_options(const std::string &a_tls_options_str)
{
        int32_t l_status;
        l_status = get_tls_options_str_val(a_tls_options_str,
                                           m_t_conf->m_tls_server_ctx_options);
        if(l_status != HLX_STATUS_OK)
        {
                return HLX_STATUS_ERROR;
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx::set_tls_server_ctx_options(long a_tls_options)
{
        m_t_conf->m_tls_server_ctx_options = a_tls_options;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_tls_server_ctx_key(const std::string &a_tls_key)
{
        m_t_conf->m_tls_server_ctx_key = a_tls_key;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_tls_server_ctx_crt(const std::string &a_tls_crt)
{
        m_t_conf->m_tls_server_ctx_crt = a_tls_crt;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_tls_client_ctx_cipher_list(const std::string &a_cipher_list)
{
        m_t_conf->m_tls_client_ctx_cipher_list = a_cipher_list;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_tls_client_ctx_ca_path(const std::string &a_tls_ca_path)
{
        m_t_conf->m_tls_client_ctx_ca_path = a_tls_ca_path;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_tls_client_ctx_ca_file(const std::string &a_tls_ca_file)
{
        m_t_conf->m_tls_client_ctx_ca_file = a_tls_ca_file;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx::set_tls_client_ctx_options(const std::string &a_tls_options_str)
{
        int32_t l_status;
        l_status = get_tls_options_str_val(a_tls_options_str,
                                           m_t_conf->m_tls_client_ctx_options);
        if(l_status != HLX_STATUS_OK)
        {
                return HLX_STATUS_ERROR;
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx::set_tls_client_ctx_options(long a_tls_options)
{
        m_t_conf->m_tls_client_ctx_options = a_tls_options;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: Running
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx::register_lsnr(lsnr *a_lsnr)
{
        if(!a_lsnr)
        {
                return HLX_STATUS_ERROR;
        }
        int32_t l_status;
        l_status = a_lsnr->init();
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error performing lsnr init.\n");
                return HLX_STATUS_ERROR;
        }
        if(is_running())
        {
                for(t_hlx_list_t::iterator i_t = m_t_hlx_list.begin();
                    i_t != m_t_hlx_list.end();
                    ++i_t)
                {
                        if(*i_t)
                        {
                                l_status = (*i_t)->add_lsnr(*a_lsnr);
                                if(l_status != STATUS_OK)
                                {
                                        NDBG_PRINT("Error performing add_lsnr.\n");
                                        return HLX_STATUS_ERROR;
                                }
                        }
                }
        }
        else
        {
                m_lsnr_list.push_back(a_lsnr);
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t hlx::get_next_subr_uuid(void)
{
        return ++m_cur_subr_uid;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlx::t_hlx_list_t &hlx::get_t_hlx_list(void)
{
        return m_t_hlx_list;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx::init_run(void)
{
        int l_status = 0;
        if(!m_is_initd)
        {
                l_status = init();
                if(HLX_STATUS_OK != l_status)
                {
                        return HLX_STATUS_ERROR;
                }
        }
        if(m_t_hlx_list.empty())
        {
                l_status = init_t_hlx_list();
                if(STATUS_OK != l_status)
                {
                        return HLX_STATUS_ERROR;
                }
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx::run(void)
{
        //NDBG_PRINT("Running...\n");
        int32_t l_status;
        l_status = init_run();
        if(HLX_STATUS_OK != l_status)
        {
                return HLX_STATUS_ERROR;
        }
        set_start_time_ms(get_time_ms());
        if(m_num_threads == 0)
        {
                (*(m_t_hlx_list.begin()))->t_run(NULL);
        }
        else
        {
                for(t_hlx_list_t::iterator i_t = m_t_hlx_list.begin();
                                i_t != m_t_hlx_list.end();
                                ++i_t)
                {
                        (*i_t)->run();
                }
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx::stop(void)
{
        int32_t l_retval = HLX_STATUS_OK;
        for (t_hlx_list_t::iterator i_t = m_t_hlx_list.begin();
                        i_t != m_t_hlx_list.end();
                        ++i_t)
        {
                (*i_t)->stop();
        }
        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx::wait_till_stopped(void)
{
        // Join all threads before exit
        for(t_hlx_list_t::iterator i_server = m_t_hlx_list.begin();
           i_server != m_t_hlx_list.end();
            ++i_server)
        {
                pthread_join(((*i_server)->m_t_run_thread), NULL);
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  client status indicating success or failure
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx::init(void)
{
        if(true == m_is_initd)
        {
                return HLX_STATUS_OK;
        }

        m_nresolver = new nresolver();

        // -------------------------------------------
        // Init resolver with cache
        // -------------------------------------------
        int32_t l_ldb_init_status;
        l_ldb_init_status = m_nresolver->init(m_dns_use_ai_cache, m_dns_ai_cache_file);
        if(STATUS_OK != l_ldb_init_status)
        {
                NDBG_PRINT("Error performing resolver init with ai_cache: %s\n",
                           m_dns_ai_cache_file.c_str());
                return HLX_STATUS_ERROR;
        }

        // not initialized yet
        for(lsnr_list_t::iterator i_t = m_lsnr_list.begin();
                        i_t != m_lsnr_list.end();
                        ++i_t)
        {
                if(*i_t)
                {
                        int32_t l_status;
                        l_status = (*i_t)->init();
                        if(l_status != STATUS_OK)
                        {
                                NDBG_PRINT("Error performing lsnr setup.\n");
                                return HLX_STATUS_ERROR;
                        }
                }
        }

        // -------------------------------------------
        // SSL init...
        // -------------------------------------------
        signal(SIGPIPE, SIG_IGN);

        // -------------------------------------------
        // SSL init...
        // -------------------------------------------
        tls_init();

        std::string l_unused;
        // Server
        m_t_conf->m_tls_server_ctx = tls_init_ctx(
                  m_t_conf->m_tls_server_ctx_cipher_list, // ctx cipher list str
                  m_t_conf->m_tls_server_ctx_options,     // ctx options
                  l_unused,                               // ctx ca file
                  l_unused,                               // ctx ca path
                  true,                                   // is server?
                  m_t_conf->m_tls_server_ctx_key,         // tls key
                  m_t_conf->m_tls_server_ctx_crt);        // tls crt
        if(NULL == m_t_conf->m_tls_server_ctx)
        {
                NDBG_PRINT("Error: performing ssl_init(server) with cipher_list: %s\n",
                           m_t_conf->m_tls_server_ctx_cipher_list.c_str());
                return HLX_STATUS_ERROR;
        }
        // Client
        m_t_conf->m_tls_client_ctx = tls_init_ctx(
                m_t_conf->m_tls_client_ctx_cipher_list, // ctx cipher list str
                m_t_conf->m_tls_client_ctx_options,     // ctx options
                m_t_conf->m_tls_client_ctx_ca_file,     // ctx ca file
                m_t_conf->m_tls_client_ctx_ca_path,     // ctx ca path
                false,                                  // is server?
                l_unused,                               // tls key
                l_unused);                              // tls crt
        if(NULL == m_t_conf->m_tls_client_ctx)
        {
                NDBG_PRINT("Error: performing ssl_init(client) with cipher_list: %s\n",
                           m_t_conf->m_tls_client_ctx_cipher_list.c_str());
                return HLX_STATUS_ERROR;
        }
        m_is_initd = true;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx::init_t_hlx_list(void)
{
        // -------------------------------------------
        // Create t_hlx list...
        // -------------------------------------------
        // 0 threads -make a single hlx
        m_t_conf->m_hlx = this;
        if(m_num_threads == 0)
        {
                int32_t l_status;
                t_hlx *l_t_hlx = new t_hlx(m_t_conf);
                for(lsnr_list_t::iterator i_t = m_lsnr_list.begin();
                                i_t != m_lsnr_list.end();
                                ++i_t)
                {
                        if(*i_t)
                        {
                                l_status = l_t_hlx->add_lsnr(*(*i_t));
                                if(l_status != STATUS_OK)
                                {
                                        delete l_t_hlx;
                                        l_t_hlx = NULL;
                                        NDBG_PRINT("Error performing add_lsnr.\n");
                                        return STATUS_ERROR;
                                }
                        }
                }
                l_status = l_t_hlx->init();
                if(l_status != STATUS_OK)
                {
                        NDBG_PRINT("Error performing init.\n");
                        return STATUS_ERROR;
                }
                m_t_hlx_list.push_back(l_t_hlx);
        }
        else
        {
                for(uint32_t i_server_idx = 0; i_server_idx < m_num_threads; ++i_server_idx)
                {
                        int32_t l_status;
                        t_hlx *l_t_hlx = new t_hlx(m_t_conf);
                        for(lsnr_list_t::iterator i_t = m_lsnr_list.begin();
                                        i_t != m_lsnr_list.end();
                                        ++i_t)
                        {
                                if(*i_t)
                                {
                                        l_status = l_t_hlx->add_lsnr(*(*i_t));
                                        if(l_status != STATUS_OK)
                                        {
                                                delete l_t_hlx;
                                                l_t_hlx = NULL;
                                                NDBG_PRINT("Error performing add_lsnr.\n");
                                                return STATUS_ERROR;
                                        }
                                }
                        }
                        l_status = l_t_hlx->init();
                        if(l_status != STATUS_OK)
                        {
                                NDBG_PRINT("Error performing init.\n");
                                return STATUS_ERROR;
                        }
                        m_t_hlx_list.push_back(l_t_hlx);
                }
        }
        return STATUS_OK;
}

} //namespace ns_hlx {
