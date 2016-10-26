//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    file_h.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    01/02/2016
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
#include "ndebug.h"
#include "t_srvr.h"
#include "mime_types.h"
#include "hlx/clnt_session.h"
#include "hlx/nbq.h"
#include "hlx/string_util.h"
#include "hlx/time_util.h"
#include "hlx/file_h.h"
#include "hlx/rqst.h"
#include "hlx/api_resp.h"
#include "hlx/srvr.h"
#include "hlx/status.h"
#include "hlx/trace.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Handler
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
file_h::file_h(void):
        default_rqst_h(),
        m_root(),
        m_index("index.html"),
        m_route()
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
file_h::~file_h(void)
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t file_h::do_get(clnt_session &a_clnt_session, rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        // GET
        // Set path
        std::string l_path;
        int32_t l_s;
        l_s = get_path(m_root, m_index, m_route, a_rqst.get_url_path(), l_path);
        if(l_s != HLX_STATUS_OK)
        {
                return H_RESP_CLIENT_ERROR;
        }
        TRC_VERBOSE("l_path: %s\n",l_path.c_str());
        return get_file(a_clnt_session, a_rqst, l_path);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t file_h::set_root(const std::string &a_root)
{
        m_root = a_root;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t file_h::set_index(const std::string &a_index)
{
        m_index = a_index;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t file_h::set_route(const std::string &a_route)
{
        m_route = a_route;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t file_h::get_file(clnt_session &a_clnt_session,
                          rqst &a_rqst,
                          const std::string &a_path)
{
        if(!a_clnt_session.m_out_q)
        {
                if(!a_clnt_session.m_t_srvr)
                {
                        TRC_ERROR("a_clnt_session.m_t_srvr == NULL\n");
                        return send_internal_server_error(a_clnt_session, a_rqst.m_supports_keep_alives);
                }
                a_clnt_session.m_out_q = a_clnt_session.m_t_srvr->get_nbq(NULL);
                if(!a_clnt_session.m_out_q)
                {
                        TRC_ERROR("a_clnt_session.m_out_q\n");
                        return send_internal_server_error(a_clnt_session, a_rqst.m_supports_keep_alives);
                }
        }

        // Make relative...
        file_u *l_fs = new file_u(a_clnt_session);
        int32_t l_s;
        l_s = l_fs->fsinit(a_path.c_str());
        if(l_s != HLX_STATUS_OK)
        {
                delete l_fs;
                // TODO -use status code to determine is actual 404
                return send_not_found(a_clnt_session, a_rqst.m_supports_keep_alives);
        }

        a_clnt_session.m_ups = l_fs;
        nbq &l_q = *(a_clnt_session.m_out_q);

        // ---------------------------------------
        // Write headers
        // ---------------------------------------
        srvr *l_srvr = a_clnt_session.get_srvr();
        if(!l_srvr)
        {
                TRC_ERROR("fsinit(%s) failed\n", a_path.c_str());
                return send_internal_server_error(a_clnt_session, a_rqst.m_supports_keep_alives);
                return H_RESP_SERVER_ERROR;
        }
        nbq_write_status(l_q, HTTP_STATUS_OK);
        nbq_write_header(l_q, "Server", l_srvr->get_server_name().c_str());
        nbq_write_header(l_q, "Date", get_date_str());

        // Get extension
        std::string l_ext = get_file_ext(a_path);
        mime_types::ext_type_map_t::const_iterator i_m = mime_types::S_EXT_TYPE_MAP.find(l_ext);
        if(i_m != mime_types::S_EXT_TYPE_MAP.end())
        {
                nbq_write_header(l_q, "Content-type", i_m->second.c_str());
        }
        else
        {
                nbq_write_header(l_q, "Content-type", "text/html");
        }

        char l_length_str[32];
        sprintf(l_length_str, "%zu", l_fs->fssize());
        nbq_write_header(l_q, "Content-Length", l_length_str);

        // TODO get last modified for file...
        nbq_write_header(l_q, "Last-Modified", get_date_str());
        if(a_rqst.m_supports_keep_alives)
        {
                nbq_write_header(l_q, "Connection", "keep-alive");
        }
        else
        {
                nbq_write_header(l_q, "Connection", "close");
        }
        l_q.write("\r\n", strlen("\r\n"));
        // Read up to 4k
        uint32_t l_read = 4096;
        if(l_read - l_q.read_avail() > 0)
        {
                l_read = l_read - l_q.read_avail();
        }
        if(l_fs->fssize() < l_read)
        {
                l_read = l_fs->fssize();
        }
        ssize_t l_ups_read_s;
        l_ups_read_s = l_fs->ups_read(l_read);
        if(l_ups_read_s < 0)
        {
                TRC_ERROR("performing ups_read\n");
        }
        return H_RESP_DONE;
}

//: ----------------------------------------------------------------------------
//: Upstream Object
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: Constructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
file_u::file_u(clnt_session &a_clnt_session):
        base_u(a_clnt_session),
        m_fd(-1),
        m_size(0),
        m_read(0)
{
}

//: ----------------------------------------------------------------------------
//: \details: Destructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
file_u::~file_u()
{
        if(m_fd > 0)
        {
                close(m_fd);
                m_fd = -1;
        }
}

//: ----------------------------------------------------------------------------
//: \details: Setup file for reading
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t file_u::fsinit(const char *a_filename)
{
        // ---------------------------------------
        // Check is a file
        // TODO
        // ---------------------------------------
        int32_t l_s = HLX_STATUS_OK;
        // Open the file
        m_fd = open(a_filename, O_RDONLY|O_NONBLOCK);
        if (m_fd < 0)
        {
                TRC_ERROR("performing open on file: %s.  Reason: %s\n", a_filename, strerror(errno));
                return HLX_STATUS_ERROR;
        }
        struct stat l_stat;
        l_s = fstat(m_fd, &l_stat);
        if(l_s != 0)
        {
                TRC_ERROR("performing stat on file: %s.  Reason: %s\n", a_filename, strerror(errno));
                if(m_fd > 0)
                {
                        close(m_fd);
                        m_fd = -1;
                }
                return HLX_STATUS_ERROR;
        }
        // Check if is regular file
        if(!(l_stat.st_mode & S_IFREG))
        {
                TRC_ERROR("opening file: %s.  Reason: is NOT a regular file\n", a_filename);
                if(m_fd > 0)
                {
                        close(m_fd);
                        m_fd = -1;
                }
                return HLX_STATUS_ERROR;
        }
        // Set size
        m_size = l_stat.st_size;

        // Start sending it
        m_read = 0;
        m_state = UPS_STATE_SENDING;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: Read part from file
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#if 0
ssize_t file_u::ups_read(char *ao_dst, size_t a_len)
{
        // Get one chunk of the file from disk
        ssize_t l_read = 0;
        l_read = read(m_fd, (void *)ao_dst, a_len);
        if (l_read == 0)
        {
                // All done; close the file.
                close(m_fd);
                m_fd = 0;
                m_state = DONE;
                return 0;
        }
        else if(l_read < 0)
        {
                //NDBG_PRINT("Error performing read. Reason: %s\n", strerror(errno));
                return HLX_STATUS_ERROR;
        }
        if(l_read > 0)
        {
                m_read += l_read;
        }
        return l_read;
}
#endif

//: ----------------------------------------------------------------------------
//: \details: Continue sending the file started by sendFile().
//:           Call this periodically. Returns nonzero when done.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ssize_t file_u::ups_read(size_t a_len)
{
        if(m_fd < 0)
        {
                return 0;
        }
        m_state = UPS_STATE_SENDING;
        if(!(m_clnt_session.m_out_q))
        {
                TRC_ERROR("m_clnt_session->m_out_q == NULL\n");
                return HLX_STATUS_ERROR;
        }
        // Get one chunk of the file from disk
        ssize_t l_read = 0;
        ssize_t l_last;
        size_t l_len_read = (a_len > (m_size - m_read))?(m_size - m_read): a_len;
        l_read = m_clnt_session.m_out_q->write_fd(m_fd, l_len_read, l_last);
        if(l_read < 0)
        {
                TRC_ERROR("performing read. Reason: %s\n", strerror(errno));
                return HLX_STATUS_ERROR;
        }

        m_read += l_read;
        //NDBG_PRINT("READ: B %9ld / %9lu / %9lu\n", l_len_read, m_read, m_size);
        if ((((size_t)l_read) < a_len) || (m_read >= m_size))
        {
                // All done; close the file.
                close(m_fd);
                m_fd = -1;
                m_state = UPS_STATE_DONE;
        }
        if(l_read > 0)
        {
                int32_t l_s;
                l_s = m_clnt_session.m_t_srvr->queue_output(m_clnt_session);
                if(l_s != HLX_STATUS_OK)
                {
                        TRC_ERROR("performing queue_output\n");
                        return HLX_STATUS_ERROR;
                }
        }
        return l_read;
}

//: ----------------------------------------------------------------------------
//: \details: Cancel and cleanup
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t file_u::ups_cancel(void)
{
        if(m_fd > 0)
        {
                close(m_fd);
                m_state = UPS_STATE_DONE;
                m_fd = -1;
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: Get path
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t get_path(const std::string &a_root,
                 const std::string &a_index,
                 const std::string &a_route,
                 const std::string &a_url_path,
                 std::string &ao_path)
{
        // TODO Path normalization and for recursing... "../"
        if(a_root.empty())
        {
                ao_path = "./";
        }
        else
        {
                ao_path = a_root;
        }
        if(ao_path[ao_path.length() - 1] != '/')
        {
                ao_path += '/';
        }
        std::string l_route = a_route;
        if(a_route[a_route.length() - 1] == '*')
        {
                l_route = a_route.substr(0, a_route.length() - 2);
        }
        std::string l_url_path;
        if(!l_route.empty() &&
           (a_url_path.find(l_route, 0) != std::string::npos))
        {
                l_url_path = a_url_path.substr(l_route.length(), a_url_path.length() - l_route.length());
        }
        else
        {
                l_url_path = a_url_path;
        }
        if(!a_index.empty() &&
           (!l_url_path.length() ||
           (l_url_path.length() == 1 && l_url_path[0] == '/')))
        {
                ao_path += a_index;
        }
        else
        {
                if(l_url_path[0] == '/')
                {
                        ao_path += l_url_path.substr(1, l_url_path.length() - 1);
                }
                else
                {
                        ao_path += l_url_path;
                }
        }
        return HLX_STATUS_OK;
}

} //namespace ns_hlx {
