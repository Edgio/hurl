//: ----------------------------------------------------------------------------
//: subrequest example:
//: compile with:
//:   g++ subrequest.cc -lhlxcore -lssl -lcrypto -lpthread -ludns -o subrequest
//: ----------------------------------------------------------------------------
#include <hlx/srvr.h>
#include <hlx/lsnr.h>
#include <hlx/proxy_h.h>
#include <hlx/default_rqst_h.h>
#include <hlx/url_router.h>
#include <hlx/api_resp.h>
#include <hlx/resp.h>
#include <hlx/rqst.h>
#include <hlx/subr.h>

#include <string.h>
#include <unistd.h>
//#include <google/profiler.h>


#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>

//#include <google/profiler.h>
#include <hlx/trace.h>
#include <hlx/access.h>

// for displaying addresses
#include <arpa/inet.h>

//: ----------------------------------------------------------------------------
//: Access logs
//: ----------------------------------------------------------------------------
std::string g_access_log_file = "";
FILE *g_accesslog_file_ptr = stdout;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t s_resp_done_cb(ns_hlx::clnt_session &a_clnt_session)
{
        //NDBG_PRINT(0, "RESP DONE\n");
        if(!g_accesslog_file_ptr)
        {
                g_accesslog_file_ptr = ::fopen(g_access_log_file.c_str(), "a");
                if(!g_accesslog_file_ptr)
                {
                        printf("Error performing fopen.\n");
                        return -1;
                }
        }

        // -------------------------------------------------
        // nginx default log format...
        // -------------------------------------------------
        // $remote_addr [$time_local] $request $status $body_bytes_sent $http_referer $http_user_agent
        // 127.0.0.1 - - [30/Mar/2016:18:13:04 -0700] "GET / HTTP/1.1" 200 612 "-" "curl/7.35.0"
        // -------------------------------------------------
        // Get access_info
        const ns_hlx::access_info &l_ai = ns_hlx::get_access_info(a_clnt_session);
        char l_cln_addr_str[INET6_ADDRSTRLEN];
        l_cln_addr_str[0] = '\0';
        if(l_ai.m_conn_cln_sa_len == sizeof(sockaddr_in))
        {
                // a thousand apologies for this monstrosity :(
                errno = 0;
                const char *l_s;
                l_s = inet_ntop(AF_INET,
                                &(((sockaddr_in *)(&l_ai.m_conn_cln_sa))->sin_addr),
                                l_cln_addr_str,
                                INET_ADDRSTRLEN);
                if(!l_s)
                {
                        printf("Error performing inet_ntop.\n");
                }
        }
        else if(l_ai.m_conn_cln_sa_len == sizeof(sockaddr_in6))
        {
                // a thousand apologies for this monstrosity :(
                errno = 0;
                const char *l_s;
                l_s = inet_ntop(AF_INET6,
                                &(((sockaddr_in6 *)(&l_ai.m_conn_cln_sa))->sin6_addr),
                                l_cln_addr_str,
                                INET6_ADDRSTRLEN);
                if(!l_s)
                {
                        printf("Error performing inet_ntop.\n");
                }
        }
        fprintf(g_accesslog_file_ptr, "%s [%s] \"%s %s HTTP/%u.%u\" %u %" PRIu64 " %" PRIu64 "  %" PRIu64 " %s %s\n",
                        l_cln_addr_str,                     // remote addr
                        "DATE STRING",                      // local time
                        l_ai.m_rqst_request.c_str(),        // request line
                        l_ai.m_rqst_method,                 // method
                        l_ai.m_rqst_http_major,             // HTTP ver major
                        l_ai.m_rqst_http_minor,             // HTTP ver minor
                        l_ai.m_resp_status,                 // status
                        l_ai.m_bytes_in,                    // TODO body bytes sent
                        l_ai.m_bytes_out,                   // TODO body bytes sent
                        l_ai.m_total_time_ms,               // TODO body bytes sent
                        l_ai.m_rqst_http_referer.c_str(),   // referer
                        l_ai.m_rqst_http_user_agent.c_str() // user agent
                        );
        fflush(g_accesslog_file_ptr);
        return 0;
}

ns_hlx::srvr *g_srvr = NULL;

class default_h: public ns_hlx::default_rqst_h
{
public:
        // GET
        ns_hlx::h_resp_t do_default(ns_hlx::clnt_session &a_clnt_session,
                                    ns_hlx::rqst &a_rqst,
                                    const ns_hlx::url_pmap_t &a_url_pmap)
        {
                char l_len_str[64];
                uint32_t l_body_len = strlen("Hello World\n");
                sprintf(l_len_str, "%u", l_body_len);
                ns_hlx::api_resp &l_api_resp = create_api_resp(a_clnt_session);
                l_api_resp.set_status(ns_hlx::HTTP_STATUS_OK);
                l_api_resp.set_header("Content-Length", l_len_str);
                l_api_resp.set_body_data("Hello World\n", l_body_len);
                ns_hlx::queue_api_resp(a_clnt_session, l_api_resp);
                return ns_hlx::H_RESP_DONE;
        }
};

class quitter: public ns_hlx::default_rqst_h
{
public:
        // GET
        ns_hlx::h_resp_t do_get(ns_hlx::clnt_session &a_clnt_session,
                                ns_hlx::rqst &a_rqst,
                                const ns_hlx::url_pmap_t &a_url_pmap)
        {
                if(g_srvr)
                {
                        g_srvr->stop();
                }
                return ns_hlx::H_RESP_DONE;
        }
};

int main(void)
{
        ns_hlx::lsnr *l_lsnr = new ns_hlx::lsnr(12345, ns_hlx::SCHEME_TCP);
        ns_hlx::rqst_h *l_rqst_h = new ns_hlx::proxy_h("https://api.twitter.com", "/twootter");
        ns_hlx::rqst_h *l_default_h = new default_h();
        ns_hlx::rqst_h *l_rqst_h_quit = new quitter();
        l_lsnr->add_route("/twootter/*", l_rqst_h);
        l_lsnr->add_route("/quit", l_rqst_h_quit);
        l_lsnr->set_default_route(l_default_h);
        g_srvr = new ns_hlx::srvr();
        g_srvr->register_lsnr(l_lsnr);
        g_srvr->set_num_threads(0);
        //ns_hlx::trc_log_file_open("error.log");
        ns_hlx::trc_log_level_set(ns_hlx::TRC_LOG_LEVEL_DEBUG);
        g_srvr->set_resp_done_cb(s_resp_done_cb);
        //g_srvr->set_num_threads(1);
        //g_srvr->set_rqst_resp_logging(true);
        //g_srvr->set_rqst_resp_logging_color(true);
        //ProfilerStart("tmp.prof");
        g_srvr->run();
        //sleep(1);
        //while(g_srvr->is_running())
        //{
        //        sleep(1);
        //        //g_srvr->display_stats();
        //}
        //ProfilerStop();
        if(g_srvr) {delete g_srvr; g_srvr = NULL;}
        if(l_rqst_h) {delete l_rqst_h; l_rqst_h = NULL;}
        if(l_rqst_h_quit) {delete l_rqst_h_quit; l_rqst_h_quit = NULL;}
}
