//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    file_h.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    12/12/2015
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
#ifndef _FILE_H_H
#define _FILE_H_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/default_rqst_h.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: file_h
//: ----------------------------------------------------------------------------
class file_h: public default_rqst_h
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        file_h(void);
        ~file_h();
        h_resp_t do_get(clnt_session &a_clnt_session, rqst &a_rqst, const url_pmap_t &a_url_pmap);
        int32_t set_root(const std::string &a_root);
        int32_t set_index(const std::string &a_index);
        int32_t set_route(const std::string &a_route);
protected:
        // -------------------------------------------------
        // Protected methods
        // -------------------------------------------------
        h_resp_t get_file(clnt_session &a_clnt_session, rqst &a_rqst, const std::string &a_path);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        file_h& operator=(const file_h &);
        file_h(const file_h &);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        std::string m_root;
        std::string m_index;
        std::string m_route;
};

} //namespace ns_hlx {

#endif
