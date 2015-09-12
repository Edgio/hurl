//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    t_server.cc
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

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "t_server.h"
#include "util.h"
#include "ndebug.h"
#include "nbq.h"
#include "nconn_tcp.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
const char *G_HTTP_RESPONSE =
"HTTP/1.1 200 OK\r\n"
"Date: Sun, 07 Jun 2015 22:49:12 GMT\r\n"
"Server: hss HTTP API Server v0.0.1\r\n"
"Content-length: 26\r\n"
"Content-Type: application/json\r\n"
"\r\n"
"[{\"xxxxxx-xxxx\":\"x.x.xx\"}]\r\n"
"\r\n"
"\r\n";

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Thread local global
//: ----------------------------------------------------------------------------
__thread t_server *g_t_server = NULL;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define COPY_SETTINGS(_field) m_settings._field = a_settings._field
t_server::t_server(const settings_struct_t &a_settings,
                   url_router *a_url_router):
        m_t_run_thread(),
        m_settings(),
        m_url_router(a_url_router),
        m_nconn_pool(a_settings.m_num_parallel),
        m_stopped(false),
        m_start_time_s(0),
        m_evr_loop(NULL),
        m_scheme(nconn::SCHEME_TCP),
        m_nconn(NULL)
{

        // Friggin effc++
        COPY_SETTINGS(m_verbose);
        COPY_SETTINGS(m_color);
        COPY_SETTINGS(m_evr_loop_type);
        COPY_SETTINGS(m_num_parallel);
        COPY_SETTINGS(m_fd);

        // Set save request data
        // TODO make flag name generic request/response
        m_settings.m_save_response = true;
        m_settings.m_color = true;

        // Create loop
        m_evr_loop = new evr_loop(evr_loop_file_readable_cb,
                                  evr_loop_file_writeable_cb,
                                  evr_loop_file_error_cb,
                                  m_settings.m_evr_loop_type,
                                  m_settings.m_num_parallel,
                                  false,
                                  true);

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_server::~t_server()
{
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
int t_server::run(void)
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
void t_server::stop(void)
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
int32_t t_server::evr_loop_file_writeable_cb(void *a_data)
{
        NDBG_PRINT("%sWRITEABLE%s %p\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, a_data);
        if(!a_data)
        {
                return STATUS_OK;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        // TODO FIX!!!
        //reqlet *l_reqlet = static_cast<reqlet *>(l_nconn->get_data1());
        t_server *l_t_server = g_t_server;

        // Cancel last timer
        l_t_server->m_evr_loop->cancel_timer(&(l_nconn->m_timer_obj));

        int32_t l_status = STATUS_OK;
        // TODO FIX!!!
        //l_status = l_nconn->nc_run_state_machine(l_t_server->m_evr_loop,
        //                                         l_reqlet->m_host_info,
        //                                         nconn::NC_MODE_WRITE);
        if(STATUS_ERROR == l_status)
        {
                if(l_nconn->m_verbose)
                {
                        NDBG_PRINT("Error: performing run_state_machine\n");
                }
                //T_SERVER_CONN_CLEANUP(l_t_server, l_nconn, l_reqlet, 901, l_nconn->m_last_error.c_str(), STATUS_ERROR);
                return STATUS_OK;
        }

        if(l_nconn->is_done())
        {
                if(l_t_server->m_settings.m_connect_only)
                {
                        //T_SERVER_CONN_CLEANUP(l_t_server, l_nconn, l_reqlet, 200, "Connected Successfully", STATUS_OK);
                }
                else
                {
                        //T_SERVER_CONN_CLEANUP(l_t_server, l_nconn, l_reqlet, 0, "", STATUS_ERROR);
                }
                return STATUS_OK;
        }


        // Add idle timeout
        //l_t_server->m_evr_loop->add_timer( l_t_server->get_timeout_s()*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_server::evr_loop_file_readable_cb(void *a_data)
{
        NDBG_PRINT("%sREADABLE%s %p\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, a_data);
        if(!a_data)
        {
                //NDBG_PRINT("a_data == NULL\n");
                return STATUS_OK;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        //reqlet *l_reqlet = static_cast<reqlet *>(l_nconn->get_data1());
        t_server *l_t_server = g_t_server;
        int32_t l_status = STATUS_OK;

        l_status = l_nconn->nc_run_state_machine(l_t_server->m_evr_loop,
                                                 nconn::NC_MODE_NONE);
        if((STATUS_ERROR == l_status) &&
           l_nconn->m_verbose)
        {
                NDBG_PRINT("Error: performing run_state_machine\n");
        }

        NDBG_PRINT("%sREADABLE%s l_nconn->is_listening(): %d\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, l_nconn->is_listening());

        if(l_nconn->is_listening())
        {
                int32_t l_fd = l_status;
                // -----------------------------------------
                // Make function
                // -----------------------------------------
                // Get connection for new fd
                //l_reqlet = new reqlet(0, 1);

                NDBG_PRINT("l_t_server->m_settings.m_verbose:       %d\n", l_t_server->m_settings.m_verbose);
                NDBG_PRINT("l_t_server->m_settings.m_save_response: %d\n", l_t_server->m_settings.m_save_response);

#if 0
                l_nconn = l_t_server->m_nconn_pool.get("TEST",
                                                       nconn::SCHEME_TCP,
                                                       l_t_server->m_settings,
                                                       nconn::TYPE_SERVER);
                if(!l_nconn)
                {
                        NDBG_PRINT("Error l_nconn == NULL\n");
                        return NULL;
                }
#else
                l_nconn = new nconn_tcp(l_t_server->m_settings.m_verbose,
                                        l_t_server->m_settings.m_color,
                                        l_t_server->m_settings.m_num_reqs_per_conn,
                                        l_t_server->m_settings.m_save_response,
                                        l_t_server->m_settings.m_collect_stats,
                                        l_t_server->m_settings.m_connect_only,
                                        nconn::TYPE_SERVER);
#endif

                // TODO create q pool... -this is gonna leak...
                l_nconn->set_in_q(new nbq(16384));
                l_nconn->set_out_q(new nbq(16384));
                http_req *l_req = new http_req();
                l_nconn->set_data1(l_req);

                // Set connected
                l_status = l_nconn->nc_set_connected(l_t_server->m_evr_loop, l_fd);
                if(l_status != STATUS_OK)
                {
                        NDBG_PRINT("Error: performing run_state_machine\n");
                        return STATUS_ERROR;
                }

                // Run state machine???
                l_status = l_nconn->nc_run_state_machine(l_t_server->m_evr_loop,
                                                         nconn::NC_MODE_READ);
                if((STATUS_ERROR == l_status) &&
                   l_nconn->m_verbose)
                {
                        NDBG_PRINT("Error: performing run_state_machine\n");
                }

                NDBG_PRINT("RAN_STATE_MACHINE: is_done(): %d\n", l_req->m_complete);

                // Send response...
                if(l_req->m_complete)
                {
                        // Get path
                        std::string l_path;
                        l_path.assign(l_req->m_p_url.m_ptr, l_req->m_p_url.m_len);
                        NDBG_PRINT("PATH: %s\n", l_path.c_str());

                        // Set resp and outq
                        http_resp *l_resp = new http_resp();
                        l_resp->set_q(l_nconn->get_out_q());

                        http_request_handler *l_request_handler = NULL;
                        url_param_map_t l_param_map;
                        l_request_handler = (http_request_handler *)l_t_server->m_url_router->find_route(l_path,l_param_map);
                        NDBG_PRINT("l_request_handler: %p\n", l_request_handler);
                        if(l_request_handler)
                        {
                                // Method switch
                                switch(l_req->m_method)
                                {
                                case HTTP_GET:
                                {
                                        l_status = l_request_handler->do_get(l_param_map, *l_req, *l_resp);
                                        if(l_status != STATUS_OK)
                                        {
                                                NDBG_PRINT("Error performing do_get\n");
                                                // TODO...
                                        }
                                        break;
                                }
                                case HTTP_POST:
                                {
                                        l_status = l_request_handler->do_post(l_param_map, *l_req, *l_resp);
                                        if(l_status != STATUS_OK)
                                        {
                                                NDBG_PRINT("Error performing do_post\n");
                                                // TODO...
                                        }
                                        break;
                                }
                                case HTTP_PUT:
                                {
                                        l_status = l_request_handler->do_put(l_param_map, *l_req, *l_resp);
                                        if(l_status != STATUS_OK)
                                        {
                                                NDBG_PRINT("Error performing do_put\n");
                                                // TODO...
                                        }
                                        break;
                                }
                                case HTTP_DELETE:
                                {
                                        l_status = l_request_handler->do_delete(l_param_map, *l_req, *l_resp);
                                        if(l_status != STATUS_OK)
                                        {
                                                NDBG_PRINT("Error performing do_delete\n");
                                                // TODO...
                                        }
                                        break;
                                }
                                default:
                                {
                                        l_status = l_request_handler->do_default(l_param_map, *l_req, *l_resp);
                                        if(l_status != STATUS_OK)
                                        {
                                                NDBG_PRINT("Error performing do_default\n");
                                                // TODO...
                                        }
                                        break;
                                }
                                }
                        }
                        // Send Error response???
                        if(l_status != STATUS_OK)
                        {
                                // TODO 500 response and cleanup
                                l_t_server->cleanup_connection(l_nconn, true, 500);
                        }

                        NDBG_PRINT("RUN_STATE_MACHINE: write response\n");

                        // Set to writing and run state machine
                        l_status = l_nconn->nc_run_state_machine(l_t_server->m_evr_loop,
                                                                 nconn::NC_MODE_WRITE);
                        if((STATUS_ERROR == l_status) &&
                           l_nconn->m_verbose)
                        {
                                NDBG_PRINT("Error: performing run_state_machine\n");
                        }

                        // TODO 200 response and cleanup
                        l_t_server->cleanup_connection(l_nconn, true, 200);

                        // TODO Keep-alives???

#if 0
                        // Create response
                        l_status = l_t_server->get_response(*l_nconn);
                        if(STATUS_OK != l_status)
                        {
                                // TODO 500 response and cleanup
                                l_t_server->cleanup_connection(l_nconn, true, 500);
                        }
#endif

#if 0
                        // Set to writing and run state machine
                        l_status = l_nconn->nc_run_state_machine(l_t_server->m_evr_loop,
                                                                 l_reqlet->m_host_info,
                                                                 nconn::NC_MODE_READ);
                        if((STATUS_ERROR == l_status) &&
                           l_nconn->m_verbose)
                        {
                                NDBG_PRINT("Error: performing run_state_machine\n");
                        }
#endif
                }
        }
        else
        {
                // Cancel last timer
                l_t_server->m_evr_loop->cancel_timer(&(l_nconn->m_timer_obj));

                l_status = l_nconn->nc_run_state_machine(l_t_server->m_evr_loop,
                                                         nconn::NC_MODE_READ);
                if((STATUS_ERROR == l_status) &&
                   l_nconn->m_verbose)
                {
                        NDBG_PRINT("Error: performing run_state_machine\n");
                }
        }

#if 0
        if(l_status >= 0)
        {
                l_reqlet->m_stat_agg.m_num_bytes_read += l_status;
        }

        // Check for done...
        if((l_nconn->is_done()) ||
           (l_status == STATUS_ERROR))
        {
                // Add stats
                add_stat_to_agg(l_reqlet->m_stat_agg, l_nconn->get_stats());
                l_nconn->reset_stats();

                // Bump stats
                if(l_status == STATUS_ERROR)
                {
                        ++(l_reqlet->m_stat_agg.m_num_errors);
                        T_SERVER_CONN_CLEANUP(l_t_server, l_nconn, l_reqlet, 901, l_nconn->m_last_error.c_str(), STATUS_ERROR);
                }
                else
                {

                        if(l_t_server->m_settings.m_connect_only)
                        {
                                T_SERVER_CONN_CLEANUP(l_t_server, l_nconn, l_reqlet, 200, "Connected Successfully", STATUS_OK);
                                return NULL;
                        }

                        if(!l_nconn->can_reuse())
                        {
                                T_SERVER_CONN_CLEANUP(l_t_server, l_nconn, l_reqlet, 0, "", STATUS_OK);
                                return NULL;
                        }

                        // TODO REMOVE
                        //NDBG_PRINT("CONN %sREUSE%s: l_nconn->can_reuse(): %d\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, l_nconn->can_reuse());

                        l_reqlet->set_response(STATUS_OK, "");
                        if(l_t_server->m_settings.m_show_summary)
                                l_t_server->append_summary(l_reqlet);
                        ++(l_t_server->m_num_done);

                        // Cancel last timer
                        l_t_server->m_evr_loop->cancel_timer(&(l_nconn->m_timer_obj));
                        l_nconn->reset_stats();

                        // Reduce num pending
                        ++(l_t_server->m_num_fetched);
                        --(l_t_server->m_num_pending);

                        // TODO Use pool
                        if(!l_t_server->m_settings.m_use_persistent_pool)
                        {
                                if(!l_t_server->is_pending_done())
                                {
                                        ++l_reqlet->m_stat_agg.m_num_conn_completed;
                                        ++l_t_server->m_num_fetched;

                                        // Send request again...
                                        l_t_server->limit_rate();
                                        l_t_server->create_request(*l_nconn, *l_reqlet);
                                        l_nconn->send_request(true);
                                }
                                else
                                {
                                        T_SERVER_CONN_CLEANUP(l_t_server, l_nconn, l_reqlet, 0, "", STATUS_OK);
                                }

                        }
                        else
                        {
                                l_status = l_t_server->m_nconn_pool.add_idle(l_nconn);
                                if(STATUS_OK != l_status)
                                {
                                        T_SERVER_CONN_CLEANUP(l_t_server, l_nconn, l_reqlet, 901, "Error setting idle", STATUS_ERROR);
                                }
                        }
                }
                return NULL;
        }

        // Add idle timeout
        l_t_server->m_evr_loop->add_timer( l_t_server->get_timeout_s()*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));
#endif

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *g_completion_timer;
int32_t t_server::evr_loop_timer_completion_cb(void *a_data)
{
        return STATUS_OK;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_server::evr_loop_file_error_cb(void *a_data)
{
        NDBG_PRINT("%sSTATUS_ERRORS%s: %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, a_data);
        if(!a_data)
        {
                //NDBG_PRINT("a_data == NULL\n");
                return STATUS_OK;
        }
        nconn* l_nconn = static_cast<nconn*>(a_data);
        t_server *l_t_server = g_t_server;

        // Cleanup
        l_t_server->cleanup_connection(l_nconn, true, 900);

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_server::evr_loop_file_timeout_cb(void *a_data)
{
        NDBG_PRINT("%sTIMEOUT%s %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, a_data);
        if(!a_data)
        {
                //NDBG_PRINT("a_data == NULL\n");
                return STATUS_OK;
        }

#if 0
        nconn* l_nconn = static_cast<nconn*>(a_data);
        reqlet *l_reqlet = static_cast<reqlet *>(l_nconn->get_data1());
        t_server *l_t_server = g_t_server;

        //printf("%sT_O%s: %p\n",ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF,
        //                l_rconn->m_timer_obj);

        // Add stats
        add_stat_to_agg(l_reqlet->m_stat_agg, l_nconn->get_stats());

        if(l_t_server->m_settings.m_verbose)
        {
                NDBG_PRINT("%sTIMING OUT CONN%s: i_conn: %lu HOST: %s THIS: %p\n",
                                ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                                l_nconn->get_id(),
                                l_reqlet->m_url.m_host.c_str(),
                                l_t_server);
        }

        // Stats
        ++l_t_server->m_num_fetched;
        ++l_reqlet->m_stat_agg.m_num_idle_killed;

        // Cleanup
        T_SERVER_CONN_CLEANUP(l_t_server, l_nconn, l_reqlet, 902, "Connection timed out", STATUS_ERROR);
#endif

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_server::evr_loop_timer_cb(void *a_data)
{
        //NDBG_PRINT("%sTIMER%s %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, a_data);
        if(!a_data)
        {
                //NDBG_PRINT("a_data == NULL\n");
                return STATUS_OK;
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_server::t_run(void *a_nothing)
{

        NDBG_PRINT("starting main loop\n");

        m_stopped = false;

        // Set thread local
        g_t_server = this;

        // Set start time
        m_start_time_s = get_time_s();

        // -------------------------------------------------
        // Create a server connection object
        // -------------------------------------------------
        m_nconn = new nconn_tcp(m_settings.m_verbose,
                                m_settings.m_color,
                                -1,
                                true,
                                false,
                                false);

        // Set to listening mode
        NDBG_PRINT("starting main loop\n");
        int32_t l_status;
        l_status = m_nconn->nc_set_listening(m_evr_loop, m_settings.m_fd);
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error performing nc_set_listening.\n");
                return NULL;
        }
        NDBG_PRINT("starting main loop\n");

        // -------------------------------------------------
        // Run server
        // -------------------------------------------------
#define MAX_READ_SIZE 16384

        NDBG_PRINT("starting main loop\n");
        //char l_read_buf[MAX_READ_SIZE];
        while(!m_stopped)
        {

                m_evr_loop->run();
#if 0
                // -------------------------------------------
                // Accept client connections
                // -------------------------------------------
                int l_client_sock_fd;
                l_client_sock_fd = accept_tcp_connection(m_settings.m_server_fd);
                if(l_client_sock_fd == STATUS_ERROR)
                {
                        NDBG_PRINT("Error performing accept_tcp_connection with socket number = %d\n",l_client_sock_fd);
                        return NULL;
                }
#endif

#if 0
                m_next_header.clear();
                m_body.clear();
                m_request.clear();

                // Setup parser
                m_http_parser.data = this;
                http_parser_init(&m_http_parser, HTTP_REQUEST);

                // -------------------------------------------
                // Read till close...
                // -------------------------------------------
                uint32_t l_read_size = 0;
                l_read_size = read(l_client_sock_fd, l_read_buf, MAX_READ_SIZE);
                //NDBG_PRINT("Read data[size: %d]\n", l_read_size);
                //if(l_read_size > 0)
                //{
                //        ns_ndebug::mem_display((const uint8_t *)l_read_buf, l_read_size);
                //}

                // -------------------------------------------
                // Parse
                // -------------------------------------------
                m_cur_msg_complete = false;
                while(1)
                {
                        size_t l_parse_status = 0;
                        int32_t l_read_buf_idx = 0;
                        //NDBG_PRINT("%sHTTP_PARSER%s: m_read_buf: %p, m_read_buf_idx: %d, l_bytes_read: %d\n",
                        //                ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF,
                        //                l_read_buf, (int)l_read_buf_idx, (int)l_read_size);
                        l_parse_status = http_parser_execute(&m_http_parser, &m_http_parser_settings, l_read_buf + l_read_buf_idx, l_read_size - l_read_buf_idx);
                        if(l_parse_status < 0)
                        {
                                NDBG_PRINT("Error: parse error.  Reason: %s: %s\n",
                                                http_errno_name((enum http_errno)m_http_parser.http_errno),
                                                http_errno_description((enum http_errno)m_http_parser.http_errno));
                                return NULL;
                        }
                        l_read_buf_idx += l_parse_status;

                        if(m_cur_msg_complete)
                                break;
                }

                // -------------------------------------------
                // Process request
                // -------------------------------------------
                std::string l_response_body = "{}";

                // do stuff...

                // -------------------------------------------
                // Show
                // -------------------------------------------
                //m_request.show(m_color);

                // -------------------------------------------
                // Write response
                // -------------------------------------------
                std::string l_response  = "";
                l_response += "HTTP/1.1 200 OK\r\n";
                l_response += "Content-Type: application/json\r\n";

                l_response += "Content-Length: ";
                char l_len_str[64]; sprintf(l_len_str, "%d", (int)l_response_body.length());
                l_response += l_len_str;
                l_response += "\r\n\r\n";
                l_response += l_response_body;

                uint32_t l_write_size = 0;
                l_write_size = write(l_client_sock_fd, l_response.c_str(), l_response.length());
                if(l_write_size != l_response.length())
                {
                        NDBG_PRINT("Error write failed. Reason[%d]: %s\n", errno, strerror(errno));
                        return NULL;

                }
                //NDBG_PRINT("Write data[size: %d]\n", l_write_size);

                // -------------------------------------------
                // Close
                // -------------------------------------------
                close(l_client_sock_fd);
#endif

        }


        m_stopped = true;
        return NULL;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_server::cleanup_connection(nconn *a_nconn, bool a_cancel_timer, int32_t a_status)
{

        //NDBG_PRINT("%sCLEANUP%s:\n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF);


        // Cancel last timer
        if(a_cancel_timer)
        {
                m_evr_loop->cancel_timer(&(a_nconn->m_timer_obj));
        }

        //NDBG_PRINT("%sADDING_BACK%s: %u\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, (uint32_t)a_nconn->get_id());

        // Add back to free list
        if(STATUS_OK != m_nconn_pool.release(a_nconn))
        {
                return STATUS_ERROR;
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_server::get_response(nconn &ao_conn)
{

        NDBG_PRINT("%sWRITE_DAT!%s\n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF);

        // Copy into send q
        nbq *l_write_q = ao_conn.get_out_q();
        l_write_q->write(G_HTTP_RESPONSE, strlen(G_HTTP_RESPONSE));
#if 0
        // Get client
        char *l_req_buf = NULL;
        uint32_t l_req_buf_len = 0;
        uint32_t l_max_buf_len = nconn_tcp::m_max_req_buf;

        if(!a_reqlet.m_multipath)
        {
                GET_NCONN_OPT(ao_conn, nconn_tcp::OPT_TCP_REQ_BUF_LEN, NULL, &l_req_buf_len);
                if(l_req_buf_len)
                {
                        //NDBG_PRINT("Bailing already set to: %u\n", l_req_buf_len);
                        return STATUS_OK;
                }
        }


        GET_NCONN_OPT(ao_conn, nconn_tcp::OPT_TCP_REQ_BUF, (void **)(&l_req_buf), &l_req_buf_len);

        // -------------------------------------------
        // Request.
        // -------------------------------------------
        const std::string &l_path_ref = a_reqlet.get_path(NULL);
        //NDBG_PRINT("HOST: %s PATH: %s\n", a_reqlet.m_url.m_host.c_str(), l_path_ref.c_str());
        if(l_path_ref.length())
        {
                l_req_buf_len = snprintf(l_req_buf, l_max_buf_len,
                                "%s %.500s HTTP/1.1\r\n", m_settings.m_verb.c_str(), l_path_ref.c_str());
        } else {
                l_req_buf_len = snprintf(l_req_buf, l_max_buf_len,
                                "%s / HTTP/1.1\r\n", m_settings.m_verb.c_str());
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

        // -------------------------------------------
        // body
        // -------------------------------------------
        if(m_settings.m_req_body)
        {
                memcpy(l_req_buf + l_req_buf_len, m_settings.m_req_body, m_settings.m_req_body_len);
                l_req_buf_len += m_settings.m_req_body_len;
        }

        // Set len
        SET_NCONN_OPT(ao_conn, nconn_tcp::OPT_TCP_REQ_BUF_LEN, NULL, l_req_buf_len);
#endif

        return STATUS_OK;
}

} //namespace ns_hlx {



