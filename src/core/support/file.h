//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    file.h
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
#ifndef _FILE_H
#define _FILE_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <unistd.h>
#include <string>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------
class nbq;

//: ----------------------------------------------------------------------------
//: filesender
//: Example based on Dan Kegel's "Introduction to non-blocking I/O"
//: ref: http://www.kegel.com/dkftpbench/nonblocking.html
//: ----------------------------------------------------------------------------
class filesender
{

public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        filesender();
        ~filesender();
        int fsinit(const char *a_filename);
        ssize_t fsread(char *ao_dst, size_t a_len);
        ssize_t fsread(nbq &ao_q, size_t a_len);
        bool fsdone(void) {return (m_state == DONE);}
        size_t fssize(void) { return m_size;}
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
                 const std::string &a_url_path,
                 std::string &ao_path);


} //namespace ns_hlx {

#endif
