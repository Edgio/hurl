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
#include "ndebug.h"
#include "nbq.h"
#include "nconn_tcp.h"
#include "nconn_ssl.h"
#include "time_util.h"
#include "url_router.h"
#include "http_cb.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define T_SERVER_SET_NCONN_OPT(_conn, _opt, _buf, _len) \
        do { \
                int _status = 0; \
                _status = _conn.set_opt((_opt), (_buf), (_len)); \
                if (_status != nconn::NC_STATUS_OK) { \
                        NDBG_PRINT("STATUS_ERROR: Failed to set_opt %d.  Status: %d.\n", _opt, _status); \
                        return STATUS_ERROR;\
                } \
        } while(0)

#define CHECK_FOR_NULL_ERROR_DEBUG(_data) \
        do {\
                if(!_data) {\
                        NDBG_PRINT("Error.\n");\
                        return STATUS_ERROR;\
                }\
        } while(0);
#define CHECK_FOR_NULL_ERROR(_data) \
        do {\
                if(!_data) {\
                        return STATUS_ERROR;\
                }\
        } while(0);

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
// TODO multi-thread support
#if 0
// ---------------------------------------------------------
// Work...
// ---------------------------------------------------------
typedef struct work_struct {
        nconn *m_nconn;
        nbq *m_in_q;
        nbq *m_out_q;
        http_req *m_req;
        http_resp *m_resp;
        t_server *m_t_server;
        // TODO Add http parser here???

        work_struct():
                m_nconn(NULL),
                m_in_q(NULL),
                m_out_q(NULL),
                m_req(NULL),
                m_resp(NULL),
                m_t_server(NULL)
        {}
private:
        DISALLOW_COPY_AND_ASSIGN(work_struct);
} work_t;
#endif

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define COPY_SETTINGS(_field) m_settings._field = a_settings._field
t_server::t_server(const server_settings_struct_t &a_settings,
                   url_router *a_url_router):
        m_t_run_thread(),
        m_settings(),
        m_url_router(a_url_router),
        m_out_q(),
        m_nconn_pool(a_settings.m_num_parallel),
        m_stopped(false),
        m_start_time_s(0),
        m_evr_loop(NULL),
        m_scheme(nconn::SCHEME_TCP),
        m_listening_nconn(NULL),
        m_out_q_nconn(NULL),
        m_out_q_fd(-1),
        m_default_handler(),
        m_http_data_pool(),
        m_stat(),
        // TODO multi-thread support
#if 0
        m_work_q(),
        m_work_q_attr(),
