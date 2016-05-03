//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    file_u.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    11/20/2015
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
#ifndef _FILE_U_H
#define _FILE_U_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/base_u.h"
#include <string>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------
class nbq;

//: ----------------------------------------------------------------------------
//: file_u
//: Example based on Dan Kegel's "Introduction to non-blocking I/O"
//: ref: http://www.kegel.com/dkftpbench/nonblocking.html
//: ----------------------------------------------------------------------------
class file_u: public base_u
{
public:
        // -------------------------------------------------
        // const
        // -------------------------------------------------
        static const uint32_t S_UPS_TYPE_FILE = 0xFFFF000A;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        file_u();
        ~file_u();
        int fsinit(const char *a_filename);
        size_t fssize(void) { return m_size;}

        // -------------------------------------------------
        // upstream methods
        // -------------------------------------------------
        ssize_t ups_read(char *ao_dst, size_t a_len);
        ssize_t ups_read(nbq &ao_q, size_t a_len);
        bool ups_done(void) {return (m_state == DONE);}
        int32_t ups_cancel(void);
        uint32_t get_type(void) { return S_UPS_TYPE_FILE;}
private:
        // -------------------------------------------------
        // Private types
        // -------------------------------------------------
        typedef enum
        {
                IDLE,
                SENDING,
                DONE
        } state_t;

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        file_u& operator=(const file_u &);
        file_u(const file_u &);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        int m_fd;
        size_t m_size;
        size_t m_read;
        state_t m_state;
};

//: ----------------------------------------------------------------------------
//: Prototypes
//: ----------------------------------------------------------------------------
int32_t get_path(const std::string &a_root,
                 const std::string &a_index,
                 const std::string &a_route,
                 const std::string &a_url_path,
                 std::string &ao_path);


} //namespace ns_hlx {

#endif
