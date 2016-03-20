//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    file.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    11/29/2015
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
#include "file.h"
#include "nbq.h"
#include "ndebug.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: Constructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
filesender::filesender():
                m_fd(-1),
                m_size(0),
                m_read(0),
                m_state(IDLE)
{
}

//: ----------------------------------------------------------------------------
//: \details: Destructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
filesender::~filesender()
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
int32_t filesender::fsinit(const char *a_filename)
{
        // ---------------------------------------
        // Check is a file
        // TODO
        // ---------------------------------------
        struct stat l_stat;
        int32_t l_status = STATUS_OK;
        l_status = stat(a_filename, &l_stat);
        if(l_status != 0)
        {
                //NDBG_PRINT("Error performing stat on file: %s.  Reason: %s\n", a_filename, strerror(errno));
                return STATUS_ERROR;
        }

        // Check if is regular file
        if(!(l_stat.st_mode & S_IFREG))
        {
                //NDBG_PRINT("Error opening file: %s.  Reason: is NOT a regular file\n", a_filename);
                return STATUS_ERROR;
        }

        // Set size
        m_size = l_stat.st_size;

        // Open the file
        m_fd = open(a_filename, O_RDONLY);
        if (m_fd < 0)
        {
                //NDBG_PRINT("Open: %s failed\n", a_filename);
                return STATUS_ERROR;
        }

        // Start sending it
        m_read = 0;
        m_state = SENDING;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: Read part from file
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ssize_t filesender::fsread(char *ao_dst, size_t a_len)
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
                return STATUS_ERROR;
        }
        if(l_read > 0)
        {
                m_read += l_read;
        }
        return l_read;
}

//: ----------------------------------------------------------------------------
//: \details: Continue sending the file started by sendFile().
//:           Call this periodically. Returns nonzero when done.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ssize_t filesender::fsread(nbq &ao_q, size_t a_len)
{
        if(m_fd < 0)
        {
                return 0;
        }

        // Get one chunk of the file from disk
        ssize_t l_read = 0;
        l_read = ao_q.write_fd(m_fd, a_len);
        if(l_read < 0)
        {
                //NDBG_PRINT("Error performing read. Reason: %s\n", strerror(errno));
                return STATUS_ERROR;
        }
        else if(l_read >= 0)
        {
                m_read += l_read;
                if ((size_t)l_read < a_len)
                {
                        // All done; close the file.
                        close(m_fd);
                        m_fd = -1;
                        m_state = DONE;
                        return 0;
                }
        }
        return l_read;
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
        return STATUS_OK;
}

}
