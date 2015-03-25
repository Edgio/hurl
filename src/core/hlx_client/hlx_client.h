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

class reqlet;
typedef std::vector <reqlet *> reqlet_vector_t;

struct total_stat_agg_struct;
typedef total_stat_agg_struct total_stat_agg_t;
typedef std::map <std::string, total_stat_agg_t> tag_stat_map_t;

namespace ns_hlx {
//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
class t_client;
typedef std::list <t_client *> t_client_list_t;

// -----------------------------------------------
// Host info
// -----------------------------------------------
typedef struct host_struct {
        std::string m_host;
        std::string m_hostname;
        std::string m_id;
        std::string m_where;

        host_struct():
                m_host(),
                m_hostname(),
                m_id(),
                m_where()
        {};
} host_t;

typedef std::list <host_t> host_list_t;
typedef std::list <std::string> server_list_t;
typedef std::map <std::string, uint32_t> summary_map_t;

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

//: ----------------------------------------------------------------------------
//: hlx_client
//: ----------------------------------------------------------------------------
class hlx_client
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        // Constructor/Destructor
        hlx_client(void);
        ~hlx_client(void);

        // Running
        int init(void);
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

        // url
        void set_url(const std::string &a_url);
        void set_wildcarding(bool a_val);

        // Host list
        int set_host_list(host_list_t &a_host_list);
        int set_server_list(server_list_t &a_server_list);

        // Specifying urls instead of hosts
        int32_t add_url(std::string &a_url);
        int32_t add_url_file(std::string &a_url_file);

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

        void set_run_time_s(int32_t a_val);
        void set_end_fetches(int32_t a_val);
        void set_num_reqs_per_conn(int32_t a_val);
        void set_rate(int32_t a_val);
        void set_request_mode(request_mode_t a_mode);
        void set_save_response(bool a_val);
        void set_collect_stats(bool a_val);

        // Socket options
        void set_sock_opt_no_delay(bool a_val);
        void set_sock_opt_send_buf_size(uint32_t a_send_buf_size);
        void set_sock_opt_recv_buf_size(uint32_t a_recv_buf_size);

        // Headers
        int set_header(const std::string &a_header);
        int set_header(const std::string &a_key, const std::string &a_val);
        void clear_headers(void);

        // SSL
        void set_ssl_cipher_list(const std::string &a_cipher_list);
        void set_ssl_ca_path(const std::string &a_ssl_ca_path);
        void set_ssl_ca_file(const std::string &a_ssl_ca_file);
        void set_ssl_sni_verify(bool a_val);
        void set_ssl_verify(bool a_val);
        int set_ssl_options(const std::string &a_ssl_options_str);
        int set_ssl_options(long a_ssl_options);

        // ---------------------------------------
        // Display/status
        // ---------------------------------------
        void display_status_line(bool a_color);
        void display_summary(bool a_color);
        std::string dump_all_responses(bool a_color,
                                       bool a_pretty,
                                       output_type_t a_output_type,
                                       int a_part_map);

        // ---------------------------------------
        // Legacy Display/status
        // ---------------------------------------
        //int32_t add_stat(const std::string &a_tag, const req_stat_t &a_req_stat);
        void display_results(double a_elapsed_time,
                             bool a_show_breakdown_flag = false);
        void display_results_http_load_style(double a_elapsed_time,
                                             bool a_show_breakdown_flag = false,
                                             bool a_one_line_flag = false);
        void display_results_line_desc(void);
        void display_results_line(void);

        void get_stats(total_stat_agg_t &ao_all_stats, bool a_get_breakdown, tag_stat_map_t &ao_breakdown_stats);
        int32_t get_stats_json(char *l_json_buf, uint32_t l_json_buf_max_len);
        void set_start_time_ms(uint64_t a_start_time_ms) {m_start_time_ms = a_start_time_ms;}


private:

        // -------------------------------------------------
        // Internal types
        // -------------------------------------------------
        typedef std::map<std::string, std::string> header_map_t;

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        HLX_CLIENT_DISALLOW_COPY_AND_ASSIGN(hlx_client)

        std::string dump_all_responses_line_dl(bool a_color,
                                               bool a_pretty,
                                               int a_part_map);

        std::string dump_all_responses_json(int a_part_map);

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

        header_map_t m_header_map;
        bool m_use_ai_cache;
        std::string m_ai_cache;
        int32_t m_num_parallel;
        uint32_t m_num_threads;
        uint32_t m_timeout_s;
        bool m_connect_only;
        bool m_show_summary;
        bool m_save_response;
        bool m_collect_stats;

        int32_t m_rate;
        int32_t m_num_end_fetches;
        int64_t m_num_reqs_per_conn;
        int32_t m_run_time_s;
        request_mode_t m_request_mode;
        bool m_split_requests_by_thread;

        // Stats
        uint64_t m_start_time_ms;
        uint64_t m_last_display_time_ms;
        total_stat_agg_t *m_last_stat;

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

        // Reqlets
        reqlet_vector_t m_reqlet_vector;

        // -----------------------------
        // State
        // -----------------------------
        bool m_is_initd;

        // -------------------------------------------------
        // Class members
        // -------------------------------------------------

};


} //namespace ns_hlx {

#endif // #ifndef _HLX_CLIENT_H

