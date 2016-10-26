//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    srvr.cc
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
#include "ndebug.h"
#include "nresolver.h"
#include "nconn_tcp.h"
#include "tls_util.h"
#include "hlx/srvr.h"

#include "t_srvr.h"
#include "hlx/stat.h"
#include "hlx/lsnr.h"
#include "hlx/status.h"
#include "hlx/time_util.h"
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <openssl/ssl.h>
#include <pthread.h>

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
srvr::srvr(void):
        m_t_conf(NULL),
        m_stats(false),
        m_num_threads(1),
        m_lsnr_list(),
        m_nresolver(NULL),
        m_dns_use_sync(false),
        m_dns_use_ai_cache(true),
        m_dns_ai_cache_file(NRESOLVER_DEFAULT_AI_CACHE_FILE),
        m_start_time_ms(0),
        m_t_srvr_list(),
        m_is_initd(false),
        m_cur_subr_uid(0),
        m_server_name("srvr"),
        m_stat_mutex(),
        m_stat_update_ms(S_STAT_UPDATE_MS_DEFAULT),
        m_stat_last_time_ms(0),
        m_stat_list_cache(),
        m_stat_cache(),
        m_stat_calc_cache()
{
        m_t_conf = new t_conf();

        pthread_mutex_init(&m_stat_mutex, NULL);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
srvr::~srvr()
{
        for(t_srvr_list_t::iterator i_t = m_t_srvr_list.begin();
                        i_t != m_t_srvr_list.end();
                        ++i_t)
        {
                if(*i_t)
                {
                        delete *i_t;
                }
        }
        m_t_srvr_list.clear();

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
//: \details: Aggregate thread stats.
//: \return:  NA
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void aggregate_stat(t_stat_cntr_t &ao_total, const t_stat_cntr_t &a_stat)
{
        add_stat(ao_total.m_upsv_stat_us_connect , a_stat.m_upsv_stat_us_connect);
        add_stat(ao_total.m_upsv_stat_us_first_response , a_stat.m_upsv_stat_us_first_response);
        add_stat(ao_total.m_upsv_stat_us_end_to_end , a_stat.m_upsv_stat_us_end_to_end);

        ao_total.m_dns_resolve_req += a_stat.m_dns_resolve_req;
        ao_total.m_dns_resolve_active += a_stat.m_dns_resolve_active;
        ao_total.m_dns_resolved += a_stat.m_dns_resolved;
        ao_total.m_dns_resolve_ev += a_stat.m_dns_resolve_ev;

        ao_total.m_upsv_conn_started += a_stat.m_upsv_conn_started;
        ao_total.m_upsv_conn_completed += a_stat.m_upsv_conn_completed;
        ao_total.m_upsv_reqs += a_stat.m_upsv_reqs;
        ao_total.m_upsv_idle_killed += a_stat.m_upsv_idle_killed;
        ao_total.m_upsv_subr_queued += a_stat.m_upsv_subr_queued;
        ao_total.m_upsv_resp += a_stat.m_upsv_resp;
        ao_total.m_upsv_resp_status_2xx += a_stat.m_upsv_resp_status_2xx;
        ao_total.m_upsv_resp_status_3xx += a_stat.m_upsv_resp_status_3xx;
        ao_total.m_upsv_resp_status_4xx += a_stat.m_upsv_resp_status_4xx;
        ao_total.m_upsv_resp_status_5xx += a_stat.m_upsv_resp_status_5xx;
        ao_total.m_upsv_errors += a_stat.m_upsv_errors;
        ao_total.m_upsv_bytes_read += a_stat.m_upsv_bytes_read;
        ao_total.m_upsv_bytes_written += a_stat.m_upsv_bytes_written;

        ao_total.m_clnt_conn_started += a_stat.m_clnt_conn_started;
        ao_total.m_clnt_conn_completed += a_stat.m_clnt_conn_completed;
        ao_total.m_clnt_reqs += a_stat.m_clnt_reqs;
        ao_total.m_clnt_idle_killed += a_stat.m_clnt_idle_killed;
        ao_total.m_clnt_resp += a_stat.m_clnt_resp;
        ao_total.m_clnt_resp_status_2xx += a_stat.m_clnt_resp_status_2xx;
        ao_total.m_clnt_resp_status_3xx += a_stat.m_clnt_resp_status_3xx;
        ao_total.m_clnt_resp_status_4xx += a_stat.m_clnt_resp_status_4xx;
        ao_total.m_clnt_resp_status_5xx += a_stat.m_clnt_resp_status_5xx;
        ao_total.m_clnt_errors += a_stat.m_clnt_errors;
        ao_total.m_clnt_bytes_read += a_stat.m_clnt_bytes_read;
        ao_total.m_clnt_bytes_written += a_stat.m_clnt_bytes_written;

        ao_total.m_pool_conn_active += a_stat.m_pool_conn_active;
        ao_total.m_pool_conn_idle += a_stat.m_pool_conn_idle;
        ao_total.m_pool_proxy_conn_active += a_stat.m_pool_proxy_conn_active;
        ao_total.m_pool_proxy_conn_idle += a_stat.m_pool_proxy_conn_idle;
        ao_total.m_pool_clnt_free += a_stat.m_pool_clnt_free;
        ao_total.m_pool_clnt_used += a_stat.m_pool_clnt_used;
        ao_total.m_pool_resp_free += a_stat.m_pool_resp_free;
        ao_total.m_pool_resp_used += a_stat.m_pool_resp_used;
        ao_total.m_pool_rqst_free += a_stat.m_pool_rqst_free;
        ao_total.m_pool_rqst_used += a_stat.m_pool_rqst_used;
        ao_total.m_pool_nbq_free += a_stat.m_pool_nbq_free;
        ao_total.m_pool_nbq_used += a_stat.m_pool_nbq_used;

        ao_total.m_total_run += a_stat.m_total_run;
        ao_total.m_total_add_timer += a_stat.m_total_add_timer;
}

//: ----------------------------------------------------------------------------
//: \details: Get srvr stats
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::get_stat(t_stat_cntr_t &ao_stat,
                    t_stat_calc_t &ao_calc_stat,
                    t_stat_cntr_list_t &ao_stat_list,
                    bool a_no_cache)
{
        pthread_mutex_lock(&m_stat_mutex);
        // Check last time
        uint64_t l_cur_time_ms = get_time_ms();
        uint64_t l_delta_ms = l_cur_time_ms - m_stat_last_time_ms;
        if(!a_no_cache)
        {
                if(l_delta_ms < m_stat_update_ms)
                {
                        ao_stat = m_stat_cache;
                        ao_calc_stat = m_stat_calc_cache;
                        ao_stat_list = m_stat_list_cache;
                        pthread_mutex_unlock(&m_stat_mutex);
                        return;
                }
        }

        // -------------------------------------------------
        // store last values -for calc'd stats
        // -------------------------------------------------
        t_stat_cntr_t l_last = m_stat_cache;

        m_stat_list_cache.clear();
        m_stat_cache.clear();
        m_stat_calc_cache.clear();

        // Aggregate
        for(t_srvr_list_t::const_iterator i_client = m_t_srvr_list.begin();
           i_client != m_t_srvr_list.end();
           ++i_client)
        {
                // Get stuff from client...
                t_stat_cntr_t l_stat;
                int32_t l_s;
                l_s = (*i_client)->get_stat(l_stat);
                if(l_s != HLX_STATUS_OK)
                {
                        // TODO -do nothing...
                        continue;
                }
                m_stat_list_cache.emplace(m_stat_list_cache.begin(), l_stat);
                aggregate_stat(m_stat_cache, l_stat);
        }

        // -------------------------------------------------
        // calc'd stats
        // -------------------------------------------------
        m_stat_calc_cache.m_clnt_req_delta = m_stat_cache.m_clnt_reqs - l_last.m_clnt_reqs;
        uint64_t l_delta_resp = m_stat_cache.m_clnt_resp - l_last.m_clnt_resp;
        m_stat_calc_cache.m_clnt_resp_delta = l_delta_resp;
        if(l_delta_resp > 0)
        {
                m_stat_calc_cache.m_clnt_resp_status_2xx_pcnt = 100.0*((float)(m_stat_cache.m_clnt_resp_status_2xx - l_last.m_clnt_resp_status_2xx))/((float)l_delta_resp);
                m_stat_calc_cache.m_clnt_resp_status_3xx_pcnt = 100.0*((float)(m_stat_cache.m_clnt_resp_status_3xx - l_last.m_clnt_resp_status_3xx))/((float)l_delta_resp);
                m_stat_calc_cache.m_clnt_resp_status_4xx_pcnt = 100.0*((float)(m_stat_cache.m_clnt_resp_status_4xx - l_last.m_clnt_resp_status_4xx))/((float)l_delta_resp);
                m_stat_calc_cache.m_clnt_resp_status_5xx_pcnt = 100.0*((float)(m_stat_cache.m_clnt_resp_status_5xx - l_last.m_clnt_resp_status_5xx))/((float)l_delta_resp);
        }
        if(l_delta_ms > 0)
        {
                m_stat_calc_cache.m_clnt_req_s = ((float)m_stat_calc_cache.m_clnt_req_delta*1000)/((float)l_delta_ms);
                m_stat_calc_cache.m_clnt_bytes_read_s = ((float)((m_stat_cache.m_clnt_bytes_read - l_last.m_clnt_bytes_read)*1000))/((float)l_delta_ms);
                m_stat_calc_cache.m_clnt_bytes_write_s = ((float)((m_stat_cache.m_clnt_bytes_written - l_last.m_clnt_bytes_written)*1000))/((float)l_delta_ms);
        }
        m_stat_calc_cache.m_upsv_req_delta = m_stat_cache.m_upsv_reqs - l_last.m_upsv_reqs;
        l_delta_resp = m_stat_cache.m_upsv_resp - l_last.m_upsv_resp;
        m_stat_calc_cache.m_upsv_resp_delta = l_delta_resp;
        if(l_delta_resp > 0)
        {
                m_stat_calc_cache.m_upsv_resp_status_2xx_pcnt = 100.0*((float)(m_stat_cache.m_upsv_resp_status_2xx - l_last.m_upsv_resp_status_2xx))/((float)l_delta_resp);
                m_stat_calc_cache.m_upsv_resp_status_3xx_pcnt = 100.0*((float)(m_stat_cache.m_upsv_resp_status_3xx - l_last.m_upsv_resp_status_3xx))/((float)l_delta_resp);
                m_stat_calc_cache.m_upsv_resp_status_4xx_pcnt = 100.0*((float)(m_stat_cache.m_upsv_resp_status_4xx - l_last.m_upsv_resp_status_4xx))/((float)l_delta_resp);
                m_stat_calc_cache.m_upsv_resp_status_5xx_pcnt = 100.0*((float)(m_stat_cache.m_upsv_resp_status_5xx - l_last.m_upsv_resp_status_5xx))/((float)l_delta_resp);
        }
        if(l_delta_ms > 0)
        {
                m_stat_calc_cache.m_upsv_req_s = ((float)m_stat_calc_cache.m_upsv_req_delta*1000)/((float)l_delta_ms);
                m_stat_calc_cache.m_upsv_bytes_read_s = ((float)((m_stat_cache.m_upsv_bytes_read - l_last.m_upsv_bytes_read)*1000))/((float)l_delta_ms);
                m_stat_calc_cache.m_upsv_bytes_write_s = ((float)((m_stat_cache.m_upsv_bytes_written - l_last.m_upsv_bytes_written)*1000))/((float)l_delta_ms);
        }
        // -------------------------------------------------
        // copy
        // -------------------------------------------------
        ao_stat = m_stat_cache;
        ao_calc_stat = m_stat_calc_cache;
        ao_stat_list = m_stat_list_cache;
        m_stat_last_time_ms = l_cur_time_ms;
        pthread_mutex_unlock(&m_stat_mutex);
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
void srvr::display_stat(void)
{
        uint32_t i_t = 0;
        for(t_srvr_list_t::const_iterator i_client = m_t_srvr_list.begin();
           i_client != m_t_srvr_list.end();
           ++i_client, ++i_t)
        {
                // Get stuff from client...
                // TODO
                t_stat_cntr_t l_stat;
                int32_t l_s;
                l_s = (*i_client)->get_stat(l_stat);
                if(l_s != HLX_STATUS_OK)
                {
                        // TODO -do nothing...
                        continue;
                }
                NDBG_OUTPUT("+-----------------------------------------------------------\n");
                NDBG_OUTPUT("| THREAD [%6d]\n", i_t);
                NDBG_OUTPUT("+-----------------------------------------------------------\n");
                DISPLAY_DNS_STAT(m_dns_resolve_req);
                DISPLAY_DNS_STAT(m_dns_resolve_active);
                DISPLAY_DNS_STAT(m_dns_resolved);
                DISPLAY_DNS_STAT(m_dns_resolve_ev);
                DISPLAY_CLN_STAT(m_upsv_conn_started);
                DISPLAY_CLN_STAT(m_upsv_conn_completed);
                DISPLAY_CLN_STAT(m_upsv_reqs);
                DISPLAY_CLN_STAT(m_upsv_idle_killed);
                DISPLAY_CLN_STAT(m_upsv_subr_queued);
                DISPLAY_SRV_STAT(m_upsv_resp);
                DISPLAY_SRV_STAT(m_upsv_resp_status_2xx);
                DISPLAY_SRV_STAT(m_upsv_resp_status_3xx);
                DISPLAY_SRV_STAT(m_upsv_resp_status_4xx);
                DISPLAY_SRV_STAT(m_upsv_resp_status_5xx);
                DISPLAY_SRV_STAT(m_upsv_errors);
                DISPLAY_SRV_STAT(m_upsv_bytes_read);
                DISPLAY_SRV_STAT(m_upsv_bytes_written);
                DISPLAY_SRV_STAT(m_clnt_conn_started);
                DISPLAY_SRV_STAT(m_clnt_conn_completed);
                DISPLAY_SRV_STAT(m_clnt_reqs);
                DISPLAY_SRV_STAT(m_clnt_idle_killed);
                DISPLAY_SRV_STAT(m_clnt_resp);
                DISPLAY_SRV_STAT(m_clnt_resp_status_2xx);
                DISPLAY_SRV_STAT(m_clnt_resp_status_3xx);
                DISPLAY_SRV_STAT(m_clnt_resp_status_4xx);
                DISPLAY_SRV_STAT(m_clnt_resp_status_5xx);
                DISPLAY_SRV_STAT(m_clnt_errors);
                DISPLAY_SRV_STAT(m_clnt_bytes_read);
                DISPLAY_SRV_STAT(m_clnt_bytes_written);
                DISPLAY_SRV_STAT(m_pool_conn_active);
                DISPLAY_SRV_STAT(m_pool_conn_idle);
                DISPLAY_SRV_STAT(m_pool_proxy_conn_active);
                DISPLAY_SRV_STAT(m_pool_proxy_conn_idle);
                DISPLAY_SRV_STAT(m_pool_clnt_free);
                DISPLAY_SRV_STAT(m_pool_clnt_used);
                DISPLAY_SRV_STAT(m_pool_resp_free);
                DISPLAY_SRV_STAT(m_pool_resp_used);
                DISPLAY_SRV_STAT(m_pool_rqst_free);
                DISPLAY_SRV_STAT(m_pool_rqst_used);
                DISPLAY_SRV_STAT(m_pool_nbq_free);
                DISPLAY_SRV_STAT(m_pool_nbq_used);
                DISPLAY_GEN_STAT(m_total_run);
                DISPLAY_GEN_STAT(m_total_add_timer);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nresolver *srvr::get_nresolver(void)
{
        return m_nresolver;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool srvr::get_dns_use_sync(void)
{
        return m_dns_use_sync;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool srvr::is_running(void)
{
        for (t_srvr_list_t::iterator i_t = m_t_srvr_list.begin();
                        i_t != m_t_srvr_list.end();
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
void srvr::set_rqst_resp_logging(bool a_val)
{
        m_t_conf->m_rqst_resp_logging = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_rqst_resp_logging_color(bool a_val)
{
        m_t_conf->m_rqst_resp_logging_color = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_stats(bool a_val)
{
        m_t_conf->m_collect_stats = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_num_threads(uint32_t a_num_threads)
{
        m_num_threads = a_num_threads;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_num_parallel(uint32_t a_num_parallel)
{
        m_t_conf->m_num_parallel = a_num_parallel;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_num_reqs_per_conn(int32_t a_num_reqs_per_conn)
{
        m_t_conf->m_num_reqs_per_conn = a_num_reqs_per_conn;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_start_time_ms(uint64_t a_start_time_ms)
{
        m_start_time_ms = a_start_time_ms;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_collect_stats(bool a_val)
{
        m_t_conf->m_collect_stats = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_count_response_status(bool a_val)
{
        m_t_conf->m_count_response_status = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_update_stats_ms(uint32_t a_update_ms)
{
        m_t_conf->m_update_stats_ms = a_update_ms;
        m_stat_update_ms = a_update_ms;
}

//: ----------------------------------------------------------------------------
//: \details: Global timeout for connect/read/write
//: \return:  NA
//: \param:   a_val: timeout in seconds
//: ----------------------------------------------------------------------------
void srvr::set_timeout_ms(uint32_t a_val)
{
        m_t_conf->m_timeout_ms = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: Set response done callback
//: \return:  NA
//: \param:   a_name: server name
//: ----------------------------------------------------------------------------
void srvr::set_resp_done_cb(resp_done_cb_t a_cb)
{
        m_t_conf->m_resp_done_cb = a_cb;
}

//: ----------------------------------------------------------------------------
//: \details: Set server name -used for requests/response headers
//: \return:  NA
//: \param:   a_name: server name
//: ----------------------------------------------------------------------------
void srvr::set_server_name(const std::string &a_name)
{
        m_server_name = a_name;
}

//: ----------------------------------------------------------------------------
//: \details: Get server name -used for requests/response headers
//: \return:  server name
//: \param:   NA
//: ----------------------------------------------------------------------------
const std::string &srvr::get_server_name(void) const
{
        return m_server_name;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_sock_opt_no_delay(bool a_val)
{
        m_t_conf->m_sock_opt_no_delay = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_sock_opt_send_buf_size(uint32_t a_send_buf_size)
{
        m_t_conf->m_sock_opt_send_buf_size = a_send_buf_size;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_sock_opt_recv_buf_size(uint32_t a_recv_buf_size)
{
        m_t_conf->m_sock_opt_recv_buf_size = a_recv_buf_size;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_dns_use_sync(bool a_val)
{
        m_dns_use_sync = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_dns_use_ai_cache(bool a_val)
{
        m_dns_use_ai_cache = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_dns_ai_cache_file(const std::string &a_file)
{
        m_dns_ai_cache_file = a_file;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_tls_server_ctx_cipher_list(const std::string &a_cipher_list)
{
        m_t_conf->m_tls_server_ctx_cipher_list = a_cipher_list;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int srvr::set_tls_server_ctx_options(const std::string &a_tls_options_str)
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
int srvr::set_tls_server_ctx_options(long a_tls_options)
{
        m_t_conf->m_tls_server_ctx_options = a_tls_options;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_tls_server_ctx_key(const std::string &a_tls_key)
{
        m_t_conf->m_tls_server_ctx_key = a_tls_key;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_tls_server_ctx_crt(const std::string &a_tls_crt)
{
        m_t_conf->m_tls_server_ctx_crt = a_tls_crt;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_tls_client_ctx_cipher_list(const std::string &a_cipher_list)
{
        m_t_conf->m_tls_client_ctx_cipher_list = a_cipher_list;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_tls_client_ctx_ca_path(const std::string &a_tls_ca_path)
{
        m_t_conf->m_tls_client_ctx_ca_path = a_tls_ca_path;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void srvr::set_tls_client_ctx_ca_file(const std::string &a_tls_ca_file)
{
        m_t_conf->m_tls_client_ctx_ca_file = a_tls_ca_file;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int srvr::set_tls_client_ctx_options(const std::string &a_tls_options_str)
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
int srvr::set_tls_client_ctx_options(long a_tls_options)
{
        m_t_conf->m_tls_client_ctx_options = a_tls_options;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: Running
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t srvr::register_lsnr(lsnr *a_lsnr)
{
        if(!a_lsnr)
        {
                return HLX_STATUS_ERROR;
        }
        int32_t l_status;
        l_status = a_lsnr->init();
        if(l_status != HLX_STATUS_OK)
        {
                NDBG_PRINT("Error performing lsnr init.\n");
                return HLX_STATUS_ERROR;
        }
        if(is_running())
        {
                for(t_srvr_list_t::iterator i_t = m_t_srvr_list.begin();
                    i_t != m_t_srvr_list.end();
                    ++i_t)
                {
                        if(*i_t)
                        {
                                l_status = (*i_t)->add_lsnr(*a_lsnr);
                                if(l_status != HLX_STATUS_OK)
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
uint64_t srvr::get_next_subr_uuid(void)
{
        return ++m_cur_subr_uid;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
srvr::t_srvr_list_t &srvr::get_t_srvr_list(void)
{
        return m_t_srvr_list;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t srvr::init_run(void)
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
        if(m_t_srvr_list.empty())
        {
                l_status = init_t_srvr_list();
                if(HLX_STATUS_OK != l_status)
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
int32_t srvr::run(void)
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
                (*(m_t_srvr_list.begin()))->t_run(NULL);
        }
        else
        {
                for(t_srvr_list_t::iterator i_t = m_t_srvr_list.begin();
                                i_t != m_t_srvr_list.end();
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
int srvr::stop(void)
{
        int32_t l_retval = HLX_STATUS_OK;
        for (t_srvr_list_t::iterator i_t = m_t_srvr_list.begin();
                        i_t != m_t_srvr_list.end();
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
int32_t srvr::wait_till_stopped(void)
{
        // Join all threads before exit
        for(t_srvr_list_t::iterator i_server = m_t_srvr_list.begin();
           i_server != m_t_srvr_list.end();
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
int srvr::init(void)
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
        if(HLX_STATUS_OK != l_ldb_init_status)
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
                        if(l_status != HLX_STATUS_OK)
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
int srvr::init_t_srvr_list(void)
{
        // -------------------------------------------
        // Create t_srvr list...
        // -------------------------------------------
        // 0 threads -make a single srvr
        m_t_conf->m_srvr = this;
        if(m_num_threads == 0)
        {
                int32_t l_status;
                t_srvr *l_t_srvr = new t_srvr(m_t_conf);
                for(lsnr_list_t::iterator i_t = m_lsnr_list.begin();
                                i_t != m_lsnr_list.end();
                                ++i_t)
                {
                        if(*i_t)
                        {
                                l_status = l_t_srvr->add_lsnr(*(*i_t));
                                if(l_status != HLX_STATUS_OK)
                                {
                                        delete l_t_srvr;
                                        l_t_srvr = NULL;
                                        NDBG_PRINT("Error performing add_lsnr.\n");
                                        return HLX_STATUS_ERROR;
                                }
                        }
                }
                l_status = l_t_srvr->init();
                if(l_status != HLX_STATUS_OK)
                {
                        NDBG_PRINT("Error performing init.\n");
                        return HLX_STATUS_ERROR;
                }
                m_t_srvr_list.push_back(l_t_srvr);
        }
        else
        {
                for(uint32_t i_server_idx = 0; i_server_idx < m_num_threads; ++i_server_idx)
                {
                        int32_t l_status;
                        t_srvr *l_t_srvr = new t_srvr(m_t_conf);
                        for(lsnr_list_t::iterator i_t = m_lsnr_list.begin();
                                        i_t != m_lsnr_list.end();
                                        ++i_t)
                        {
                                if(*i_t)
                                {
                                        l_status = l_t_srvr->add_lsnr(*(*i_t));
                                        if(l_status != HLX_STATUS_OK)
                                        {
                                                delete l_t_srvr;
                                                l_t_srvr = NULL;
                                                NDBG_PRINT("Error performing add_lsnr.\n");
                                                return HLX_STATUS_ERROR;
                                        }
                                }
                        }
                        l_status = l_t_srvr->init();
                        if(l_status != HLX_STATUS_OK)
                        {
                                NDBG_PRINT("Error performing init.\n");
                                return HLX_STATUS_ERROR;
                        }
                        m_t_srvr_list.push_back(l_t_srvr);
                }
        }
        return HLX_STATUS_OK;
}

} //namespace ns_hlx {
