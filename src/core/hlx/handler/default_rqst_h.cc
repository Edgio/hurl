//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    default_rqst_h.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    08/29/2015
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
#include "hlx/default_rqst_h.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
default_rqst_h::default_rqst_h(void)
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
default_rqst_h::~default_rqst_h()
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t default_rqst_h::do_get(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        return send_not_implemented(a_hconn, a_rqst);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t default_rqst_h::do_post(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        return send_not_implemented(a_hconn, a_rqst);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t default_rqst_h::do_put(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        return send_not_implemented(a_hconn, a_rqst);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t default_rqst_h::do_delete(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        return send_not_implemented(a_hconn, a_rqst);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t default_rqst_h::do_default(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        return send_not_implemented(a_hconn, a_rqst);
}

} //namespace ns_hlx {
