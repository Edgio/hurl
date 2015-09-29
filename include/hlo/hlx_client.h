//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hlx_client.h
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
#ifndef _HLX_CLIENT_H
#define _HLX_CLIENT_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <string>
#include <list>
#include <vector>
#include <map>
#include <stdint.h>
#include <math.h>

#include "hlo/hlx_common.h"

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define HLX_CLIENT_STATUS_OK 0
#define HLX_CLIENT_STATUS_ERROR -1

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define HLX_CLIENT_DISALLOW_ASSIGN(class_name)\
    class_name& operator=(const class_name &);
#define HLX_CLIENT_DISALLOW_COPY(class_name)\
    class_name(const class_name &);
#define HLX_CLIENT_DISALLOW_COPY_AND_ASSIGN(class_name)\
    HLX_CLIENT_DISALLOW_COPY(class_name)\
    HLX_CLIENT_DISALLOW_ASSIGN(class_name)

//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------
struct ssl_ctx_st;
typedef ssl_ctx_st SSL_CTX;

namespace ns_hlx {

class resolver;
class t_client;

//: ----------------------------------------------------------------------------
//: hlx_client
//: ----------------------------------------------------------------------------
class hlx_client
{
public:
        //: --------------------------------------------------------------------
        //: Types
        //: --------------------------------------------------------------------
        typedef std::list <t_client *> t_client_list_t;
        typedef std::map<uint16_t, uint32_t > status_code_count_map_t;

        // -----------------------------------------------
        // xstat
        // -----------------------------------------------
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

        // -----------------------------------------------
        // All Stat Aggregation..
        // -----------------------------------------------
        typedef struct t_stat_struct
        {
                // Stats
                xstat_t m_stat_us_connect;
                xstat_t m_stat_us_first_response;
                xstat_t m_stat_us_end_to_end;

                // Totals
                uint64_t m_total_bytes;
                uint64_t m_total_reqs;

                // Client stats
                uint64_t m_num_resolved;
                uint64_t m_num_conn_started;
                uint64_t m_num_conn_completed;
                uint64_t m_num_idle_killed;
                uint64_t m_num_errors;
                uint64_t m_num_bytes_read;

                status_code_count_map_t m_status_code_count_map;

                t_stat_struct():
                        m_stat_us_connect(),
                        m_stat_us_first_response(),
                        m_stat_us_end_to_end(),
                        m_total_bytes(0),
                        m_total_reqs(0),
                        m_num_resolved(0),
                        m_num_conn_started(0),
                        m_num_conn_completed(0),
                        m_num_idle_killed(0),
                        m_num_errors(0),
                        m_num_bytes_read(0),
                        m_status_code_count_map()
                {}
                void clear();
        } t_stat_t;

        // -----------------------------------------------
        // Summary info
        // -----------------------------------------------
        typedef std::map <std::string, uint32_t> summary_map_t;
        typedef struct summary_info_struct {
                uint32_t m_success;
                uint32_t m_error_addr;
                uint32_t m_error_conn;
                uint32_t m_error_unknown;
                uint32_t m_ssl_error_self_signed;
                uint32_t m_ssl_error_expired;
                uint32_t m_ssl_error_other;
                summary_map_t m_ssl_protocols;
                summary_map_t m_ssl_ciphers;

                summary_info_struct();
        } summary_info_t;
        typedef std::map <std::string, t_stat_t> tag_stat_map_t;

        // -----------------------------------------------
        // Output formats
        // -----------------------------------------------
        typedef enum {
                OUTPUT_LINE_DELIMITED,
                OUTPUT_JSON
        } output_type_t;

        typedef enum {
                PART_HOST = 1,
                PART_SERVER = 1 << 1,
                PART_STATUS_CODE = 1 << 2,
                PART_HEADERS = 1 << 3,
                PART_BODY = 1 << 4
        } output_part_t;

        typedef enum {
                REQUEST_MODE_SEQUENTIAL = 0,
                REQUEST_MODE_RANDOM,
                REQUEST_MODE_ROUND_ROBIN
        } request_mode_t;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        // Constructor/Destructor
        hlx_client(void);
        ~hlx_client(void);

