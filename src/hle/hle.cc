//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hle.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    02/07/2014
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
#include "ndebug.h"
#include "hle.h"
#include "t_client.h"
#include "reqlet_repo.h"
#include "reqlet.h"

//#include "util.h"

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hle::set_header(const std::string &a_header_key, const std::string &a_header_val)
{
        int32_t l_retval = STATUS_OK;
        m_header_map[a_header_key] = a_header_val;
        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hle::run(host_list_t &a_host_list)
{
        int32_t l_retval = STATUS_OK;
        reqlet_repo *l_reqlet_repo = NULL;

        // Check is initialized
        if(!m_is_initd)
        {
                l_retval = init();
                if(STATUS_OK != l_retval)
                {
                        NDBG_PRINT("Error: performing init.\n");
                        return STATUS_ERROR;
                }
        }

        // Create the reqlet list
        l_reqlet_repo = reqlet_repo::get();
        uint32_t l_reqlet_num = 0;
        for(host_list_t::iterator i_host = a_host_list.begin();
                        i_host != a_host_list.end();
                        ++i_host, ++l_reqlet_num)
        {
                // Create a re
                reqlet *l_reqlet = new reqlet(l_reqlet_num, 1);
                l_reqlet->init_with_url(m_url);

                // override host
                l_reqlet->set_host(*i_host);

                // Add to list
                l_reqlet_repo->add_reqlet(l_reqlet);

        }

        // -------------------------------------------
        // Create t_client list...
        // -------------------------------------------
        for(uint32_t i_client_idx = 0; i_client_idx < m_num_threads; ++i_client_idx)
        {

                if(m_verbose)
                {
                        NDBG_PRINT("Creating...\n");
                }

                // Construct with settings...
                t_client *l_t_client = new t_client(
                                m_verbose,
                                m_color,
                                m_sock_opt_recv_buf_size,
                                m_sock_opt_send_buf_size,
                                m_sock_opt_no_delay,
                                m_timeout_s,
                                m_cipher_list,
                                m_ssl_ctx,
                                m_evr_loop_type,
                                m_start_parallel
                );

                for(header_map_t::iterator i_header = m_header_map.begin();i_header != m_header_map.end(); ++i_header)
                {
                        l_t_client->set_header(i_header->first, i_header->second);
                }

                m_t_client_list.push_back(l_t_client);
        }

        // -------------------------------------------
        // Run...
        // -------------------------------------------
        for(t_client_list_t::iterator i_t_client = m_t_client_list.begin();
                        i_t_client != m_t_client_list.end();
                        ++i_t_client)
        {
                if(m_verbose)
                {
                        NDBG_PRINT("Running...\n");
                }
                (*i_t_client)->run();
        }

        return l_retval;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hle::stop(void)
{
        int32_t l_retval = STATUS_OK;

        for (t_client_list_t::iterator i_t_client = m_t_client_list.begin();
                        i_t_client != m_t_client_list.end();
                        ++i_t_client)
        {
                (*i_t_client)->stop();
        }

        return l_retval;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hle::wait_till_stopped(void)
{
        int32_t l_retval = STATUS_OK;

        // -------------------------------------------
        // Join all threads before exit
        // -------------------------------------------
        for(t_client_list_t::iterator i_client = m_t_client_list.begin();
                        i_client != m_t_client_list.end(); ++i_client)
        {

                //if(m_verbose)
                //{
                // 	NDBG_PRINT("joining...\n");
                //}
                pthread_join(((*i_client)->m_t_run_thread), NULL);

        }

        return l_retval;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool hle::is_running(void)
{
        for (t_client_list_t::iterator i_client_hle = m_t_client_list.begin();
                        i_client_hle != m_t_client_list.end(); ++i_client_hle)
        {
                if((*i_client_hle)->is_running())
                        return true;
        }

        return false;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hle::init(void)
{
        // Check if already is initd
        if(m_is_initd)
                return STATUS_OK;

        // SSL init...
        m_ssl_ctx = nconn_ssl_init(m_cipher_list);
        if(NULL == m_ssl_ctx) {
                NDBG_PRINT("Error: performing ssl_init with cipher_list: %s\n", m_cipher_list.c_str());
                return STATUS_ERROR;
        }

        //NDBG_PRINT("%sINIT%s: DONE\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF);

        // -------------------------------------------
        // Start async resolver
        // -------------------------------------------
        //t_async_resolver::get()->run();

        m_is_initd = true;
        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hle::hle(void):
        m_t_client_list(),
        m_ssl_ctx(NULL),
        m_is_initd(false),
        m_cipher_list(""),
        m_verbose(false),
        m_color(false),
        m_quiet(false),
        m_sock_opt_recv_buf_size(0),
        m_sock_opt_send_buf_size(0),
        m_sock_opt_no_delay(false),
        m_evr_loop_type(EVR_LOOP_EPOLL),
        m_start_parallel(100),
        m_num_threads(4),
        m_url(),
        m_header_map(),
        m_timeout_s(HLE_DEFAULT_CONN_TIMEOUT_S)
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hle::~hle()
{
        // -------------------------------------------
        // Delete t_client list...
        // -------------------------------------------
        for(t_client_list_t::iterator i_client_hle = m_t_client_list.begin();
                        i_client_hle != m_t_client_list.end(); )
        {

                t_client *l_t_client_ptr = *i_client_hle;
                delete l_t_client_ptr;
                m_t_client_list.erase(i_client_hle++);

        }

        // SSL Cleanup
        nconn_kill_locks();

        // TODO Deprecated???
        //EVP_cleanup();

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hle *hle::get(void)
{
        if (m_singleton_ptr == NULL) {
                //If not yet created, create the singleton instance
                m_singleton_ptr = new hle();

                // Initialize

        }
        return m_singleton_ptr;
}

//: ----------------------------------------------------------------------------
//: Class variables
//: ----------------------------------------------------------------------------
// the pointer to the singleton for the instance 
hle *hle::m_singleton_ptr;

