//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn.cc
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
//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "ndebug.h"
#include "hurl/nconn/nconn.h"
#include "hurl/status.h"
#include "hurl/support/time_util.h"
#include "hurl/support/trace.h"
#include "hurl/support/nbq.h"
#include "hurl/nconn/conn_status.h"
#include <errno.h>
#include <string.h>
#include <strings.h>
namespace ns_hurl {
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn::nc_run_state_machine(evr_mode_t a_mode,
                                    nbq *a_in_q,
                                    uint32_t &ao_read,
                                    nbq *a_out_q,
                                    uint32_t &ao_written)
{
        //NDBG_PRINT("%sRUN_STATE_MACHINE%s: CONN[%p] STATE[%d] MODE: %d --START\n",
        //                ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF, this, m_nc_state, a_mode);
state_top:
        //NDBG_PRINT("%sRUN_STATE_MACHINE%s: CONN[%p] STATE[%d] MODE: %d\n",
        //                ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF, this, m_nc_state, a_mode);
        switch (m_nc_state)
        {
        // -------------------------------------------------
        // STATE: FREE
        // -------------------------------------------------
        case NC_STATE_FREE:
        {
                int32_t l_s;
                l_s = ncsetup();
                if(l_s != NC_STATUS_OK)
                {
                        //NDBG_PRINT("Error performing ncsetup\n");
                        return NC_STATUS_ERROR;
                }

                // TODO -check for errors
                m_nc_state = NC_STATE_CONNECTING;

                // Stats
                if(m_collect_stats_flag)
                {
                        m_connect_start_time_us = get_time_us();
                }
                goto state_top;
        }
        // -------------------------------------------------
        // STATE: LISTENING
        // -------------------------------------------------
        case NC_STATE_LISTENING:
        {
                int32_t l_s;
                l_s = ncaccept();
                if(l_s < 0)
                {
                        //NDBG_PRINT("Error performing ncaccept\n");
                        return NC_STATUS_ERROR;
                }
                //NDBG_PRINT("%sRUN_STATE_MACHINE%s: ACCEPT[%d]\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, l_s);
                // Returning client fd
                return l_s;
        }
        // -------------------------------------------------
        // STATE: CONNECTING
        // -------------------------------------------------
        case NC_STATE_CONNECTING:
        {
                int32_t l_s;
                //NDBG_PRINT("%sConnecting%s: host: %s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, m_label.c_str());
                l_s = ncconnect();
                if(l_s == NC_STATUS_ERROR)
                {
                        //NDBG_PRINT("Error performing ncconnect for host: %s.\n", m_label.c_str());
                        return NC_STATUS_ERROR;
                }
                if(is_connecting())
                {
                        //NDBG_PRINT("Still connecting...\n");
                        return NC_STATUS_AGAIN;
                }
                //NDBG_PRINT("%sConnected%s: label: %s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, m_label.c_str());
                TRC_DEBUG("Connected: label: %s\n", m_label.c_str());
                // Returning client fd
                // If OK -change state to connected???
                m_nc_state = NC_STATE_CONNECTED;
                if(m_connected_cb)
                {
                        l_s = m_connected_cb(this, m_data);
                        if(l_s != HURL_STATUS_OK)
                        {
                                //NDBG_PRINT("LABEL[%s]: Error performing m_read_cb\n", m_label.c_str());
                                return NC_STATUS_ERROR;
                        }
                }
                if(m_collect_stats_flag)
                {
                        m_stat.m_tt_connect_us = get_delta_time_us(m_connect_start_time_us);
                }
                if(m_connect_only)
                {
                        m_nc_state = NC_STATE_DONE;
                        return NC_STATUS_EOF;
                }
                if(a_out_q->read_avail())
                {
                        a_mode = EVR_MODE_WRITE;
                }
                goto state_top;
        }
        // -------------------------------------------------
        // STATE: ACCEPTING
        // -------------------------------------------------
        case NC_STATE_ACCEPTING:
        {
                int32_t l_s;
                l_s = ncaccept();
                if(l_s == NC_STATUS_ERROR)
                {
                        //NDBG_PRINT("Error performing ncaccept\n");
                        return NC_STATUS_ERROR;
                }
                if(is_accepting())
                {
                        //NDBG_PRINT("Still connecting...\n");
                        return NC_STATUS_AGAIN;
                }
                m_nc_state = NC_STATE_CONNECTED;
                goto state_top;
        }
        // -------------------------------------------------
        // STATE: CONNECTED
        // -------------------------------------------------
        case NC_STATE_CONNECTED:
        {
                switch(a_mode)
                {
                case EVR_MODE_READ:
                {
                        int32_t l_s = NC_STATUS_OK;
                        l_s = nc_read(a_in_q, ao_read);
                        if(m_collect_stats_flag && (ao_read > 0))
                        {
                                m_stat.m_total_bytes += ao_read;
                                if(m_stat.m_tt_first_read_us == 0)
                                {
                                        m_stat.m_tt_first_read_us = get_delta_time_us(m_request_start_time_us);
                                }
                        }
                        //NDBG_PRINT("l_s: %d\n", l_s);
                        switch(l_s){
                        case NC_STATUS_EOF:
                        {
                                m_nc_state = NC_STATE_DONE;
                        }
                        case NC_STATUS_ERROR:
                        case NC_STATUS_AGAIN:
                        case NC_STATUS_OK:
                        {
                                return l_s;
                        }
                        case NC_STATUS_READ_UNAVAILABLE:
                        {
                                return l_s;
                        }
                        default:
                        {
                                break;
                        }
                        }
                        return l_s;
                }
                case EVR_MODE_WRITE:
                {
                        int32_t l_s = NC_STATUS_OK;
                        l_s = nc_write(a_out_q, ao_written);
                        switch(l_s){
                        case NC_STATUS_EOF:
                        {
                                m_nc_state = NC_STATE_DONE;
                        }
                        case NC_STATUS_ERROR:
                        case NC_STATUS_AGAIN:
                        case NC_STATUS_OK:
                        {
                                return l_s;
                        }

                        default:
                        {
                                break;
                        }
                        }
                        return l_s;
                }
                default:
                {
                        return NC_STATUS_ERROR;
                }
                }
                return NC_STATUS_ERROR;
        }
        // -------------------------------------------------
        // STATE: DONE
        // -------------------------------------------------
        case NC_STATE_DONE:
        {
                //NDBG_PRINT("return EOF\n");
                return NC_STATUS_EOF;
        }
        // -------------------------------------------------
        // default
        // -------------------------------------------------
        default:
        {
                return NC_STATUS_ERROR;
        }
        }
        return NC_STATUS_ERROR;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn::nc_read(nbq *a_in_q, uint32_t &ao_read)
{
        //NDBG_PRINT("%sTRY_READ%s: a_in_q: %p\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, a_in_q);
        ao_read = 0;
        if(!a_in_q)
        {
                TRC_ERROR("a_in_q == NULL\n");
                return NC_STATUS_ERROR;
        }
        // -------------------------------------------------
        // while connection is readable...
        //   read up to next read size
        //   if size read == read_q free size
        //     add block to queue
        // -------------------------------------------------
        int32_t l_s = 0;
        uint32_t l_read_size = 0;
        do {
                if(a_in_q->read_avail_is_max_limit())
                {
                        return NC_STATUS_READ_UNAVAILABLE;
                }
                if(a_in_q->b_write_avail() <= 0)
                {
                        int32_t l_s = a_in_q->b_write_add_avail();
                        if(l_s <= 0)
                        {
                                //NDBG_PRINT("Error performing b_write_add_avail\n");
                                return NC_STATUS_ERROR;
                        }
                }
                l_read_size = a_in_q->b_write_avail();
                //NDBG_PRINT("%sTRY_READ%s: l_read_size: %d\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, l_read_size);
                char *l_buf = a_in_q->b_write_ptr();
                //NDBG_PRINT("%sTRY_READ%s: m_out_q->read_ptr(): %p l_read_size: %d\n",
                //                ANSI_COLOR_FG_RED, ANSI_COLOR_OFF,
                //                l_buf,
                //                l_read_size);
                l_s = ncread(l_buf, l_read_size);
                //NDBG_PRINT("%sTRY_READ%s: l_bytes_read: %d\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_s);
                //NDBG_PRINT("%sTRY_READ%s: l_bytes_read: %d old_size: %d-error:%d: %s\n",
                //                ANSI_COLOR_FG_RED, ANSI_COLOR_OFF,
                //                l_s,
                //                (int)a_in_q->read_avail(),
                //                errno, strerror(errno));
                if(l_s < 0)
                {
                        switch(l_s)
                        {
                        case NC_STATUS_ERROR:
                        case NC_STATUS_AGAIN:
                        case NC_STATUS_OK:
                        case NC_STATUS_EOF:
                        {
                                return l_s;
                        }
                        default:
                        {
                                break;
                        }
                        }
                        //???
                        continue;
                }
                else if(l_s == 0)
                {
                        //???
                        continue;
                }
                ao_read += l_s;
                //ns_hurl::mem_display((uint8_t *)(l_buf), l_bytes_read);
                if(m_read_cb)
                {
                        int32_t l_rcb_status = m_read_cb(m_read_cb_data, l_buf, l_s, a_in_q->get_cur_write_offset());
                        if(l_rcb_status != HURL_STATUS_OK)
                        {
                                TRC_ERROR("LABEL[%s]: performing m_read_cb\n", m_label.c_str());
                                return NC_STATUS_ERROR;
                        }
                }
                a_in_q->b_write_incr(l_s);
        // Read as much as can...
        } while((l_s > 0) &&
                ((uint32_t)l_s <= l_read_size));
        return NC_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn::nc_write(nbq *a_out_q, uint32_t &ao_written)
{
        //NDBG_PRINT("%sTRY_WRITE%s: m_out_q: %p\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, a_out_q);
        ao_written = 0;
        if(!a_out_q)
        {
                TRC_ERROR("a_out_q == NULL\n");
                return NC_STATUS_ERROR;
        }
        if(!a_out_q->read_avail())
        {
                //TRC_ERROR("Error a_out_q->read_avail() == 0\n");
                return NC_STATUS_OK;
        }
        // -------------------------------------------------
        // while connection is writeable...
        //   wrtie up to next write size
        //   if size write == write_q free size
        //     add
        // -------------------------------------------------
        int32_t l_s;
        do {
                //NDBG_PRINT("%sTRY_WRITE%s: m_out_q->b_read_ptr(): %p m_out_q->b_read_avail(): %d\n",
                //                ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF,
                //                a_out_q->b_read_ptr(),
                //                a_out_q->b_read_avail());
                l_s = ncwrite(a_out_q->b_read_ptr(), a_out_q->b_read_avail());
                //NDBG_PRINT("%sTRY_WRITE%s: l_bytes_written: %d\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, l_s);
                if(l_s < 0)
                {
                        switch(l_s)
                        {
                        case NC_STATUS_AGAIN:
                        {
                                return l_s;
                        }
                        default:
                        {
                                //NDBG_PRINT("Error performing ncwrite: status: %d\n", l_bytes_written);
                                return NC_STATUS_ERROR;
                        }
                        }
                        //???
                        continue;
                }
                else if(l_s == 0)
                {
                        //???
                        return NC_STATUS_OK;
                }
                ao_written += l_s;
                if(m_write_cb)
                {
                        int32_t l_wcb_status = m_write_cb(m_data, a_out_q->b_read_ptr(), l_s, 0);
                        if(l_wcb_status != HURL_STATUS_OK)
                        {
                                TRC_ERROR("Error performing m_write_cb\n");
                                return NC_STATUS_ERROR;
                        }
                }
                // and not error?
                a_out_q->b_read_incr(l_s);
                a_out_q->shrink();
        } while((l_s > 0) &&
                a_out_q->read_avail());
        return NC_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool nconn::can_reuse(void)
{
        //NDBG_PRINT("CONN ka num %ld / %ld \n", m_num_reqs, m_num_reqs_per_conn);
        if(((m_num_reqs_per_conn == -1) ||
            (m_num_reqs < m_num_reqs_per_conn)))
        {
                return true;
        }
        else
        {
                //NDBG_PRINT("CONN m_num_reqs: %ld m_num_reqs_per_conn: %ld \n",
                //                m_num_reqs,
                //                m_num_reqs_per_conn);
                return false;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn::nc_set_listening(int32_t a_val)
{
        //NDBG_PRINT("%sRUN_STATE_MACHINE%s: SET_LISTENING[%d]\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, a_val);
        int32_t l_s;
        l_s = ncset_listening(a_val);
        if(l_s != NC_STATUS_OK)
        {
                return HURL_STATUS_ERROR;
        }

        m_nc_state = NC_STATE_LISTENING;
        return HURL_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn::nc_set_listening_nb(int32_t a_val)
{
        //NDBG_PRINT("%sRUN_STATE_MACHINE%s: SET_LISTENING[%d]\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, a_val);
        int32_t l_s;
        l_s = ncset_listening_nb(a_val);
        if(l_s != NC_STATUS_OK)
        {
                return HURL_STATUS_ERROR;
        }

        m_nc_state = NC_STATE_LISTENING;
        return HURL_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn::nc_set_accepting(int a_fd)
{
        int32_t l_s;
        l_s = ncset_accepting(a_fd);
        if(l_s != NC_STATUS_OK)
        {
                return HURL_STATUS_ERROR;
        }
        m_nc_state = NC_STATE_ACCEPTING;
        return HURL_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn::nc_set_connected(void)
{
        int32_t l_s;
        l_s = ncset_connected();
        if(l_s != NC_STATUS_OK)
        {
                return HURL_STATUS_ERROR;
        }
        m_nc_state = NC_STATE_CONNECTED;
        return HURL_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn::nc_cleanup()
{
        //NDBG_PRINT("%s--CONN--%s[%s] last_state: %d this: %p\n",
        //                ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
        //                m_label.c_str(),
        //                m_nc_state,
        //                this);
        //NDBG_PRINT_BT();
        TRC_VERBOSE("tearing down: label: %s\n", m_label.c_str());
        int32_t l_s;
        l_s = nccleanup();
        m_nc_state = NC_STATE_FREE;
        m_num_reqs = 0;
        if(l_s != NC_STATUS_OK)
        {
                TRC_ERROR("Error performing nccleanup.\n");
                return HURL_STATUS_ERROR;
        }
        m_data = NULL;
        m_host_info_is_set = false;
        return HURL_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn::nconn(void):
      m_evr_loop(NULL),
      m_evr_fd(),
      m_scheme(SCHEME_NONE),
      m_label(),
      m_stat(),
      m_collect_stats_flag(false),
      m_ctx(NULL),
      m_data(NULL),
      m_connect_start_time_us(0),
      m_request_start_time_us(0),
      m_conn_status(CONN_STATUS_OK),
      m_last_error(""),
      m_host_info(),
      m_host_info_is_set(false),
      m_num_reqs_per_conn(-1),
      m_num_reqs(0),
      m_connect_only(false),
      m_remote_sa(),
      m_remote_sa_len(0),
      m_nc_state(NC_STATE_FREE),
      m_id(0),
      m_idx(0),
      m_pool_id(0),
      m_connected_cb(NULL),
      m_read_cb(NULL),
      m_read_cb_data(NULL),
      m_write_cb(NULL)
{
        // Set stats
        if(m_collect_stats_flag)
        {
                conn_stat_init(m_stat);
        }
        //NDBG_PRINT("%s--CONN--%s last_state: %d this: %p\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, m_nc_state, this);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn::~nconn(void)
{
        //NDBG_PRINT("%s--CONN--%s last_state: %d this: %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, m_nc_state, this);
}

//: ----------------------------------------------------------------------------
//: nconn_utils
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
conn_status_t nconn_get_status(nconn &a_nconn)
{
        return a_nconn.get_status();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &nconn_get_last_error_str(nconn &a_nconn)
{
        return a_nconn.get_last_error();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void conn_stat_init(conn_stat_t &a_stat)
{
        bzero(&a_stat, sizeof(conn_stat_t));
}

} // ns_hurl
