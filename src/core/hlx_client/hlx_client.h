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

#if 0
#include "util.h"
#include "ndebug.h"
#include "resolver.h"
#include "reqlet.h"
#include "ssl_util.h"
#include "nconn_ssl.h"
#include "nconn_tcp.h"

#include <string.h>

// getrlimit
#include <sys/time.h>
#include <sys/resource.h>

// signal
#include <signal.h>

// Shared pointer
//#include <tr1/memory>

#include <list>
#include <algorithm>
#include <unordered_set>

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h> // For getopt_long
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

// Json parser
#include <jsoncpp/json/json.h>

// Profiler
#define ENABLE_PROFILER 1
#ifdef ENABLE_PROFILER
#include <google/profiler.h>
#endif
#endif

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define HLX_CLIENT_STATUS_OK 0
#define HLX_CLIENT_STATUS_ERROR -1

#if 0
#define NB_ENABLE  1
#define NB_DISABLE 0

#define MAX_READLINE_SIZE 1024


// Version
#define HLE_VERSION_MAJOR 0
#define HLE_VERSION_MINOR 0
#define HLE_VERSION_MACRO 1
#define HLE_VERSION_PATCH "alpha"

#define HLE_DEFAULT_CONN_TIMEOUT_S 10
#endif

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

#if 0
#define SET_HEADER(_key, _val) l_settings.m_header_map[_key] =_val

#define T_CLIENT_CONN_CLEANUP(a_t_client, a_conn, a_reqlet, a_status, a_response) \
        do { \
                a_reqlet->set_response(a_status, a_response); \
                if(a_t_client->m_settings.m_show_summary) reqlet_repo::get()->append_summary(a_reqlet);\
                if(a_status >= 500) reqlet_repo::get()->up_done(true); \
                else reqlet_repo::get()->up_done(false); \
                a_t_client->cleanup_connection(a_conn); \
        }while(0)

#define HLE_SET_NCONN_OPT(_conn, _opt, _buf, _len) \
        do { \
                int _status = 0; \
                _status = _conn.set_opt((_opt), (_buf), (_len)); \
                if (_status != STATUS_OK) { \
                        NDBG_PRINT("STATUS_ERROR: Failed to set_opt %d.  Status: %d.\n", _opt, _status); \
                        return NULL;\
                } \
        } while(0)

#define HLE_GET_NCONN_OPT(_conn, _opt, _buf, _len) \
        do { \
                int _status = 0; \
                _status = _conn.get_opt((_opt), (_buf), (_len)); \
                if (_status != STATUS_OK) { \
                        NDBG_PRINT("STATUS_ERROR: Failed to get_opt %d.  Status: %d.\n", _opt, _status); \
                        return NULL;\
                } \
        } while(0)
#endif


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

#if 0
class t_client;
struct ssl_ctx_st;
typedef ssl_ctx_st SSL_CTX;
#endif


#if 0
typedef std::list <t_client *> t_client_list_t;
typedef std::map <std::string, std::string> header_map_t;
typedef std::vector<nconn *> nconn_vector_t;
typedef std::list<uint32_t> conn_id_list_t;
typedef std::unordered_set<uint32_t> conn_id_set_t;
typedef std::map <std::string, std::string> header_map_t;
typedef std::list <reqlet *> reqlet_list_t;
typedef std::map <std::string, uint32_t> summary_map_t;
#endif


#if 0
//: ----------------------------------------------------------------------------
//: Settings
//: ----------------------------------------------------------------------------
typedef struct settings_struct
{
        bool m_verbose;
        bool m_color;
        bool m_quiet;
        bool m_show_stats;
        bool m_show_summary;

        // request options
        std::string m_url;
        header_map_t m_header_map;

        // run options
        t_client_list_t m_t_client_list;
        evr_loop_type_t m_evr_loop_type;
        int32_t m_start_parallel;
        uint32_t m_num_threads;
        uint32_t m_timeout_s;
        bool m_connect_only;

        // tcp options
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;

        // SSL options
        SSL_CTX* m_ssl_ctx;
        std::string m_cipher_list_str;
        std::string m_ssl_options_str;
        long m_ssl_options;
        bool m_ssl_verify;
        bool m_ssl_sni;
        std::string m_ssl_ca_file;
        std::string m_ssl_ca_path;

        // ---------------------------------
        // Defaults...
        // ---------------------------------
        settings_struct(void) :
                m_verbose(false),
                m_color(false),
                m_quiet(false),
                m_show_stats(false),
                m_show_summary(false),
                m_url(),
                m_header_map(),
                m_t_client_list(),
                m_evr_loop_type(EVR_LOOP_EPOLL),
                m_start_parallel(128),
                m_num_threads(8),
                m_timeout_s(HLE_DEFAULT_CONN_TIMEOUT_S),
                m_connect_only(false),
                m_sock_opt_recv_buf_size(0),
                m_sock_opt_send_buf_size(0),
                m_sock_opt_no_delay(false),
                m_ssl_ctx(NULL),
                m_cipher_list_str(""),
                m_ssl_options_str(""),
                m_ssl_options(0),
                m_ssl_verify(false),
                m_ssl_sni(false),
                m_ssl_ca_file(""),
                m_ssl_ca_path("")
        {}

private:
        DISALLOW_COPY_AND_ASSIGN(settings_struct);

} settings_struct_t;
#endif


//: ----------------------------------------------------------------------------
//: hlx_client
//: ----------------------------------------------------------------------------
class hlx_client
{

public:


        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        // Running
        int stop(void);
        void wait_till_stopped(void);
        bool is_running(void);

        // Settings
        int set_header(const std::string &a_header);
        int set_header(const std::string &a_key, const std::string &a_val);
        void clear_headers(void);

        // Display/status
        void display_status_line(bool a_color);
        void display_summary(bool a_color);
        std::string dump_all_responses(bool a_color, bool a_pretty, output_type_t a_output_type, int a_part_map);


        //reqlet *get_reqlet(void);
        //int32_t add_reqlet(reqlet *a_reqlet);

        // Get the singleton instance
        static hlx_client *get(void);


        //uint32_t get_num_reqlets(void) {return m_num_reqlets;};
        //uint32_t get_num_get(void) {return m_num_get;};
        //bool empty(void) {return m_reqlet_list.empty();};
        //bool done(void) {return (m_num_get >= m_num_reqlets);};
        //void up_done(bool a_error) { ++m_num_done; if(a_error)++m_num_error;};
        //void up_resolved(bool a_error) {if(a_error)++m_num_error; else ++m_num_resolved;};
        //std::string dump_all_responses(bool a_color, bool a_pretty, output_type_t a_output_type, int a_part_map);
        //reqlet *try_get_resolved(void);
        //int32_t append_summary(reqlet *a_reqlet);
        //void display_summary(bool a_color);
private:

        // -------------------------------------------------
        // Internal types
        // -------------------------------------------------
        typedef std::map<std::string, std::string> header_map_t;

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        HLX_CLIENT_DISALLOW_COPY_AND_ASSIGN(hlx_client)
        hlx_client(void);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        header_map_t m_header_map;

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

        // -------------------------------------------------
        // Class members
        // -------------------------------------------------
        // the pointer to the singleton for the instance
        static hlx_client *m_singleton_ptr;

};




#if 0
//: ----------------------------------------------------------------------------
//: t_client
//: ----------------------------------------------------------------------------
class reqlet_repo
{

public:
        // -------------------------------------------------
        // enums
        // -------------------------------------------------
        reqlet *get_reqlet(void);
        int32_t add_reqlet(reqlet *a_reqlet);

        // Get the singleton instance
        static reqlet_repo *get(void);

        uint32_t get_num_reqlets(void) {return m_num_reqlets;};
        uint32_t get_num_get(void) {return m_num_get;};
        bool empty(void) {return m_reqlet_list.empty();};
        bool done(void) {return (m_num_get >= m_num_reqlets);};
        void up_done(bool a_error) { ++m_num_done; if(a_error)++m_num_error;};
        void up_resolved(bool a_error) {if(a_error)++m_num_error; else ++m_num_resolved;};
        void display_status_line(bool a_color);
        std::string dump_all_responses(bool a_color, bool a_pretty, output_type_t a_output_type, int a_part_map);
        reqlet *try_get_resolved(void);
        int32_t append_summary(reqlet *a_reqlet);
        void display_summary(bool a_color);
private:
        DISALLOW_COPY_AND_ASSIGN(reqlet_repo)
        reqlet_repo(void):
                m_reqlet_list(),
                m_reqlet_list_iter(),
                m_mutex(),
                m_num_reqlets(0),
                m_num_get(0),
                m_num_done(0),
                m_num_resolved(0),
                m_num_error(0),
                m_summary_success(0),
                m_summary_error_addr(0),
                m_summary_error_conn(0),
                m_summary_error_unknown(0),
                m_summary_ssl_error_self_signed(0),
                m_summary_ssl_error_expired(0),
                m_summary_ssl_error_other(0),
                m_summary_ssl_protocols(),
                m_summary_ssl_ciphers()
        {
                // Init mutex
                pthread_mutex_init(&m_mutex, NULL);
        };

        reqlet_list_t m_reqlet_list;
        reqlet_list_t::iterator m_reqlet_list_iter;
        pthread_mutex_t m_mutex;
        uint32_t m_num_reqlets;
        uint32_t m_num_get;
        uint32_t m_num_done;
        uint32_t m_num_resolved;
        uint32_t m_num_error;

        // -------------------------------------------------
        // Summary info
        // -------------------------------------------------
        // Connectivity
        uint32_t m_summary_success;
        uint32_t m_summary_error_addr;
        uint32_t m_summary_error_conn;
        uint32_t m_summary_error_unknown;

        // SSL info
        uint32_t m_summary_ssl_error_self_signed;
        uint32_t m_summary_ssl_error_expired;
        uint32_t m_summary_ssl_error_other;

