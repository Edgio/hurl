//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hlx.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    03/11/2015
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
#ifndef _HLX_STAT_H
#define _HLX_STAT_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
// For fixed size types
#include <stdint.h>
#include <map>
#include <list>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::map<uint16_t, uint32_t > status_code_count_map_t;

// TODO DEBUG???
//typedef std::map<std::string, void *> subr_pending_resolv_map_t;

//: ----------------------------------------------------------------------------
//: xstat
//: ----------------------------------------------------------------------------
typedef struct xstat_struct
{
        double m_sum_x;
        double m_sum_x2;
        double m_min;
        double m_max;
        uint64_t m_num;

        double min() const { return m_min; }
        double max() const { return m_max; }
        double mean() const { return (m_num > 0) ? m_sum_x / m_num : 0.0; }
        double var() const { return (m_num > 0) ? (m_sum_x2 - m_sum_x) / m_num : 0.0; }
        double stdev() const;

        xstat_struct():
                m_sum_x(0.0),
                m_sum_x2(0.0),
                m_min(1000000000.0),
                m_max(0.0),
                m_num(0)
        {}

        void clear()
        {
                m_sum_x = 0.0;
                m_sum_x2 = 0.0;
                m_min = 1000000000.0;
                m_max = 0.0;
                m_num = 0;
        };
} xstat_t;

//: ----------------------------------------------------------------------------
//: counter stats
//: ----------------------------------------------------------------------------
typedef struct t_stat_cntr_struct
{
        // Stats
        xstat_t m_upsv_stat_us_connect;
        xstat_t m_upsv_stat_us_first_response;
        xstat_t m_upsv_stat_us_end_to_end;

        // Dns stats
        uint64_t m_dns_resolve_req;
        uint64_t m_dns_resolve_active;
        uint64_t m_dns_resolved;
        uint64_t m_dns_resolve_ev;

        // Upstream stats
        uint64_t m_upsv_conn_started;
        uint64_t m_upsv_conn_completed;
        uint64_t m_upsv_reqs;
        uint64_t m_upsv_idle_killed;
        uint64_t m_upsv_subr_queued;

        // Upstream resp
        uint64_t m_upsv_resp;
        uint64_t m_upsv_resp_status_2xx;
        uint64_t m_upsv_resp_status_3xx;
        uint64_t m_upsv_resp_status_4xx;
        uint64_t m_upsv_resp_status_5xx;

        // clnt counts
        uint64_t m_upsv_errors;
        uint64_t m_upsv_bytes_read;
        uint64_t m_upsv_bytes_written;

        // Server stats
        uint64_t m_clnt_conn_started;
        uint64_t m_clnt_conn_completed;
        uint64_t m_clnt_reqs;
        uint64_t m_clnt_idle_killed;

        // Response stats
        uint64_t m_clnt_resp;
        uint64_t m_clnt_resp_status_2xx;
        uint64_t m_clnt_resp_status_3xx;
        uint64_t m_clnt_resp_status_4xx;
        uint64_t m_clnt_resp_status_5xx;

        // clnt counts
        uint64_t m_clnt_errors;
        uint64_t m_clnt_bytes_read;
        uint64_t m_clnt_bytes_written;

        // Pool stats
        uint64_t m_pool_conn_active;
        uint64_t m_pool_conn_idle;
        uint64_t m_pool_proxy_conn_active;
        uint64_t m_pool_proxy_conn_idle;
        uint64_t m_pool_clnt_free;
        uint64_t m_pool_clnt_used;
        uint64_t m_pool_resp_free;
        uint64_t m_pool_resp_used;
        uint64_t m_pool_rqst_free;
        uint64_t m_pool_rqst_used;
        uint64_t m_pool_nbq_free;
        uint64_t m_pool_nbq_used;

        // Totals
        uint64_t m_total_run;
        uint64_t m_total_add_timer;

        t_stat_cntr_struct():
                m_upsv_stat_us_connect(),
                m_upsv_stat_us_first_response(),
                m_upsv_stat_us_end_to_end(),

                m_dns_resolve_req(0),
                m_dns_resolve_active(0),
                m_dns_resolved(0),
                m_dns_resolve_ev(0),

                m_upsv_conn_started(0),
                m_upsv_conn_completed(0),
                m_upsv_reqs(0),
                m_upsv_idle_killed(0),
                m_upsv_subr_queued(0),
                m_upsv_resp(0),
                m_upsv_resp_status_2xx(0),
                m_upsv_resp_status_3xx(0),
                m_upsv_resp_status_4xx(0),
                m_upsv_resp_status_5xx(0),
                m_upsv_errors(0),
                m_upsv_bytes_read(0),
                m_upsv_bytes_written(0),

                m_clnt_conn_started(0),
                m_clnt_conn_completed(0),
                m_clnt_reqs(0),
                m_clnt_idle_killed(0),
                m_clnt_resp(0),
                m_clnt_resp_status_2xx(0),
                m_clnt_resp_status_3xx(0),
                m_clnt_resp_status_4xx(0),
                m_clnt_resp_status_5xx(0),
                m_clnt_errors(0),
                m_clnt_bytes_read(0),
                m_clnt_bytes_written(0),

                m_pool_conn_active(0),
                m_pool_conn_idle(0),
                m_pool_proxy_conn_active(0),
                m_pool_proxy_conn_idle(0),
                m_pool_clnt_free(0),
                m_pool_clnt_used(0),
                m_pool_resp_free(0),
                m_pool_resp_used(0),
                m_pool_rqst_free(0),
                m_pool_rqst_used(0),
                m_pool_nbq_free(0),
                m_pool_nbq_used(0),

                m_total_run(0),
                m_total_add_timer(0)
        {}
        void clear()
        {
                m_upsv_stat_us_connect.clear();
                m_upsv_stat_us_connect.clear();
                m_upsv_stat_us_first_response.clear();
                m_upsv_stat_us_end_to_end.clear();

                m_dns_resolve_req = 0;
                m_dns_resolve_active = 0;
                m_dns_resolved = 0;
                m_dns_resolve_ev = 0;

                m_upsv_conn_started = 0;
                m_upsv_conn_completed = 0;
                m_upsv_reqs = 0;
                m_upsv_idle_killed = 0;
                m_upsv_subr_queued = 0;
                m_upsv_resp = 0;
                m_upsv_resp_status_2xx = 0;
                m_upsv_resp_status_3xx = 0;
                m_upsv_resp_status_4xx = 0;
                m_upsv_resp_status_5xx = 0;
                m_upsv_errors = 0;
                m_upsv_bytes_read = 0;
                m_upsv_bytes_written = 0;

                m_clnt_conn_started = 0;
                m_clnt_conn_completed = 0;
                m_clnt_reqs = 0;
                m_clnt_idle_killed = 0;
                m_clnt_resp = 0;
                m_clnt_resp_status_2xx = 0;
                m_clnt_resp_status_3xx = 0;
                m_clnt_resp_status_4xx = 0;
                m_clnt_resp_status_5xx = 0;
                m_clnt_errors = 0;
                m_clnt_bytes_read = 0;
                m_clnt_bytes_written = 0;

                m_pool_conn_active = 0;
                m_pool_conn_idle = 0;
                m_pool_proxy_conn_active = 0;
                m_pool_proxy_conn_idle = 0;
                m_pool_clnt_free = 0;
                m_pool_clnt_used = 0;
                m_pool_resp_free = 0;
                m_pool_resp_used = 0;
                m_pool_rqst_free = 0;
                m_pool_rqst_used = 0;
                m_pool_nbq_free = 0;
                m_pool_nbq_used = 0;

                m_total_run = 0;
                m_total_add_timer = 0;
        }
} t_stat_cntr_t;
typedef std::list <t_stat_cntr_t> t_stat_cntr_list_t;

