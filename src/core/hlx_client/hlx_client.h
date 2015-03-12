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

namespace ns_hlx {
//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------

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
typedef std::list <std::string> host_str_list_t;

// -----------------------------------------------
// Output formats
// -----------------------------------------------
typedef enum {
        OUTPUT_LINE_DELIMITED,
        OUTPUT_JSON
} output_type_t;

typedef enum {
        PART_HOST = 1,
        PART_STATUS_CODE = 1 << 1,
        PART_HEADERS = 1 << 2,
        PART_BODY = 1 << 3
} output_part_t;

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

        // Host list
        int set_host_list(const host_list_t &a_host_list);
        int set_host_list(const host_str_list_t &a_host_list);

        // Run options
        void set_connect_only(bool a_val);
        void set_ai_cache(const std::string &a_ai_cache);
        void set_timeout_s(uint32_t a_timeout_s);
        void set_num_threads(uint32_t a_num_threads);
        void set_start_parallel(uint32_t a_start_parallel);

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
private:

        // -------------------------------------------------
        // Internal types
        // -------------------------------------------------
        typedef std::map<std::string, std::string> header_map_t;

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        HLX_CLIENT_DISALLOW_COPY_AND_ASSIGN(hlx_client)

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        // Settings
        header_map_t m_header_map;
        std::string m_ai_cache;

        // SSL
        std::string m_cipher_list;
        long m_ssl_options;

        //reqlet_list_t m_reqlet_list;
        //reqlet_list_t::iterator m_reqlet_list_iter;
        //pthread_mutex_t m_mutex;
        //uint32_t m_num_reqlets;
        //uint32_t m_num_get;
        //uint32_t m_num_done;
        //uint32_t m_num_resolved;
        //uint32_t m_num_error;

        // -----------------------------
        // Summary info
        // -----------------------------
        //// Connectivity
        //uint32_t m_summary_success;
        //uint32_t m_summary_error_addr;
        //uint32_t m_summary_error_conn;
        //uint32_t m_summary_error_unknown;

        //// SSL info
        //uint32_t m_summary_ssl_error_self_signed;
        //uint32_t m_summary_ssl_error_expired;
        //uint32_t m_summary_ssl_error_other;

        //summary_map_t m_summary_ssl_protocols;
        //summary_map_t m_summary_ssl_ciphers;

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

