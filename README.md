# ft_ping

This is a re-implementation of the ping command from inetutils 2.0 library.

In order to build just run:
```bash
make
```

In order to build and give network capabilities to the binary run:
```bash
make USE_RAW_SOCKET=true
```

This implementation contains the following feature as command line options:
```bash
$ ./ft_ping -?
Usage: ft_ping [OPTION...] HOST ...
Send ICMP ECHO_REQUEST packets to network hosts.

Options:
  -v                 verbose output
  -f                 flood ping (root only)
  -i <interval>      interval in seconds between ping messages [default 1s]
  -c <count>         number of messages to send, 0 is infinity [default 0]
  -p <pattern>       fill ICMP packet with given pattern (hex)
  -t <N>             specify N as time-to-live
  -?                 give this help list
```


## Testing

For testing I have created a battery of "black box" test that will compare the **exit status**, **standard output**, and **messages sent and received**.

The test is implemented using [Robot Framework](https://robotframework.org/), which will run inside a docker container in order to isolate the test environment. Messages will be sent to `127.0.0.1` (localhost) which has been previously configured to not respond automatically to ICMP messages.

In order to launch the test just type:
```bash
make test
```
