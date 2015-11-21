
#if 0
   Asynchronous example
       This example shows a simple interactive getaddrinfo_a() front-end.  The notification facility is not
       demonstrated.

       An example session might look like this:

           $ ./a.out
           > a ftp.us.kernel.org enoent.linuxfoundation.org gnu.cz
           > c 2
           [2] gnu.cz: Request not canceled
           > w 0 1
           [00] ftp.us.kernel.org: Finished
           > l
           [00] ftp.us.kernel.org: 216.165.129.139
           [01] enoent.linuxfoundation.org: Processing request in progress
           [02] gnu.cz: 87.236.197.13
           > l
           [00] ftp.us.kernel.org: 216.165.129.139
           [01] enoent.linuxfoundation.org: Name or service not known
           [02] gnu.cz: 87.236.197.13

       The program source is as follows:
#endif

#define _GNU_SOURCE
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct gaicb **reqs = NULL;
static int nreqs = 0;

static char *
getcmd(void) {
        static char buf[256];

        fputs("> ", stdout);
        fflush(stdout);
        if (fgets(buf, sizeof(buf), stdin) == NULL)
                return NULL;

        if (buf[strlen(buf) - 1] == '\n')
                buf[strlen(buf) - 1] = 0;

        return buf;
}

/* Add requests for specified hostnames */
static void add_requests(void)
{
        int nreqs_base = nreqs;
        char *host;
        int ret;

        while ((host = strtok(NULL, " "))) {
                nreqs++;
                reqs = realloc(reqs, nreqs * sizeof(reqs[0]));

                reqs[nreqs - 1] = calloc(1, sizeof(*reqs[0]));
                reqs[nreqs - 1]->ar_name = strdup(host);
        }

        /* Queue nreqs_base..nreqs requests. */
        ret = getaddrinfo_a(GAI_NOWAIT, &reqs[nreqs_base], nreqs - nreqs_base, NULL);
        if (ret) {
                fprintf(stderr, "getaddrinfo_a() failed: %s\n", gai_strerror(ret));
                exit(EXIT_FAILURE);
        }
}

/* Wait until at least one of specified requests completes */
static void wait_requests(void)
{
        char *id;
        int i, ret, n;
        struct gaicb const **wait_reqs = calloc(nreqs, sizeof(*wait_reqs));
        /* NULL elements are ignored by gai_suspend(). */

        while ((id = strtok(NULL, " ")) != NULL) {
                n = atoi(id);

                if (n >= nreqs) {
                        printf("Bad request number: %s\n", id);
                        return;
                }

                wait_reqs[n] = reqs[n];
        }

        ret = gai_suspend(wait_reqs, nreqs, NULL);
        if (ret) {
                printf("gai_suspend(): %s\n", gai_strerror(ret));
                return;
        }

        for (i = 0; i < nreqs; i++) {
                if (wait_reqs[i] == NULL)
                        continue;

                ret = gai_error(reqs[i]);
                if (ret == EAI_INPROGRESS)
                        continue;

                printf("[%02d] %s: %s\n", i, reqs[i]->ar_name, ret == 0 ? "Finished" : gai_strerror(ret));
        }
}

/* Cancel specified requests */
static void cancel_requests(void)
{
        char *id;
        int ret, n;

        while ((id = strtok(NULL, " ")) != NULL) {
                n = atoi(id);

                if (n >= nreqs) {
                        printf("Bad request number: %s\n", id);
                        return;
                }

                ret = gai_cancel(reqs[n]);
                printf("[%s] %s: %s\n", id, reqs[atoi(id)]->ar_name, gai_strerror(ret));
        }
}

/* List all requests */
static void list_requests(void)
{
        int i, ret;
        char host[NI_MAXHOST];
        struct addrinfo *res;

        for (i = 0; i < nreqs; i++) {
                printf("[%02d] %s: ", i, reqs[i]->ar_name);
                ret = gai_error(reqs[i]);

                if (!ret) {
                        res = reqs[i]->ar_result;

                        ret = getnameinfo(res->ai_addr, res->ai_addrlen, host, sizeof(host),
                        NULL, 0, NI_NUMERICHOST);
                        if (ret) {
                                fprintf(stderr, "getnameinfo() failed: %s\n", gai_strerror(ret));
                                exit(EXIT_FAILURE);
                        }
                        puts(host);
                } else {
                        puts(gai_strerror(ret));
                }
        }
}

int main(int argc, char *argv[])
{
        char *cmdline;
        char *cmd;

        while ((cmdline = getcmd()) != NULL) {
                cmd = strtok(cmdline, " ");

                if (cmd == NULL) {
                        list_requests();
                } else {
                        switch (cmd[0]) {
                        case 'a':
                                add_requests();
                                break;
                        case 'w':
                                wait_requests();
                                break;
                        case 'c':
                                cancel_requests();
                                break;
                        case 'l':
                                list_requests();
                                break;
                        default:
                                fprintf(stderr, "Bad command: %c\n", cmd[0]);
                                break;
                        }
                }
        }
        exit(EXIT_SUCCESS);
}