        summary_map_t m_summary_ssl_protocols;
        summary_map_t m_summary_ssl_ciphers;

        // -------------------------------------------------
        // Class members
        // -------------------------------------------------
        // the pointer to the singleton for the instance
        static reqlet_repo *m_singleton_ptr;

};

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define ARESP(_str) l_responses_str += _str
std::string reqlet_repo::dump_all_responses(bool a_color, bool a_pretty, output_type_t a_output_type, int a_part_map)
{
        std::string l_responses_str = "";
        std::string l_host_color = "";
        std::string l_server_color = "";
        std::string l_id_color = "";
        std::string l_status_color = "";
        std::string l_header_color = "";
        std::string l_body_color = "";
        std::string l_off_color = "";
        char l_buf[2048];
        if(a_color)
        {
                l_host_color = ANSI_COLOR_FG_BLUE;
                l_server_color = ANSI_COLOR_FG_RED;
                l_id_color = ANSI_COLOR_FG_CYAN;
                l_status_color = ANSI_COLOR_FG_MAGENTA;
                l_header_color = ANSI_COLOR_FG_GREEN;
                l_body_color = ANSI_COLOR_FG_YELLOW;
                l_off_color = ANSI_COLOR_OFF;
        }

        if(a_output_type == OUTPUT_JSON)
        {
                ARESP("[\n");
        }

        int l_cur_reqlet = 0;
        int l_reqlet_num = m_reqlet_list.size();
        for(reqlet_list_t::iterator i_reqlet = m_reqlet_list.begin();
            i_reqlet != m_reqlet_list.end();
            ++i_reqlet, ++l_cur_reqlet)
        {


                bool l_fbf = false;
                if(a_output_type == OUTPUT_JSON)
                {
                        if(a_pretty) if(a_output_type == OUTPUT_JSON) ARESP("  ");
                        ARESP("{");
                        if(a_pretty) if(a_output_type == OUTPUT_JSON) ARESP("\n");
                }

                // Host
                if(a_part_map & PART_HOST)
                {
                        if(a_pretty) if(a_output_type == OUTPUT_JSON) ARESP("    ");
                        sprintf(l_buf, "\"%shost%s\": \"%s\"",
                                        l_host_color.c_str(), l_off_color.c_str(),
                                        (*i_reqlet)->m_url.m_host.c_str());
                        ARESP(l_buf);
                        l_fbf = true;


                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        if(a_pretty) if(a_output_type == OUTPUT_JSON) ARESP("    ");
                        sprintf(l_buf, "\"%sserver%s\": \"%s:%d\"",
                                        l_server_color.c_str(), l_server_color.c_str(),
                                        (*i_reqlet)->m_url.m_host.c_str(),
                                        (*i_reqlet)->m_url.m_port
                                        );
                        ARESP(l_buf);
                        l_fbf = true;

                        if(!(*i_reqlet)->m_url.m_id.empty())
                        {
                                if(l_fbf) {ARESP(", "); l_fbf = false;}
                                if(a_pretty) if(a_output_type == OUTPUT_JSON) ARESP("    ");
                                sprintf(l_buf, "\"%sid%s\": \"%s\"",
                                                l_id_color.c_str(), l_id_color.c_str(),
                                                (*i_reqlet)->m_url.m_id.c_str()
                                                );
                                ARESP(l_buf);
                                l_fbf = true;
                        }

                        if(!(*i_reqlet)->m_url.m_where.empty())
                        {
                                if(l_fbf) {ARESP(", "); l_fbf = false;}
                                if(a_pretty) if(a_output_type == OUTPUT_JSON) ARESP("    ");
                                sprintf(l_buf, "\"%swhere%s\": \"%s\"",
                                                l_id_color.c_str(), l_id_color.c_str(),
                                                (*i_reqlet)->m_url.m_where.c_str()
                                                );
                                ARESP(l_buf);
                                l_fbf = true;
                        }


                        l_fbf = true;
                }

                // Status Code
                if(a_part_map & PART_STATUS_CODE)
                {
                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        if(a_pretty) if(a_output_type == OUTPUT_JSON) ARESP("\n    ");
                        const char *l_status_val_color = "";
                        if(a_color)
                        {
                                if((*i_reqlet)->m_response_status == 200) l_status_val_color = ANSI_COLOR_FG_GREEN;
                                else l_status_val_color = ANSI_COLOR_FG_RED;
                        }
                        sprintf(l_buf, "\"%sstatus-code%s\": %s%d%s",
                                        l_status_color.c_str(), l_off_color.c_str(),
                                        l_status_val_color, (*i_reqlet)->m_response_status, l_off_color.c_str());
                        ARESP(l_buf);
                        l_fbf = true;
                }

                // Headers
                // TODO -only in json mode for now
                if(a_output_type == OUTPUT_JSON)
                if(a_part_map & PART_HEADERS)
                {
                        for(header_map_t::iterator i_header = (*i_reqlet)->m_response_headers.begin();
                                        i_header != (*i_reqlet)->m_response_headers.end();
                            ++i_header)
                        {
                                if(l_fbf) {ARESP(", "); l_fbf = false;}
                                if(a_pretty) if(a_output_type == OUTPUT_JSON) NDBG_OUTPUT("\n    ");
                                sprintf(l_buf, "\"%s%s%s\": \"%s\"",
                                                l_header_color.c_str(), i_header->first.c_str(), l_off_color.c_str(),
                                                i_header->second.c_str());
                                ARESP(l_buf);
                                l_fbf = true;
                        }
                }

                // Headers
                // TODO -only in json mode for now
                //if(a_output_type == OUTPUT_JSON)
                //if(a_part_map & PART_HEADERS)
                if(1)
                {
                        for(header_map_t::iterator i_header = (*i_reqlet)->m_conn_info.begin();
                                        i_header != (*i_reqlet)->m_conn_info.end();
                            ++i_header)
                        {
                                if(l_fbf) {ARESP(", "); l_fbf = false;}
                                if(a_pretty) if(a_output_type == OUTPUT_JSON) NDBG_OUTPUT("\n    ");
                                sprintf(l_buf, "\"%s%s%s\": \"%s\"",
                                                l_header_color.c_str(), i_header->first.c_str(), l_off_color.c_str(),
                                                i_header->second.c_str());
                                ARESP(l_buf);
                                l_fbf = true;
                        }
                }

                // Body
                if(a_part_map & PART_BODY)
                {
                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        if(a_pretty) if(a_output_type == OUTPUT_JSON) ARESP("\n    ");
                        if(!(*i_reqlet)->m_response_body.empty())
                        {
                                sprintf(l_buf, "\"%sbody%s\": %s",
                                                l_body_color.c_str(), l_off_color.c_str(),
                                                (*i_reqlet)->m_response_body.c_str());
                        }
                        else
                        {
                                sprintf(l_buf, "\"%sbody%s\": \"NO_RESPONSE\"",
                                                l_body_color.c_str(), l_off_color.c_str());
                        }
                        ARESP(l_buf);
                        l_fbf = true;
                }

                if(a_output_type == OUTPUT_JSON)
                {
                        if(a_pretty) ARESP("\n  ");
                        ARESP("}");
                        if(l_cur_reqlet < (l_reqlet_num - 1)) ARESP(", ");
                }
                ARESP("\n");
        }

        if(a_output_type == OUTPUT_JSON)
        {
                ARESP("]\n");
        }

        return l_responses_str;

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t reqlet_repo::append_summary(reqlet *a_reqlet)
{

        // Examples:
        //
        // Address resolution
        //{"status-code": 900, "body": "Address resolution failed."},
        //
        // Connectivity
        //
        //{"status-code": 0, "body": "NO_RESPONSE"
        //
        //{"status-code": 902, "body": "Connection timed out"
        //
        //{"status-code": 901, "body": "Error Connection refused. Reason: Connection refused"
        //{"status-code": 901, "body": "Error Unkown. Reason: No route to host"
        //
        //{"status-code": 901, "body": "SSL_ERROR_SYSCALL 0: error:00000000:lib(0):func(0):reason(0). Connection reset by peer"
        //{"status-code": 901, "body": "SSL_ERROR_SYSCALL 0: error:00000000:lib(0):func(0):reason(0). An EOF was observed that violates the protocol"
        //{"status-code": 901, "body": "SSL_ERROR_SSL 0: error:140770FC:SSL routines:SSL23_GET_SERVER_HELLO:unknown protocol."
        //{"status-code": 901, "body": "SSL_ERROR_SSL 0: error:14077410:SSL routines:SSL23_GET_SERVER_HELLO:sslv3 alert handshake failure."}
        //{"status-code": 901, "body": "SSL_ERROR_SSL 0: error:14077438:SSL routines:SSL23_GET_SERVER_HELLO:tlsv1 alert internal error."
        //{"status-code": 901, "body": "SSL_ERROR_SSL 0: error:14077458:SSL routines:SSL23_GET_SERVER_HELLO:tlsv1 unrecognized name."

        pthread_mutex_lock(&m_mutex);

        if(a_reqlet->m_response_status == 900)
        {
                ++m_summary_error_addr;
        }
        else if((a_reqlet->m_response_status == 0) ||
                (a_reqlet->m_response_status == 901) ||
                (a_reqlet->m_response_status == 902))
        {
                // Missing ca
                if(a_reqlet->m_response_body.find("unable to get local issuer certificate") != std::string::npos)
                {
                        ++m_summary_ssl_error_other;
                }
                // expired
                if(a_reqlet->m_response_body.find("certificate has expired") != std::string::npos)
                {
                        ++m_summary_ssl_error_expired;
                }
                // expired
                if(a_reqlet->m_response_body.find("self signed certificate") != std::string::npos)
                {
                        ++m_summary_ssl_error_self_signed;
                }

                ++m_summary_error_conn;
        }
        else if(a_reqlet->m_response_status == 200)
        {
                ++m_summary_success;

                header_map_t::iterator i_h;
                if((i_h = a_reqlet->m_conn_info.find("Protocol")) != a_reqlet->m_conn_info.end())
                {
                        ++m_summary_ssl_protocols[i_h->second];
                }
                if((i_h = a_reqlet->m_conn_info.find("Cipher")) != a_reqlet->m_conn_info.end())
                {
                        ++m_summary_ssl_ciphers[i_h->second];
                }
        }
        else
        {
                ++m_summary_error_unknown;
        }

        pthread_mutex_unlock(&m_mutex);

        // TODO
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void reqlet_repo::display_summary(bool a_color)
{
        std::string l_header_str = "";
        std::string l_protocol_str = "";
        std::string l_cipher_str = "";
        std::string l_off_color = "";

        if(a_color)
        {
                l_header_str = ANSI_COLOR_FG_CYAN;
                l_protocol_str = ANSI_COLOR_FG_GREEN;
                l_cipher_str = ANSI_COLOR_FG_YELLOW;
                l_off_color = ANSI_COLOR_OFF;
        }

        NDBG_OUTPUT("****************** %sSUMMARY%s ****************** \n", l_header_str.c_str(), l_off_color.c_str());
        NDBG_OUTPUT("| total hosts:                     %u\n",m_num_reqlets);
        NDBG_OUTPUT("| success:                         %u\n",m_summary_success);
        NDBG_OUTPUT("| error address lookup:            %u\n",m_summary_error_addr);
        NDBG_OUTPUT("| error connectivity:              %u\n",m_summary_error_conn);
        NDBG_OUTPUT("| error unknown:                   %u\n",m_summary_error_unknown);
        NDBG_OUTPUT("| ssl error cert expired           %u\n",m_summary_ssl_error_expired);
        NDBG_OUTPUT("| ssl error cert self-signed       %u\n",m_summary_ssl_error_self_signed);
        NDBG_OUTPUT("| ssl error other                  %u\n",m_summary_ssl_error_other);

        // Sort
        typedef std::map<uint32_t, std::string> _sorted_map_t;
        _sorted_map_t l_sorted_map;
        NDBG_OUTPUT("+--------------- %sSSL PROTOCOLS%s -------------- \n", l_protocol_str.c_str(), l_off_color.c_str());
        l_sorted_map.clear();
        for(summary_map_t::iterator i_s = m_summary_ssl_protocols.begin(); i_s != m_summary_ssl_protocols.end(); ++i_s)
        l_sorted_map[i_s->second] = i_s->first;
        for(_sorted_map_t::reverse_iterator i_s = l_sorted_map.rbegin(); i_s != l_sorted_map.rend(); ++i_s)
        NDBG_OUTPUT("| %-32s %u\n", i_s->second.c_str(), i_s->first);
        NDBG_OUTPUT("+--------------- %sSSL CIPHERS%s ---------------- \n", l_cipher_str.c_str(), l_off_color.c_str());
        l_sorted_map.clear();
        for(summary_map_t::iterator i_s = m_summary_ssl_ciphers.begin(); i_s != m_summary_ssl_ciphers.end(); ++i_s)
        l_sorted_map[i_s->second] = i_s->first;
        for(_sorted_map_t::reverse_iterator i_s = l_sorted_map.rbegin(); i_s != l_sorted_map.rend(); ++i_s)
        NDBG_OUTPUT("| %-32s %u\n", i_s->second.c_str(), i_s->first);

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
reqlet *reqlet_repo::get_reqlet(void)
{
        reqlet *l_reqlet = NULL;

        pthread_mutex_lock(&m_mutex);
        if(!m_reqlet_list.empty() &&
           (m_num_get < m_num_reqlets))
        {
                l_reqlet = *m_reqlet_list_iter;
                ++m_num_get;
                ++m_reqlet_list_iter;
        }
        pthread_mutex_unlock(&m_mutex);

        return l_reqlet;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
reqlet *reqlet_repo::try_get_resolved(void)
{
        reqlet *l_reqlet = NULL;
        int32_t l_status;

        l_reqlet = get_reqlet();
        if(NULL == l_reqlet)
        {
                return NULL;
        }

        // Try resolve
        l_status = l_reqlet->resolve();
        if(l_status != STATUS_OK)
        {
                // TODO Set response and error
                up_resolved(true);
                append_summary(l_reqlet);
                return NULL;
        }

        up_resolved(false);
        return l_reqlet;

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t reqlet_repo::add_reqlet(reqlet *a_reqlet)
{
        bool l_was_empty = false;
        if(m_reqlet_list.empty())
        {
                l_was_empty = true;
        }

        //NDBG_PRINT("Adding to repo.\n");
        m_reqlet_list.push_back(a_reqlet);
        ++m_num_reqlets;

        if(l_was_empty)
        {
                m_reqlet_list_iter = m_reqlet_list.begin();
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void reqlet_repo::display_status_line(bool a_color)
{
        if(a_color)
        {
                printf("Done/Resolved/Req'd/Total/Error %s%8u%s / %s%8u%s / %s%8u%s / %s%8u%s / %s%8u%s\n",
                                ANSI_COLOR_FG_GREEN, m_num_done, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_MAGENTA, m_num_resolved, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_YELLOW, m_num_get, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_BLUE, m_num_reqlets, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_RED, m_num_error, ANSI_COLOR_OFF);
        }
        else
        {
                printf("Done/Resolved/Req'd/Total/Error %8u / %8u / %8u / %8u / %8u\n",m_num_done, m_num_resolved, m_num_get, m_num_reqlets, m_num_error);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
reqlet_repo *reqlet_repo::get(void)
{
        if (m_singleton_ptr == NULL) {
                //If not yet created, create the singleton instance
                m_singleton_ptr = new reqlet_repo();

                // Initialize

        }
        return m_singleton_ptr;
}

//: ----------------------------------------------------------------------------
//: Class variables
//: ----------------------------------------------------------------------------
// the pointer to the singleton for the instance
reqlet_repo *reqlet_repo::m_singleton_ptr;

//: ----------------------------------------------------------------------------
//: t_client
//: ----------------------------------------------------------------------------
class t_client
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        t_client(const settings_struct_t &a_settings);

        ~t_client();

        int run(void);
        void *t_run(void *a_nothing);
        void stop(void);
        bool is_running(void) { return !m_stopped; }
        int32_t set_header(const std::string &a_header_key, const std::string &a_header_val);
        void set_ssl_ctx(SSL_CTX * a_ssl_ctx) { m_settings.m_ssl_ctx = a_ssl_ctx;};
        uint32_t get_timeout_s(void) { return m_settings.m_timeout_s;};

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // Needs to be public for now -to join externally
        pthread_t m_t_run_thread;
        settings_struct_t m_settings;

        // -------------------------------------------------
        // Static (class) methods
        // -------------------------------------------------
        static void *evr_loop_file_writeable_cb(void *a_data);
        static void *evr_loop_file_readable_cb(void *a_data);
        static void *evr_loop_file_error_cb(void *a_data);
        static void *evr_loop_file_timeout_cb(void *a_data);
        static void *evr_loop_timer_cb(void *a_data);
        static void *evr_loop_timer_completion_cb(void *a_data);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(t_client)

        //Helper for pthreads
        static void *t_run_static(void *a_context)
        {
                return reinterpret_cast<t_client *>(a_context)->t_run(NULL);
        }

        int32_t start_connections(void);
        int32_t cleanup_connection(nconn *a_nconn, bool a_cancel_timer = true);
        int32_t create_request(nconn &ao_conn, reqlet &a_reqlet);
        nconn *create_new_nconn(uint32_t a_id, const reqlet &a_reqlet);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        // client config

        sig_atomic_t m_stopped;

        nconn_vector_t m_nconn_vector;
        conn_id_list_t m_conn_free_list;
        conn_id_set_t m_conn_used_set;

        int64_t m_num_fetches;
        int64_t m_num_fetched;
        int64_t m_num_pending;

        //uint64_t m_unresolved_count;

        // Get evr_loop
        evr_loop *m_evr_loop;

};

//: ----------------------------------------------------------------------------
//: Thread local global
//: ----------------------------------------------------------------------------
__thread t_client *g_t_client = NULL;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define COPY_SETTINGS(_field) m_settings._field = a_settings._field
t_client::t_client(const settings_struct_t &a_settings):
        m_t_run_thread(),
        m_settings(),
        m_stopped(false),
        m_nconn_vector(a_settings.m_start_parallel),
        m_conn_free_list(),
        m_conn_used_set(),
        m_num_fetches(-1),
        m_num_fetched(0),
        m_num_pending(0),
        m_evr_loop(NULL)
{
        for(int32_t i_conn = 0; i_conn < a_settings.m_start_parallel; ++i_conn)
        {
                m_nconn_vector[i_conn] = NULL;
                m_conn_free_list.push_back(i_conn);
        }

        // Friggin effc++
        COPY_SETTINGS(m_verbose);
        COPY_SETTINGS(m_color);
        COPY_SETTINGS(m_quiet);
        COPY_SETTINGS(m_show_stats);
        COPY_SETTINGS(m_show_summary);

        COPY_SETTINGS(m_url);
        COPY_SETTINGS(m_header_map);

        COPY_SETTINGS(m_t_client_list);
        COPY_SETTINGS(m_evr_loop_type);
        COPY_SETTINGS(m_start_parallel);
        COPY_SETTINGS(m_num_threads);
        COPY_SETTINGS(m_timeout_s);
        COPY_SETTINGS(m_connect_only);

        COPY_SETTINGS(m_sock_opt_recv_buf_size);
        COPY_SETTINGS(m_sock_opt_send_buf_size);
        COPY_SETTINGS(m_sock_opt_no_delay);

        COPY_SETTINGS(m_ssl_ctx);
        COPY_SETTINGS(m_cipher_list_str);

        COPY_SETTINGS(m_ssl_options_str);
        COPY_SETTINGS(m_ssl_options);
        COPY_SETTINGS(m_ssl_verify);
        COPY_SETTINGS(m_ssl_sni);
        COPY_SETTINGS(m_ssl_ca_file);
        COPY_SETTINGS(m_ssl_ca_path);

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_client::~t_client()
{
        for(uint32_t i_conn = 0; i_conn < m_nconn_vector.size(); ++i_conn)
        {
                if(m_nconn_vector[i_conn])
                {
                        delete m_nconn_vector[i_conn];
                        m_nconn_vector[i_conn] = NULL;
                }
        }

        if(m_evr_loop)
        {
                delete m_evr_loop;
        }
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int t_client::run(void)
{

        int32_t l_pthread_error = 0;

        l_pthread_error = pthread_create(&m_t_run_thread,
                        NULL,
                        t_run_static,
                        this);
        if (l_pthread_error != 0)
        {
                // failed to create thread

                NDBG_PRINT("Error: creating thread.  Reason: %s\n.", strerror(l_pthread_error));
                return STATUS_ERROR;

        }

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_client::stop(void)
{
        m_stopped = true;
        int32_t l_status;
        l_status = m_evr_loop->stop();
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error performing stop.\n");
        }
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_file_writeable_cb(void *a_data)
{
        if(!a_data)
        {
                //NDBG_PRINT("a_data == NULL\n");
                return NULL;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        reqlet *l_reqlet = static_cast<reqlet *>(l_nconn->get_data1());
        t_client *l_t_client = g_t_client;

        // Cancel last timer
        l_t_client->m_evr_loop->cancel_timer(&(l_nconn->m_timer_obj));

        int32_t l_status = STATUS_OK;
        l_status = l_nconn->run_state_machine(l_t_client->m_evr_loop, l_reqlet->m_host_info);
        if(STATUS_ERROR == l_status)
        {
                if(l_nconn->m_verbose)
                {
                        NDBG_PRINT("Error: performing run_state_machine\n");
                }
                T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 901, l_nconn->m_last_error.c_str());
                return NULL;
        }

        if(l_nconn->is_done())
        {
                if(l_t_client->m_settings.m_connect_only)
                {
                        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 200, "Connected Successfully");
                }
                else
                {
                        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 0, "");
                }
                return NULL;
        }


        // Add idle timeout
        l_t_client->m_evr_loop->add_timer( l_t_client->get_timeout_s()*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));

        return NULL;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_file_readable_cb(void *a_data)
{
        if(!a_data)
        {
                //NDBG_PRINT("a_data == NULL\n");
                return NULL;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        reqlet *l_reqlet = static_cast<reqlet *>(l_nconn->get_data1());
        t_client *l_t_client = g_t_client;

        // Cancel last timer
        l_t_client->m_evr_loop->cancel_timer(&(l_nconn->m_timer_obj));

        int32_t l_status = STATUS_OK;
        l_status = l_nconn->run_state_machine(l_t_client->m_evr_loop, l_reqlet->m_host_info);
        if(STATUS_ERROR == l_status)
        {
                if(l_nconn->m_verbose)
                {
                        NDBG_PRINT("Error: performing run_state_machine\n");
                }
                T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 901, l_nconn->m_last_error.c_str());
                return NULL;
        }

        if(l_status >= 0)
        {
                l_reqlet->m_stat_agg.m_num_bytes_read += l_status;
        }

        // Check for done...
        if((l_nconn->is_done()) ||
           (l_status == STATUS_ERROR))
        {
                if(l_status == STATUS_ERROR)
                {
                        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 901, l_nconn->m_last_error.c_str());
                }
                else
                {
                        if(l_t_client->m_settings.m_connect_only)
                        {
                                T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 200, "Connected Successfully");
                        }
                        else
                        {
                                T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 0, "");
                        }
                }
                return NULL;
        }

        // Add idle timeout
        l_t_client->m_evr_loop->add_timer( l_t_client->get_timeout_s()*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));

        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *g_completion_timer;
void *t_client::evr_loop_timer_completion_cb(void *a_data)
{
        return NULL;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_file_error_cb(void *a_data)
{
        //NDBG_PRINT("%sSTATUS_ERRORS%s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF);
        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_file_timeout_cb(void *a_data)
{
        if(!a_data)
        {
                //NDBG_PRINT("a_data == NULL\n");
                return NULL;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        reqlet *l_reqlet = static_cast<reqlet *>(l_nconn->get_data1());
        t_client *l_t_client = g_t_client;

        //printf("%sT_O%s: %p\n",ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF,
        //                l_rconn->m_timer_obj);

        // Add stats
        //add_stat_to_agg(l_reqlet->m_stat_agg, l_nconn->get_stats());
        if(l_t_client->m_settings.m_verbose)
        {
                NDBG_PRINT("%sTIMING OUT CONN%s: i_conn: %lu HOST: %s THIS: %p\n",
                                ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                                l_nconn->get_id(),
                                l_reqlet->m_url.m_host.c_str(),
                                l_t_client);
        }

        // Cleanup
        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 902, "Connection timed out");

        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_timer_cb(void *a_data)
{
        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::t_run(void *a_nothing)
{

        // Set thread local
        g_t_client = this;

        // Create loop
        m_evr_loop = new evr_loop(
                        evr_loop_file_readable_cb,
                        evr_loop_file_writeable_cb,
                        evr_loop_file_error_cb,
                        m_settings.m_evr_loop_type,
                        m_settings.m_start_parallel);

        reqlet_repo *l_reqlet_repo = reqlet_repo::get();

        // -------------------------------------------
        // Main loop.
        // -------------------------------------------
        //NDBG_PRINT("starting main loop\n");
        while(!m_stopped &&
                        !l_reqlet_repo->done())
        {

                // -------------------------------------------
                // Start Connections
                // -------------------------------------------
                //NDBG_PRINT("%sSTART_CONNECTIONS%s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF);
                if(!l_reqlet_repo->done())
                {
                        int32_t l_status;
                        l_status = start_connections();
                        if(l_status != STATUS_OK)
                        {
                                NDBG_PRINT("%sSTART_CONNECTIONS%s ERROR!\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                                return NULL;
                        }
                }

                // Run loop
                m_evr_loop->run();

        }

        //NDBG_PRINT("%sFINISHING_CONNECTIONS%s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF);

        // Still awaiting responses -wait...
        uint64_t l_cur_time = get_time_s();
        uint64_t l_end_time = l_cur_time + m_settings.m_timeout_s;
        while(!m_stopped && (m_num_pending > 0) && (l_cur_time < l_end_time))
        {
                // Run loop
                //NDBG_PRINT("waiting: m_num_pending: %d --time-left: %d\n", (int)m_num_pending, int(l_end_time - l_cur_time));
                m_evr_loop->run();

                // TODO -this is pretty hard polling -make option???
                usleep(10000);
                l_cur_time = get_time_s();

        }
        //NDBG_PRINT("%sDONE_CONNECTIONS%s\n", ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF);

        m_stopped = true;

        return NULL;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *t_client::create_new_nconn(uint32_t a_id, const reqlet &a_reqlet)
{
        nconn *l_nconn = NULL;


        if(a_reqlet.m_url.m_scheme == nconn::SCHEME_TCP)
        {
                // TODO SET OPTIONS!!!
                l_nconn = new nconn_tcp(m_settings.m_verbose,
                                        m_settings.m_color,
                                        1,
                                        true,
                                        false,
                                        m_settings.m_connect_only);
        }
        else if(a_reqlet.m_url.m_scheme == nconn::SCHEME_SSL)
        {
                // TODO SET OPTIONS!!!
                l_nconn = new nconn_ssl(m_settings.m_verbose,
                                        m_settings.m_color,
                                        1,
                                        true,
                                        false,
                                        m_settings.m_connect_only);
        }

        // -------------------------------------------
        // Set options
        // -------------------------------------------
        // Set generic options
        HLE_SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_RECV_BUF_SIZE, NULL, m_settings.m_sock_opt_recv_buf_size);
        HLE_SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_SEND_BUF_SIZE, NULL, m_settings.m_sock_opt_send_buf_size);
        HLE_SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_NO_DELAY, NULL, m_settings.m_sock_opt_no_delay);

        // Set ssl options
        if(a_reqlet.m_url.m_scheme == nconn::SCHEME_SSL)
        {
                HLE_SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_CIPHER_STR,
                                              m_settings.m_cipher_list_str.c_str(),
                                              m_settings.m_cipher_list_str.length());

                HLE_SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_CTX,
                                             m_settings.m_ssl_ctx,
                                             sizeof(m_settings.m_ssl_ctx));

                HLE_SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_VERIFY,
                                              &(m_settings.m_ssl_verify),
                                              sizeof(m_settings.m_ssl_verify));

                //HLE_SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_OPTIONS,
                //                              &(m_settings.m_ssl_options),
                //                              sizeof(m_settings.m_ssl_options));

        }

        return l_nconn;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::start_connections(void)
{
        int32_t l_status;
        reqlet_repo *l_reqlet_repo = reqlet_repo::get();
        reqlet *l_reqlet = NULL;

        // Find an empty client slot.
        //NDBG_PRINT("m_conn_free_list.size(): %Zu\n", m_conn_free_list.size());
        for (conn_id_list_t::iterator i_conn = m_conn_free_list.begin();
               (i_conn != m_conn_free_list.end()) &&
               (!l_reqlet_repo->done()) &&
               !m_stopped;
             )
        {

                // Loop trying to get reqlet
                l_reqlet = NULL;
                while(((l_reqlet = l_reqlet_repo->try_get_resolved()) == NULL) && (!l_reqlet_repo->done()));
                if((l_reqlet == NULL) && l_reqlet_repo->done())
                {
                        // Bail out
                        return STATUS_OK;
                }

                // Start client for this reqlet
                //NDBG_PRINT("i_conn: %d\n", *i_conn);
                nconn *l_nconn = m_nconn_vector[*i_conn];
                // TODO Check for NULL

                if(l_nconn &&
                   (l_nconn->m_scheme != l_reqlet->m_url.m_scheme))
                {
                        // Destroy nconn and recreate
                        delete l_nconn;
                        l_nconn = NULL;
                }


                if(!l_nconn)
                {
                        // Create nconn
                        l_nconn = create_new_nconn(*i_conn, *l_reqlet);
                        if(!l_nconn)
                        {
                                NDBG_PRINT("Error performing create_new_nconn\n");
                                return STATUS_ERROR;
                        }
                }

                // Assign the reqlet for this client
                l_nconn->set_data1(l_reqlet);

                // Bump stats
                ++(l_reqlet->m_stat_agg.m_num_conn_started);

                // Create request
                if(!m_settings.m_connect_only)
                {
                        create_request(*l_nconn, *l_reqlet);
                }

                m_conn_used_set.insert(*i_conn);
                m_conn_free_list.erase(i_conn++);

                // TODO Make configurable
                m_evr_loop->add_timer(m_settings.m_timeout_s*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));

                // Add to num pending
                ++m_num_pending;

                //NDBG_PRINT("%sCONNECT%s: %s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF, l_reqlet->m_url.m_host.c_str());
                l_nconn->set_host(l_reqlet->m_url.m_host);
                l_status = l_nconn->run_state_machine(m_evr_loop, l_reqlet->m_host_info);
                if(STATUS_OK != l_status)
                {
                        NDBG_PRINT("Error: Performing do_connect\n");
                        T_CLIENT_CONN_CLEANUP(this, l_nconn, l_reqlet, 500, "Performing do_connect");
                        continue;
                }
        }

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::create_request(nconn &ao_conn,
                                 reqlet &a_reqlet)
{
        // Get client
        char *l_req_buf = NULL;
        uint32_t l_req_buf_len = 0;
        uint32_t l_max_buf_len = nconn_tcp::m_max_req_buf;

        GET_NCONN_OPT(ao_conn, nconn_tcp::OPT_TCP_REQ_BUF, (void **)(&l_req_buf), &l_req_buf_len);

        // -------------------------------------------
        // Request.
        // -------------------------------------------
        const std::string &l_path_ref = a_reqlet.get_path(NULL);
        //NDBG_PRINT("HOST: %s PATH: %s\n", a_reqlet.m_url.m_host.c_str(), l_path_ref.c_str());
        if(l_path_ref.length())
        {
                l_req_buf_len = snprintf(l_req_buf, l_max_buf_len,
                                "GET %.500s HTTP/1.1\r\n", l_path_ref.c_str());
        } else {
                l_req_buf_len = snprintf(l_req_buf, l_max_buf_len,
                                "GET / HTTP/1.1\r\n");
        }

        // -------------------------------------------
        // Add repo headers
        // -------------------------------------------
        bool l_specd_host = false;

        // Loop over reqlet map
        for(header_map_t::const_iterator i_header = m_settings.m_header_map.begin();
                        i_header != m_settings.m_header_map.end();
                        ++i_header)
        {
                if(!i_header->first.empty() && !i_header->second.empty())
                {
                        //printf("Adding HEADER: %s: %s\n", i_header->first.c_str(), i_header->second.c_str());
                        l_req_buf_len += snprintf(l_req_buf + l_req_buf_len, l_max_buf_len - l_req_buf_len,
                                        "%s: %s\r\n", i_header->first.c_str(), i_header->second.c_str());

                        if (strcasecmp(i_header->first.c_str(), "host") == 0)
                        {
                                l_specd_host = true;
                        }
                }
        }

        // -------------------------------------------
        // Default Host if unspecified
        // -------------------------------------------
        if (!l_specd_host)
        {
                l_req_buf_len += snprintf(l_req_buf + l_req_buf_len, l_max_buf_len - l_req_buf_len,
                                "Host: %s\r\n", a_reqlet.m_url.m_host.c_str());
        }

        // -------------------------------------------
        // End of request terminator...
        // -------------------------------------------
        l_req_buf_len += snprintf(l_req_buf + l_req_buf_len, l_max_buf_len - l_req_buf_len, "\r\n");

        // Set len
        SET_NCONN_OPT(ao_conn, nconn_tcp::OPT_TCP_REQ_BUF_LEN, NULL, l_req_buf_len);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::cleanup_connection(nconn *a_nconn, bool a_cancel_timer)
{

        uint64_t l_conn_id = a_nconn->get_id();

        // Cancel last timer
        if(a_cancel_timer)
        {
                m_evr_loop->cancel_timer(&(a_nconn->m_timer_obj));
        }
        a_nconn->reset_stats();
        a_nconn->cleanup(m_evr_loop);

        // Add back to free list
        m_conn_free_list.push_back(l_conn_id);
        m_conn_used_set.erase(l_conn_id);

        // Reduce num pending
        ++m_num_fetched;
        --m_num_pending;

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::set_header(const std::string &a_header_key, const std::string &a_header_val)
{
        int32_t l_retval = STATUS_OK;
        m_settings.m_header_map[a_header_key] = a_header_val;
        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: Signal handler
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool g_test_finished = false;
bool g_cancelled = false;
settings_struct *g_settings = NULL;
int32_t stop(settings_struct_t &a_settings);
void sig_handler(int signo)
{
        if (signo == SIGINT)
        {
                // Kill program
                //NDBG_PRINT("SIGINT\n");
                g_test_finished = true;
                g_cancelled = true;
                stop(*g_settings);
        }
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int kbhit()
{
        struct timeval tv;
        fd_set fds;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        //STDIN_FILENO is 0
        select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        return FD_ISSET(STDIN_FILENO, &fds);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nonblock(int state)
{
        struct termios ttystate;

        //get the terminal state
        tcgetattr(STDIN_FILENO, &ttystate);

        if (state == NB_ENABLE)
        {
                //turn off canonical mode
                ttystate.c_lflag &= ~ICANON;
                //minimum of number input read.
                ttystate.c_cc[VMIN] = 1;
        } else if (state == NB_DISABLE)
        {
                //turn on canonical mode
                ttystate.c_lflag |= ICANON;
        }
        //set the terminal attributes.
        tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool is_running(settings_struct_t &a_settings)
{
        for (t_client_list_t::iterator i_client_hle = a_settings.m_t_client_list.begin();
             i_client_hle != a_settings.m_t_client_list.end();
             ++i_client_hle)
        {
                if((*i_client_hle)->is_running())
                        return true;
        }

        return false;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void command_exec(settings_struct_t &a_settings)
{
        int i = 0;
        char l_cmd = ' ';
        bool l_sent_stop = false;
        //bool l_first_time = true;

        nonblock(NB_ENABLE);

        reqlet_repo *l_reqlet_repo = reqlet_repo::get();

        //: ------------------------------------
        //:   Loop forever until user quits
        //: ------------------------------------
        while (!g_test_finished)
        {
                i = kbhit();
                if (i != 0)
                {

                        l_cmd = fgetc(stdin);

                        switch (l_cmd)
                        {

                        // Quit -only works when not reading from stdin
                        case 'q':
                        {
                                g_test_finished = true;
                                g_cancelled = true;
                                stop(a_settings);
                                l_sent_stop = true;
                                break;
                        }

                        // Default
                        default:
                        {
                                break;
                        }
                        }
                }

                // TODO add define...
                usleep(200000);

                if(a_settings.m_show_stats)
                {
                        l_reqlet_repo->display_status_line(a_settings.m_color);
                }

                if (!is_running(a_settings))
                {
                        //NDBG_PRINT("IS NOT RUNNING.\n");
                        g_test_finished = true;
                }

        }

        // Send stop -if unsent
        if(!l_sent_stop)
        {
                stop(a_settings);
                l_sent_stop = true;
        }

        nonblock(NB_DISABLE);

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t add_line(FILE *a_file_ptr, host_list_t &a_host_list)
{

        char l_readline[MAX_READLINE_SIZE];
        while(fgets(l_readline, sizeof(l_readline), a_file_ptr))
        {
                size_t l_readline_len = strnlen(l_readline, MAX_READLINE_SIZE);
                if(MAX_READLINE_SIZE == l_readline_len)
                {
                        // line was truncated
                        // Bail out -reject lines longer than limit
                        // -host names ought not be too long
                        printf("Error: hostnames must be shorter than %d chars\n", MAX_READLINE_SIZE);
                        return STATUS_ERROR;
                }
                // read full line
                // Nuke endline
                l_readline[l_readline_len - 1] = '\0';
                std::string l_string(l_readline);
                l_string.erase( std::remove_if( l_string.begin(), l_string.end(), ::isspace ), l_string.end() );
                host_t l_host;
                l_host.m_host = l_string;
                if(!l_string.empty())
                        a_host_list.push_back(l_host);
                //NDBG_PRINT("READLINE: %s\n", l_readline);
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t run(settings_struct_t &a_settings, host_list_t &a_host_list)
{
        int32_t l_retval = STATUS_OK;
        reqlet_repo *l_reqlet_repo = NULL;

        // Create the reqlet list
        l_reqlet_repo = reqlet_repo::get();
        uint32_t l_reqlet_num = 0;
        for(host_list_t::iterator i_host = a_host_list.begin();
                        i_host != a_host_list.end();
                        ++i_host, ++l_reqlet_num)
        {
                // Create a re
                reqlet *l_reqlet = new reqlet(l_reqlet_num, 1);
                l_reqlet->init_with_url(a_settings.m_url);

                // Get host and port if exist
                parsed_url l_url;
                l_url.parse(i_host->m_host);

                if(strchr(i_host->m_host.c_str(), (int)':'))
                {
                        l_reqlet->set_host(l_url.m_host);
                        l_reqlet->set_port(l_url.m_port);
                }
                else
                {
                        l_reqlet->set_host(i_host->m_host);
                }

                if(!i_host->m_hostname.empty())
                {
                     l_reqlet->m_url.m_hostname = i_host->m_hostname;
                }
                if(!i_host->m_id.empty())
                {
                     l_reqlet->m_url.m_id = i_host->m_id;
                }
                if(!i_host->m_where.empty())
                {
                     l_reqlet->m_url.m_where = i_host->m_where;
                }

                // Add to list
                l_reqlet_repo->add_reqlet(l_reqlet);

        }

        // -------------------------------------------
        // Create t_client list...
        // -------------------------------------------
        for(uint32_t i_client_idx = 0; i_client_idx < a_settings.m_num_threads; ++i_client_idx)
        {

                //if(a_settings.m_verbose)
                //{
                //        NDBG_PRINT("Creating...\n");
                //}

                // Construct with settings...
                t_client *l_t_client = new t_client(a_settings);
                for(header_map_t::iterator i_header = a_settings.m_header_map.begin();
                    i_header != a_settings.m_header_map.end();
                    ++i_header)
                {
                        l_t_client->set_header(i_header->first, i_header->second);
                }

                a_settings.m_t_client_list.push_back(l_t_client);
        }

        // -------------------------------------------
        // Run...
        // -------------------------------------------
        for(t_client_list_t::iterator i_t_client = a_settings.m_t_client_list.begin();
                        i_t_client != a_settings.m_t_client_list.end();
                        ++i_t_client)
        {
                //if(a_settings.m_verbose)
                //{
                //        NDBG_PRINT("Running...\n");
                //}
                (*i_t_client)->run();
        }

        return l_retval;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t stop(settings_struct_t &a_settings)
{
        int32_t l_retval = STATUS_OK;

        for (t_client_list_t::iterator i_t_client = a_settings.m_t_client_list.begin();
                        i_t_client != a_settings.m_t_client_list.end();
                        ++i_t_client)
        {
                (*i_t_client)->stop();
        }

        return l_retval;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t wait_till_stopped(settings_struct_t &a_settings)
{
        int32_t l_retval = STATUS_OK;

        // -------------------------------------------
        // Join all threads before exit
        // -------------------------------------------
        for(t_client_list_t::iterator i_client = a_settings.m_t_client_list.begin();
            i_client != a_settings.m_t_client_list.end();
            ++i_client)
        {

                //if(m_verbose)
                //{
                //      NDBG_PRINT("joining...\n");
                //}
                pthread_join(((*i_client)->m_t_run_thread), NULL);

        }
        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: Print the version.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_version(FILE* a_stream, int a_exit_code)
{

        // print out the version information
        fprintf(a_stream, "hle HTTP Load Runner.\n");
        fprintf(a_stream, "Copyright (C) 2014 Edgecast Networks.\n");
        fprintf(a_stream, "               Version: %d.%d.%d.%s\n",
                        HLE_VERSION_MAJOR,
                        HLE_VERSION_MINOR,
                        HLE_VERSION_MACRO,
                        HLE_VERSION_PATCH);
        exit(a_exit_code);

}

//: ----------------------------------------------------------------------------
//: \details: Print the command line help.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_usage(FILE* a_stream, int a_exit_code)
{
        fprintf(a_stream, "Usage: hle -u [http[s]://]hostname[:port]/path [options]\n");
        fprintf(a_stream, "Options are:\n");
        fprintf(a_stream, "  -h, --help           Display this help and exit.\n");
        fprintf(a_stream, "  -v, --version        Display the version number and exit.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "URL Options -or without parameter\n");
        fprintf(a_stream, "  -u, --url            URL -REQUIRED.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Hostname Input Options -also STDIN:\n");
        fprintf(a_stream, "  -f, --host_file      Host name file.\n");
        fprintf(a_stream, "  -x, --execute        Script to execute to get host names.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Settings:\n");
        fprintf(a_stream, "  -p, --parallel       Num parallel.\n");
        fprintf(a_stream, "  -t, --threads        Number of parallel threads.\n");
        fprintf(a_stream, "  -H, --header         Request headers -can add multiple ie -H<> -H<>...\n");
        fprintf(a_stream, "  -T, --timeout        Timeout (seconds).\n");
        fprintf(a_stream, "  -R, --recv_buffer    Socket receive buffer size.\n");
        fprintf(a_stream, "  -S, --send_buffer    Socket send buffer size.\n");
        fprintf(a_stream, "  -D, --no_delay       Socket TCP no-delay.\n");
        fprintf(a_stream, "  -A, --ai_cache       Path to Address Info Cache (DNS lookup cache).\n");
        fprintf(a_stream, "  -C, --connect_only   Only connect -do not send request.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "SSL Settings:\n");
        fprintf(a_stream, "  -y, --cipher         Cipher --see \"openssl ciphers\" for list.\n");
        fprintf(a_stream, "  -O, --ssl_options    SSL Options string.\n");
        fprintf(a_stream, "  -V, --ssl_verify     Verify server certificate.\n");
        fprintf(a_stream, "  -N, --ssl_sni        Use SSL SNI.\n");
        fprintf(a_stream, "  -F, --ssl_ca_file    SSL CA File.\n");
        fprintf(a_stream, "  -L, --ssl_ca_path    SSL CA Path.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Print Options:\n");
        fprintf(a_stream, "  -r, --verbose        Verbose logging\n");
        fprintf(a_stream, "  -c, --color          Color\n");
        fprintf(a_stream, "  -q, --quiet          Suppress output\n");
        fprintf(a_stream, "  -s, --show_progress  Show progress\n");
        fprintf(a_stream, "  -m, --show_summary   Show summary output\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Output Options: -defaults to line delimited\n");
        fprintf(a_stream, "  -o, --output         File to write output to. Defaults to stdout\n");
        fprintf(a_stream, "  -l, --line_delimited Output <HOST> <RESPONSE BODY> per line\n");
        fprintf(a_stream, "  -j, --json           JSON { <HOST>: \"body\": <RESPONSE> ...\n");
        fprintf(a_stream, "  -P, --pretty         Pretty output\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Debug Options:\n");
        fprintf(a_stream, "  -G, --gprofile       Google profiler output file\n");

        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Note: If running large jobs consider enabling tcp_tw_reuse -eg:\n");
        fprintf(a_stream, "echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse\n");

        fprintf(a_stream, "\n");

        exit(a_exit_code);

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int main(int argc, char** argv)
{

        settings_struct_t l_settings;

        // For sighandler
        g_settings = &l_settings;

        // -------------------------------------------
        // Setup default headers before the user
        // -------------------------------------------
        SET_HEADER("User-Agent", "EdgeCast Parallel Curl hle ");
        //SET_HEADER("User-Agent", "ONGA_BONGA (╯°□°）╯︵ ┻━┻)");
        //SET_HEADER("User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/33.0.1750.117 Safari/537.36");
        //SET_HEADER("x-select-backend", "self");
        SET_HEADER("Accept", "*/*");
        //SET_HEADER("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8");
        //SET_HEADER("Accept-Encoding", "gzip,deflate");
        //SET_HEADER("Connection", "keep-alive");

        // -------------------------------------------
        // Get args...
        // -------------------------------------------
        char l_opt;
        std::string l_argument;
        int l_option_index = 0;
        struct option l_long_options[] =
                {
                { "help",           0, 0, 'h' },
                { "version",        0, 0, 'v' },
                { "url",            1, 0, 'u' },
                { "host_file",      1, 0, 'f' },
                { "host_file_json", 1, 0, 'J' },
                { "execute",        1, 0, 'x' },
                { "parallel",       1, 0, 'p' },
                { "threads",        1, 0, 't' },
                { "header",         1, 0, 'H' },
                { "timeout",        1, 0, 'T' },
                { "recv_buffer",    1, 0, 'R' },
                { "send_buffer",    1, 0, 'S' },
                { "no_delay",       1, 0, 'D' },
                { "ai_cache",       1, 0, 'A' },
                { "connect_only",   0, 0, 'C' },
                { "cipher",         1, 0, 'y' },
                { "ssl_options",    1, 0, 'O' },
                { "ssl_verify",     0, 0, 'V' },
                { "ssl_sni",        0, 0, 'N' },
                { "ssl_ca_file",    1, 0, 'F' },
                { "ssl_ca_path",    1, 0, 'L' },
                { "verbose",        0, 0, 'r' },
                { "color",          0, 0, 'c' },
                { "quiet",          0, 0, 'q' },
                { "show_progress",  0, 0, 's' },
                { "show_summary",   0, 0, 'm' },
                { "output",         1, 0, 'o' },
                { "line_delimited", 0, 0, 'l' },
                { "json",           0, 0, 'j' },
                { "pretty",         0, 0, 'P' },
                { "gprofile",       1, 0, 'G' },

                // list sentinel
                { 0, 0, 0, 0 }
        };

        std::string l_gprof_file;
        std::string l_execute_line;
        std::string l_host_file_str;
        std::string l_host_file_json_str;
        std::string l_url;
        std::string l_ai_cache;
        std::string l_output_file = "";

        // Defaults
        reqlet_repo::output_type_t l_output_mode = reqlet_repo::OUTPUT_JSON;
        //bool l_output_part_user_specd = false;
        int l_output_part =   reqlet_repo::PART_HOST
                            | reqlet_repo::PART_STATUS_CODE
                            | reqlet_repo::PART_HEADERS
                            | reqlet_repo::PART_BODY
                            ;
        bool l_output_pretty = false;

        // -------------------------------------------
        // Assume unspecified arg url...
        // TODO Unsure if good way to allow unspecified
        // arg...
        // -------------------------------------------
        bool is_opt = false;
        for(int i_arg = 1; i_arg < argc; ++i_arg) {

                if(argv[i_arg][0] == '-') {
                        // next arg is for the option
                        is_opt = true;
                }
                else if(argv[i_arg][0] != '-' && is_opt == false) {
                        // Stuff in url field...
                        l_url = std::string(argv[i_arg]);
                        //if(l_settings.m_verbose)
                        //{
                        //      NDBG_PRINT("Found unspecified argument: %s --assuming url...\n", l_url.c_str());
                        //}
                        break;
                } else {
                        // reset option flag
                        is_opt = false;
                }

        }

        // -------------------------------------------------
        // Args...
        // -------------------------------------------------
        char l_short_arg_list[] = "hvu:f:J:x:y:O:VNF:L:p:t:H:T:R:S:DA:Crcqsmo:ljPG:";
        while ((l_opt = getopt_long_only(argc, argv, l_short_arg_list, l_long_options, &l_option_index)) != -1)
        {

                if (optarg)
                        l_argument = std::string(optarg);
                else
                        l_argument.clear();
                //NDBG_PRINT("arg[%c=%d]: %s\n", l_opt, l_option_index, l_argument.c_str());

                switch (l_opt)
                {
                // ---------------------------------------
                // Help
                // ---------------------------------------
                case 'h':
                {
                        print_usage(stdout, 0);
                        break;
                }
                // ---------------------------------------
                // Version
                // ---------------------------------------
                case 'v':
                {
                        print_version(stdout, 0);
                        break;
                }
                // ---------------------------------------
                // URL
                // ---------------------------------------
                case 'u':
                {
                        l_url = l_argument;
                        break;
                }
                // ---------------------------------------
                // Host file
                // ---------------------------------------
                case 'f':
                {
                        l_host_file_str = l_argument;
                        break;
                }
                // ---------------------------------------
                // Host file JSON
                // ---------------------------------------
                case 'J':
                {
                        l_host_file_json_str = l_argument;
                        break;
                }
                // ---------------------------------------
                // Execute line
                // ---------------------------------------
                case 'x':
                {
                        l_execute_line = l_argument;
                        break;
                }
                // ---------------------------------------
                // cipher
                // ---------------------------------------
                case 'y':
                {
                        std::string l_cipher_str = l_argument;
                        if (strcasecmp(l_cipher_str.c_str(), "fastsec") == 0)
                                l_cipher_str = "RC4-MD5";
                        else if (strcasecmp(l_cipher_str.c_str(), "highsec") == 0)
                                l_cipher_str = "DES-CBC3-SHA";
                        else if (strcasecmp(l_cipher_str.c_str(), "paranoid") == 0)
                                l_cipher_str = "AES256-SHA";

                        l_settings.m_cipher_list_str = l_cipher_str;
                        break;
                }
                // ---------------------------------------
                // ssl options
                // ---------------------------------------
                case 'O':
                {
                        l_settings.m_ssl_options_str = l_argument;
                        // TODO Convert to options long

                        int32_t l_status;
                        l_status = get_ssl_options_str_val(l_settings.m_ssl_options_str, l_settings.m_ssl_options);
                        if(l_status != STATUS_OK)
                        {
                                return STATUS_ERROR;
                        }
                        break;
                }
                // ---------------------------------------
                // ssl verify
                // ---------------------------------------
                case 'V':
                {
                        l_settings.m_ssl_verify = true;
                        break;
                }
                // ---------------------------------------
                // ssl sni
                // ---------------------------------------
                case 'N':
                {
                        l_settings.m_ssl_sni = true;
                        break;
                }
                // ---------------------------------------
                // ssl ca file
                // ---------------------------------------
                case 'F':
                {
                        l_settings.m_ssl_ca_file = l_argument;
                        break;
                }
                // ---------------------------------------
                // ssl ca path
                // ---------------------------------------
                case 'L':
                {
                        l_settings.m_ssl_ca_path = l_argument;
                        break;
                }
                // ---------------------------------------
                // parallel
                // ---------------------------------------
                case 'p':
                {
                        int l_start_parallel = 1;
                        //NDBG_PRINT("arg: --parallel: %s\n", optarg);
                        //l_settings.m_start_type = START_PARALLEL;
                        l_start_parallel = atoi(optarg);
                        if (l_start_parallel < 1)
                        {
                                printf("parallel must be at least 1\n");
                                print_usage(stdout, -1);
                        }
                        l_settings.m_start_parallel = l_start_parallel;
                        break;
                }
                // ---------------------------------------
                // num threads
                // ---------------------------------------
                case 't':
                {
                        int l_max_threads = 1;
                        //NDBG_PRINT("arg: --threads: %s\n", l_argument.c_str());
                        l_max_threads = atoi(optarg);
                        if (l_max_threads < 1)
                        {
                                printf("num-threads must be at least 1\n");
                                print_usage(stdout, -1);
                        }
                        l_settings.m_num_threads = l_max_threads;
                        break;
                }
                // ---------------------------------------
                // Header
                // ---------------------------------------
                case 'H':
                {
                        int32_t l_status;
                        std::string l_header_key;
                        std::string l_header_val;
                        l_status = break_header_string(l_argument, l_header_key, l_header_val);
                        if(l_status != 0)
                        {
                                printf("Error header string[%s] is malformed\n", l_argument.c_str());
                                print_usage(stdout, -1);
                        }

                        // Add to reqlet_repo map
                        SET_HEADER(l_header_key, l_header_val);
                        // TODO Check status???
                        break;
                }
                // ---------------------------------------
                // Timeout
                // ---------------------------------------
                case 'T':
                {
                        int l_timeout_s = -1;
                        //NDBG_PRINT("arg: --threads: %s\n", l_argument.c_str());
                        l_timeout_s = atoi(optarg);
                        if (l_timeout_s < 1)
                        {
                                printf("connection timeout must be at least 1\n");
                                print_usage(stdout, -1);
                        }
                        l_settings.m_timeout_s = l_timeout_s;
                        break;
                }
                // ---------------------------------------
                // sock_opt_recv_buf_size
                // ---------------------------------------
                case 'R':
                {
                        int l_sock_opt_recv_buf_size = atoi(optarg);
                        // TODO Check value...
                        l_settings.m_sock_opt_recv_buf_size = l_sock_opt_recv_buf_size;
                        break;
                }
                // ---------------------------------------
                // sock_opt_send_buf_size
                // ---------------------------------------
                case 'S':
                {
                        int l_sock_opt_send_buf_size = atoi(optarg);
                        // TODO Check value...
                        l_settings.m_sock_opt_send_buf_size = l_sock_opt_send_buf_size;
                        break;
                }
                // ---------------------------------------
                // No delay
                // ---------------------------------------
                case 'D':
                {
                        l_settings.m_sock_opt_no_delay = true;
                        break;
                }
                // ---------------------------------------
                // Address Info cache
                // ---------------------------------------
                case 'A':
                {
                        l_ai_cache = l_argument;
                        break;
                }
                // ---------------------------------------
                // connect only
                // ---------------------------------------
                case 'C':
                {
                        l_settings.m_connect_only = true;
                        break;
                }
                // ---------------------------------------
                // verbose
                // ---------------------------------------
                case 'r':
                {
                        l_settings.m_verbose = true;
                        break;
                }
                // ---------------------------------------
                // color
                // ---------------------------------------
                case 'c':
                {
                        l_settings.m_color = true;
                        break;
                }
                // ---------------------------------------
                // quiet
                // ---------------------------------------
                case 'q':
                {
                        l_settings.m_quiet = true;
                        break;
                }
                // ---------------------------------------
                // show progress
                // ---------------------------------------
                case 's':
                {
                        l_settings.m_show_stats = true;
                        break;
                }
                // ---------------------------------------
                // show progress
                // ---------------------------------------
                case 'm':
                {
                        l_settings.m_show_summary = true;
                        break;
                }
                // ---------------------------------------
                // output file
                // ---------------------------------------
                case 'o':
                {
                        l_output_file = l_argument;
                        break;
                }
                // ---------------------------------------
                // line delimited
                // ---------------------------------------
                case 'l':
                {
                        l_output_mode = reqlet_repo::OUTPUT_LINE_DELIMITED;
                        break;
                }
                // ---------------------------------------
                // json output
                // ---------------------------------------
                case 'j':
                {
                        l_output_mode = reqlet_repo::OUTPUT_JSON;
                        break;
                }
                // ---------------------------------------
                // pretty output
                // ---------------------------------------
                case 'P':
                {
                        l_output_pretty = true;
                        break;
                }
                // ---------------------------------------
                // Google Profiler Output File
                // ---------------------------------------
                case 'G':
                {
                        l_gprof_file = l_argument;
                        break;
                }
                // What???
                case '?':
                {
                        // Required argument was missing
                        // '?' is provided when the 3rd arg to getopt_long does not begin with a ':', and is preceeded
                        // by an automatic error message.
                        fprintf(stdout, "  Exiting.\n");
                        print_usage(stdout, -1);
                        break;
                }
                // Huh???
                default:
                {
                        fprintf(stdout, "Unrecognized option.\n");
                        print_usage(stdout, -1);
                        break;
                }
                }
        }

        // Check for required url argument
        if(l_url.empty())
        {
                fprintf(stdout, "No URL specified.\n");
                print_usage(stdout, -1);
        }
        // else set url
        l_settings.m_url = l_url;


        host_list_t l_host_list;
        // -------------------------------------------------
        // Host list processing
        // -------------------------------------------------
        // Read from command
        if(!l_execute_line.empty())
        {
                FILE *fp;
                int32_t l_status = STATUS_OK;

                fp = popen(l_execute_line.c_str(), "r");
                // Error executing...
                if (fp == NULL)
                {
                }

                l_status = add_line(fp, l_host_list);
                if(STATUS_OK != l_status)
                {
                        return STATUS_ERROR;
                }

                l_status = pclose(fp);
                // Error reported by pclose()
                if (l_status == -1)
                {
                        printf("Error: performing pclose\n");
                        return STATUS_ERROR;
                }
                // Use macros described under wait() to inspect `status' in order
                // to determine success/failure of command executed by popen()
                else
                {
                }
        }
        // Read from file
        else if(!l_host_file_str.empty())
        {
                FILE * l_file;
                int32_t l_status = STATUS_OK;

                l_file = fopen(l_host_file_str.c_str(),"r");
                if (NULL == l_file)
                {
                        printf("Error opening file: %s.  Reason: %s\n", l_host_file_str.c_str(), strerror(errno));
                        return STATUS_ERROR;
                }

                l_status = add_line(l_file, l_host_list);
                if(STATUS_OK != l_status)
                {
                        return STATUS_ERROR;
                }

                //NDBG_PRINT("ADD_FILE: DONE: %s\n", a_url_file.c_str());

                l_status = fclose(l_file);
                if (0 != l_status)
                {
                        NDBG_PRINT("Error performing fclose.  Reason: %s\n", strerror(errno));
                        return STATUS_ERROR;
                }
        }
        else if(!l_host_file_json_str.empty())
        {
                // TODO Create a function to do all this mess
                // ---------------------------------------
                // Check is a file
                // TODO
                // ---------------------------------------
                struct stat l_stat;
                int32_t l_status = STATUS_OK;
                l_status = stat(l_host_file_json_str.c_str(), &l_stat);
                if(l_status != 0)
                {
                        NDBG_PRINT("Error performing stat on file: %s.  Reason: %s\n", l_host_file_json_str.c_str(), strerror(errno));
                        return STATUS_ERROR;
                }

                // Check if is regular file
                if(!(l_stat.st_mode & S_IFREG))
                {
                        NDBG_PRINT("Error opening file: %s.  Reason: is NOT a regular file\n", l_host_file_json_str.c_str());
                        return STATUS_ERROR;
                }

                // ---------------------------------------
                // Open file...
                // ---------------------------------------
                FILE * l_file;
                l_file = fopen(l_host_file_json_str.c_str(),"r");
                if (NULL == l_file)
                {
                        NDBG_PRINT("Error opening file: %s.  Reason: %s\n", l_host_file_json_str.c_str(), strerror(errno));
                        return STATUS_ERROR;
                }

                // ---------------------------------------
                // Read in file...
                // ---------------------------------------
                int32_t l_size = l_stat.st_size;
                int32_t l_read_size;
                char *l_buf = (char *)malloc(sizeof(char)*l_size);
                l_read_size = fread(l_buf, 1, l_size, l_file);
                if(l_read_size != l_size)
                {
                        NDBG_PRINT("Error performing fread.  Reason: %s [%d:%d]\n", strerror(errno), l_read_size, l_size);
                        return STATUS_ERROR;
                }
                std::string l_buf_str = l_buf;

                Json::Value l_json_value(Json::objectValue);
                Json::Reader l_json_reader;
                bool l_result = l_json_reader.parse(l_buf_str, l_json_value);
                if (!l_result)
                {
                        NDBG_PRINT("Failed to parse JSON document: %s. Reason: %s\n", l_host_file_json_str.c_str(), l_json_reader.getFormattedErrorMessages().c_str());
                        fclose(l_file);
                        // Best effort -not checking return cuz we outtie
                        return STATUS_ERROR;
                }

                // For each line add
                for( Json::ValueIterator itr = l_json_value.begin() ; itr != l_json_value.end() ; itr++ )
                {
                        const Json::Value &l_value = (*itr);

                        if(l_value.isObject())
                        {
                                host_t l_host;

                                //
                                // "host" : "irobdownload.blob.core.windows.net:443",
                                // "hostname" : "irobdownload.blob.core.windows.net",
                                // "id" : "DE4D",
                                // "where" : "edge"

                                l_host.m_host = l_value.get("host", "NO_HOST").asString();
                                l_host.m_hostname = l_value.get("hostname", "NO_HOSTNAME").asString();
                                l_host.m_id = l_value.get("id", "NO_ID").asString();
                                l_host.m_where = l_value.get("where", "NO_WHERE").asString();
                                // TODO Check exist...
                                l_host_list.push_back(l_host);
                        }

                }

                // ---------------------------------------
                // Close file...
                // ---------------------------------------
                l_status = fclose(l_file);
                if (STATUS_OK != l_status)
                {
                        NDBG_PRINT("Error performing fclose.  Reason: %s\n", strerror(errno));
                        return STATUS_ERROR;
                }
        }
        // Read from stdin
        else
        {
                int32_t l_status = STATUS_OK;
                l_status = add_line(stdin, l_host_list);
                if(STATUS_OK != l_status)
                {
                        return STATUS_ERROR;
                }
        }
        if(l_settings.m_verbose)
        {
                NDBG_PRINT("Showing hostname list:\n");
                for(host_list_t::iterator i_host = l_host_list.begin(); i_host != l_host_list.end(); ++i_host)
                {
                        NDBG_OUTPUT("%s\n", i_host->m_host.c_str());
                }
        }

        // -------------------------------------------
        // Sigint handler
        // -------------------------------------------
        if (signal(SIGINT, sig_handler) == SIG_ERR)
        {
                printf("Error: can't catch SIGINT\n");
                return -1;
        }
        // TODO???
        //signal(SIGPIPE, SIG_IGN);


        // -------------------------------------------
        // Init resolver with cache
        // -------------------------------------------
        int32_t l_ldb_init_status;
        l_ldb_init_status = resolver::get()->init(l_ai_cache, true);
        if(STATUS_OK != l_ldb_init_status)
        {
                return -1;
        }

        // -------------------------------------------
        // SSL init...
        // -------------------------------------------
        l_settings.m_ssl_ctx = ssl_init(l_settings.m_cipher_list_str, // ctx cipher list str
                                        l_settings.m_ssl_options,     // ctx options
                                        l_settings.m_ssl_ca_file,     // ctx ca file
                                        l_settings.m_ssl_ca_path);    // ctx ca path
        if(NULL == l_settings.m_ssl_ctx)
        {
                NDBG_PRINT("Error: performing ssl_init with cipher_list: %s\n", l_settings.m_cipher_list_str.c_str());
                return STATUS_ERROR;
        }

        SSL_CTX* ssl_init(const std::string &a_cipher_list,
                          long a_options = 0,
                          bool a_verify = false,
                          const std::string &a_ca_file = "",
                          const std::string &a_ca_path = "");

        // Start Profiler
        if (!l_gprof_file.empty())
        {
                ProfilerStart(l_gprof_file.c_str());
        }

        // Run
        int32_t l_run_status = 0;
        l_run_status = run(l_settings, l_host_list);
        if(0 != l_run_status)
        {
                printf("Error: performing hle::run");
                return -1;
        }

        //uint64_t l_start_time_ms = get_time_ms();

        // -------------------------------------------
        // Run command exec
        // -------------------------------------------
        // Copy in settings
        command_exec(l_settings);

        if(l_settings.m_verbose)
        {
                printf("Finished -joining all threads\n");
        }

        // Wait for completion
        wait_till_stopped(l_settings);

        // One more status for the lovers
        reqlet_repo::get()->display_status_line(l_settings.m_color);

        if (!l_gprof_file.empty())
        {
                ProfilerStop();
        }

        //uint64_t l_end_time_ms = get_time_ms() - l_start_time_ms;

        // -------------------------------------------
        // Results...
        // -------------------------------------------
        if(!g_cancelled && !l_settings.m_quiet)
        {
                bool l_use_color = l_settings.m_color;
                if(!l_output_file.empty()) l_use_color = false;
                std::string l_responses_str;
                l_responses_str = reqlet_repo::get()->dump_all_responses(l_use_color, l_output_pretty, l_output_mode, l_output_part);
                if(l_output_file.empty())
                {
                        NDBG_OUTPUT("%s\n", l_responses_str.c_str());
                }
                else
                {
                        int32_t l_num_bytes_written = 0;
                        int32_t l_status = 0;
                        // Open
                        FILE *l_file_ptr = fopen(l_output_file.c_str(), "w+");
                        if(l_file_ptr == NULL)
                        {
                                NDBG_PRINT("Error performing fopen. Reason: %s\n", strerror(errno));
                                return STATUS_ERROR;
                        }

                        // Write
                        l_num_bytes_written = fwrite(l_responses_str.c_str(), 1, l_responses_str.length(), l_file_ptr);
                        if(l_num_bytes_written != (int32_t)l_responses_str.length())
                        {
                                NDBG_PRINT("Error performing fwrite. Reason: %s\n", strerror(errno));
                                fclose(l_file_ptr);
                                return STATUS_ERROR;
                        }

                        // Close
                        l_status = fclose(l_file_ptr);
                        if(l_status != 0)
                        {
                                NDBG_PRINT("Error performing fclose. Reason: %s\n", strerror(errno));
                                return STATUS_ERROR;
                        }

                }

        }

        // -------------------------------------------
        // Summary...
        // -------------------------------------------
        if(l_settings.m_show_summary)
        {
                reqlet_repo::get()->display_summary(l_settings.m_color);
        }

        // -------------------------------------------
        // Cleanup...
        // -------------------------------------------
        // Delete t_client list...
        for(t_client_list_t::iterator i_client_hle = l_settings.m_t_client_list.begin();
                        i_client_hle != l_settings.m_t_client_list.end(); )
        {
                t_client *l_t_client_ptr = *i_client_hle;
                delete l_t_client_ptr;
                l_settings.m_t_client_list.erase(i_client_hle++);
        }

        // SSL Cleanup
        ssl_kill_locks();

        // TODO Deprecated???
        //EVP_cleanup();

        //if(l_settings.m_verbose)
        //{
        //      NDBG_PRINT("Cleanup\n");
        //}

        return 0;

}

#endif


} //namespace ns_hlx {

#endif // #ifndef _HLX_CLIENT_H

