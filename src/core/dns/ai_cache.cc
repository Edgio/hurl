//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    ai_cache.cc
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
//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hurl/dns/ai_cache.h"
#include "hurl/support/ndebug.h"
#include "hurl/support/time_util.h"
#include "hurl/nconn/host_info.h"
#include "hurl/status.h"
#include "base64/base64.h"
// json support
//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Werror"
#include "rapidjson/document.h"
//#pragma GCC diagnostic pop
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
namespace ns_hurl {
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ai_cache::ai_cache(const std::string &a_ai_cache_file):
        m_ai_cache_map(),
        m_ai_cache_file(a_ai_cache_file)
{
        if(!m_ai_cache_file.empty())
        {
                int32_t l_status = read(m_ai_cache_file, m_ai_cache_map);
                if(l_status != HURL_STATUS_OK)
                {
                        //NDBG_PRINT("Error: read with cache file: %s.\n",
                        //                m_ai_cache_file.c_str());
                }
        }
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ai_cache::~ai_cache()
{
        if(!m_ai_cache_file.empty())
        {
                int32_t l_status = sync(m_ai_cache_file, m_ai_cache_map);
                if(l_status != HURL_STATUS_OK)
                {
                        //NDBG_PRINT("Error: sync with ai_cache file: %s.\n",
                        //                m_ai_cache_file.c_str());
                }
        }

        for(ai_cache_map_t::iterator i_h = m_ai_cache_map.begin();
            i_h != m_ai_cache_map.end();
            ++i_h)
        {
                if(i_h->second)
                {
                        delete i_h->second;
                        i_h->second = NULL;
                }
        }
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
host_info *ai_cache::lookup(const std::string a_label)
{
        host_info *l_host_info = NULL;
        if(m_ai_cache_map.find(a_label) != m_ai_cache_map.end())
        {
                l_host_info = m_ai_cache_map[a_label];
                // Check expires
                //NDBG_PRINT("Seconds left: %ld\n", (l_host_info->m_expires_s - get_time_s()));
                if(l_host_info->m_expires_s &&
                   (get_time_s() > l_host_info->m_expires_s))
                {
                        //NDBG_PRINT("KEY: %s EXPIRED! -diff: %lu -expires: %lu cur: %lu -l_host_info: %p\n",
                        //                a_label.c_str(),
                        //                get_time_s() - l_host_info->m_expires_s,
                        //                l_host_info->m_expires_s,
                        //                get_time_s(),
                        //                l_host_info);
                        m_ai_cache_map.erase(a_label);
                        delete l_host_info;
                        l_host_info = NULL;
                }
        }
        return l_host_info;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
host_info *ai_cache::lookup(const std::string a_label, host_info *a_host_info)
{
        host_info *l_host_info = lookup(a_label);
        if(!l_host_info)
        {
                m_ai_cache_map[a_label] = a_host_info;
                l_host_info = a_host_info;
        }
        else
        {
                delete a_host_info;
        }
        return l_host_info;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void ai_cache::add(const std::string a_label, host_info *a_host_info)
{
        ai_cache_map_t::iterator i_h = m_ai_cache_map.find(a_label);
        if(i_h != m_ai_cache_map.end())
        {
                if(i_h->second)
                {
                        delete i_h->second;
                        i_h->second = NULL;
                }
        }
        m_ai_cache_map[a_label] = a_host_info;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t ai_cache::sync(const std::string &a_ai_cache_file,
                       const ai_cache_map_t &a_ai_cache_map)
{
        int32_t l_status = 0;
        FILE *l_file_ptr = fopen(a_ai_cache_file.c_str(), "w+");
        if(l_file_ptr == NULL)
        {
                //NDBG_PRINT("Error performing fopen. Reason: %s\n", strerror(errno));
                return HURL_STATUS_ERROR;
        }
        fprintf(l_file_ptr, "[");
        uint32_t l_len = a_ai_cache_map.size();
        uint32_t i_len = 0;
        for(ai_cache_map_t::const_iterator i_entry = a_ai_cache_map.begin();
            i_entry != a_ai_cache_map.end();
            ++i_entry)
        {
                std::string l_ai_val64;
                l_ai_val64 = base64_encode((const unsigned char *)(i_entry->second),
                                           sizeof(host_info));
                fprintf(l_file_ptr, "{");
                fprintf(l_file_ptr, "\"host\": \"%s\",", i_entry->first.c_str());
                fprintf(l_file_ptr, "\"ai\": \"%s\"", l_ai_val64.c_str());
                ++i_len;
                if(i_len == l_len)
                fprintf(l_file_ptr, "}");
                else
                fprintf(l_file_ptr, "},");
        }
        fprintf(l_file_ptr, "]");
        l_status = fclose(l_file_ptr);
        if(l_status != 0)
        {
                NDBG_PRINT("Error performing fclose. Reason: %s\n", strerror(errno));
                return HURL_STATUS_ERROR;
        }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t ai_cache::read(const std::string &a_ai_cache_file,
                       ai_cache_map_t &ao_ai_cache_map)
{
        struct stat l_stat;
        int32_t l_status = HURL_STATUS_OK;
        l_status = stat(a_ai_cache_file.c_str(), &l_stat);
        if(l_status != 0)
        {
                //NDBG_PRINT("Error performing stat on file: %s.  Reason: %s\n",
                //           a_ai_cache_file.c_str(), strerror(errno));
                return HURL_STATUS_OK;
        }

        if(!(l_stat.st_mode & S_IFREG))
        {
                //NDBG_PRINT("Error opening file: %s.  Reason: is NOT a regular file\n",
                //           a_ai_cache_file.c_str());
                return HURL_STATUS_OK;
        }

        FILE * l_file;
        l_file = fopen(a_ai_cache_file.c_str(),"r");
        if (NULL == l_file)
        {
                //NDBG_PRINT("Error opening file: %s.  Reason: %s\n",
                //           a_ai_cache_file.c_str(), strerror(errno));
                return HURL_STATUS_OK;
        }

        // Read in file...
        int32_t l_size = l_stat.st_size;
        int32_t l_read_size;
        char *l_buf = (char *)malloc(sizeof(char)*l_size);
        l_read_size = fread(l_buf, 1, l_size, l_file);
        if(l_read_size != l_size)
        {
                //NDBG_PRINT("Error performing fread.  Reason: %s [%d:%d]\n",
                //                strerror(errno), l_read_size, l_size);
                return HURL_STATUS_OK;
        }
        std::string l_buf_str;
        l_buf_str.assign(l_buf, l_size);
        // NOTE: rapidjson assert's on errors -interestingly
        rapidjson::Document l_doc;
        l_doc.Parse(l_buf_str.c_str());
        if(!l_doc.IsArray())
        {
                //NDBG_PRINT("Error reading json from file: %s. Data not an array\n",
                //                a_ai_cache_file.c_str());
                if(l_buf)
                {
                        free(l_buf);
                        l_buf = NULL;
                }
                return HURL_STATUS_OK;
        }
        // rapidjson uses SizeType instead of size_t.
        for(rapidjson::SizeType i_record = 0; i_record < l_doc.Size(); ++i_record)
        {
                if(!l_doc[i_record].IsObject())
                {
                        //NDBG_PRINT("Error reading json from file: %s. Array member not an object\n",
                        //           a_ai_cache_file.c_str());
                        if(l_buf)
                        {
                                free(l_buf);
                                l_buf = NULL;
                        }
                        return HURL_STATUS_OK;
                }
                std::string l_host;
                std::string l_ai;
                if(l_doc[i_record].HasMember("host"))
                {
                        l_host = l_doc[i_record]["host"].GetString();

                        if(l_doc[i_record].HasMember("ai"))
                        {
                                l_ai = l_doc[i_record]["ai"].GetString();

                                std::string l_ai_decoded;
                                l_ai_decoded = base64_decode(l_ai);
                                host_info *l_host_info = new host_info();
                                memcpy(l_host_info, l_ai_decoded.data(), l_ai_decoded.length());
                                ao_ai_cache_map[l_host] = l_host_info;
                        }
                }
        }
        l_status = fclose(l_file);
        if (HURL_STATUS_OK != l_status)
        {
                NDBG_PRINT("Error performing fclose.  Reason: %s\n", strerror(errno));
                if(l_buf)
                {
                        free(l_buf);
                        l_buf = NULL;
                }
                return HURL_STATUS_ERROR;
        }
        if(l_buf)
        {
                free(l_buf);
                l_buf = NULL;
        }
        return HURL_STATUS_OK;
}
}