        // Running
        int run(void);
        int stop(void);
        void wait_till_stopped(void);
        bool is_running(void);

        // ---------------------------------------
        // Settings
        // ---------------------------------------
        // General
        void set_quiet(bool a_val);
        void set_verbose(bool a_val);
        void set_color(bool a_val);
        void set_start_time_ms(uint64_t a_start_time_ms);

        // url
        void set_url(const std::string &a_url);
        void set_wildcarding(bool a_val);

        // data
        int set_data(const char *a_data, uint32_t a_len);

        // Host list
        int set_host_list(const host_list_t &a_host_list);
        int set_server_list(const server_list_t &a_server_list);

        // Specifying urls instead of hosts
        int32_t add_url(const std::string &a_url);
        int32_t add_url_file(const std::string &a_url_file);

        // Split requests
        void set_split_requests_by_thread(bool a_val);

        // Run options
        void set_connect_only(bool a_val);
        void set_use_ai_cache(bool a_val);
        void set_ai_cache(const std::string &a_ai_cache);
        void set_timeout_s(uint32_t a_timeout_s);
        void set_num_threads(uint32_t a_num_threads);
        void set_num_parallel(uint32_t a_num_parallel);
        void set_show_summary(bool a_val);

        void set_end_fetches(int32_t a_val);
        void set_num_reqs_per_conn(int32_t a_val);
        void set_rate(int32_t a_val);
        void set_request_mode(request_mode_t a_mode);
        void set_save_response(bool a_val);

        void set_collect_stats(bool a_val);
        void set_use_persistent_pool(bool a_val);

        // Socket options
        void set_sock_opt_no_delay(bool a_val);
        void set_sock_opt_send_buf_size(uint32_t a_send_buf_size);
        void set_sock_opt_recv_buf_size(uint32_t a_recv_buf_size);

        // Headers
        int set_header(const std::string &a_header);
        int set_header(const std::string &a_key, const std::string &a_val);
        void clear_headers(void);

        // Verb
        void set_verb(const std::string &a_verb);

        // Port
        void set_port(uint16_t a_port);

        // Scheme
        void set_scheme(scheme_type_t a_scheme);

        // SSL
        void set_ssl_cipher_list(const std::string &a_cipher_list);
        void set_ssl_ca_path(const std::string &a_ssl_ca_path);
        void set_ssl_ca_file(const std::string &a_ssl_ca_file);
        void set_ssl_sni_verify(bool a_val);
        void set_ssl_verify(bool a_val);
        int set_ssl_options(const std::string &a_ssl_options_str);
        int set_ssl_options(long a_ssl_options);

        // ---------------------------------------
        // responses/status
        // ---------------------------------------
        std::string dump_all_responses(bool a_color,
                                       bool a_pretty,
                                       output_type_t a_output_type,
                                       int a_part_map);

        void get_stats(t_stat_t &ao_all_stats, bool a_get_breakdown, tag_stat_map_t &ao_breakdown_stats);
        int32_t get_stats_json(char *l_json_buf, uint32_t l_json_buf_max_len);
        void get_summary_info(summary_info_t &ao_summary_info);

private:

        // -------------------------------------------------
        // Internal types
        // -------------------------------------------------
        typedef std::map<std::string, std::string> header_map_t;

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        HLX_CLIENT_DISALLOW_COPY_AND_ASSIGN(hlx_client)
        std::string dump_all_responses_line_dl(bool a_color,bool a_pretty,int a_part_map);
        std::string dump_all_responses_json(int a_part_map);
        int init(void);
        int init_client_list(void);
        void add_to_total_stat_agg(t_stat_t &ao_stat_agg, const t_stat_t &a_add_total_stat);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        // General
        bool m_verbose;
        bool m_color;
        bool m_quiet;

        // Run Settings
        std::string m_url;
        std::string m_url_file;

        bool m_wildcarding;

        char *m_req_body;
        uint32_t m_req_body_len;

        header_map_t m_header_map;
        std::string m_verb;

        uint16_t m_port;
        scheme_type_t m_scheme;

