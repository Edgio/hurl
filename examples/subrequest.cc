//: ----------------------------------------------------------------------------
//: subrequest example:
//: compile with:
//:   g++ subrequest.cc -lhlxcore -lssl -lcrypto -lpthread -ludns -o subrequest
//: ----------------------------------------------------------------------------
#include <hlx/hlx.h>
#include <string.h>
#include <unistd.h>
//#include <google/profiler.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>

ns_hlx::hlx *g_hlx = NULL;

class twootter_getter: public ns_hlx::default_rqst_h
{
public:
        // GET
        ns_hlx::h_resp_t do_get(ns_hlx::hconn &a_hconn,
                                ns_hlx::rqst &a_rqst,
                                const ns_hlx::url_pmap_t &a_url_pmap)
        {
                ns_hlx::subr &l_subr = ns_hlx::create_subr(a_hconn);
                l_subr.init_with_url("https://api.twitter.com/1.1/statuses/lookup.json");
                l_subr.set_completion_cb(s_completion_cb);
                l_subr.set_header("User-Agent", "hlx");
                l_subr.set_header("Accept", "*/*");
                l_subr.set_keepalive(true);
                queue_subr(a_hconn, l_subr);
                return ns_hlx::H_RESP_DONE;
        }

        // Completion
        static int32_t s_completion_cb(ns_hlx::subr &a_subr,
                                       ns_hlx::nconn &a_nconn,
                                       ns_hlx::resp &a_resp)
        {
                // Get body of resp
                const char *l_buf = a_resp.get_body_data();
                uint64_t l_len = a_resp.get_body_len();

                // Create length string
                char l_len_str[64];
                sprintf(l_len_str, "%" PRIu64 "", l_len);

                // Create resp
                ns_hlx::api_resp &l_api_resp = ns_hlx::create_api_resp(*(a_subr.get_requester_hconn()));
                l_api_resp.set_status(ns_hlx::HTTP_STATUS_OK);
                l_api_resp.set_header("Content-Length", l_len_str);
                l_api_resp.set_body_data(l_buf, l_len);

                // Queue
                ns_hlx::queue_api_resp(*(a_subr.get_requester_hconn()), l_api_resp);

                return 0;
        }
};

class quitter: public ns_hlx::default_rqst_h
{
public:
        // GET
        ns_hlx::h_resp_t do_get(ns_hlx::hconn &a_hconn,
                                ns_hlx::rqst &a_rqst,
                                const ns_hlx::url_pmap_t &a_url_pmap)
        {
                if(g_hlx)
                {
                        g_hlx->stop();
                }
                return ns_hlx::H_RESP_DONE;
        }
};

int main(void)
{
        ns_hlx::lsnr *l_lsnr = new ns_hlx::lsnr(12345, ns_hlx::SCHEME_TCP);
        ns_hlx::rqst_h *l_rqst_h = new twootter_getter();
        ns_hlx::rqst_h *l_rqst_h_quit = new quitter();
        l_lsnr->register_endpoint("/twootter", l_rqst_h);
        l_lsnr->register_endpoint("/quit", l_rqst_h_quit);
        g_hlx = new ns_hlx::hlx();
        g_hlx->register_lsnr(l_lsnr);
        g_hlx->set_num_threads(0);
        //g_hlx->set_num_threads(1);
        //g_hlx->set_verbose(true);
        //g_hlx->set_color(true);
        //ProfilerStart("tmp.prof");
        g_hlx->run();
        //sleep(1);
        //while(g_hlx->is_running())
        //{
        //        sleep(1);
        //        //g_hlx->display_stats();
        //}
        //ProfilerStop();
        if(g_hlx) {delete g_hlx; g_hlx = NULL;}
        if(l_rqst_h) {delete l_rqst_h; l_rqst_h = NULL;}
        if(l_rqst_h_quit) {delete l_rqst_h_quit; l_rqst_h_quit = NULL;}
}
