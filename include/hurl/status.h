//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    status.h
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
#ifndef _STATUS_H
#define _STATUS_H

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
// TODO enum instead???
#ifndef HURL_STATUS_OK
#define HURL_STATUS_OK 0
#endif

#ifndef HURL_STATUS_ERROR
#define HURL_STATUS_ERROR -1
#endif

#ifndef HURL_STATUS_AGAIN
#define HURL_STATUS_AGAIN -2
#endif

#ifndef HURL_STATUS_BUSY
#define HURL_STATUS_BUSY -3
#endif

#ifndef HURL_STATUS_DONE
#define HURL_STATUS_DONE -4
#endif

#endif
