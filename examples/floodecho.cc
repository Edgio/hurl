//: ----------------------------------------------------------------------------
//: fanout example:
//: compile with:
//:   g++ fanout.cc -lhlxcore -lssl -lcrypto -lpthread -o fanout
//: ----------------------------------------------------------------------------
#include <hlx/hlx.h>
#include <hlx/phurl_h.h>
//#include <google/profiler.h>

class hello_from: public ns_hlx::phurl_h
{
public:
        int32_t create_resp(ns_hlx::hlx &a_hlx, ns_hlx::subr &a_subr, ns_hlx::phurl_h_resp *l_fanout_resp)
        {
                // Get body of resp
                char l_buf[4096];
                l_buf[0] = '\0';
                for(ns_hlx::hlx_resp_list_t::iterator i_resp = l_fanout_resp->m_resp_list.begin();
                    i_resp != l_fanout_resp->m_resp_list.end();
                    ++i_resp)
                {
                        char *l_status_buf = NULL;
                        int l_as_len = asprintf(&l_status_buf, "Hello From: %s -STATUS: %u\n",
                                        (*i_resp)->m_subr->get_host().c_str(),
                                        (*i_resp)->m_resp->get_status());
                        strncat(l_buf, l_status_buf, l_as_len);
                        free(l_status_buf);
                }
                uint64_t l_len = strnlen(l_buf, 2048);

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
                delete l_fanout_resp;
                return 0;
        }
};

class quitter: public ns_hlx::default_rqst_h
{
public:
        // GET
        ns_hlx::h_resp_t do_get(ns_hlx::hlx &a_hlx,
                                ns_hlx::hconn &a_hconn,
                                ns_hlx::rqst &a_rqst,
                                const ns_hlx::url_pmap_t &a_url_pmap)
        {
                a_hlx.stop();
                return ns_hlx::H_RESP_DONE;
        }
};

int main(void)
{
        ns_hlx::lsnr *l_lsnr = new ns_hlx::lsnr(12345, ns_hlx::SCHEME_TCP);
        hello_from *l_hello_from = new hello_from();

        // ---------------------------------------
        // Setup parallel hurl endpoint
        // ---------------------------------------
        // Add hosts
        l_hello_from->add_host("www.yahoo.com", 443);
        l_hello_from->add_host("www.google.com", 443);
        l_hello_from->add_host("www.reddit.com", 443);

        // Setup subr template
        ns_hlx::subr &l_subr = l_hello_from->get_subr_template();
        l_subr.set_port(443);
        l_subr.set_scheme(ns_hlx::SCHEME_TLS);

        // Add endpoints
        l_lsnr->register_endpoint("/phurl", l_hello_from);
        l_lsnr->register_endpoint("/quit", new quitter());

        ns_hlx::hlx *l_hlx = new ns_hlx::hlx();
        l_hlx->add_lsnr(l_lsnr);
        l_hlx->set_num_threads(0);
        l_hlx->set_use_persistent_pool(true);
        l_hlx->set_num_parallel(32);
        //ProfilerStart("tmp.prof");
        l_hlx->run();
        delete l_hlx;
        //ProfilerStop();
}
