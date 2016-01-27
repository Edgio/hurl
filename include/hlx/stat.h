//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
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
#include <math.h>

// For fixed size types
#include <stdint.h>
#include <string>
#include <map>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::map<uint16_t, uint32_t > status_code_count_map_t;

// TODO DEBUG???
typedef std::map<std::string, void *> subr_pending_resolv_map_t;

//: ----------------------------------------------------------------------------
//: Stats
//: ----------------------------------------------------------------------------
// xstat
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
        double stdev() const { return sqrt(var()); }

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

// All Stat Aggregation..
typedef struct t_stat_struct
{
        // Stats
        xstat_t m_stat_us_connect;
        xstat_t m_stat_us_first_response;
        xstat_t m_stat_us_end_to_end;

        // Upstream stats
        uint64_t m_num_ups_resolve_req;
        uint64_t m_num_ups_resolve_active;
        uint64_t m_num_ups_resolved;
        uint64_t m_num_ups_resolve_ev;
        uint64_t m_num_ups_conn_started;
        uint32_t m_cur_ups_conn_count;
        uint64_t m_num_ups_conn_completed;
        uint64_t m_num_ups_reqs;
        uint64_t m_num_ups_idle_killed;
        uint64_t m_num_ups_subr_pending;

        // TODO DEBUG???
        //subr_pending_resolv_map_t m_subr_pending_resolv_map;

        // Server stats
        uint64_t m_num_cln_conn_started;
        uint32_t m_cur_cln_conn_count;
        uint64_t m_num_cln_conn_completed;
        uint64_t m_num_cln_reqs;
        uint64_t m_num_cln_idle_killed;

        // Totals
        uint64_t m_num_run;
        uint64_t m_total_bytes;
        uint64_t m_total_reqs;
        uint64_t m_num_errors;
        uint64_t m_num_bytes_read;
        uint64_t m_num_bytes_written;
        status_code_count_map_t m_status_code_count_map;

        t_stat_struct():
                m_stat_us_connect(),
                m_stat_us_first_response(),
                m_stat_us_end_to_end(),

                m_num_ups_resolve_req(0),
                m_num_ups_resolve_active(0),
                m_num_ups_resolved(0),
                m_num_ups_resolve_ev(0),
                m_num_ups_conn_started(0),
                m_cur_ups_conn_count(0),
                m_num_ups_conn_completed(0),
                m_num_ups_reqs(0),
                m_num_ups_idle_killed(0),
                m_num_ups_subr_pending(0),

                // TODO DEBUG???
                //m_subr_pending_resolv_map(),

                m_num_cln_conn_started(0),
                m_cur_cln_conn_count(0),
                m_num_cln_conn_completed(0),
                m_num_cln_reqs(0),
                m_num_cln_idle_killed(0),

                m_num_run(0),
                m_total_bytes(0),
                m_total_reqs(0),
                m_num_errors(0),
                m_num_bytes_read(0),
                m_num_bytes_written(0),
                m_status_code_count_map()
        {}
        void clear()
        {
                // Stats
                m_stat_us_connect.clear();
                m_stat_us_connect.clear();
                m_stat_us_first_response.clear();
                m_stat_us_end_to_end.clear();

                // Client stats
                m_num_ups_resolve_req = 0;
                m_num_ups_resolve_active = 0;
                m_num_ups_resolved = 0;
                m_num_ups_resolve_ev = 0;
                m_num_ups_conn_started = 0;
                m_cur_ups_conn_count = 0;
                m_num_ups_conn_completed = 0;
                m_num_ups_reqs = 0;
                m_num_ups_idle_killed = 0;

                // TODO DEBUG???
                //m_subr_pending_resolv_map.clear();

                m_num_cln_conn_started = 0;
                m_cur_cln_conn_count = 0;
                m_num_cln_conn_completed = 0;
                m_num_cln_reqs = 0;
                m_num_cln_idle_killed = 0;

                // Totals
                m_num_run = 0;
                m_total_bytes = 0;
                m_total_reqs = 0;
                m_num_errors = 0;
                m_num_bytes_read = 0;
                m_num_bytes_written = 0;

                m_status_code_count_map.clear();
        }
} t_stat_t;

} //namespace ns_hlx {

#endif


