//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
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
#include "hurl/support/ndebug.h"
#include "hurl/http/cb.h"
#include "hurl/http/rqst.h"
#include "hurl/http/resp.h"
#include "hurl/support/trace.h"
#include "hurl/status.h"
#include <string.h>
//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define CALC_OFFSET(_hmsg, _at) \
        ((_at - _hmsg->m_cur_buf) + _hmsg->m_cur_off)

#define CHECK_FOR_NULL_OK(_data) \
        do {\
                if(!_data) {\
                        return 0;\
                }\
        } while(0);

#define CHECK_FOR_NULL_ERROR(_data) \
        do {\
                if(!_data) {\
                        return HURL_STATUS_ERROR;\
                }\
        } while(0);

namespace ns_hurl {
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_message_begin(http_parser* a_parser)
{
        hmsg *l_hmsg = static_cast <hmsg *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_hmsg);
        return 0;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hp_on_url(http_parser* a_parser, const char *a_at, size_t a_length)
{
        hmsg *l_hmsg = static_cast <hmsg *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_hmsg);
        if(l_hmsg->m_save && (l_hmsg->get_type() == hmsg::TYPE_RQST))
        {
                rqst *l_rqst = static_cast<rqst *>(l_hmsg);
                l_rqst->m_p_url.m_off = CALC_OFFSET(l_hmsg, a_at);
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
        hmsg *l_hmsg = static_cast <hmsg *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_hmsg);
        //l_clnt->m_status_code = a_parser->status_code;
        if(l_hmsg->get_type() == hmsg::TYPE_RESP)
        {
                resp *l_resp = static_cast<resp *>(l_hmsg);
                l_resp->set_status((http_status_t)a_parser->status_code);
                if(l_hmsg->m_save)
                {
                        l_resp->m_p_status.m_off = CALC_OFFSET(l_hmsg, a_at);
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
        hmsg *l_hmsg = static_cast <hmsg *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_hmsg);
        if(l_hmsg->m_save)
        {
                cr_struct l_cr;
                l_cr.m_off = CALC_OFFSET(l_hmsg, a_at);
                l_cr.m_len = a_length;
                l_hmsg->m_p_h_list_key.push_back(l_cr);

                // signalling for expect headers
                if(l_hmsg->get_type() == hmsg::TYPE_RQST)
                {
                        const char l_exp[] = "Expect";
                        if((a_length == (sizeof(l_exp)-1)) && (a_at[0] == 'E'))
                        {
                                if(strncasecmp(l_exp, a_at, a_length) == 0)
                                {
                                        rqst *l_rqst = static_cast<rqst *>(l_hmsg);
                                        l_rqst->m_expect = true;
                                }
                        }
                }
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
        hmsg *l_hmsg = static_cast <hmsg *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_hmsg);
        if(l_hmsg->m_save)
        {
                cr_struct l_cr;
                l_cr.m_off = CALC_OFFSET(l_hmsg, a_at);
                l_cr.m_len = a_length;
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
        hmsg *l_hmsg = static_cast <hmsg *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_hmsg);
        if(l_hmsg->m_save)
        {
                l_hmsg->m_http_major = a_parser->http_major;
                l_hmsg->m_http_minor = a_parser->http_minor;
                if(l_hmsg->get_type() == hmsg::TYPE_RQST)
                {
                        rqst *l_rqst = static_cast<rqst *>(l_hmsg);
                        l_rqst->m_method = a_parser->method;
                }
        }
        if(l_hmsg->get_type() == hmsg::TYPE_RESP)
        {
                if(!l_hmsg->m_expect_resp_body_flag)
                {
                        return 1;
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
        hmsg *l_hmsg = static_cast <hmsg *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_hmsg);
        if(l_hmsg->m_save)
        {
                if(!l_hmsg->m_p_body.m_off)
                {
                        l_hmsg->m_p_body.m_off = CALC_OFFSET(l_hmsg, a_at);
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
        hmsg *l_hmsg = static_cast <hmsg *>(a_parser->data);
        CHECK_FOR_NULL_OK(l_hmsg);
        l_hmsg->m_complete = true;
        l_hmsg->m_supports_keep_alives = http_should_keep_alive(a_parser);
        return 0;
}

} //namespace ns_hurl {
