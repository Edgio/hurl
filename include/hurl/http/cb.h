//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    cb.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    07/20/2015
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
#ifndef _CB_H
#define _CB_H
//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "http_parser/http_parser.h"
#include <stdint.h>
namespace ns_hurl {
//: ----------------------------------------------------------------------------
//: parser callbacks
//: ----------------------------------------------------------------------------
int hp_on_message_begin(http_parser* a_parser);
int hp_on_url(http_parser* a_parser, const char *a_at, size_t a_length);
int hp_on_status(http_parser* a_parser, const char *a_at, size_t a_length);
int hp_on_header_field(http_parser* a_parser, const char *a_at, size_t a_length);
int hp_on_header_value(http_parser* a_parser, const char *a_at, size_t a_length);
int hp_on_headers_complete(http_parser* a_parser);
int hp_on_body(http_parser* a_parser, const char *a_at, size_t a_length);
int hp_on_message_complete(http_parser* a_parser);
} // ns_hurl
#endif
