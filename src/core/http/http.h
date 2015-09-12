//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    http.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    07/20/2015
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
#ifndef _HTTP_H
#define _HTTP_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "ndebug.h"
#include "nbq.h"
#include <string>
#include <list>
#include <map>

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
struct http_parser;

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class nbq;

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef struct cr_struct {
        const char *m_ptr;
        uint32_t m_len;

        cr_struct():
                m_ptr(NULL),
                m_len(0)
        {}

        void clear(void)
        {
                m_ptr = NULL;
                m_len = 0;
        }
} cr_t;

typedef std::list <cr_t> cr_list_t;

// For parsed headers
typedef std::list <std::string> str_list_t;
typedef std::map <std::string, str_list_t> kv_map_list_t;

// TODO Hack to support getting connection meta
typedef std::map <std::string, std::string> conn_info_t;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class http_req
{
public:

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        http_req();
        ~http_req();
        void clear(void);

        // Get parsed results
        // TODO -copy for now -zero copy later???
        const kv_map_list_t &get_headers(void);
        const std::string &get_body(void);
        void set_q(nbq *a_q) { m_q = a_q;}
        nbq *get_q(void) { return m_q;}

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------

        // ---------------------------------------
        // raw http request offsets
        // ---------------------------------------
        cr_t m_p_url;
        cr_t m_p_status;
        cr_list_t m_p_h_list_key;
        cr_list_t m_p_h_list_val;
        cr_t m_p_body;
        int m_http_major;
        int m_http_minor;
        int m_method;

        // ---------------------------------------
        // ...
        // ---------------------------------------
        uint16_t m_status;
        bool m_complete;

private:

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(http_req);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        kv_map_list_t m_headers;
        std::string m_body;
        nbq *m_q;
};

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class http_resp
{
public:

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        http_resp();
        ~http_resp();
        void clear(void);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        void set_status(uint16_t a_code) {m_status = a_code;}
        uint16_t get_status(void) {return m_status;}
        void set_body(const std::string & a_body) {m_body = a_body;}

        // Get parsed results
        // TODO -copy for now -zero copy later???
        const kv_map_list_t &get_headers(void);
        const std::string &get_body(void);

        void set_q(nbq *a_q) { m_q = a_q;}
        nbq *get_q(void) { return m_q;}

        // ---------------------------------------
        // raw http request offsets
        // ---------------------------------------
        cr_t m_p_status;
        cr_list_t m_p_h_list_key;
        cr_list_t m_p_h_list_val;
        cr_t m_p_body;

        // ---------------------------------------
        // ...
        // ---------------------------------------
public:
        bool m_complete;

        // TODO Hack to support getting connection meta
        conn_info_t m_conn_info;

private:

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(http_resp);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        uint16_t m_status;
        kv_map_list_t m_headers;
        std::string m_body;
        nbq *m_q;
};

//: ----------------------------------------------------------------------------
//: Callbacks
//: ----------------------------------------------------------------------------
int hp_on_message_begin(http_parser* a_parser);
int hp_on_url(http_parser* a_parser, const char *a_at, size_t a_length);
int hp_on_status(http_parser* a_parser, const char *a_at, size_t a_length);
int hp_on_header_field(http_parser* a_parser, const char *a_at, size_t a_length);
int hp_on_header_value(http_parser* a_parser, const char *a_at, size_t a_length);
int hp_on_headers_complete(http_parser* a_parser);
int hp_on_body(http_parser* a_parser, const char *a_at, size_t a_length);
int hp_on_message_complete(http_parser* a_parser);

} // ns_hlx

#endif






