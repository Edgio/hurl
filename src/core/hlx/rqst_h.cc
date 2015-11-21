//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    rqst_h.cc
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
#include "hlx/hlx.h"
#include "nconn.h"
#include "ndebug.h"
#include "time_util.h"

#include <inttypes.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Custom responses
//: ----------------------------------------------------------------------------
const char *G_RESP_GETFILE_NOT_FOUND = "{ \"errors\": ["
                                       "{\"code\": 404, \"message\": \"Not found\"}"
                                       "], \"success\": false}\r\n";

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t rqst_h::send_not_found(hlx &a_hlx, hconn &a_hconn, const rqst &a_rqst)
{
        api_resp &l_api_resp = a_hlx.create_api_resp();
        l_api_resp.set_status(HTTP_STATUS_NOT_FOUND);
        l_api_resp.set_header("Server","hss/0.0.1");
        l_api_resp.set_header("Date", get_date_str());
        l_api_resp.set_header("Content-type", "application/json");
        char l_length_str[64];
        uint32_t l_resp_len = sizeof(char)*strlen(G_RESP_GETFILE_NOT_FOUND);
        sprintf(l_length_str, "%u", l_resp_len);
        l_api_resp.set_header("Content-Length", l_length_str);
        if(a_rqst.m_supports_keep_alives)
        {
                l_api_resp.set_header("Connection", "keep-alive");
        }
        else
        {
                l_api_resp.set_header("Connection", "close");
        }
        char *l_resp = (char *)malloc(l_resp_len);
        memcpy(l_resp, G_RESP_GETFILE_NOT_FOUND, l_resp_len);
        l_api_resp.set_body_data(l_resp, l_resp_len);
        a_hlx.queue_api_resp(a_hconn, l_api_resp);
        return H_RESP_DONE;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t rqst_h::get_file(hlx &a_hlx,
                          hconn &a_hconn,
                          rqst &a_rqst,
                          const std::string &a_path)
{
        // Make relative...
        std::string l_path = "." + a_path;
        // ---------------------------------------
        // Check is a file
        // TODO
        // ---------------------------------------
        struct stat l_stat;
        int32_t l_status = STATUS_OK;
        l_status = stat(l_path.c_str(), &l_stat);
        if(l_status != 0)
        {
                //NDBG_PRINT("Error performing stat on file: %s.  Reason: %s\n", l_path.c_str(), strerror(errno));
                return send_not_found(a_hlx, a_hconn, a_rqst);
        }

        // Check if is regular file
        if(!(l_stat.st_mode & S_IFREG))
        {
                //NDBG_PRINT("Error opening file: %s.  Reason: is NOT a regular file\n", l_path.c_str());
                return send_not_found(a_hlx, a_hconn, a_rqst);
        }
        // Open file...
        FILE * l_file;
        l_file = fopen(l_path.c_str(),"r");
        if (NULL == l_file)
        {
                //NDBG_PRINT("Error opening file: %s.  Reason: %s\n", l_path.c_str(), strerror(errno));
                return send_not_found(a_hlx, a_hconn, a_rqst);
        }

        // Read in file...
        int32_t l_size = l_stat.st_size;
        // Bounds check -remove later
        if(l_size > 16*1024)
        {
                //NDBG_PRINT("Error file size exceeds current size limits\n");
                return send_not_found(a_hlx, a_hconn, a_rqst);
        }
        char *l_body = (char *)malloc(sizeof(char)*l_size);

        // TODO loop....
        int32_t l_read_size;
        l_read_size = fread(l_body, 1, l_size, l_file);
        if(l_read_size != l_size)
        {
                //NDBG_PRINT("Error performing fread.  Reason: %s [%d:%d]\n",
                //                strerror(errno), l_read_size, l_size);
                free(l_body);
                fclose(l_file);
                return send_not_found(a_hlx, a_hconn, a_rqst);
        }

        // ---------------------------------------
        // Close file...
        // ---------------------------------------
        l_status = fclose(l_file);
        if (STATUS_OK != l_status)
        {
                //NDBG_PRINT("Error performing fclose.  Reason: %s\n", strerror(errno));
                free(l_body);
                return send_not_found(a_hlx, a_hconn, a_rqst);
        }

        // Create response
        api_resp &l_api_resp = a_hlx.create_api_resp();
        l_api_resp.set_status(HTTP_STATUS_OK);
        l_api_resp.set_header("Server", "hss/0.0.1");
        l_api_resp.set_header("Date", get_date_str());
        l_api_resp.set_header("Content-type", "text/html");
        char l_length_str[64];
        sprintf(l_length_str, "%d", l_size);
        l_api_resp.set_header("Content-Length", l_length_str);

        // TODO get last modified for file...
        l_api_resp.set_header("Last-Modified", get_date_str());
        if(a_rqst.m_supports_keep_alives)
        {
                l_api_resp.set_header("Connection", "keep-alive");
        }
        else
        {
                l_api_resp.set_header("Connection", "close");
        }
        l_api_resp.set_body_data(l_body, l_size);
        a_hlx.queue_api_resp(a_hconn, l_api_resp);
        return H_RESP_DONE;
}

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
h_resp_t default_rqst_h::do_get(hlx &a_hlx, hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        return send_not_found(a_hlx, a_hconn, a_rqst);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t default_rqst_h::do_post(hlx &a_hlx, hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        return send_not_found(a_hlx, a_hconn, a_rqst);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t default_rqst_h::do_put(hlx &a_hlx, hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        return send_not_found(a_hlx, a_hconn, a_rqst);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t default_rqst_h::do_delete(hlx &a_hlx, hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        return send_not_found(a_hlx, a_hconn, a_rqst);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t default_rqst_h::do_default(hlx &a_hlx, hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        return send_not_found(a_hlx, a_hconn, a_rqst);
}

} //namespace ns_hlx {
