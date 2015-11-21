//: ----------------------------------------------------------------------------
//: hlx_server example:
//: compile with:
//:   g++ basic.cc -lhlxcore -lssl -lcrypto -lpthread -o basic
//: ----------------------------------------------------------------------------
#include <hlx/hlx.h>
#include <string.h>

class bananas_getter: public ns_hlx::default_rqst_h
{
public:
        // GET
        ns_hlx::h_resp_t do_get(ns_hlx::hlx &a_hlx,
                                ns_hlx::hconn &a_hconn,
                                ns_hlx::rqst &a_rqst,
                                const ns_hlx::url_pmap_t &a_url_pmap)
        {
                char l_len_str[64];
                uint32_t l_body_len = strlen("Hello World\n");
                sprintf(l_len_str, "%u", l_body_len);
                ns_hlx::api_resp &l_api_resp = a_hlx.create_api_resp();
                l_api_resp.set_status(ns_hlx::HTTP_STATUS_OK);
                l_api_resp.set_header("Content-Length", l_len_str);
                l_api_resp.set_body_data("Hello World\n", l_body_len);
                a_hlx.queue_api_resp(a_hconn, l_api_resp);
                return ns_hlx::H_RESP_DONE;
        }
};

int main(void)
{
        ns_hlx::lsnr *l_lsnr = new ns_hlx::lsnr(13345, ns_hlx::SCHEME_TCP);
        ns_hlx::rqst_h *l_rqst_h = new bananas_getter();
        l_lsnr->add_endpoint("/bananas", l_rqst_h);
        ns_hlx::hlx *l_hlx = new ns_hlx::hlx();
        l_hlx->add_lsnr(l_lsnr);
        // Run in foreground w/ threads == 0
        l_hlx->set_num_threads(0);
        l_hlx->run();
        return 0;
}