#endif
        m_is_initd(false)
{

        // Friggin effc++
        COPY_SETTINGS(m_verbose);
        COPY_SETTINGS(m_color);
        COPY_SETTINGS(m_evr_loop_type);
        COPY_SETTINGS(m_num_parallel);
        COPY_SETTINGS(m_fd);
        COPY_SETTINGS(m_scheme);
        COPY_SETTINGS(m_tls_key);
        COPY_SETTINGS(m_tls_crt);
        COPY_SETTINGS(m_ssl_ctx);
        COPY_SETTINGS(m_ssl_cipher_list);
        COPY_SETTINGS(m_ssl_options_str);
        COPY_SETTINGS(m_ssl_options);
        COPY_SETTINGS(m_ssl_ca_file);
        COPY_SETTINGS(m_ssl_ca_path);

        // Set save request data
        // TODO make flag name generic request/response
        m_settings.m_save_response = true;
        m_settings.m_color = true;
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

        // TODO multi-thread support
#if 0
        // Destroy work q attr
        int32_t l_status;
        l_status = pthread_workqueue_attr_destroy_np(&m_work_q_attr);
        if (l_status != 0)
        {
                NDBG_PRINT("Error performing pthread_workqueue_attr_destroy_np\n");
                // TODO error???
        }
        // TODO destroy work q???
#endif

        if(m_out_q_nconn)
        {
                delete m_out_q_nconn;
        }
        if(m_listening_nconn)
        {
                delete m_listening_nconn;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_server::init(void)
{
        if(m_is_initd)
        {
                return STATUS_OK;
        }

        // Create loop
        m_evr_loop = new evr_loop(evr_loop_file_readable_cb,
                                  evr_loop_file_writeable_cb,
                                  evr_loop_file_error_cb,
                                  m_settings.m_evr_loop_type,
                                  // TODO Need to make epoll vector resizeable...
                                  512,
                                  false);

        int32_t l_status;
        // TODO multi-thread support
#if 0
        // ---------------------------------------
        // Request queue
        // TODO -consider separate class...
        // ---------------------------------------
        l_status = pthread_workqueue_attr_init_np(&m_work_q_attr);
        if (l_status != 0)
        {
                NDBG_PRINT("Error performing pthread_workqueue_attr_init_np\n");
                return STATUS_ERROR;
        }
        int32_t l_over_commit = 0;
        l_status = pthread_workqueue_attr_setovercommit_np(&m_work_q_attr, l_over_commit);
        if (l_status != 0)
        {
                NDBG_PRINT("Error performing pthread_workqueue_attr_setovercommit_np\n");
                return STATUS_ERROR;
        }
        l_status = pthread_workqueue_create_np(&m_work_q, &m_work_q_attr);
        if (l_status != 0)
        {
                NDBG_PRINT("Error performing pthread_workqueue_create_np\n");
                return STATUS_ERROR;
        }

        // Fake nconn -used for out-bound I/O notifications
        m_out_q_nconn = m_nconn_pool.create_conn(m_settings.m_scheme, m_settings,nconn::TYPE_NONE);
        m_out_q_nconn->set_idx(sc_work_q_conn_idx);
        m_out_q_fd = m_evr_loop->add_event(m_out_q_nconn);
        if(m_out_q_fd == STATUS_ERROR)
        {
                NDBG_PRINT("Error performing m_evr_loop->add_event\n");
                return STATUS_ERROR;
        }
#endif

        // Create a server connection object
        m_listening_nconn = m_nconn_pool.create_conn(m_settings.m_scheme);

        // Config
        l_status = config_conn(m_listening_nconn);
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error: performing config_conn\n");
                return STATUS_ERROR;
        }

        l_status = m_listening_nconn->nc_set_listening(m_evr_loop, m_settings.m_fd);
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error performing nc_set_listening.\n");
                return STATUS_ERROR;
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *t_server::get_new_client_conn(int a_fd)
{
        nconn *l_nconn;
        int32_t l_status;
        l_status = m_nconn_pool.get(m_settings.m_scheme, &l_nconn);
        if(!l_nconn ||
           (l_status != nconn::NC_STATUS_OK))
        {
                NDBG_PRINT("Error: performing m_nconn_pool.get\n");
                return NULL;
        }

        //NDBG_PRINT("%sGET_NEW%s: %u a_nconn: %p\n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, (uint32_t)l_nconn->get_idx(), l_nconn);

        // Config
        l_status = config_conn(l_nconn);
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error: performing config_conn\n");
                return NULL;
        }

        ++m_stat.m_cur_conn_count;
        ++m_stat.m_num_conn_started;

        // TODO create q pool... -this is gonna leak...
        nbq *l_out_q = new nbq(16384);
        l_nconn->set_out_q(l_out_q);

        http_data_t * l_data = static_cast<http_data_t *>(l_nconn->get_data());

        // Set connected
        l_status = reset_conn_input_q(l_data, l_nconn);
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error: performing run_state_machine\n");
                cleanup_connection(l_nconn, l_data->m_timer_obj);
                return NULL;
        }

        // Set connected
        l_status = l_nconn->nc_set_accepting(m_evr_loop, a_fd);
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error: performing run_state_machine\n");
                cleanup_connection(l_nconn, l_data->m_timer_obj);
                return NULL;
        }

#if 0
        // Initalize the http response parser
        m_http_parser.data = this;
        http_parser_init(&m_http_parser, HTTP_REQUEST);
