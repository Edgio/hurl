//: ----------------------------------------------------------------------------
//: hlx_server example:
//: compile with:
//:   g++ hlx_server_ex.cc -lhlxcore -lssl -lcrypto -lpthread -o hlx_server_ex
//: ----------------------------------------------------------------------------
#include <hlx/hlx.h>
#include <string.h>
class bananas_getter: public ns_hlx::default_http_request_handler
{
public:
        // GET
        int32_t do_get(const ns_hlx::url_param_map_t &a_url_param_map,
                       ns_hlx::http_req &a_request,
                       ns_hlx::http_resp &ao_response)
        {
                char l_len_str[64];
                uint32_t l_body_len = strlen("Hello World\n");
                sprintf(l_len_str, "%u", l_body_len);
                ao_response.write_status(ns_hlx::HTTP_STATUS_OK);
                ao_response.write_header("Content-Type", "html/txt");
                ao_response.write_header("Content-Length", l_len_str);
                ao_response.write_body("Hello World\n", l_body_len);
                return 0;
        }
};

int main(void)
{
        ns_hlx::listener *l_listener = new ns_hlx::listener(12345, ns_hlx::SCHEME_TCP);
        l_listener->add_endpoint("/bananas", new bananas_getter());
        ns_hlx::hlx *l_hlx = new ns_hlx::hlx();
        l_hlx->add_listener(l_listener);
        l_hlx->set_num_threads(0);
        l_hlx->run();
}