        bool m_use_ai_cache;
        std::string m_ai_cache;
        int32_t m_num_parallel;
        uint32_t m_num_threads;
        uint32_t m_timeout_s;
        bool m_connect_only;
        bool m_show_summary;
        bool m_save_response;
        bool m_collect_stats;
        bool m_use_persistent_pool;

        int32_t m_rate;
        int32_t m_num_end_fetches;
        int64_t m_num_reqs_per_conn;
        request_mode_t m_request_mode;
        bool m_split_requests_by_thread;

        // Stats
        uint64_t m_start_time_ms;
        uint64_t m_last_display_time_ms;

        // Socket options
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;

        // SSL
        SSL_CTX* m_ssl_ctx;
        std::string m_ssl_cipher_list;
        std::string m_ssl_options_str;
        long m_ssl_options;
        bool m_ssl_verify;
        bool m_ssl_sni;
        std::string m_ssl_ca_file;
        std::string m_ssl_ca_path;

        // t_client
        t_client_list_t m_t_client_list;
        int m_evr_loop_type;

        // Host list
        host_list_t m_host_list;

        // resolver
        resolver *m_resolver;

        // -----------------------------
        // State
        // -----------------------------
        bool m_is_initd;

        // -------------------------------------------------
        // Class members
        // -------------------------------------------------

};


//: ----------------------------------------------------------------------------
//: \details: Update stat with new value
//: \return:  n/a
//: \param:   ao_stat stat to be updated
//: \param:   a_val value to update stat with
//: ----------------------------------------------------------------------------
inline void update_stat(hlx_client::xstat_t &ao_stat, double a_val)
{
        ao_stat.m_num++;
        ao_stat.m_sum_x += a_val;
        ao_stat.m_sum_x2 += a_val*a_val;
        if(a_val > ao_stat.m_max) ao_stat.m_max = a_val;
        if(a_val < ao_stat.m_min) ao_stat.m_min = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: Add stats
//: \return:  n/a
//: \param:   ao_stat stat to be updated
//: \param:   a_from_stat stat to add
//: ----------------------------------------------------------------------------
inline void add_stat(hlx_client::xstat_t &ao_stat, const hlx_client::xstat_t &a_from_stat)
{
        ao_stat.m_num += a_from_stat.m_num;
        ao_stat.m_sum_x += a_from_stat.m_sum_x;
        ao_stat.m_sum_x2 += a_from_stat.m_sum_x2;
        if(a_from_stat.m_min < ao_stat.m_min)
                ao_stat.m_min = a_from_stat.m_min;
        if(a_from_stat.m_max > ao_stat.m_max)
                ao_stat.m_max = a_from_stat.m_max;
}

//: ----------------------------------------------------------------------------
//: \details: Clear stat
//: \return:  n/a
//: \param:   ao_stat stat to be cleared
//: ----------------------------------------------------------------------------
inline void clear_stat(hlx_client::xstat_t &ao_stat)
{
        ao_stat.m_sum_x = 0.0;
        ao_stat.m_sum_x2 = 0.0;
        ao_stat.m_min = 0.0;
        ao_stat.m_max = 0.0;
        ao_stat.m_num = 0;
}

//: ----------------------------------------------------------------------------
//: \details: Show stat
//: \return:  n/a
//: \param:   ao_stat stat display
//: ----------------------------------------------------------------------------
inline void show_stat(const hlx_client::xstat_t &ao_stat)
{
        printf("Stat: Mean: %4.2f, StdDev: %4.2f, Min: %4.2f, Max: %4.2f Num: %lu\n",
               ao_stat.mean(),
               ao_stat.stdev(),
               ao_stat.m_min,
               ao_stat.m_max,
               ao_stat.m_num);
}

//: ----------------------------------------------------------------------------
//: Example...
//: ----------------------------------------------------------------------------
#if 0
int main(void)
{
    xstat_t l_s;
    update_stat(l_s, 30.0);
    update_stat(l_s, 15.0);
    update_stat(l_s, 22.0);
    update_stat(l_s, 10.0);
    update_stat(l_s, 24.0);
    show_stat(l_s);
    return 0;
}
#endif

} //namespace ns_hlx {

#endif // #ifndef _HLX_CLIENT_H
