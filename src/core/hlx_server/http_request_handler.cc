//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    http_request_handler.cc
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
#include "http_request_handler.h"
#include "ndebug.h"

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
default_http_request_handler::default_http_request_handler(void)
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
default_http_request_handler::~default_http_request_handler()
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t default_http_request_handler::do_get(const url_param_map_t &a_url_param_map,
                                             const http_req &a_request,
                                             http_resp &ao_response)
{
        NDBG_PRINT("do_get\n");
        //http_msg l_response;
        //return l_response;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t default_http_request_handler::do_post(const url_param_map_t &a_url_param_map,
                                              const http_req &a_request,
                                              http_resp &ao_response)
{
        NDBG_PRINT("do_post\n");
        //http_msg l_response;
        //return l_response;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t default_http_request_handler::do_put(const url_param_map_t &a_url_param_map,
                                             const http_req &a_request,
                                             http_resp &ao_response)
{
        NDBG_PRINT("do_put\n");
        //http_msg l_response;
        //return l_response;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t default_http_request_handler::do_delete(const url_param_map_t &a_url_param_map,
                                                const http_req &a_request,
                                                http_resp &ao_response)
{
        NDBG_PRINT("do_delete\n");
        //http_msg l_response;
        //return l_response;
        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t default_http_request_handler::do_default(const url_param_map_t &a_url_param_map,
                                                 const http_req &a_request,
                                                 http_resp &ao_response)
{
        NDBG_PRINT("do_default\n");
        //http_msg l_response;
        //return l_response;
        return STATUS_OK;
}



} //namespace ns_hlx {



