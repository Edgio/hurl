//! ----------------------------------------------------------------------------
//! Copyright Edgio Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include "status.h"
#include "support/ndebug.h"
#include "support/time_util.h"
#include "support/trace.h"
#include "support/nbq.h"
#include "nconn/nconn.h"
#include "nconn/conn_status.h"
#include <errno.h>
#include <string.h>
#include <strings.h>
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
int32_t nconn::nc_read(nbq *a_in_q, char **ao_buf, uint32_t &ao_read)
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
        if((a_in_q->get_max_read_queue() > 0) &&
           ((a_in_q->read_avail() + a_in_q->b_write_avail()) > (uint64_t)a_in_q->get_max_read_queue()))
        {
                l_read_size = a_in_q->get_max_read_queue() - a_in_q->read_avail();
        }
        else
        {
                l_read_size = a_in_q->b_write_avail();
        }
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
                return l_s;
        }
        else if(l_s == 0)
        {
                //???
                return l_s;
        }
        ao_read += l_s;
        //ns_hurl::mem_display((uint8_t *)(l_buf), l_bytes_read);
        *ao_buf = l_buf;
        a_in_q->b_write_incr(l_s);
        return NC_STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
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
                return l_s;
        }
        else if(l_s == 0)
        {
                //???
                return NC_STATUS_OK;
        }
        ao_written += l_s;
        // and not error?
        a_out_q->b_read_incr(l_s);
        // no shrink???
        //a_out_q->shrink();
        return NC_STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
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
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
int32_t nconn::nc_set_listening(int32_t a_val)
{
        //NDBG_PRINT("%sRUN_STATE_MACHINE%s: SET_LISTENING[%d]\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, a_val);
        int32_t l_s;
        l_s = ncset_listening(a_val);
        if(l_s != NC_STATUS_OK)
        {
                return STATUS_ERROR;
        }
        m_nc_state = NC_STATE_LISTENING;
        return STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
int32_t nconn::nc_set_listening_nb(int32_t a_val)
{
        //NDBG_PRINT("%sRUN_STATE_MACHINE%s: SET_LISTENING[%d]\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, a_val);
        int32_t l_s;
        l_s = ncset_listening_nb(a_val);
        if(l_s != NC_STATUS_OK)
        {
                return STATUS_ERROR;
        }
        m_nc_state = NC_STATE_LISTENING;
        return STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
int32_t nconn::nc_set_accepting(int a_fd)
{
        int32_t l_s;
        l_s = ncset_accepting(a_fd);
        if(l_s != NC_STATUS_OK)
        {
                return STATUS_ERROR;
        }
        m_nc_state = NC_STATE_ACCEPTING;
        return STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
int32_t nconn::nc_set_connected(void)
{
        int32_t l_s;
        l_s = ncset_connected();
        if(l_s != NC_STATUS_OK)
        {
                return STATUS_ERROR;
        }
        m_nc_state = NC_STATE_CONNECTED;
        return STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
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
                return STATUS_ERROR;
        }
        m_data = NULL;
        m_host_info_is_set = false;
        return STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
nconn::nconn(void):
      m_evr_loop(NULL),
      m_evr_fd(),
      m_scheme(SCHEME_NONE),
      m_label(),
#if 0
      m_stat(),
      m_collect_stats_flag(false),
#endif
      m_ctx(NULL),
      m_data(NULL),
#if 0
      m_connect_start_time_us(0),
      m_request_start_time_us(0),
#endif
      m_conn_status(CONN_STATUS_OK),
      m_last_error(""),
      m_host_data(NULL),
      m_host_info(),
      m_host_info_is_set(false),
      m_num_reqs_per_conn(-1),
      m_num_reqs(0),
#if 0
      m_connect_only(false),
#endif
      m_remote_sa(),
      m_remote_sa_len(0),
      m_nc_state(NC_STATE_FREE),
      m_id(0),
      m_idx(0),
      m_pool_id(0),
      m_alpn(ALPN_HTTP_VER_V1_1),
      m_alpn_buf(NULL),
      m_alpn_buf_len(0),
      m_timer_obj(NULL)
{
#if 0
        // Set stats
        if(m_collect_stats_flag)
        {
                conn_stat_init(m_stat);
        }
#endif
        //NDBG_PRINT("%s--CONN--%s last_state: %d this: %p\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, m_nc_state, this);
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
nconn::~nconn(void)
{
        //NDBG_PRINT("%s--CONN--%s last_state: %d this: %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, m_nc_state, this);
        if(m_alpn_buf)
        {
                free(m_alpn_buf);
                m_alpn_buf = NULL;
                m_alpn_buf_len = 0;
        }
}
//! ----------------------------------------------------------------------------
//! nconn_utils
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
conn_status_t nconn_get_status(nconn &a_nconn)
{
        return a_nconn.get_status();
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
const std::string &nconn_get_last_error_str(nconn &a_nconn)
{
        return a_nconn.get_last_error();
}
#if 0
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
void conn_stat_init(conn_stat_t &a_stat)
{
        bzero(&a_stat, sizeof(conn_stat_t));
}
#endif
} // ns_hurl
