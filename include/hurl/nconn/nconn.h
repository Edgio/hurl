//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn.h
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
#ifndef _NCONN_H
#define _NCONN_H
//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hurl/nconn/scheme.h"
#include "hurl/nconn/conn_status.h"
#include "hurl/nconn/host_info.h"
#include "hurl/evr/evr.h"
// For memcpy -TODO move into impl file
#include <string.h>
#include <string>
//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define SET_NCONN_OPT(_conn, _opt, _buf, _len) do {\
                int _status = 0;\
                _status = _conn.set_opt((_opt), (_buf), (_len));\
                if (_status != STATUS_OK) {\
                        NDBG_PRINT("STATUS_ERROR: Failed to set_opt %d.  Status: %d.\n", _opt, _status); \
                        return STATUS_ERROR;\
                }\
        } while(0)
#define NCONN_ERROR(status, ...) do {\
                  char _buf[1024];\
                  snprintf(_buf, sizeof(_buf), __VA_ARGS__);\
                  m_last_error.assign(_buf);\
                  m_conn_status = status;\
                  TRC_ERROR(__VA_ARGS__);\
          } while(0)
namespace ns_hurl {
//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class nbq;
struct host_info;
//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
// conn stat
typedef struct conn_stat_struct
{
        uint32_t m_body_bytes;
        uint32_t m_total_bytes;
        uint64_t m_tt_connect_us;
        uint64_t m_tt_first_read_us;
        uint64_t m_tt_completion_us;
        int32_t m_last_state;
        int32_t m_error;
} conn_stat_t;
void conn_stat_init(conn_stat_t &a_stat);
//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class nconn
{
public:
        // -------------------------------------------------
        // connection status
        // -------------------------------------------------
        typedef enum status_enum {
                NC_STATUS_FREE = -1,
                NC_STATUS_OK = -2,
                NC_STATUS_AGAIN = -3,
                NC_STATUS_ERROR = -4,
                NC_STATUS_UNSUPPORTED = -5,
                NC_STATUS_EOF = -6,
                NC_STATUS_BREAK = -7,
                NC_STATUS_IDLE = -8,
                NC_STATUS_NONE = -9
        } status_t;
        // -------------------------------------------------
        // Successful read/write callbacks
        // -------------------------------------------------
        typedef int32_t (*nconn_cb_t)(void *, char *, uint32_t, uint64_t);
        typedef int32_t (*nconn_data_cb_t)(nconn *, void *);
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        nconn(void);
        virtual ~nconn();
        // -------------------------------------------------
        // ctx
        // -------------------------------------------------
        void set_ctx(void * a_data) {m_ctx = a_data;}
        void *get_ctx(void) {return m_ctx;}
        // -------------------------------------------------
        // Data
        // -------------------------------------------------
        void set_data(void * a_data) {m_data = a_data;}
        void *get_data(void) {return m_data;}
        // -------------------------------------------------
        // evr
        // -------------------------------------------------
        void set_evr_loop(evr_loop * a_evr_loop) {m_evr_loop = a_evr_loop;}
        evr_loop *get_evr_loop(void) {return m_evr_loop;}
        // -------------------------------------------------
        // Stats
        // -------------------------------------------------
        void reset_stats(void) { conn_stat_init(m_stat); }
        const conn_stat_t &get_stats(void) const { return m_stat;}
        // -------------------------------------------------
        // Getters
        // -------------------------------------------------
        uint64_t get_id(void) {return m_id;}
        uint32_t get_idx(void) {return m_idx;}
        uint32_t get_pool_id(void) {return m_pool_id;}
        const std::string &get_label(void) {return m_label;}
        scheme_t get_scheme(void) {return m_scheme;}
        bool get_collect_stats_flag(void) {return m_collect_stats_flag;}
        uint64_t get_request_start_time_us(void) {return m_request_start_time_us;}
        uint64_t get_stat_tt_connect_us(void) {return m_stat.m_tt_connect_us;}
        uint64_t get_connect_start_time_us(void) {return m_connect_start_time_us;}
        bool get_connect_only(void) { return m_connect_only;}
        const std::string &get_last_error(void) { return m_last_error;}
        conn_status_t get_status(void) { return m_conn_status;}
        host_info get_host_info(void) {return m_host_info;}
        bool get_host_info_is_set(void) {return m_host_info_is_set;}
        void get_remote_sa(sockaddr_storage &ao_sa, socklen_t &ao_sa_len)
        {
                memcpy(&ao_sa, &m_remote_sa, m_remote_sa_len);
                ao_sa_len = m_remote_sa_len;
        };
        // -------------------------------------------------
        // Setters
        // -------------------------------------------------
        void set_label(const std::string &a_label) {m_label = a_label;}
        void set_id(uint64_t a_id) {m_id = a_id;}
        void set_idx(uint32_t a_id) {m_idx = a_id;}
        void set_pool_id(uint32_t a_id) {m_pool_id = a_id;}
        void set_host_info(const host_info &a_host_info) {m_host_info = a_host_info; m_host_info_is_set = true;}
        void set_num_reqs_per_conn(int64_t a_n) {m_num_reqs_per_conn = a_n;}
        void set_collect_stats(bool a_flag) {m_collect_stats_flag = a_flag;}
        void set_connect_only(bool a_flag) {m_connect_only = a_flag;}
        void set_connected_cb(nconn_data_cb_t a_cb) {m_connected_cb = a_cb;}
        void set_read_cb(nconn_cb_t a_cb) {m_read_cb = a_cb;}
        void set_read_cb_data(void * a_data) {m_read_cb_data = a_data;}
        void set_write_cb(nconn_cb_t a_cb) {m_write_cb = a_cb;}
        void set_collect_stats_flag(bool a_val) {m_collect_stats_flag = a_val;}
        void set_request_start_time_us(uint64_t a_val) {m_request_start_time_us = a_val;}
        void set_stat_tt_completion_us(uint64_t a_val){ m_stat.m_tt_completion_us = a_val;}
        void set_stat_tt_connect_us(uint64_t a_val){ m_stat.m_tt_connect_us = a_val;}
        void set_connect_start_time_us(uint64_t a_val) {m_connect_start_time_us = a_val;}
        void set_status(conn_status_t a_status) { m_conn_status = a_status;}
        void setup_evr_fd(evr_event_cb_t a_read_cb,
                          evr_event_cb_t a_write_cb,
                          evr_event_cb_t a_error_cb)
        {
                m_evr_fd.m_magic = EVR_EVENT_FD_MAGIC;
                m_evr_fd.m_read_cb = a_read_cb;
                m_evr_fd.m_write_cb = a_write_cb;
                m_evr_fd.m_error_cb = a_error_cb;
                m_evr_fd.m_data = this;
        }
        // -------------------------------------------------
        // State
        // -------------------------------------------------
        bool is_free(void) { return (m_nc_state == NC_STATE_FREE);}
        bool is_done(void) { return (m_nc_state == NC_STATE_DONE);}
        void set_state_done(void) { m_nc_state = NC_STATE_DONE; }
        void bump_num_requested(void) {++m_num_reqs;}
        bool can_reuse(void);
        // -------------------------------------------------
        // Running
        // -------------------------------------------------
        int32_t nc_run_state_machine(evr_mode_t a_mode,
                                     nbq *a_in_q,
                                     uint32_t &ao_read,
                                     nbq *a_out_q,
                                     uint32_t &ao_written);
        int32_t nc_read(nbq *a_in_q, uint32_t &ao_read);
        int32_t nc_write(nbq *a_out_q, uint32_t &ao_written);
        int32_t nc_set_listening(int32_t a_val);
        int32_t nc_set_listening_nb(int32_t a_val);
        int32_t nc_set_accepting(int a_fd);
        int32_t nc_set_connected(void);
        int32_t nc_cleanup();
        // -------------------------------------------------
        // Virtual Methods
        // -------------------------------------------------
        virtual int32_t set_opt(uint32_t a_opt, const void *a_buf, uint32_t a_len) = 0;
        virtual int32_t get_opt(uint32_t a_opt, void **a_buf, uint32_t *a_len) = 0;
        virtual bool is_listening(void) = 0;
        virtual bool is_connecting(void) = 0;
        virtual bool is_accepting(void) = 0;

protected:
        // -------------------------------------------------
        // Protected Virtual methods
        // -------------------------------------------------
        virtual int32_t ncsetup() = 0;
        virtual int32_t ncread(char *a_buf, uint32_t a_buf_len) = 0;
        virtual int32_t ncwrite(char *a_buf, uint32_t a_buf_len) = 0;
        virtual int32_t ncaccept() = 0;
        virtual int32_t ncconnect() = 0;
        virtual int32_t nccleanup() = 0;
        virtual int32_t ncset_listening(int32_t a_val) = 0;
        virtual int32_t ncset_listening_nb(int32_t a_val) = 0;
        virtual int32_t ncset_accepting(int a_fd) = 0;
        virtual int32_t ncset_connected(void) = 0;
        // -------------------------------------------------
        // Protected members
        // -------------------------------------------------
        evr_loop *m_evr_loop;
        evr_fd_t m_evr_fd;
        scheme_t m_scheme;
        std::string m_label;
        conn_stat_t m_stat;
        bool m_collect_stats_flag;
        void *m_ctx;
        void *m_data;
        uint64_t m_connect_start_time_us;
        uint64_t m_request_start_time_us;
        conn_status_t m_conn_status;
        std::string m_last_error;
        host_info m_host_info;
        bool m_host_info_is_set;
        int64_t m_num_reqs_per_conn;
        int64_t m_num_reqs;
        bool m_connect_only;
        sockaddr_storage m_remote_sa;
        socklen_t m_remote_sa_len;
private:
        // ---------------------------------------
        // Connection state
        // ---------------------------------------
        typedef enum nc_conn_state
        {
                NC_STATE_FREE = 0,
                NC_STATE_LISTENING,
                NC_STATE_ACCEPTING,
                NC_STATE_CONNECTING,
                NC_STATE_CONNECTED,
                NC_STATE_DONE
        } nc_conn_state_t;
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        nconn& operator=(const nconn &);
        nconn(const nconn &);
        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        nc_conn_state_t m_nc_state;
        uint64_t m_id;
        uint32_t m_idx;
        uint32_t m_pool_id;
        nconn_data_cb_t m_connected_cb;
        nconn_cb_t m_read_cb;
        void *m_read_cb_data;
        nconn_cb_t m_write_cb;
};
} //namespace ns_hurl {
#endif
