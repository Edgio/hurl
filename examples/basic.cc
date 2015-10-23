//: ----------------------------------------------------------------------------
//: hlx_server example:
//: compile with:
//:   g++ hlx_server_ex.cc -lhlxcore -lssl -lcrypto -lpthread -o hlx_server_ex
//: ----------------------------------------------------------------------------
#include <hlx/hlx.h>
#include <string.h>

class bananas_getter : public ns_hlx::default_http_request_handler
{
public:
        // GET
        int32_t do_get(ns_hlx::hlx &a_hlx,
                       ns_hlx::nconn &a_nconn,
                       ns_hlx::http_req &a_request,
                       const ns_hlx::url_param_map_t &a_url_param_map,
                       ns_hlx::http_resp &ao_resp)
        {
                char l_len_str[64];
                uint32_t l_body_len = strlen("Hello World\n");
                sprintf(l_len_str, "%u", l_body_len);
                ao_resp.write_status(ns_hlx::HTTP_STATUS_OK);
                ao_resp.write_header("Content-Length", l_len_str);
                ao_resp.write_body("Hello World\n", l_body_len);
                return 0;
        }
};

int main(void)
{
        ns_hlx::listener *l_listener = new ns_hlx::listener(13345, ns_hlx::SCHEME_TCP);
        l_listener->add_endpoint("/bananas", new bananas_getter());
        ns_hlx::hlx *l_hlx = new ns_hlx::hlx();
        l_hlx->add_listener(l_listener);
        l_hlx->set_num_threads(16);
        l_hlx->run();
        l_hlx->wait_till_stopped();
        return 0;
}