//: ----------------------------------------------------------------------------
//: calculated stats
//: ----------------------------------------------------------------------------
typedef struct t_stat_calc_struct
{
        // clnt
        uint64_t m_clnt_req_delta;
        uint64_t m_clnt_resp_delta;
        float m_clnt_req_s;
        float m_clnt_bytes_read_s;
        float m_clnt_bytes_write_s;
        float m_clnt_resp_status_2xx_pcnt;
        float m_clnt_resp_status_3xx_pcnt;
        float m_clnt_resp_status_4xx_pcnt;
        float m_clnt_resp_status_5xx_pcnt;

        // ups
        uint64_t m_upsv_req_delta;
        uint64_t m_upsv_resp_delta;
        float m_upsv_req_s;
        float m_upsv_resp_s;
        float m_upsv_bytes_read_s;
        float m_upsv_bytes_write_s;
        float m_upsv_resp_status_2xx_pcnt;
        float m_upsv_resp_status_3xx_pcnt;
        float m_upsv_resp_status_4xx_pcnt;
        float m_upsv_resp_status_5xx_pcnt;

        t_stat_calc_struct():
                m_clnt_req_delta(0),
                m_clnt_resp_delta(0),
                m_clnt_req_s(0.0),
                m_clnt_bytes_read_s(0.0),
                m_clnt_bytes_write_s(0.0),
                m_clnt_resp_status_2xx_pcnt(0.0),
                m_clnt_resp_status_3xx_pcnt(0.0),
                m_clnt_resp_status_4xx_pcnt(0.0),
                m_clnt_resp_status_5xx_pcnt(0.0),

                m_upsv_req_delta(0),
                m_upsv_resp_delta(0),
                m_upsv_req_s(0.0),
                m_upsv_resp_s(0.0),
                m_upsv_bytes_read_s(0.0),
                m_upsv_bytes_write_s(0.0),
                m_upsv_resp_status_2xx_pcnt(0.0),
                m_upsv_resp_status_3xx_pcnt(0.0),
                m_upsv_resp_status_4xx_pcnt(0.0),
                m_upsv_resp_status_5xx_pcnt(0.0)
        {}
        void clear()
        {
                m_clnt_req_delta = 0;
                m_clnt_resp_delta = 0;
                m_clnt_req_s = 0.0;
                m_upsv_resp_s = 0.0;
                m_clnt_bytes_read_s = 0.0;
                m_clnt_bytes_write_s = 0.0;
                m_clnt_resp_status_2xx_pcnt = 0.0;
                m_clnt_resp_status_3xx_pcnt = 0.0;
                m_clnt_resp_status_4xx_pcnt = 0.0;
                m_clnt_resp_status_5xx_pcnt = 0.0;

                m_upsv_req_delta = 0;
                m_upsv_resp_delta = 0;
                m_upsv_req_s = 0.0;
                m_upsv_bytes_read_s = 0.0;
                m_upsv_bytes_write_s = 0.0;
                m_upsv_resp_status_2xx_pcnt = 0.0;
                m_upsv_resp_status_3xx_pcnt = 0.0;
                m_upsv_resp_status_4xx_pcnt = 0.0;
                m_upsv_resp_status_5xx_pcnt = 0.0;
        }
} t_stat_calc_t;

//: ----------------------------------------------------------------------------
//: Prototypes
//: ----------------------------------------------------------------------------
void update_stat(xstat_t &ao_stat, double a_val);
void add_stat(xstat_t &ao_stat, const xstat_t &a_from_stat);
void clear_stat(xstat_t &ao_stat);
void show_stat(const xstat_t &ao_stat);
} //namespace ns_hlx {

#endif


