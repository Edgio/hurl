//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hlx_common.h
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
#ifndef _HLX_COMMON_H
#define _HLX_COMMON_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <string>
#include <list>
#include <map>

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define HTTP_DISALLOW_ASSIGN(class_name)\
    class_name& operator=(const class_name &);
#define HTTP_DISALLOW_COPY(class_name)\
    class_name(const class_name &);
#define HTTP_DISALLOW_COPY_AND_ASSIGN(class_name)\
    HTTP_DISALLOW_COPY(class_name)\
    HTTP_DISALLOW_ASSIGN(class_name)

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

// Scheme
typedef enum {
        SCHEME_NONE = 0,
        SCHEME_HTTP,
        SCHEME_HTTPS
} scheme_type_t;

// Status
typedef enum http_status_enum {
        HTTP_STATUS_OK = 200,
        HTTP_STATUS_NOT_FOUND = 404,
} http_status_t;

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

// Host
typedef struct host_struct {
        std::string m_host;
        std::string m_hostname;
        std::string m_id;
        std::string m_where;
        std::string m_url;
        host_struct():
                m_host(),
                m_hostname(),
                m_id(),
                m_where(),
                m_url()
        {};
} host_t;

typedef std::list <cr_t> cr_list_t;
typedef std::list <std::string> str_list_t;
typedef std::map <std::string, str_list_t> kv_map_list_t;
typedef std::map <std::string, std::string> conn_info_t;
typedef std::list <host_t> host_list_t;
typedef std::list <std::string> server_list_t;

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
        const std::string &get_url_path(void);

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
        bool m_supports_keep_alives;
private:

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        HTTP_DISALLOW_COPY_AND_ASSIGN(http_req);
        int32_t parse_url(void);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        kv_map_list_t m_headers;
        std::string m_body;
        nbq *m_q;

        // Parsed members
        bool m_url_parsed;
        std::string m_parsed_url_path;

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

        // Write parts
        int32_t write_status(http_status_t a_status);
        int32_t write_header(const char *a_key, const char *a_val);
        int32_t write_header(const char *a_key, uint32_t a_key_len, const char *a_val, uint32_t a_val_len);
        int32_t write_body(const char *a_body, uint32_t a_body_len);

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
        HTTP_DISALLOW_COPY_AND_ASSIGN(http_resp);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        uint16_t m_status;
        kv_map_list_t m_headers;
        std::string m_body;
        nbq *m_q;
};

} // ns_hlx

#endif






