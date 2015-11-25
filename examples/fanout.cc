//: ----------------------------------------------------------------------------
//: fanout example:
//: compile with:
//:   g++ fanout.cc -lhlxcore -lssl -lcrypto -lpthread -o fanout
//: ----------------------------------------------------------------------------
#include <hlx/hlx.h>
#include <string.h>
#include <set>
#include <list>
//#include <google/profiler.h>

typedef std::set <uint64_t> resp_uid_set_t;

typedef struct host_struct {
        std::string m_host;
        uint16_t m_port;
        host_struct():
                m_host(),
                m_port(0)
        {};
} host_t;
typedef std::list <host_t> host_list_t;

class hlx_resp {
public:
        ns_hlx::subr *m_subr;
        ns_hlx::resp *m_resp;
        hlx_resp():
                m_subr(NULL),
                m_resp(NULL)
        {}
private:
        // Disallow copy/assign
        hlx_resp& operator=(const hlx_resp &);
        hlx_resp(const hlx_resp &);
};
typedef std::list <hlx_resp *> hlx_resp_list_t;

class fanout_resp {
public:
        // Results
        pthread_mutex_t m_mutex;
        resp_uid_set_t m_pending_uid_set;
        hlx_resp_list_t m_resp_list;

        fanout_resp(void) :
                m_mutex(),
                m_pending_uid_set(),
                m_resp_list()
        {
                pthread_mutex_init(&m_mutex, NULL);
        }

        ~fanout_resp(void)
        {
                for(hlx_resp_list_t::iterator i_resp = m_resp_list.begin(); i_resp != m_resp_list.end(); ++i_resp)
                {
                        if(*i_resp)
                        {
                                if((*i_resp)->m_resp)
                                {
                                        delete (*i_resp)->m_resp;
                                        (*i_resp)->m_resp = NULL;
                                }
                                delete *i_resp;
                                *i_resp = NULL;
                        }
                }
        }

private:
        // Disallow copy/assign
        fanout_resp& operator=(const fanout_resp &);
        fanout_resp(const fanout_resp &);
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

class fanout_getter: public ns_hlx::default_rqst_h
{
public:
        // GET
        ns_hlx::h_resp_t do_get(ns_hlx::hlx &a_hlx,
                                ns_hlx::hconn &a_hconn,
                                ns_hlx::rqst &a_rqst,
                                const ns_hlx::url_pmap_t &a_url_pmap)
        {
                // Create request state
                fanout_resp *l_fanout_resp = new fanout_resp();

                for(host_list_t::iterator i_host = m_host_list.begin(); i_host != m_host_list.end(); ++i_host)
                {
                        ns_hlx::subr &l_subr = a_hlx.create_subr(m_subr_template);
                        l_subr.set_host(i_host->m_host);
                        l_subr.reset_label();
                        l_subr.set_data(l_fanout_resp);

                        pthread_mutex_lock(&(l_fanout_resp->m_mutex));
                        l_fanout_resp->m_pending_uid_set.insert(l_subr.get_uid());
                        pthread_mutex_unlock(&(l_fanout_resp->m_mutex));

                        int32_t l_status = 0;
                        l_status = a_hlx.queue_subr(&a_hconn, l_subr);
                        if(l_status != HLX_STATUS_OK)
                        {
                                printf("Error: performing add_subreq.\n");
                                return ns_hlx::H_RESP_ERROR;
                        }
                }
                return ns_hlx::H_RESP_DONE;
        }

        // Completion
        static int32_t s_completion_cb(ns_hlx::hlx &a_hlx,
                                       ns_hlx::subr &a_subr,
                                       ns_hlx::nconn &a_nconn,
                                       ns_hlx::resp &a_resp)
        {
                fanout_resp *l_fanout_resp = static_cast<fanout_resp *>(a_subr.get_data());
                if(!l_fanout_resp)
                {
                        return -1;
                }
                hlx_resp *l_resp = new hlx_resp();
                l_resp->m_resp = &a_resp;
                l_resp->m_subr = &a_subr;
                pthread_mutex_lock(&(l_fanout_resp->m_mutex));
                l_fanout_resp->m_pending_uid_set.erase(a_subr.get_uid());
                l_fanout_resp->m_resp_list.push_back(l_resp);
                pthread_mutex_unlock(&(l_fanout_resp->m_mutex));

                // Check for done
                if(l_fanout_resp->m_pending_uid_set.empty())
                {
                        create_resp(a_hlx, a_subr, l_fanout_resp);
                }
                return 0;
        }

        // Error
        static int32_t s_error_cb(ns_hlx::hlx &a_hlx,
                                  ns_hlx::subr &a_subr,
                                  ns_hlx::nconn &a_nconn)
        {
                fanout_resp *l_fanout_resp = static_cast<fanout_resp *>(a_subr.get_data());
                if(!l_fanout_resp)
                {
                        return -1;
                }
                pthread_mutex_lock(&(l_fanout_resp->m_mutex));
                l_fanout_resp->m_pending_uid_set.erase(a_subr.get_uid());
                pthread_mutex_unlock(&(l_fanout_resp->m_mutex));

                // Check for done
                if(l_fanout_resp->m_pending_uid_set.empty())
                {
                        create_resp(a_hlx, a_subr, l_fanout_resp);
                }
                return 0;
        }

        static int32_t create_resp(ns_hlx::hlx &a_hlx,
                                   ns_hlx::subr &a_subr,
                                   fanout_resp *l_fanout_resp)
        {
                // Get body of resp
                char l_buf[2048];
                l_buf[0] = '\0';
                for(hlx_resp_list_t::iterator i_resp = l_fanout_resp->m_resp_list.begin();
                    i_resp != l_fanout_resp->m_resp_list.end();
                    ++i_resp)
                {
                        char *l_status_buf = NULL;
                        int l_as_len = asprintf(&l_status_buf, "STATUS: %u\n", (*i_resp)->m_resp->get_status());
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

        fanout_getter(void):
                ns_hlx::default_rqst_h(),
                m_subr_template(),
                m_host_list()
        {
                // Setup host list
                host_t l_host;
                l_host.m_host = "www.yahoo.com";
                l_host.m_port = 443;
                m_host_list.push_back(l_host);
                l_host.m_host = "www.google.com";
                l_host.m_port = 443;
                m_host_list.push_back(l_host);
                l_host.m_host = "www.reddit.com";
                l_host.m_port = 443;
                m_host_list.push_back(l_host);

                // Setup template
                m_subr_template.init_with_url("https://blorp/");
                m_subr_template.set_completion_cb(s_completion_cb);
                m_subr_template.set_error_cb(s_error_cb);
                m_subr_template.set_header("User-Agent", "hlx");
                m_subr_template.set_header("Accept", "*/*");
                m_subr_template.set_header("Connection", "keep-alive");
                m_subr_template.set_keepalive(true);
                m_subr_template.set_save(true);
                m_subr_template.set_detach_resp(true);
        }


        ns_hlx::subr m_subr_template;
        host_list_t m_host_list;
};

int main(void)
{
        ns_hlx::lsnr *l_lsnr = new ns_hlx::lsnr(12345, ns_hlx::SCHEME_TCP);
        l_lsnr->add_endpoint("/fanout", new fanout_getter());
        l_lsnr->add_endpoint("/quit", new quitter());
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
