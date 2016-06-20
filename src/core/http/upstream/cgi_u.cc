//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    cgi_u.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    05/28/2016
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
#include "clnt_session.h"
#include "t_srvr.h"
#include "evr.h"
#include "nbq.h"
#include "hlx/cgi_u.h"
#include "hlx/status.h"
#include "hlx/trace.h"
#include "hlx/api_resp.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

//: ----------------------------------------------------------------------------
//: Constants
//: TODO would like to use constexpr -but need to support < C++11
//: ----------------------------------------------------------------------------
#ifndef CGI_U_FD_ERROR_MSG
#define CGI_U_FD_ERROR_MSG "cgi error\n"
#endif

#ifndef CGI_U_FD_TIMEOUT_MSG
#define CGI_U_FD_TIMEOUT_MSG "cgi timeout\n"
#endif

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Event loop callbacks
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t evr_fd_readable_cb(void *a_data)
{
        //NDBG_PRINT("READABLE\n");
        cgi_u *l_cgi_u = static_cast<cgi_u *>(a_data);
        if(!l_cgi_u)
        {
                TRC_ERROR("l_cgi_u == NULL\n");
                return HLX_STATUS_ERROR;
        }
        // ---------------------------------------
        // buffered
        // ---------------------------------------
        int32_t l_s;
        l_s = l_cgi_u->init_q();
        if(l_s == HLX_STATUS_ERROR)
        {
                TRC_ERROR("l_cgi_u->init_q()\n");
                return HLX_STATUS_ERROR;
        }
        nbq *l_nbq = NULL;
        l_nbq = l_cgi_u->m_nbq;
        // ---------------------------------------
        // TODO unbuffered
        // ---------------------------------------
        //l_nbq = l_clnt_session.m_out_q;
        //if(!l_nbq)
        //{
        //        TRC_ERROR("l_nbq == NULL\n");
        //        return HLX_STATUS_ERROR;
        //}

        ssize_t l_read_total = 0;
        ssize_t l_read = 0;
        // ---------------------------------------
        // TODO unbuffered
        // ---------------------------------------
        //bool l_q_output = false;
        do {
                ssize_t l_last;
                l_read = l_nbq->write_fd(l_cgi_u->m_fd, 1024, l_last);
                //NDBG_PRINT("l_read: %ld l_last: %ld\n", l_read, l_last);
                if(l_read == HLX_STATUS_ERROR)
                {
                        //TRC_ERROR("performing read. Reason: %s\n", strerror(errno));
                        if(errno == EAGAIN)
                        {
                                break;
                        }
                        return HLX_STATUS_ERROR;
                }
                if(l_read > 0)
                {
                        // ---------------------------------------
                        // TODO unbuffered
                        // ---------------------------------------
                        //l_q_output = true;
                        l_read_total += l_read;
                }
                if(l_last == 0)
                {
                        //l_cgi_u->m_pid = -1;
                        // ---------------------------------------
                        // TODO unbuffered
                        // ---------------------------------------
                        //l_q_output = true;
                        l_cgi_u->ups_cancel();
                        break;
                }
        } while(l_read > 0);

        // ---------------------------------------
        // TODO unbuffered
        // ---------------------------------------
        //if(l_q_output)
        //{
        //        int32_t l_s;
        //        l_s = l_clnt_session.m_t_srvr->queue_output(l_clnt_session);
        //        if(l_s != HLX_STATUS_OK)
        //        {
        //                TRC_ERROR("performing queue_output\n");
        //                return HLX_STATUS_ERROR;
        //        }
        //}

        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t evr_fd_writeable_cb(void *a_data)
{
        //NDBG_PRINT("WRITEABLE\n");
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t evr_fd_error_cb(void *a_data)
{
        cgi_u *l_cgi_u = static_cast<cgi_u *>(a_data);
        if(!l_cgi_u)
        {
                TRC_ERROR("l_cgi_u == NULL\n");
                return HLX_STATUS_ERROR;
        }
        TRC_WARN("cgi fd error\n");
        int32_t l_s;
        // ---------------------------------------
        // buffered
        // ---------------------------------------
        l_s = l_cgi_u->init_q();
        if(l_s == HLX_STATUS_ERROR)
        {
                TRC_ERROR("l_cgi_u->init_q()\n");
                return HLX_STATUS_ERROR;
        }
        int64_t l_written;
        l_written = l_cgi_u->m_nbq->write(CGI_U_FD_ERROR_MSG, sizeof(CGI_U_FD_ERROR_MSG));
        if(l_written < 0)
        {
                TRC_ERROR("performing nbq->write\n");
                return HLX_STATUS_ERROR;
        }
        l_s = l_cgi_u->ups_cancel();
        if(l_s != HLX_STATUS_OK)
        {
                TRC_ERROR("performing ups_cancel\n");
                return HLX_STATUS_ERROR;
        }
        // ---------------------------------------
        // TODO unbuffered
        // ---------------------------------------
        //clnt_session &l_clnt_session = l_cgi_u->get_clnt_session();
        //nbq *l_nbq = l_clnt_session.m_out_q;
        //if(!l_nbq)
        //{
        //        TRC_ERROR("l_nbq == NULL\n");
        //        //return HLX_STATUS_ERROR;
        //}
        //int64_t l_written;
        //l_written = l_nbq->write(CGI_U_FD_ERROR_MSG, sizeof(CGI_U_FD_ERROR_MSG));
        //if(l_written < 0)
        //{
        //        TRC_ERROR("performing nbq->write\n");
        //        //return HLX_STATUS_ERROR;
        //}
        //l_s = l_clnt_session.m_t_srvr->queue_output(l_clnt_session);
        //if(l_s != HLX_STATUS_OK)
        //{
        //        TRC_ERROR("performing queue_output\n");
        //        return HLX_STATUS_ERROR;
        //}

        l_cgi_u->flush();
        // TODO write error message
        // TODO flush output

        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t evr_fd_timeout_cb(void *a_ctx, void *a_data)
{
        cgi_u *l_cgi_u = static_cast<cgi_u *>(a_data);
        if(!l_cgi_u)
        {
                TRC_ERROR("l_cgi_u == NULL\n");
                return HLX_STATUS_ERROR;
        }
        TRC_WARN("cgi fd timeout\n");
        int32_t l_s;
        // ---------------------------------------
        // buffered
        // ---------------------------------------
        l_s = l_cgi_u->init_q();
        if(l_s == HLX_STATUS_ERROR)
        {
                TRC_ERROR("l_cgi_u->init_q()\n");
                return HLX_STATUS_ERROR;
        }
        int64_t l_written;
        l_written = l_cgi_u->m_nbq->write(CGI_U_FD_TIMEOUT_MSG, sizeof(CGI_U_FD_TIMEOUT_MSG));
        if(l_written < 0)
        {
                TRC_ERROR("performing nbq->write\n");
                return HLX_STATUS_ERROR;
        }

        l_s = l_cgi_u->ups_cancel();
        if(l_s != HLX_STATUS_OK)
        {
                TRC_ERROR("performing ups_cancel\n");
                return HLX_STATUS_ERROR;
        }

        // ---------------------------------------
        // TODO unbuffered
        // ---------------------------------------
        //clnt_session &l_clnt_session = l_cgi_u->get_clnt_session();
        //nbq *l_nbq = l_clnt_session.m_out_q;
        //if(!l_nbq)
        //{
        //        TRC_ERROR("l_nbq == NULL\n");
        //        //return HLX_STATUS_ERROR;
        //}
        //int64_t l_written;
        //l_written = l_nbq->write(CGI_U_FD_TIMEOUT_MSG, sizeof(CGI_U_FD_TIMEOUT_MSG));
        //if(l_written < 0)
        //{
        //        TRC_ERROR("performing nbq->write\n");
        //        //return HLX_STATUS_ERROR;
        //}
        //l_s = l_clnt_session.m_t_srvr->queue_output(l_clnt_session);
        //if(l_s != HLX_STATUS_OK)
        //{
        //        TRC_ERROR("performing queue_output\n");
        //        return HLX_STATUS_ERROR;
        //}

        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: Constructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
cgi_u::cgi_u(clnt_session &a_clnt_session, int32_t a_timeout_ms):
        base_u(a_clnt_session),
        m_pid(),
        m_fd(-1),
        m_evr_fd(),
        m_evr_loop(NULL),
        m_timeout_ms(a_timeout_ms),
        m_timer_obj(NULL),
        m_nbq(NULL),
        m_supports_keep_alives(false)
{
}

//: ----------------------------------------------------------------------------
//: \details: Destructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
cgi_u::~cgi_u()
{
        if(m_clnt_session.m_t_srvr == NULL)
        {
                TRC_ERROR("m_clnt_session.m_t_srvr == NULL\n");
        }
        m_clnt_session.m_t_srvr->release_nbq(m_nbq);
        m_nbq = NULL;
}

//: ----------------------------------------------------------------------------
//: \details: Setup script for exec
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t cgi_u::cginit(const char *a_path, const query_map_t &a_query_map)
{
        if(!m_clnt_session.m_t_srvr)
        {
                TRC_ERROR("m_clnt_session.m_t_srvr == NULL\n");
                return HLX_STATUS_ERROR;
        }
        m_evr_loop = m_clnt_session.m_t_srvr->get_evr_loop();
        if(!m_evr_loop)
        {
                TRC_ERROR("m_clnt_session.m_t_srvr->m_evr_loop == NULL\n");
                return HLX_STATUS_ERROR;
        }

        // ---------------------------------------
        // Write headers
        // TODO unbuffered
        // ---------------------------------------
        //nbq &l_q = *(m_clnt_session.m_out_q);
        //srvr *l_srvr = get_srvr(m_clnt_session);
        //if(!l_srvr)
        //{
        //        TRC_ERROR("get_srvr == NULL\n");
        //        return HLX_STATUS_ERROR;
        //}
        //nbq_write_status(l_q, HTTP_STATUS_OK);
        //nbq_write_header(l_q, "Server", l_srvr->get_server_name().c_str());
        //nbq_write_header(l_q, "Date", get_date_str());
        //nbq_write_header(l_q, "Content-type", "text/html");
        //nbq_write_header(l_q, "Connection", "close");
        //l_q.write("\r\n", strlen("\r\n"));

        // ---------------------------------------
        // Check is +x file
        // ---------------------------------------
        struct stat l_stat;
        int32_t l_s = HLX_STATUS_OK;
        l_s = ::stat(a_path, &l_stat);
        if(l_s != 0)
        {
                TRC_ERROR("performing stat on path: %s. Reason: %s\n", a_path, strerror(errno));
                return HLX_STATUS_ERROR;
        }
        if(!(l_stat.st_mode & S_IFREG) ||
           !(l_stat.st_mode & S_IXUSR))
        {
                TRC_ERROR("path: %s is not regular or executable\n", a_path);
                return HLX_STATUS_ERROR;
        }

        int l_fds[2];
        // ---------------------------------------
        // create pipe between parent/child
        // ---------------------------------------
        errno = 0;
        l_s = pipe(l_fds);
        if (l_s == -1)
        {
                TRC_ERROR("performing pipe. Reason: %s\n", strerror(errno));
                return HLX_STATUS_ERROR;
        }
        // ---------------------------------------
        // fork...
        // ---------------------------------------
        errno = 0;
        m_pid = fork();
        if(m_pid == -1)
        {
                TRC_ERROR("performing fork. Reason: %s\n", strerror(errno));
                if(l_fds[0] > 0) { close(l_fds[0]); l_fds[0] = -1; }
                if(l_fds[1] > 0) { close(l_fds[1]); l_fds[1] = -1; }
                return HLX_STATUS_ERROR;
        }

        // ---------------------------------------
        // child...
        // ---------------------------------------
        if(m_pid == 0)
        {
                //TRC_PRINT("PID[%4d]: \n", m_pid);
                close(STDIN_FILENO);
                close(STDOUT_FILENO);
                close(STDERR_FILENO);
                close(l_fds[0]);
                l_fds[0] = -1;

                // dup2 stdout
                l_s  = TEMP_FAILURE_RETRY(dup2(l_fds[1], STDOUT_FILENO));
                if(l_s == -1)
                {
                        TRC_ERROR("performing dup2[status %d].  Reason: %s\n", l_s, strerror(errno));
                        close(l_fds[1]);
                        l_fds[1] = -1;
                        return HLX_STATUS_ERROR;
                }
                l_s  = TEMP_FAILURE_RETRY(dup2(l_fds[1], STDERR_FILENO));
                if(l_s == -1)
                {
                        TRC_ERROR("performing dup2[status %d].  Reason: %s\n", l_s, strerror(errno));
                        close(l_fds[1]);
                        l_fds[1] = -1;
                        return HLX_STATUS_ERROR;
                }

                // -----------------------------------------
                // TODO not cleaning up -since fork/exec
                // -let process cleanup?
                // -----------------------------------------
                char *const l_argv[2] = { const_cast<char *>(a_path), 0 };
                char **l_envp;
                l_envp = (char **)malloc(sizeof(char *)*a_query_map.size()+1);
                uint32_t i_i = 0;
                for(query_map_t::const_iterator i_q = a_query_map.begin();
                    i_q != a_query_map.end();
                    ++i_q, ++i_i)
                {
                        int l_as;
                        l_as = asprintf(&l_envp[i_i], "%s=%s",i_q->first.c_str(),i_q->second.c_str());
                        if(l_as == -1)
                        {
                                TRC_ERROR("asprintf. Reason: %s\n", strerror(errno));
                                continue;
                        }
                }
                l_envp[i_i] = 0;
                //TRC_PRINT("starting script.\n");
                errno = 0;
                l_s = execve(l_argv[0], l_argv, l_envp);
                // exec failed...
                if(l_s == -1)
                {
                        TRC_ERROR("performing execve. Reason: %s\n", strerror(errno));
                }
                // failed...
                _exit(1);
        }
        // ---------------------------------------
        // parent...
        // ---------------------------------------
        else
        {
                //TRC_PRINT("PID[%4d]: \n", l_pid);
                close(l_fds[1]);
                l_fds[1] = -1;

                m_fd = l_fds[0];

                // Set non-blocking...
                l_s = fcntl(m_fd, F_SETFL, O_NONBLOCK);
                if(l_s == -1)
                {
                        TRC_ERROR("performing fcntl(%d, F_SETFL, O_NONBLOCK). Reason: %s\n",
                                m_fd, strerror(errno));
                        close(m_fd);
                        m_fd = -1;
                        return HLX_STATUS_ERROR;
                }
        }

        // ---------------------------------------
        // Ensure out_q
        // ---------------------------------------
        if(!m_clnt_session.m_out_q)
        {
                if(!m_clnt_session.m_t_srvr)
                {
                        TRC_ERROR("a_clnt_session.m_t_srvr == NULL\n");
                        close(m_fd);
                        m_fd = -1;
                        return HLX_STATUS_ERROR;
                }
                m_clnt_session.m_out_q = m_clnt_session.m_t_srvr->get_nbq();
                if(!m_clnt_session.m_out_q)
                {
                        TRC_ERROR("a_clnt_session.m_out_q\n");
                        close(m_fd);
                        m_fd = -1;
                        return HLX_STATUS_ERROR;
                }
        }

        // ---------------------------------------
        // Add fd to event loop
        // ---------------------------------------
        m_evr_fd.m_magic = EVR_EVENT_FD_MAGIC;
        m_evr_fd.m_read_cb = evr_fd_readable_cb;
        m_evr_fd.m_write_cb = evr_fd_writeable_cb;
        m_evr_fd.m_error_cb = evr_fd_error_cb;
        m_evr_fd.m_data = this;

        // Add to event handler
        l_s = m_evr_loop->add_fd(m_fd,
                                 EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_RD_HUP|EVR_FILE_ATTR_MASK_ET,
                                 &m_evr_fd);
        if(l_s != HLX_STATUS_OK)
        {
                TRC_ERROR("Couldn't add socket m_fp\n");
                close(m_fd);
                return HLX_STATUS_ERROR;
        }

        // -------------------------------------------------
        // TODO remove after keep-alives supported for cgi
        // Disable keep-alives for now for cgi
        // -------------------------------------------------
        if(m_clnt_session.m_rqst)
        {
                m_clnt_session.m_rqst->m_supports_keep_alives = false;
        }

        // Add timeout
        if(m_timeout_ms > 0)
        {
                l_s = m_clnt_session.m_t_srvr->add_timer((uint32_t)m_timeout_ms,
                                                         evr_fd_timeout_cb,
                                                         this,
                                                         (void **)(&m_timer_obj));
                if(l_s != HLX_STATUS_OK)
                {
                        TRC_ERROR("performing add_timer\n");
                        close(m_fd);
                        return HLX_STATUS_ERROR;
                }
        }

        m_state = UPS_STATE_SENDING;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t cgi_u::init_q(void)
{
        if(m_nbq == NULL)
        {
                if(m_clnt_session.m_t_srvr == NULL)
                {
                        TRC_ERROR("m_clnt_session.m_t_srvr == NULL\n");
                        return HLX_STATUS_ERROR;
                }
                m_nbq = m_clnt_session.m_t_srvr->get_nbq();
                if(m_nbq == NULL)
                {
                        TRC_ERROR("m_nbq == NULL\n");
                        return HLX_STATUS_ERROR;
                }
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t cgi_u::flush(void)
{
        // ---------------------------------------
        // buffered
        // ---------------------------------------
        if(!m_nbq)
        {
                return HLX_STATUS_OK;
        }
        nbq *l_nbq = m_clnt_session.m_out_q;
        if(!l_nbq)
        {
                TRC_ERROR("l_nbq == NULL\n");
                return HLX_STATUS_ERROR;
        }

        // ---------------------------------------
        // write headers
        // buffered
        // ---------------------------------------
        srvr *l_srvr = get_srvr(m_clnt_session);
        if(!l_srvr)
        {
                TRC_ERROR("get_srvr == NULL\n");
                return HLX_STATUS_ERROR;
        }
        char l_length_str[32];
        sprintf(l_length_str, "%lu", m_nbq->read_avail());
        nbq_write_status(*l_nbq, HTTP_STATUS_OK);
        nbq_write_header(*l_nbq, "Server", l_srvr->get_server_name().c_str());
        nbq_write_header(*l_nbq, "Date", get_date_str());
        nbq_write_header(*l_nbq, "Content-Type", "text/html");
        nbq_write_header(*l_nbq, "Content-Length", l_length_str);
        if(m_supports_keep_alives)
        {
                nbq_write_header(*l_nbq, "Connection", "keep-alive");
        }
        else
        {
                nbq_write_header(*l_nbq, "Connection", "close");
        }
        l_nbq->write("\r\n", strlen("\r\n"));

        int64_t l_s;
        l_s = l_nbq->write_q(*m_nbq);
        if(l_s == HLX_STATUS_ERROR)
        {
                TRC_ERROR("l_nbq->write_q\n");
                return HLX_STATUS_ERROR;
        }
        int64_t l_sx;
        l_sx = m_clnt_session.m_t_srvr->queue_output(m_clnt_session);
        if(l_sx != HLX_STATUS_OK)
        {
                TRC_ERROR("performing queue_output\n");
                return HLX_STATUS_ERROR;

        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ssize_t cgi_u::ups_read(size_t a_len)
{
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t cgi_u::ups_cancel(void)
{
        //NDBG_PRINT("%sCANCEL%s: ...\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF);
        m_state = UPS_STATE_DONE;
        if(m_fd > 0)
        {
                int32_t l_s;
                l_s = m_evr_loop->del_fd(m_fd);
                if(l_s != HLX_STATUS_OK)
                {
                        TRC_ERROR("performing del_fd(%d)\n", m_fd);
                        return HLX_STATUS_ERROR;
                }
                close(m_fd);
                m_fd = -1;
        }
        if(m_pid > 0)
        {
                int32_t l_s;
                //NDBG_PRINT("%sKILL%s: ...\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                l_s = kill(m_pid, SIGKILL);
                if(l_s != 0)
                {
                        TRC_ERROR("performing kill(%d, SIGKILL)\n", m_pid);
                        return HLX_STATUS_ERROR;
                }
                pid_t l_pid;
                l_pid = ::wait(0);
                if(l_pid != m_pid)
                {
                        TRC_WARN("wait pid(%d) != cgi pid(%d)\n", l_pid, m_pid);
                }
                //NDBG_PRINT("KILL: m_pid: %d\n", m_pid);
                //NDBG_PRINT("KILL: l_pid: %d\n", l_pid);
                m_pid = -1;
        }
        // ---------------------------------------
        // buffered
        // ---------------------------------------
        int32_t l_s;
        l_s = flush();
        if(l_s != HLX_STATUS_OK)
        {
                TRC_ERROR("flush()\n");
                return HLX_STATUS_ERROR;
        }
        return HLX_STATUS_OK;
}


//: ----------------------------------------------------------------------------
//: Example...
//: ----------------------------------------------------------------------------
#if 0

#if 0
~>cat /tmp/shell_test.sh
#!/bin/bash
printenv
while true;
do
    echo "FROM_STDOUT"
    sleep 1
    >&2 echo "FROM_STDERR"
    sleep 1
done
#endif

#if 0
>g++ -Wall -Werror ./exec_test.cc -o exec_test && ./exec_test
Exec test.
PID[11114]:
PID[   0]:
CHILD_WROTE: starting script.

CHILD_WROTE: USER=superman
PWD=/home/rmorrison/gproj/hlx
SHLVL=1
LOGNAME=batman
_=/usr/bin/printenv

CHILD_WROTE: FROM_STDOUT

CHILD_WROTE: FROM_STDERR

CHILD_WROTE: FROM_STDOUT
...
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/wait.h>

int main(void)
{
        int l_fds[2];
        printf("Exec test.\n");

        // ---------------------------------------
        // create pipe between parent/child
        // ---------------------------------------
        int l_s;
        errno = 0;
        l_s = pipe(l_fds);
        if (l_s == -1)
        {
                printf("performing pipe. Reason: %s\n", strerror(errno));
                return -1;
        }

        // ---------------------------------------
        // fork...
        // ---------------------------------------
        pid_t l_pid;
        errno = 0;
        l_pid = fork();
        if(l_pid == -1)
        {
                printf("performing fork. Reason: %s\n", strerror(errno));
                return -1;
        }

        // ---------------------------------------
        // child...
        // ---------------------------------------
        if(l_pid == 0)
        {
                printf("PID[%4d]: \n", l_pid);
                close(STDIN_FILENO);
                close(STDOUT_FILENO);
                close(STDERR_FILENO);
                close(l_fds[0]);

                // dup2 stdout
                l_s  = TEMP_FAILURE_RETRY(dup2(l_fds[1], STDOUT_FILENO));
                if(l_s == -1)
                {
                        printf("performing dup2[status %d].  Reason: %s\n", l_s, strerror(errno));
                        return -1;
                }
                l_s  = TEMP_FAILURE_RETRY(dup2(l_fds[1], STDERR_FILENO));
                if(l_s == -1)
                {
                        printf("performing dup2[status %d].  Reason: %s\n", l_s, strerror(errno));
                        return -1;
                }

                char const *l_argv[] = { "/tmp/shell_test.sh", 0 };
                char const *l_envp[] =
                {
                    "USER=superman",
                    "LOGNAME=batman",
                    0
                };

                errno = 0;
                printf("starting script.\n");
                l_s = execve(l_argv[0], (char **)l_argv, (char**)l_envp);
                // exec failed...
                if(l_s == -1)
                {
                        printf("performing execve. Reason: %s\n", strerror(errno));
                }

                // failed...
                _exit(1);
        }
        // ---------------------------------------
        // parent...
        // ---------------------------------------
        else
        {
                printf("PID[%4d]: \n", l_pid);
                close(l_fds[1]);

                int l_cgi_fd = l_fds[0];

                // Set non-blocking...
                l_s = fcntl(l_cgi_fd, F_SETFL, O_NONBLOCK);
                if(l_s == -1)
                {
                        printf("performing fcntl(%d, F_SETFL, O_NONBLOCK). Reason: %s\n",
                                        l_cgi_fd, strerror(errno));
                        close(l_cgi_fd);
                        return -1;
                }

                // Read forever...
                char l_buf[4096];
                while (1)
                {
                        ssize_t l_c;
                        l_c = read(l_cgi_fd, l_buf, sizeof(l_buf));
                        if (l_c == -1)
                        {
                                if (errno == EINTR)
                                {
                                        continue;
                                }
                                if (errno == EAGAIN)
                                {
                                        continue;
                                }
                                else
                                {
                                        perror("read");
                                        exit(1);
                                }
                        }
                        else if (l_c == 0)
                        {
                                break;
                        }
                        else
                        {
                                printf("CHILD_WROTE: %.*s\n", (int)l_c, l_buf);
                        }
                }
                close(l_fds[0]);
                wait(0);
        }
        return 0;
}
#endif

}
