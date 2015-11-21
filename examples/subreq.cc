//: ----------------------------------------------------------------------------
//: hlx_server example:
//: compile with:
//:   g++ subreq.cc -lhlxcore -lssl -lcrypto -lpthread -o subreq
//: ----------------------------------------------------------------------------
#include <hlx/hlx.h>
#include <string.h>

class twootter_getter: public ns_hlx::default_rqst_h
{
public:
        // GET
        ns_hlx::h_resp_t do_get(ns_hlx::hlx &a_hlx,
                                ns_hlx::hconn &a_hconn,
                                ns_hlx::rqst &a_rqst,
                                const ns_hlx::url_pmap_t &a_url_pmap)
        {
                ns_hlx::subr &l_subr = a_hlx.create_subr();
                l_subr.init_with_url("https://api.twitter.com/1.1/statuses/lookup.json");
                l_subr.set_completion_cb(s_completion_cb);
                l_subr.set_header("User-Agent", "hlx");
                l_subr.set_header("Accept", "*/*");
                l_subr.set_keepalive(true);
                a_hlx.queue_subr(&a_hconn, l_subr);
                return ns_hlx::H_RESP_DONE;
        }

        // Completion
        static int32_t s_completion_cb(ns_hlx::hlx &a_hlx,
                                       ns_hlx::subr &a_subr,
                                       ns_hlx::nconn &a_nconn,
                                       ns_hlx::resp &a_resp)
        {
                // Get body of resp
                char *l_buf = NULL;
                uint64_t l_len;
                a_resp.get_body_allocd(&l_buf, l_len);

                // Create length string
                char l_len_str[64];
                sprintf(l_len_str, "%lu", l_len);

                // Create resp
                ns_hlx::api_resp &l_api_resp = a_hlx.create_api_resp();
                l_api_resp.set_status(ns_hlx::HTTP_STATUS_OK);
                l_api_resp.set_header("Content-Length", l_len_str);
                l_api_resp.set_body_data(l_buf, l_len);

                // Queue
                a_hlx.queue_api_resp(*(a_subr.get_requester_hconn()), l_api_resp);

                // Free memory
                if(l_buf)
                {
                        free(l_buf);
                        l_buf = NULL;
                }

                return 0;
        }
};

int main(void)
{
        ns_hlx::lsnr *l_lsnr = new ns_hlx::lsnr(12345, ns_hlx::SCHEME_TCP);
        l_lsnr->add_endpoint("/twootter", new twootter_getter());
        ns_hlx::hlx *l_hlx = new ns_hlx::hlx();
        l_hlx->add_lsnr(l_lsnr);
        l_hlx->set_num_threads(0);
        //l_hlx->set_verbose(true);
        //l_hlx->set_color(true);
        l_hlx->set_use_persistent_pool(true);
        l_hlx->run();
}
