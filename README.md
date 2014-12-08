 hlo: HTTP  Server Utilities
=========

## What are they
A few utilities for testing and curling from http servers. 

## *hlo* HTTP Server Load Tester
*hlo* is an http server load tester similar to ab/siege/weighttp/wrk with support for mulithreading, parallelism, ssl, url ranges, and an api-server for querying the running performance statistics.  *hlo* is primarily useful for benchmarking http server applications.

* **A little more about URLs Ranges**: 
*hlo* has support for range expansion in urls which is useful for testing a server's capability to serve from many files. *hlo* will expand the ranges specified in the wildcards and perform requests in user configurable orders (see the "--mode" option in help).
eg: "http://127.0.0.1:8089/[1-100]/my_[1-9]_file.html". 

* **Stats API**:
If  *hlo* is started with *-P port_number* option,  *hlo* listens on the user specified port for stats requests:
For example if hlo is run with:
```bash
~>hlo "http://127.0.0.1:8089/index.html" -P12345
Running 1 parallel connections with: 1 reqs/conn, 1 threads
+-----------/-----------+-----------+-----------+--------------+-----------+-------------+-----------+
|    Cmpltd /     Total |    IdlKil |    Errors | kBytes Recvd |   Elapsed |       Req/s |      MB/s |
+-----------/-----------+-----------+-----------+--------------+-----------+-------------+-----------+
|      2603 /      2603 |         0 |         0 |      2158.15 |     0.20s |       0.00s |     0.00s |
|      5250 /      5250 |         0 |         0 |      4352.78 |     0.40s |   13235.00s |    10.72s |
|      7831 /      7831 |         0 |         0 |      6492.69 |     0.60s |   12905.00s |    10.45s |
|     10390 /     10390 |         0 |         0 |      8614.37 |     0.80s |   12795.00s |    10.36s |
|     13131 /     13131 |         0 |         0 |     10886.93 |     1.00s |   13705.00s |    11.10s |
|     15737 /     15737 |         0 |         0 |     13047.57 |     1.20s |   13030.00s |    10.55s |
...
```
*hlo* can be queried:
```bash
~>curl -s http://127.0.0.1:12345 | json_pp
{
   "data" : [
      {
         "value" : {
            "count" : 331578,
            "time" : 1414786046242
         },
         "key" : "http://127.0.0.1:8089/index.html"
      }
   ]
}
```

####An example
```bash
hlo "http://127.0.0.1/index.html" --num_calls=100 -p100 -f100000 -c
```

####Options
```bash
Usage: hlo [http[s]://]hostname[:port]/path [options]
Options are:
  -h, --help         Display this help and exit.
  -v, --version      Display the version number and exit.
  
Input Options:
  -u, --url_file     URL file.
  -w, --no_wildcards Dont wildcard the url.
  
Settings:
  -y, --cipher       Cipher --see "openssl ciphers" for list.
  -p, --parallel     Num parallel.
  -f, --fetches      Num fetches.
  -N, --num_calls    Number of requests per connection
  -k, --keep_alive   Re-use connections for all requests
  -t, --threads      Number of parallel threads.
  -H, --header       Request headers -can add multiple ie -H<> -H<>...
  -l, --seconds      Run for <N> seconds .
  -r, --rate         Max Request Rate.
  -M, --mode         Requests mode [roundrobin|sequential|random].
  -R, --recv_buffer  Socket receive buffer size.
  -S, --send_buffer  Socket send buffer size.
  -D, --no_delay     Socket TCP no-delay.
  -T, --timeout      Timeout (seconds).
  
Print Options:
  -x, --verbose      Verbose logging
  -c, --color        Color
  -q, --quiet        Suppress progress output
  
Stat Options:
  -P, --data_port    Start HTTP Stats Daemon on port.
  -B, --breakdown    Show breakdown
  -X, --http_load    Display in http load mode [MODE] -Legacy support
  -G, --gprofile     Google profiler output file
  
Note: If running long jobs consider enabling tcp_tw_reuse -eg:
echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse

```


## *hle* Parallel Curl
*hle* is a parallel curling utility useful for pulling a single url from many different hosts. *hle* supports reading line delimited hosts from stdin, a shell command string, or a file.  

####An example
```bash
printf "www.google.com\nwww.yahoo.com\nwww.reddit.com\n" | hle -p2 -t3 -u"https://bloop.com/" -s -c -T5
```

####Options
```bash
Usage: hle [http[s]://]hostname[:port]/path [options]
Options are:
  -h, --help           Display this help and exit.
  -v, --version        Display the version number and exit.
  
URL Options -or without parameter
  -u, --url            URL -REQUIRED.
  
Hostname Input Options -also STDIN:
  -f, --host_file      Host name file.
  -x, --execute        Script to execute to get host names.
  
Settings:
  -y, --cipher         Cipher --see "openssl ciphers" for list.
  -p, --parallel       Num parallel.
  -t, --threads        Number of parallel threads.
  -H, --header         Request headers -can add multiple ie -H<> -H<>...
  -T, --timeout        Timeout (seconds).
  -R, --recv_buffer    Socket receive buffer size.
  -S, --send_buffer    Socket send buffer size.
  -D, --no_delay       Socket TCP no-delay.
  -A, --ai_cache       Path to Address Info Cache (DNS lookup cache).
  
Print Options:
  -r, --verbose        Verbose logging
  -c, --color          Color
  -q, --quiet          Suppress output
  -s, --show_progress  Show progress
  
Output Options: -defaults to line delimited
  -l, --line_delimited Output <HOST> <RESPONSE BODY> per line
  -j, --json           JSON { <HOST>: "body": <RESPONSE> ...
  -P, --pretty         Pretty output
  
Debug Options:
  -G, --gprofile       Google profiler output file
  
Note: If running large jobs consider enabling tcp_tw_reuse -eg:
echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse
```

## *hlp* HTTP Playback
*hlp* plays back requests from a playback file -see [here](https://github.com/EdgeCast/hlo/blob/master/tests/hlp/data/sample.txt) for example of playback format.

####An example
```bash
hlp -a sample.txt -I 127.0.0.1 -c
```

####Options
```bash
Usage: hlp [options]
Options are:
  -h, --help         Display this help and exit.
  -v, --version      Display the version number and exit.
  
Playback Options:
  -a, --playback     Playback file.
  -I, --pb_dest_addr Hard coded destination address for playback.
  -o, --pb_dest_port Hard coded destination port for playback.
  -s, --scale        Time scaling (float).
  -t, --threads      Number of threads.
  
Settings:
  -y, --cipher       Cipher --see "openssl ciphers" for list.
  -H, --header       Request headers -can add multiple ie -H<> -H<>...
  -R, --recv_buffer  Socket receive buffer size.
  -S, --send_buffer  Socket send buffer size.
  -D, --no_delay     Socket TCP no-delay.
  -T, --timeout      Timeout (seconds).
  
Print Options:
  -x, --verbose      Verbose logging
  -c, --color        Color
  -q, --quiet        Suppress progress output
  -e, --extra_info   Extra Info output
  
Stat Options:
  -B, --breakdown    Show breakdown
  
Example:
  hlp -a sample.txt -I 127.0.0.1 -c
  
Note: If running long jobs consider enabling tcp_tw_reuse -eg:
echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse
```

## Building

###Install dependecies:
Library requirements:
* libgoogle-perftools-dev
* libmicrohttpd-dev
* libleveldb-dev

```bash
sudo apt-get install libgoogle-perftools-dev libmicrohttpd-dev libleveldb-dev
```

###Building the tools
```bash
./build.sh
```

And optionally install
```bash
cd ./build
sudo make install
```

