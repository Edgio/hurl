//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn_tcp.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    02/07/2014
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
#ifndef _NCONN_TCP_H
#define _NCONN_TCP_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "nconn.h"
#include "ndebug.h"

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class nconn_tcp: public nconn
{
public:
        // ---------------------------------------
        // Options
        // ---------------------------------------
        typedef enum tcp_opt_enum
        {
                OPT_TCP_RECV_BUF_SIZE = 0,
                OPT_TCP_SEND_BUF_SIZE = 1,
                OPT_TCP_NO_DELAY = 2,

                OPT_TCP_SENTINEL = 999
        } tcp_opt_t;

        nconn_tcp(bool a_verbose,
                  bool a_color,
                  int64_t a_max_reqs_per_conn = -1,
                  bool a_save_response_in_reqlet = false,
                  bool a_collect_stats = false,
                  bool a_connect_only = false,
                  type_t a_type = TYPE_CLIENT):
                          nconn(a_verbose,
                                a_color,
                                a_max_reqs_per_conn,
                                a_save_response_in_reqlet,
                                a_collect_stats,
                                a_connect_only,
                                a_type),
                          m_tcp_state(TCP_STATE_FREE),
                          m_fd(-1),
                          m_sock_opt_recv_buf_size(),
                          m_sock_opt_send_buf_size(),
                          m_sock_opt_no_delay(false),
                          m_timeout_s(10)

        {
                m_scheme = SCHEME_TCP;
        };

        // Destructor
        ~nconn_tcp()
        {
        };


        // TODO REMOVE!!!
#if 0
        int32_t run_state_machine(evr_loop *a_evr_loop, const host_info_t &a_host_info);
        int32_t send_request(bool is_reuse = false);
#endif

        int32_t set_opt(uint32_t a_opt, const void *a_buf, uint32_t a_len);
        int32_t get_opt(uint32_t a_opt, void **a_buf, uint32_t *a_len);
        bool is_listening(void) {return (m_tcp_state == TCP_STATE_LISTENING);};
        bool is_connecting(void) {return (m_tcp_state == TCP_STATE_CONNECTING);};
        bool is_free(void) { return (m_tcp_state == TCP_STATE_FREE);}

        // TODO Experimental refactoring
        int32_t ncsetup(evr_loop *a_evr_loop);
        int32_t ncread(char *a_buf, uint32_t a_buf_len);
        int32_t ncwrite(char *a_buf, uint32_t a_buf_len);
        int32_t ncaccept(void);
        int32_t ncconnect(evr_loop *a_evr_loop);
        int32_t nccleanup(void);

private:

        // ---------------------------------------
        // Connection state
        // ---------------------------------------
        typedef enum tcp_conn_state
        {
                TCP_STATE_FREE = 0,
                TCP_STATE_LISTENING,
                TCP_STATE_CONNECTING,
                TCP_STATE_CONNECTED,
                TCP_STATE_READING,
                TCP_STATE_WRITING,
                TCP_STATE_DONE
        } tcp_conn_state_t;

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(nconn_tcp)

        // TODO REMOVE!!!
#if 0
        int32_t receive_response(void);
#endif

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        tcp_conn_state_t m_tcp_state;

protected:
        // -------------------------------------------------
        // Protected methods
        // -------------------------------------------------
        int32_t set_listening(evr_loop *a_evr_loop, int32_t a_val);
        int32_t set_connected(evr_loop *a_evr_loop, int a_fd);

        // -------------------------------------------------
        // Protected members
        // -------------------------------------------------
        int m_fd;

        // Socket options
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;
        uint32_t m_timeout_s;

};

} //namespace ns_hlx {

#endif



