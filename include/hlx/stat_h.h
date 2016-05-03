//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    stat_h.h
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
#ifndef _STAT_H_H
#define _STAT_H_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/default_rqst_h.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: file_h
//: ----------------------------------------------------------------------------
class stat_h: public default_rqst_h
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        stat_h(void);
        ~stat_h();
        h_resp_t do_get(clnt_session &a_clnt_session, rqst &a_rqst, const url_pmap_t &a_url_pmap);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        stat_h& operator=(const stat_h &);
        stat_h(const stat_h &);
};

} //namespace ns_hlx {

#endif
