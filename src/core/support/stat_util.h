//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    ssl_util.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    10/26/2015
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
//:                          Stats Support
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "../../../include/hlx/stat.h"

namespace ns_hlx {

void update_stat(xstat_t &ao_stat, double a_val);
void add_stat(xstat_t &ao_stat, const xstat_t &a_from_stat);
void clear_stat(xstat_t &ao_stat);
void show_stat(const xstat_t &ao_stat);

} //namespace ns_hlx {
