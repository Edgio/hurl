//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    h_resp.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    03/11/2015
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
#ifndef _H_RESP_H
#define _H_RESP_H

namespace ns_hlx {

// ---------------------------------------
// Handler status
// ---------------------------------------
typedef enum {

        H_RESP_NONE = 0,
        H_RESP_DONE,
        H_RESP_DEFERRED,
        H_RESP_SERVER_ERROR,
        H_RESP_CLIENT_ERROR

} h_resp_t;

}

#endif


