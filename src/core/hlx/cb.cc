//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    cb.cc
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

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/rqst.h"
#include "hlx/resp.h"
#include "hlx/trace.h"

#include "cb.h"
#include "hconn.h"
#include "ndebug.h"
#include "nconn.h"

#include <string.h>

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define CALC_OFFSET(_hconn, _at) \
        ((_at - _hconn->m_cur_buf) + _hconn->m_cur_off)

#define CHECK_FOR_NULL_OK(_data) \
        do {\
                if(!_data) {\
                        return 0;\
                }\
        } while(0);

#define CHECK_FOR_NULL_ERROR(_data) \
        do {\
                if(!_data) {\
                        return STATUS_ERROR;\
                }\
        } while(0);

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_parse(void *a_data, char *a_buf, uint32_t a_len, uint64_t a_off)
{
        CHECK_FOR_NULL_ERROR(a_data);
        hconn *l_hconn = static_cast<hconn *>(a_data);
        l_hconn->m_cur_off = a_off;
        l_hconn->m_cur_buf = a_buf;

        size_t l_parse_status = 0;
        //NDBG_PRINT("%sHTTP_PARSER%s: m_read_buf: %p, m_read_buf_idx: %d, l_bytes_read: %d\n",
        //                ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF,
        //                a_buf, (int)0, (int)a_len);
        l_parse_status = http_parser_execute(&(l_hconn->m_http_parser),
                                             &(l_hconn->m_http_parser_settings),
                                             a_buf,
                                             a_len);
        //m_read_buf_idx += l_bytes_read;
        if(l_parse_status < (size_t)a_len)
        {
                TRC_ERROR("Parse error.  Reason: %s: %s\n",
                           http_errno_name((enum http_errno)l_hconn->m_http_parser.http_errno),
                           http_errno_description((enum http_errno)l_hconn->m_http_parser.http_errno));
                return STATUS_ERROR;
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_message_begin(http_parser* a_parser)
{
        hconn *l_hconn = static_cast <hconn *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_hconn);
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_url(http_parser* a_parser, const char *a_at, size_t a_length)
{
        hconn *l_hconn = static_cast <hconn *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_hconn);
        if(l_hconn->m_save && (l_hconn->m_hmsg && (l_hconn->m_hmsg->get_type() == hmsg::TYPE_RQST)))
        {
                rqst *l_rqst = static_cast<rqst *>(l_hconn->m_hmsg);
                l_rqst->m_p_url.m_off = CALC_OFFSET(l_hconn, a_at);
                l_rqst->m_p_url.m_len = a_length;
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_status(http_parser* a_parser, const char *a_at, size_t a_length)
{
        hconn *l_hconn = static_cast <hconn *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_hconn);
        //l_hconn->m_status_code = a_parser->status_code;
        if((l_hconn->m_hmsg && (l_hconn->m_hmsg->get_type() == hmsg::TYPE_RESP)))
        {
                resp *l_resp = static_cast<resp *>(l_hconn->m_hmsg);
                l_resp->set_status((http_status_t)a_parser->status_code);
                if(l_hconn->m_save)
                {
                        l_resp->m_p_status.m_off = CALC_OFFSET(l_hconn, a_at);
                        l_resp->m_p_status.m_len = a_length;
                }
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_header_field(http_parser* a_parser, const char *a_at, size_t a_length)
{
        hconn *l_hconn = static_cast <hconn *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_hconn);
        if(l_hconn->m_save && l_hconn->m_hmsg)
        {
                hmsg *l_hmsg = static_cast<hmsg *>(l_hconn->m_hmsg);
                cr_struct l_cr;
                l_cr.m_off = CALC_OFFSET(l_hconn, a_at);
                l_cr.m_len = a_length;
                l_hmsg->m_p_h_list_key.push_back(l_cr);
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_header_value(http_parser* a_parser, const char *a_at, size_t a_length)
{
        hconn *l_hconn = static_cast <hconn *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_hconn);
        if(l_hconn->m_save && l_hconn->m_hmsg)
        {
                cr_struct l_cr;
                l_cr.m_off = CALC_OFFSET(l_hconn, a_at);
                l_cr.m_len = a_length;
                hmsg *l_hmsg = static_cast<hmsg *>(l_hconn->m_hmsg);
                l_hmsg->m_p_h_list_val.push_back(l_cr);
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_headers_complete(http_parser* a_parser)
{
        hconn *l_hconn = static_cast <hconn *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_hconn);
        if(l_hconn->m_save && l_hconn->m_hmsg)
        {
                hmsg *l_hmsg = static_cast<hmsg *>(l_hconn->m_hmsg);
                l_hmsg->m_http_major = a_parser->http_major;
                l_hmsg->m_http_minor = a_parser->http_minor;
                if(l_hconn->m_hmsg->get_type() == hmsg::TYPE_RQST)
                {
                        rqst *l_rqst = static_cast<rqst *>(l_hconn->m_hmsg);
                        l_rqst->m_method = a_parser->method;
                }
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_body(http_parser* a_parser, const char *a_at, size_t a_length)
{
        hconn *l_hconn = static_cast <hconn *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_hconn);
        if(l_hconn->m_save && l_hconn->m_hmsg)
        {
                hmsg *l_hmsg = static_cast<hmsg *>(l_hconn->m_hmsg);
                if(!l_hmsg->m_p_body.m_off)
                {
                        l_hmsg->m_p_body.m_off = CALC_OFFSET(l_hconn, a_at);
                        l_hmsg->m_p_body.m_len = a_length;
                }
                else
                {
                        l_hmsg->m_p_body.m_len += a_length;
                }
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_message_complete(http_parser* a_parser)
{
        hconn *l_hconn = static_cast <hconn *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_hconn);
        if(l_hconn->m_hmsg)
        {
                hmsg *l_hmsg = static_cast<hmsg *>(l_hconn->m_hmsg);
                l_hmsg->m_complete = true;
                l_hmsg->m_supports_keep_alives = http_should_keep_alive(a_parser);
        }
        return 0;
}

} //namespace ns_hlx {
