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
#include "hlx/hlx.h"
#include "nconn.h"
#include "ndebug.h"
#include "time_util.h"

#include <inttypes.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define WRITE_STATUS(_status) do { \
        int32_t _result = nwrite_status(a_nconn, _status);\
        if(_result != STATUS_OK) { NDBG_PRINT("Error performing _write_status.\n"); return STATUS_ERROR; }\
} while(0)

#define WRITE_HEADER(_key, _val) do { \
        int32_t _status = nwrite_header(a_nconn, _key,_val);\
        if(_status != STATUS_OK) { NDBG_PRINT("Error performing _write_header.\n"); return STATUS_ERROR; }\
} while(0)

#define WRITE_BODY(_data,_len) do { \
        int32_t _status = nwrite_body(a_nconn, body,_len);\
        if(_status != STATUS_OK) { NDBG_PRINT("Error performing write_body.\n"); return STATUS_ERROR; }\
} while(0)

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
int32_t http_request_handler::send_not_found(nconn &a_nconn, const http_req &a_request, const char *a_resp_str, http_resp &ao_resp)
{
        ao_resp.write_status(HTTP_STATUS_NOT_FOUND);
        ao_resp.write_header("Server","hss/0.0.1");
        ao_resp.write_header("Date", get_date_str());
        ao_resp.write_header("Content-type", "application/json");
        char l_length_str[64];
        sprintf(l_length_str, "%lu", strlen(a_resp_str));
        ao_resp.write_header("Content-Length", l_length_str);
        if(a_request.m_supports_keep_alives)
        {
                ao_resp.write_header("Connection", "keep-alive");
        }
        else
        {
                ao_resp.write_header("Connection", "close");
        }
        int32_t l_status;
        l_status = ao_resp.write_body(a_resp_str, strlen(a_resp_str));
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error performing _write_body.\n");
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http_request_handler::get_file(nconn &a_nconn,
                                       http_req &a_request,
                                       const std::string &a_path,
                                       http_resp &ao_resp)
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
                return send_not_found(a_nconn, a_request, G_RESP_GETFILE_NOT_FOUND, ao_resp);
        }

        // Check if is regular file
        if(!(l_stat.st_mode & S_IFREG))
        {
                //NDBG_PRINT("Error opening file: %s.  Reason: is NOT a regular file\n", l_path.c_str());
                return send_not_found(a_nconn, a_request, G_RESP_GETFILE_NOT_FOUND, ao_resp);
        }
        // Open file...
        FILE * l_file;
        l_file = fopen(l_path.c_str(),"r");
        if (NULL == l_file)
        {
                //NDBG_PRINT("Error opening file: %s.  Reason: %s\n", l_path.c_str(), strerror(errno));
                return send_not_found(a_nconn, a_request, G_RESP_GETFILE_NOT_FOUND, ao_resp);
        }

        // Read in file...
        int32_t l_size = l_stat.st_size;
        // Bounds check -remove later
        if(l_size > 16*1024)
        {
                //NDBG_PRINT("Error file size exceeds current size limits\n");
                return send_not_found(a_nconn, a_request, G_RESP_GETFILE_NOT_FOUND, ao_resp);
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
                return send_not_found(a_nconn, a_request, G_RESP_GETFILE_NOT_FOUND, ao_resp);
        }

        // ---------------------------------------
        // Close file...
        // ---------------------------------------
        l_status = fclose(l_file);
        if (STATUS_OK != l_status)
        {
                //NDBG_PRINT("Error performing fclose.  Reason: %s\n", strerror(errno));
                free(l_body);
                return send_not_found(a_nconn, a_request, G_RESP_GETFILE_NOT_FOUND, ao_resp);
        }

        // Create response
        ao_resp.write_status(HTTP_STATUS_OK);
        ao_resp.write_header("Server", "hss/0.0.1");
        ao_resp.write_header("Date", get_date_str());
        ao_resp.write_header("Content-type", "text/html");
        char l_length_str[64];
        sprintf(l_length_str, "%d", l_size);
        ao_resp.write_header("Content-Length", l_length_str);

        // TODO get last modified for file...
        ao_resp.write_header("Last-Modified", get_date_str());
        if(a_request.m_supports_keep_alives)
        {
                ao_resp.write_header("Connection", "keep-alive");
        }
        else
        {
                ao_resp.write_header("Connection", "close");
        }
        ao_resp.write_body(l_body, l_size);

        if(l_body)
        {
                free(l_body);
                l_body = NULL;
        }
        return 0;
}

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
int32_t default_http_request_handler::do_get(hlx &a_hlx, nconn &a_nconn, http_req &a_request, const url_param_map_t &a_url_param_map, http_resp &ao_resp)
{
        return send_not_found(a_nconn, a_request, G_RESP_GETFILE_NOT_FOUND, ao_resp);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t default_http_request_handler::do_post(hlx &a_hlx, nconn &a_nconn, http_req &a_request, const url_param_map_t &a_url_param_map, http_resp &ao_resp)
{
        return send_not_found(a_nconn, a_request, G_RESP_GETFILE_NOT_FOUND, ao_resp);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t default_http_request_handler::do_put(hlx &a_hlx, nconn &a_nconn, http_req &a_request, const url_param_map_t &a_url_param_map, http_resp &ao_resp)
{
        return send_not_found(a_nconn, a_request, G_RESP_GETFILE_NOT_FOUND, ao_resp);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t default_http_request_handler::do_delete(hlx &a_hlx, nconn &a_nconn, http_req &a_request, const url_param_map_t &a_url_param_map, http_resp &ao_resp)
{
        return send_not_found(a_nconn, a_request, G_RESP_GETFILE_NOT_FOUND, ao_resp);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t default_http_request_handler::do_default(hlx &a_hlx, nconn &a_nconn, http_req &a_request, const url_param_map_t &a_url_param_map, http_resp &ao_resp)
{
        return send_not_found(a_nconn, a_request, G_RESP_GETFILE_NOT_FOUND, ao_resp);
}

} //namespace ns_hlx {
