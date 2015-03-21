//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    api_server.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    12/07/2014
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
#include "api_server.h"
#include "hlx_client.h"
#include "ndebug.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define MAX_PENDING_CONNECT_REQUESTS 5
#define MAX_READ_SIZE 16384
#define RESP_BUFFER_SIZE (1024*1024)

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
// Set socket option macro...
#define SET_SOCK_OPT(_sock_fd, _sock_opt_level, _sock_opt_name, _sock_opt_val) \
        do { \
                int _l__sock_opt_val = _sock_opt_val; \
                int _l_status = 0; \
                _l_status = ::setsockopt(_sock_fd, \
                                _sock_opt_level, \
                                _sock_opt_name, \
                                &_l__sock_opt_val, \
                                sizeof(_l__sock_opt_val)); \
                                if (_l_status == -1) { \
                                        NDBG_PRINT("STATUS_ERROR: Failed to set %s count.  Reason: %s.\n", #_sock_opt_name, strerror(errno)); \
                                        return STATUS_ERROR;\
                                } \
                                \
        } while(0)

//: ----------------------------------------------------------------------------
//: http-parser callbacks
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int api_server::hp_on_message_begin(http_parser* a_parser)
{
#if 0
        api_server *l_api_server = static_cast <api_server *>(a_parser->data);
        if(l_api_server)
        {
                if(l_api_server->m_verbose)
                {
                        NDBG_OUTPUT("message begin\n");
                }
        }
#endif

        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int api_server::hp_on_url(http_parser* a_parser, const char *a_at, size_t a_length)
{
        api_server *l_api_server = static_cast <api_server *>(a_parser->data);
        if(l_api_server)
        {
#if 0
                if(l_api_server->m_verbose)
                {
                        if(l_api_server->m_color)
                                NDBG_OUTPUT("url:   %s%.*s%s\n", ANSI_COLOR_FG_YELLOW, (int)a_length, a_at, ANSI_COLOR_OFF);
                        else
                                NDBG_OUTPUT("url:   %.*s\n", (int)a_length, a_at);
                }
#endif

                std::string l_url;
                l_url.assign(a_at, a_length);
                //l_api_server->m_request.set_uri(l_url);

        }

        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int api_server::hp_on_header_field(http_parser* a_parser, const char *a_at, size_t a_length)
{
        api_server *l_api_server = static_cast <api_server *>(a_parser->data);
        if(l_api_server)
        {
#if 0
                if(l_api_server->m_verbose)
                {
                        if(l_api_server->m_color)
                                NDBG_OUTPUT("field:  %s%.*s%s\n", ANSI_COLOR_FG_BLUE, (int)a_length, a_at, ANSI_COLOR_OFF);
                        else
                                NDBG_OUTPUT("field:  %.*s\n", (int)a_length, a_at);
                }
#endif

                //l_api_server->m_next_header.assign(a_at, a_length);
        }

        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int api_server::hp_on_header_value(http_parser* a_parser, const char *a_at, size_t a_length)
{
        api_server *l_api_server = static_cast <api_server *>(a_parser->data);
        if(l_api_server)
        {
#if 0
                if(l_api_server->m_verbose)
                {
                        if(l_api_server->m_color)
                                NDBG_OUTPUT("value:  %s%.*s%s\n", ANSI_COLOR_FG_GREEN, (int)a_length, a_at, ANSI_COLOR_OFF);
                        else
                                NDBG_OUTPUT("value:  %.*s\n", (int)a_length, a_at);
                }
#endif

                std::string l_val;
                l_val.assign(a_at, a_length);
                //l_api_server->m_request.set_header(l_api_server->m_next_header, l_val);

        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int api_server::hp_on_headers_complete(http_parser* a_parser)
{
        api_server *l_api_server = static_cast <api_server *>(a_parser->data);
        if(l_api_server)
        {
#if 0
                if(l_api_server->m_verbose)
                {
                        NDBG_OUTPUT("headers_complete\n");
                }
#endif
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int api_server::hp_on_body(http_parser* a_parser, const char *a_at, size_t a_length)
{
        api_server *l_api_server = static_cast <api_server *>(a_parser->data);
        if(l_api_server)
        {
#if 0
                if(l_api_server->m_verbose)
                {
                        if(l_api_server->m_color)
                                NDBG_OUTPUT("body:  %s%.*s%s\n", ANSI_COLOR_FG_YELLOW, (int)a_length, a_at, ANSI_COLOR_OFF);
                        else
                                NDBG_OUTPUT("body:  %.*s\n", (int)a_length, a_at);
                }
#endif

                //l_api_server->m_body.append(a_at, a_length);

        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int api_server::hp_on_message_complete(http_parser* a_parser)
{
        api_server *l_api_server = static_cast <api_server *>(a_parser->data);
        if(l_api_server)
        {
#if 0
                if(l_api_server->m_verbose)
                {
                        NDBG_OUTPUT("message complete\n");
                }
#endif

                // we outtie
                //l_api_server->m_request.set_body(l_api_server->m_body);
                l_api_server->m_cur_msg_complete = true;
        }

        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t api_server::start(uint16_t a_port)
{
        int32_t l_pthread_error = 0;
        m_port = a_port;

        m_stopped = false;
        l_pthread_error = pthread_create(&m_t_run_thread,
                        NULL,
                        t_run_static,
                        this);
        if (l_pthread_error != 0)
        {
                // failed to create thread

                NDBG_PRINT("Error: creating thread.  Reason: %s\n.", strerror(l_pthread_error));
                return STATUS_ERROR;

        }

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t create_tcp_server_socket(uint16_t a_port)
{
        int l_sock_fd;
        sockaddr_in l_server_address;
        int32_t l_status;

        // -------------------------------------------
        // Create socket for incoming connections
        // -------------------------------------------
        l_sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (l_sock_fd < 0)
        {
                NDBG_PRINT("Error socket() failed. Reason[%d]: %s\n", errno, strerror(errno));
                return STATUS_ERROR;
        }

        // -------------------------------------------
        // Set socket options
        // -Enable Socket reuse.
        // -------------------------------------------
        SET_SOCK_OPT(l_sock_fd, SOL_SOCKET, SO_REUSEADDR, 1);

        // -------------------------------------------
        // Construct local address structure
        // -------------------------------------------
        memset(&l_server_address, 0, sizeof(l_server_address)); // Zero out structure

        l_server_address.sin_family      = AF_INET;             // Internet address family
        l_server_address.sin_addr.s_addr = htonl(INADDR_ANY);   // Any incoming interface
        l_server_address.sin_port        = htons(a_port);         // Local port

        // -------------------------------------------
        // Bind to the local address
        // -------------------------------------------
        l_status = bind(l_sock_fd, (struct sockaddr *) &l_server_address, sizeof(l_server_address));
        if (l_status < 0)
        {
                NDBG_PRINT("Error bind() failed (port: %d). Reason[%d]: %s\n", a_port, errno, strerror(errno));
                return STATUS_ERROR;
        }

        // -------------------------------------------
        // Mark the socket so it will listen for
        // incoming connections
        // -------------------------------------------
        l_status = listen(l_sock_fd, MAX_PENDING_CONNECT_REQUESTS);
        if (l_status < 0)
        {
                NDBG_PRINT("Error listen() failed. Reason[%d]: %s\n", errno, strerror(errno));
                return STATUS_ERROR;
        }

        return l_sock_fd;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int accept_tcp_connection(int a_server_sock_fd)
{
    int l_client_sock_fd;
    sockaddr_in l_client_address;
    uint32_t l_sockaddr_in_length;

    //Set the size of the in-out parameter
    l_sockaddr_in_length = sizeof(sockaddr_in);

    //Wait for a client to connect
    l_client_sock_fd = accept(a_server_sock_fd, (struct sockaddr *)&l_client_address, &l_sockaddr_in_length);
    if (l_client_sock_fd < 0)
    {
            //NDBG_PRINT("Error accept failed. Reason[%d]: %s\n", errno, strerror(errno));
            return STATUS_ERROR;
    }

    return l_client_sock_fd;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *api_server::t_run(void *a_nothing)
{

        // -------------------------------------------
        //  Create server socket
        // -------------------------------------------
        m_server_fd = create_tcp_server_socket(m_port);
        if(m_server_fd == STATUS_ERROR)
        {
            NDBG_PRINT("Error performing create_tcp_server_socket with port number = %d", m_port);
            return NULL;
        }

        // -------------------------------------------
        // Main loop.
        // -------------------------------------------
        //NDBG_PRINT("starting main loop\n");
        char l_read_buf[MAX_READ_SIZE];
        while(!m_stopped)
        {
                // -------------------------------------------
                // Accept client connections
                // -------------------------------------------
                int l_client_sock_fd;
                l_client_sock_fd = accept_tcp_connection(m_server_fd);
                if(l_client_sock_fd == STATUS_ERROR)
                {
                        //NDBG_PRINT("Error performing accept_tcp_connection with socket number = %d\n",l_client_sock_fd);
                        return NULL;
                }

                //m_next_header.clear();
                //m_body.clear();
                //m_request.clear();

                // Setup parser
                m_http_parser.data = this;
                http_parser_init(&m_http_parser, HTTP_REQUEST);

                // -------------------------------------------
                // Read till close...
                // -------------------------------------------
                uint32_t l_read_size = 0;
                l_read_size = read(l_client_sock_fd, l_read_buf, MAX_READ_SIZE);
                //NDBG_PRINT("Read data[size: %d]\n", l_read_size);
                //if(l_read_size > 0)
                //{
                //        ns_ndebug::mem_display((const uint8_t *)l_read_buf, l_read_size);
                //}

                // -------------------------------------------
                // Parse
                // -------------------------------------------
                m_cur_msg_complete = false;
                while(1)
                {
                        size_t l_parse_status = 0;
                        int32_t l_read_buf_idx = 0;
                        //NDBG_PRINT("%sHTTP_PARSER%s: m_read_buf: %p, m_read_buf_idx: %d, l_bytes_read: %d\n",
                        //                ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF,
                        //                l_read_buf, (int)l_read_buf_idx, (int)l_read_size);
                        l_parse_status = http_parser_execute(&m_http_parser, &m_http_parser_settings, l_read_buf + l_read_buf_idx, l_read_size - l_read_buf_idx);
                        if(l_parse_status < 0)
                        {
                                NDBG_PRINT("Error: parse error.  Reason: %s: %s\n",
                                                http_errno_name((enum http_errno)m_http_parser.http_errno),
                                                http_errno_description((enum http_errno)m_http_parser.http_errno));
                                return NULL;
                        }
                        l_read_buf_idx += l_parse_status;

                        if(m_cur_msg_complete)
                                break;
                }

                // -------------------------------------------
                // Process request
                // -------------------------------------------
                std::string l_response_body = "{ \"dude\": \"cool\"}";

                if(m_hlx_client)
                {
                        char l_char_buf[2048];
                        m_hlx_client->get_stats_json(l_char_buf, 2048);
                        l_response_body = l_char_buf;
                }

                // -------------------------------------------
                // Show
                // -------------------------------------------
                //m_request.show(m_color);

                // -------------------------------------------
                // Write response
                // -------------------------------------------
                std::string l_response  = "";
                l_response += "HTTP/1.1 200 OK\r\n";
                l_response += "Content-Type: application/json\r\n";
                l_response += "Access-Control-Allow-Origin: *";
                l_response += "Access-Control-Allow-Credentials: true";
                l_response += "Access-Control-Max-Age: 86400";
                l_response += "Content-Length: ";
                char l_len_str[64]; sprintf(l_len_str, "%d", (int)l_response_body.length());
                l_response += l_len_str;
                l_response += "\r\n\r\n";
                l_response += l_response_body;

                uint32_t l_write_size = 0;
                l_write_size = write(l_client_sock_fd, l_response.c_str(), l_response.length());
                if(l_write_size != l_response.length())
                {
                        NDBG_PRINT("Error write failed. Reason[%d]: %s\n", errno, strerror(errno));
                        return NULL;

                }
                //NDBG_PRINT("Write data[size: %d]\n", l_write_size);

                // -------------------------------------------
                // Close
                // -------------------------------------------
                close(l_client_sock_fd);

        }

        m_stopped = true;

        return NULL;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t api_server::stop(void)
{
        int32_t l_retval = STATUS_OK;

        m_stopped = true;
        shutdown(m_server_fd, SHUT_RD);

        return l_retval;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t api_server::wait_till_stopped(void)
{
        int32_t l_retval = STATUS_OK;

        // -------------------------------------------
        // Join thread(s) before exit
        // -------------------------------------------
        pthread_join(m_t_run_thread, NULL);

        return l_retval;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
api_server::api_server(void):
        m_verbose(false),
        m_color(false),
        m_stats(false),
        m_port(23456),
        m_hlx_client(NULL),
        m_http_parser_settings(),
        m_http_parser(),
        m_stopped(true),
        m_t_run_thread(),
        m_server_fd(-1),
        m_cur_msg_complete(false)
{

        // Set up callbacks...
        m_http_parser_settings.on_message_begin = hp_on_message_begin;
        m_http_parser_settings.on_url = hp_on_url;
        m_http_parser_settings.on_status = NULL;
        m_http_parser_settings.on_header_field = hp_on_header_field;
        m_http_parser_settings.on_header_value = hp_on_header_value;
        m_http_parser_settings.on_headers_complete = hp_on_headers_complete;
        m_http_parser_settings.on_body = hp_on_body;
        m_http_parser_settings.on_message_complete = hp_on_message_complete;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
api_server::~api_server()
{
        // TODO
}

