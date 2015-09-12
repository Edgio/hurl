//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
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
#include "ndebug.h"
#include "host_info.h"
#include "req_stat.h"
#include "nbq.h"

#include "http/http.h"
#include "http_parser/http_parser.h"

#include <string>

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define SET_NCONN_OPT(_conn, _opt, _buf, _len) \
do { \
        int _status = 0; \
        _status = _conn.set_opt((_opt), (_buf), (_len)); \
        if (_status != STATUS_OK) { \
                NDBG_PRINT("STATUS_ERROR: Failed to set_opt %d.  Status: %d.\n", _opt, _status); \
                return STATUS_ERROR;\
        } \
} while(0)

#define GET_NCONN_OPT(_conn, _opt, _buf, _len) \
do { \
        int _status = 0; \
        _status = _conn.get_opt((_opt), (_buf), (_len)); \
        if (_status != STATUS_OK) { \
                NDBG_PRINT("STATUS_ERROR: Failed to get_opt %d.  Status: %d.\n", _opt, _status); \
                return STATUS_ERROR;\
        } \
} while(0)

#define NCONN_ERROR(...)\
do { \
        char _buf[1024];\
        sprintf(_buf, __VA_ARGS__);\
        m_last_error = _buf;\
        if(m_verbose)\
        {\
                fprintf(stdout, "%s:%s.%d: ", __FILE__, __FUNCTION__, __LINE__); \
                fprintf(stdout, __VA_ARGS__);\
                fprintf(stdout, "\n");\
                fflush(stdout); \
        }\
} while(0)

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class evr_loop;
class nbq;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class nconn
{
public:

        // ---------------------------------------
        // Protocol
        // ---------------------------------------
        typedef enum scheme_enum {

                SCHEME_TCP = 0,
                SCHEME_SSL,
                SCHEME_NONE

        } scheme_t;

        typedef enum type_enum {

                TYPE_CLIENT = 0,
                TYPE_SERVER,
                TYPE_NONE

        } type_t;

        typedef enum status_enum {

                NC_STATUS_FREE = -1,
                NC_STATUS_OK = -2,
                NC_STATUS_AGAIN = -3,
                NC_STATUS_ERROR = -4,
                NC_STATUS_UNSUPPORTED = -5,
                NC_STATUS_NONE = -6

        } status_t;

        typedef enum mode_enum {

                NC_MODE_READ = 0,
                NC_MODE_WRITE,
                NC_MODE_NONE

        } mode_t;

        // -------------------------------------------------
        // Const
        // -------------------------------------------------
        nconn(bool a_verbose,
              bool a_color,
              int64_t a_max_reqs_per_conn = -1,
              bool a_save = false,
              bool a_collect_stats = false,
              bool a_connect_only = false,
              type_t a_type = TYPE_CLIENT);

        // Destructor
        virtual ~nconn();

        void set_host(const std::string &a_host) {m_host = a_host;};

        void set_data1(void * a_data) {m_data1 = a_data;}
        void *get_data1(void) {return m_data1;}
        void set_data2(void * a_data) {m_data2 = a_data;}
        void *get_data2(void) {return m_data2;}

        void reset_stats(void) { stat_init(m_stat); }
        const req_stat_t &get_stats(void) const { return m_stat;};
        uint64_t get_id(void) {return m_id;}
        void set_id(uint64_t a_id) {m_id = a_id;}
        uint64_t get_idx(void) {return m_idx;}
        void set_idx(uint64_t a_id) {m_idx = a_id;}
        bool can_reuse(void);

        // -------------------------------------------------
        // Virtual Methods
        // -------------------------------------------------
        virtual int32_t set_opt(uint32_t a_opt, const void *a_buf, uint32_t a_len) = 0;
        virtual int32_t get_opt(uint32_t a_opt, void **a_buf, uint32_t *a_len) = 0;
        virtual bool is_listening(void) = 0;
        virtual bool is_connecting(void) = 0;
        virtual bool is_free(void) = 0;

        void set_in_q(nbq *a_q) { m_in_q = a_q;};
        void set_out_q(nbq *a_q) { m_out_q = a_q;};
        nbq *get_in_q(void) { return m_in_q;};
        nbq *get_out_q(void) { return m_out_q;};

        int32_t nc_run_state_machine(evr_loop *a_evr_loop, mode_t a_mode);
        int32_t nc_read(void);
        int32_t nc_write(void);
        int32_t nc_set_listening(evr_loop *a_evr_loop, int32_t a_val);
        int32_t nc_set_connected(evr_loop *a_evr_loop, int a_fd);
        int32_t nc_cleanup(void);
        bool is_done(void) { return (m_nc_state == NC_STATE_DONE);}
        void set_state_done(void) { m_nc_state = NC_STATE_DONE; }

        void bump_num_requested(void) {++m_num_reqs;}

        // -------------------------------------------------
        // Public static methods
        // -------------------------------------------------

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // TODO hide this!
        scheme_t m_scheme;
        bool m_verbose;
        bool m_color;
        std::string m_host;
        req_stat_t m_stat;
        bool m_save;
        bool m_collect_stats_flag;
        void *m_data1;
        void *m_data2;
        uint64_t m_connect_start_time_us;
        uint64_t m_request_start_time_us;
        bool m_supports_keep_alives;
        void *m_timer_obj;
        std::string m_last_error;
        type_t m_type;
        host_info_t m_host_info;

private:

        // ---------------------------------------
        // Connection state
        // ---------------------------------------
        typedef enum nc_conn_state
        {
                NC_STATE_FREE = 0,
                NC_STATE_LISTENING,
                NC_STATE_CONNECTING,
                NC_STATE_CONNECTED,
                NC_STATE_DONE
        } nc_conn_state_t;

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(nconn)

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        nc_conn_state_t m_nc_state;
        uint64_t m_id;
        uint32_t m_idx;

protected:
        // -------------------------------------------------
        // Protected methods
        // -------------------------------------------------
        virtual int32_t ncsetup(evr_loop *a_evr_loop) = 0;
        virtual int32_t ncread(char *a_buf, uint32_t a_buf_len) = 0;
        virtual int32_t ncwrite(char *a_buf, uint32_t a_buf_len) = 0;
        virtual int32_t ncaccept(void) = 0;
        virtual int32_t ncconnect(evr_loop *a_evr_loop) = 0;
        virtual int32_t nccleanup(void) = 0;
        virtual int32_t set_listening(evr_loop *a_evr_loop, int32_t a_val) = 0;
        virtual int32_t set_connected(evr_loop *a_evr_loop, int a_fd) = 0;

        // -------------------------------------------------
        // Protected members
        // -------------------------------------------------
        http_parser_settings m_http_parser_settings;
        http_parser m_http_parser;

        int64_t m_num_reqs_per_conn;
        int64_t m_num_reqs;
        bool m_connect_only;

        nbq *m_in_q;
        nbq *m_out_q;

};

} //namespace ns_hlx {

#endif



