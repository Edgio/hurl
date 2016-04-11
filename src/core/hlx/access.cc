//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    access.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    04/09/2016
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
#include "hlx/access.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
access_info::access_info(void):
        m_conn_cln_sa(),
        m_conn_cln_sa_len(0),
        m_conn_srv_sa(),
        m_conn_srv_sa_len(0),
        m_rqst_host(),
        m_rqst_scheme(),
        m_rqst_request(),
        m_rqst_query_string(),
        m_rqst_http_user_agent(),
        m_rqst_http_referer(),
        m_rqst_request_length(),
        m_resp_time_local(),
        m_resp_status(),
        m_resp_bytes_sent()
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
access_info::~access_info(void)
{

}

}