#endif

        return l_nconn;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_server::reset_conn_input_q(http_data_t *a_data, nconn *a_nconn)
{
        if(a_nconn->get_in_q())
        {
                a_nconn->get_in_q()->reset();
        }
        else
        {
                a_nconn->set_in_q(new nbq(16384));
        }
        return STATUS_OK;
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
        // Cleanup server connection
        //cleanup_connection(m_listening_nconn, NULL, 200);

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
void t_server::get_stats_copy(hlx_server::t_stat_t &ao_stat)
{
        ao_stat = m_stat;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_server::evr_loop_file_writeable_cb(void *a_data)
{
        //NDBG_PRINT("%sWRITEABLE%s %p\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, a_data);
        if(!a_data)
        {
                //NDBG_PRINT("a_data == NULL\n");
                return STATUS_OK;
        }
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_data());
        http_data_t *l_data = static_cast<http_data_t *>(l_nconn->get_data());
        CHECK_FOR_NULL_ERROR(l_data->m_ctx);
        t_server *l_t_server = static_cast<t_server *>(l_data->m_ctx);

        if(l_nconn->is_free())
        {
                return STATUS_OK;
        }

        // Cancel last timer
        l_t_server->m_evr_loop->cancel_timer(l_data->m_timer_obj);

        // Check for out data to write
        int32_t l_status = STATUS_OK;
        l_status = l_nconn->nc_run_state_machine(l_t_server->m_evr_loop, nconn::NC_MODE_WRITE);
        if(STATUS_ERROR == l_status)
        {
                //if(l_nconn->m_verbose)
                //{
                //        NDBG_PRINT("Error: performing run_state_machine\n");
                //}
                l_t_server->cleanup_connection(l_nconn, l_data->m_timer_obj);
                return STATUS_OK;
        }

        if(l_nconn->is_done())
        {
                l_t_server->cleanup_connection(l_nconn, l_data->m_timer_obj);
                return STATUS_OK;
        }

        // Add idle timeout
        l_t_server->m_evr_loop->add_timer( l_t_server->get_timeout_s()*1000, evr_loop_file_timeout_cb, l_nconn, &(l_data->m_timer_obj));

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
//uint32_t g_req_num = 0;
int32_t t_server::evr_loop_file_readable_cb(void *a_data)
{
        //NDBG_PRINT("%sREADABLE%s %p\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, a_data);
        if(!a_data)
        {
                //NDBG_PRINT("a_data == NULL\n");
                return STATUS_OK;
        }
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_data());
        http_data_t *l_data = static_cast<http_data_t *>(l_nconn->get_data());
        CHECK_FOR_NULL_ERROR(l_data->m_ctx);
        t_server *l_t_server = static_cast<t_server *>(l_data->m_ctx);

        // Cancel last timer
        l_t_server->m_evr_loop->cancel_timer(l_data->m_timer_obj);

        int32_t l_status = STATUS_OK;

        // TODO multi-thread support
#if 0
        // ---------------------------------------
        // Check for out q notifications
        // ---------------------------------------
        //NDBG_PRINT("%sREADABLE%s l_nconn->get_idx(): 0x%08X\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, l_nconn->get_idx());
        if(l_nconn->get_idx() == t_server::sc_work_q_conn_idx)
        {
                // while dequeue out q notices...
                while(!l_t_server->m_out_q.empty())
                {
                        // call
                        nconn *l_nconn = l_t_server->m_out_q.front();
                        l_t_server->m_out_q.pop();
                        l_status = l_t_server->evr_loop_file_writeable_cb(l_nconn);
                        if(l_status != STATUS_OK)
                        {
                                NDBG_PRINT("Error: performing evr_loop_file_writeable_cb\n");
                                return STATUS_ERROR;
                        }
                }
                return STATUS_OK;
        }
#endif

        //NDBG_PRINT("%sREADABLE%s l_nconn->is_listening(): %d\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, l_nconn->is_listening());
        if(l_nconn->is_listening())
        {
                // Returns new client fd on success
                l_status = l_nconn->nc_run_state_machine(l_t_server->m_evr_loop, nconn::NC_MODE_NONE);
                // TODO Check status...
                //if((STATUS_ERROR == l_status) &&
                //   l_nconn->m_verbose)
                //{
                //        NDBG_PRINT("Error: performing run_state_machine\n");
                //        return STATUS_ERROR;
                //}
                // Get new connected client conn
                l_nconn = l_t_server->get_new_client_conn(l_status);
                if(!l_nconn)
                {
                        NDBG_PRINT("Error performing get_new_client_conn");
                        return STATUS_ERROR;
                }
        }
        else
        {
                do {
                        l_status = l_nconn->nc_run_state_machine(l_t_server->m_evr_loop, nconn::NC_MODE_READ);
                        if(l_status > 0)
                        {
                                l_t_server->m_stat.m_num_bytes_read += l_status;
                        }
                        if(l_status == nconn::NC_STATUS_EOF)
                        {
                                l_t_server->cleanup_connection(l_nconn, l_data->m_timer_obj);
                                return STATUS_OK;
                        }
                        else if(l_status == STATUS_ERROR)
                        {
                                l_t_server->cleanup_connection(l_nconn, l_data->m_timer_obj);
                                return STATUS_ERROR;
                        }

                        // TODO Check status???
                        //if((STATUS_ERROR == l_status) && l_nconn->m_verbose)
                        //{
                        //        NDBG_PRINT("Error: performing run_state_machine\n");
                        //        l_t_server->cleanup_connection(l_nconn, true, 500);
                        //}
                        //NDBG_PRINT("l_req->m_complete: %d\n", l_req->m_complete);
                        if(l_data->m_http_req.m_complete)
                        {
                                //NDBG_PRINT("g_req_num: %d\n", ++g_req_num);
                                ++l_t_server->m_stat.m_num_reqs;

                                // TODO multi-thread support
        #if 0
                                l_t_server->add_work(l_nconn, &(l_data->m_http_req));
        #endif
                                // -----------------------------------------------------
                                // main loop request handling...
                                // -----------------------------------------------------
                                int32_t l_req_status;
                                l_req_status = l_t_server->handle_req(l_nconn, &(l_data->m_http_req));
                                if(l_req_status != STATUS_OK)
                                {
                                        NDBG_PRINT("Error performing handle_req\n");
                                        l_t_server->cleanup_connection(l_nconn, l_data->m_timer_obj);
                                        return STATUS_ERROR;
                                }
                                l_data->m_http_req.clear();

                                // -----------------------------------------------------
                                // main loop request handling...
                                // -----------------------------------------------------
                                if(l_nconn->get_out_q() && l_nconn->get_out_q()->read_avail())
                                {
                                        // Try write if out q
                                        int32_t l_write_status;
                                        l_write_status = l_nconn->nc_run_state_machine(l_t_server->m_evr_loop, nconn::NC_MODE_WRITE);
                                        if(l_write_status > 0)
                                        {
                                                l_t_server->m_stat.m_num_bytes_written += l_write_status;
                                        }
                                        // TODO Check status???
                                        //if((l_write_status == STATUS_ERROR) && l_nconn->m_verbose)
                                        //{
                                        //        NDBG_PRINT("Error: performing run_state_machine\n");
                                        //        l_t_server->cleanup_connection(l_nconn, true);
                                        //}
                                }

                                if(l_status != nconn::NC_STATUS_EOF)
                                {
                                        //NDBG_PRINT("RESETTING.\n");
                                        l_status = l_t_server->reset_conn_input_q(l_data, l_nconn);
                                        if(l_status != STATUS_OK)
                                        {
                                                NDBG_PRINT("Error performing reset_conn_input_q\n");
                                                l_t_server->cleanup_connection(l_nconn, l_data->m_timer_obj);
                                                return STATUS_ERROR;
                                        }
                                }
                        }
                } while(l_status != nconn::NC_STATUS_AGAIN);
                l_t_server->m_evr_loop->add_timer(l_t_server->get_timeout_s()*1000, evr_loop_file_timeout_cb, l_nconn, &(l_data->m_timer_obj));
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_server::evr_loop_timer_completion_cb(void *a_data)
{
        NDBG_PRINT("%sTIMER_COMPLETION%s: %p\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, a_data);
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
        CHECK_FOR_NULL_ERROR(a_data);
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_data());
        http_data_t *l_data = static_cast<http_data_t *>(l_nconn->get_data());
        CHECK_FOR_NULL_ERROR(l_data->m_ctx);
        t_server *l_t_server = static_cast<t_server *>(l_data->m_ctx);

        if(l_nconn->is_free())
        {
                return STATUS_OK;
        }

        ++l_t_server->m_stat.m_num_idle_killed;

        if(l_t_server->m_settings.m_verbose)
        {
                NDBG_PRINT("%sTIMING OUT CONN%s: i_conn: %lu THIS: %p\n",
                                ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                                l_nconn->get_id(),
                                l_t_server);
        }
        l_t_server->cleanup_connection(l_nconn, l_data->m_timer_obj);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_server::evr_loop_timer_cb(void *a_data)
{
        NDBG_PRINT("%sTIMER%s %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, a_data);
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
int32_t t_server::evr_loop_file_error_cb(void *a_data)
{
        //NDBG_PRINT("%sERROR%s %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, a_data);
        CHECK_FOR_NULL_ERROR(a_data);
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_data());
        http_data_t *l_data = static_cast<http_data_t *>(l_nconn->get_data());
        CHECK_FOR_NULL_ERROR(l_data->m_ctx);
        t_server *l_t_server = static_cast<t_server *>(l_data->m_ctx);
        //if(l_nconn->is_free())
        //{
        //        return STATUS_OK;
        //}
        ++l_t_server->m_stat.m_num_errors;
        l_t_server->cleanup_connection(l_nconn, l_data->m_timer_obj);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_server::t_run(void *a_nothing)
{
        m_stopped = false;
        int32_t l_status;
        l_status = init();
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error performing init.\n");
                return NULL;
        }

        // Set start time
        m_start_time_s = get_time_s();

        // TODO Test -remove
        //uint64_t l_last_time_ms = get_time_ms();
        //uint64_t l_num_run = 0;

        // -------------------------------------------------
        // Run server
        // -------------------------------------------------
        while(!m_stopped)
        {
                l_status = m_evr_loop->run();

                // TODO Test -remove
                //++l_num_run;
                //if(get_time_ms() - l_last_time_ms > 500)
                //{
                //        NDBG_PRINT("Running: l_num_run: %lu\n", l_num_run);
                //        l_last_time_ms = get_time_ms();
                //}
        }
        //NDBG_PRINT("Stopped...\n");
        m_stopped = true;
        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_server::cleanup_connection(nconn *a_nconn, evr_timer_event_t *a_timer_obj)
{
        // Cancel last timer
        if(a_timer_obj)
        {
                m_evr_loop->cancel_timer(a_timer_obj);
        }

        nbq *l_out_q = a_nconn->get_out_q();
        if(l_out_q)
        {
                delete l_out_q;
                a_nconn->set_out_q(NULL);
        }
        nbq *l_in_q = a_nconn->get_in_q();
        if(l_in_q)
        {
                delete l_in_q;
                a_nconn->set_in_q(NULL);
        }

        //NDBG_PRINT("%sADDING_BACK%s: %u a_nconn: %p\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, (uint32_t)a_nconn->get_idx(), a_nconn);
        // Add back to free list
        if(STATUS_OK != m_nconn_pool.release(a_nconn))
        {
                return STATUS_ERROR;
        }

        --m_stat.m_cur_conn_count;
        ++m_stat.m_num_conn_completed;

        return STATUS_OK;
}

// TODO multi-thread support
#if 0
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_server::add_work(nconn *a_nconn, http_req *a_req)
{
        // -------------------------------------------------
        // TODO block (enqueue) new work on a connection
        // until all previous work completed.
        // -------------------------------------------------

        // Set resp and outq
        http_resp *l_resp = new http_resp();
        l_resp->set_q(a_nconn->get_out_q());

        work_t *l_work = new work_t();
        l_work->m_nconn = a_nconn;
        l_work->m_in_q = a_nconn->get_in_q();
        l_work->m_out_q = a_nconn->get_out_q();
        l_work->m_req = a_req;
        l_work->m_t_server = this;
        l_work->m_resp = l_resp;

        int32_t l_status;
        l_status = pthread_workqueue_additem_np(m_work_q, do_work, l_work, NULL, NULL);
        if (l_status != 0)
        {
                NDBG_PRINT("Error performing pthread_workqueue_additem_np. Reason: %s\n", strerror(l_status));
                return STATUS_ERROR;
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_server::do_work(void *a_work)
{
        //NDBG_PRINT("a_work:          %p\n", a_work);

        work_t *l_work = (work_t *)a_work;
        int32_t l_status = STATUS_OK;
        // Get path
        std::string l_path;

        //NDBG_PRINT("l_work->m_req:   %p\n", l_work->m_req);
        //NDBG_PRINT("l_work->m_resp:  %p\n", l_work->m_resp);
        //NDBG_PRINT("l_work->m_nconn: %p\n", l_work->m_nconn);

        http_req *l_req = l_work->m_req;
        http_resp *l_resp = l_work->m_resp;
        nconn *l_nconn = l_work->m_nconn;
        t_server *l_t_server = l_work->m_t_server;

        l_path.assign(l_req->m_p_url.m_ptr, l_req->m_p_url.m_len);
        //NDBG_PRINT("PATH: %s\n", l_path.c_str());
        //NDBG_PRINT("l_t_server->m_url_router: %p\n", l_t_server->m_url_router);
        http_request_handler *l_request_handler = NULL;
        url_param_map_t l_param_map;
        l_request_handler = (http_request_handler *)l_t_server->m_url_router->find_route(l_path,l_param_map);
        //NDBG_PRINT("l_request_handler: %p\n", l_request_handler);
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
        // How to signal error???
#if 0
        // Send Error response???
        if(l_status != STATUS_OK)
        {
                // TODO 500 response and cleanup
                l_t_server->cleanup_connection(l_nconn, true);
        }
#endif

        // Add to q
        l_t_server->m_out_q.push(l_nconn);

        // Signal work back to main thread...
        //NDBG_PRINT("%sSignalling back...%s\n", ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF);
        l_status = l_t_server->m_evr_loop->signal_event(l_t_server->m_out_q_fd);
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error performing m_evr_loop->signal_event(%d)\n", l_t_server->m_out_q_fd);
                goto do_work_done;
        }

do_work_done:
        delete l_work;
        return;
}
#endif

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_server::handle_req(nconn *a_nconn, http_req *a_req)
{
        int32_t l_status = STATUS_OK;
        std::string l_path;

        http_resp *l_resp = new http_resp();
        nbq *l_out_q = a_nconn->get_out_q();
        l_resp->set_q(l_out_q);
        l_out_q->reset_write();

        l_path.assign(a_req->m_p_url.m_ptr, a_req->m_p_url.m_len);
        //NDBG_PRINT("PATH: %s\n", l_path.c_str());
        //NDBG_PRINT("l_t_server->m_url_router: %p\n", m_url_router);
        http_request_handler *l_request_handler = NULL;
        url_param_map_t l_param_map;
        l_request_handler = (http_request_handler *)m_url_router->find_route(l_path,l_param_map);
        //NDBG_PRINT("l_request_handler: %p\n", l_request_handler);
        if(l_request_handler)
        {
                // Method switch
                switch(a_req->m_method)
                {
                case HTTP_GET:
                {
                        l_status = l_request_handler->do_get(l_param_map, *a_req, *l_resp);
                        if(l_status != STATUS_OK)
                        {
                                NDBG_PRINT("Error performing do_get\n");
                                // TODO...
                        }
                        break;
                }
                case HTTP_POST:
                {
                        l_status = l_request_handler->do_post(l_param_map, *a_req, *l_resp);
                        if(l_status != STATUS_OK)
                        {
                                NDBG_PRINT("Error performing do_post\n");
                                // TODO...
                        }
                        break;
                }
                case HTTP_PUT:
                {
                        l_status = l_request_handler->do_put(l_param_map, *a_req, *l_resp);
                        if(l_status != STATUS_OK)
                        {
                                NDBG_PRINT("Error performing do_put\n");
                                // TODO...
                        }
                        break;
                }
                case HTTP_DELETE:
                {
                        l_status = l_request_handler->do_delete(l_param_map, *a_req, *l_resp);
                        if(l_status != STATUS_OK)
                        {
                                NDBG_PRINT("Error performing do_delete\n");
                                // TODO...
                        }
                        break;
                }
                default:
                {
                        l_status = l_request_handler->do_default(l_param_map, *a_req, *l_resp);
                        if(l_status != STATUS_OK)
                        {
                                NDBG_PRINT("Error performing do_default\n");
                                // TODO...
                        }
                        break;
                }
                }
        }
        else
        {
                // Send default response
                //NDBG_PRINT("Default response\n");
                m_default_handler.do_get(l_param_map, *a_req, *l_resp);
        }

        if(l_resp)
        {
                delete l_resp;
                l_resp = NULL;
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_server::config_conn(nconn *a_nconn)
{
        a_nconn->set_num_reqs_per_conn(m_settings.m_num_reqs_per_conn);
        a_nconn->set_collect_stats(m_settings.m_collect_stats);
        a_nconn->set_connect_only(m_settings.m_connect_only);

        // -------------------------------------------
        // Set options
        // -------------------------------------------
        // Set generic options
        T_SERVER_SET_NCONN_OPT((*a_nconn), nconn_tcp::OPT_TCP_RECV_BUF_SIZE, NULL, m_settings.m_sock_opt_recv_buf_size);
        T_SERVER_SET_NCONN_OPT((*a_nconn), nconn_tcp::OPT_TCP_SEND_BUF_SIZE, NULL, m_settings.m_sock_opt_send_buf_size);
        T_SERVER_SET_NCONN_OPT((*a_nconn), nconn_tcp::OPT_TCP_NO_DELAY, NULL, m_settings.m_sock_opt_no_delay);

        // Set ssl options
        if(a_nconn->m_scheme == nconn::SCHEME_SSL)
        {
                T_SERVER_SET_NCONN_OPT((*a_nconn),
                                       nconn_ssl::OPT_SSL_CIPHER_STR,
                                       m_settings.m_ssl_cipher_list.c_str(),
                                       m_settings.m_ssl_cipher_list.length());

                T_SERVER_SET_NCONN_OPT((*a_nconn),
                                       nconn_ssl::OPT_SSL_CTX,
                                       m_settings.m_ssl_ctx,
                                       sizeof(m_settings.m_ssl_ctx));

                T_SERVER_SET_NCONN_OPT((*a_nconn),
                                       nconn_ssl::OPT_SSL_VERIFY,
                                       &(m_settings.m_ssl_verify),
                                       sizeof(m_settings.m_ssl_verify));

                //T_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_OPTIONS,
                //                              &(a_settings.m_ssl_options),
                //                              sizeof(a_settings.m_ssl_options));

                if(!m_settings.m_tls_crt.empty())
                {
                        T_SERVER_SET_NCONN_OPT((*a_nconn),
                                               nconn_ssl::OPT_SSL_TLS_CRT,
                                               m_settings.m_tls_crt.c_str(),
                                               m_settings.m_tls_crt.length());
                }
                if(!m_settings.m_tls_key.empty())
                {
                        T_SERVER_SET_NCONN_OPT((*a_nconn),
                                               nconn_ssl::OPT_SSL_TLS_KEY,
                                               m_settings.m_tls_key.c_str(),
                                               m_settings.m_tls_key.length());
                }
        }

        if(!a_nconn->get_data())
        {
                http_data_t *l_data = m_http_data_pool.get_free();
                if(l_data)
                {
                        l_data->m_http_resp.clear();
                        l_data->m_http_req.clear();
                }
                else
                {
                        l_data = new http_data_t();
                        m_http_data_pool.add(l_data);
                }
                //NDBG_PRINT("Adding http_data: %p.\n", l_data);
                a_nconn->set_data(l_data);
                l_data->m_verbose = m_settings.m_verbose;
                l_data->m_color = m_settings.m_color;
                l_data->m_nconn = a_nconn;
                l_data->m_ctx = this;
                l_data->m_http_rx = NULL;
                l_data->m_timer_obj = NULL;
                l_data->m_save = m_settings.m_save_response;
                l_data->m_type = HTTP_DATA_TYPE_SERVER;
                l_data->m_supports_keep_alives = false;
                l_data->m_status_code = 0;
                l_data->m_http_parser_settings.on_status = hp_on_status;
                l_data->m_http_parser_settings.on_message_complete = hp_on_message_complete;
                if(l_data->m_save)
                {
                        l_data->m_http_parser_settings.on_message_begin = hp_on_message_begin;
                        l_data->m_http_parser_settings.on_url = hp_on_url;
                        l_data->m_http_parser_settings.on_header_field = hp_on_header_field;
                        l_data->m_http_parser_settings.on_header_value = hp_on_header_value;
                        l_data->m_http_parser_settings.on_headers_complete = hp_on_headers_complete;
                        l_data->m_http_parser_settings.on_body = hp_on_body;
                }
                l_data->m_http_parser.data = l_data;
                http_parser_init(&(l_data->m_http_parser), HTTP_REQUEST);
                a_nconn->set_read_cb(http_parse);
        }
        else
        {
                http_data_t *l_data = static_cast<http_data_t *>(a_nconn->get_data());
                l_data->m_http_resp.clear();
                l_data->m_http_req.clear();
        }

        return STATUS_OK;
}

} //namespace ns_hlx {
