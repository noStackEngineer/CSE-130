# Assignment 2: HTTP Server




## Server Execution Steps:

	Listen for incoming connections
	Loop forever:
		1. accept a new client connection
		2. process conection
		3. close connection

- Process one connection at a time



## Listening Stage:
1. check that `port` is valid
    - 1 <= port <= 65535
    - if not, stderr "Invalid Port\n" and exit(1)

2. use `ls_new()`
    - creates a socket
    - binds the socket
    - sets it to listen for incoming connections
    - this socket is used for the rest of the program's run

declare a pointer to server as a handle to the socket
    using `Listen_Socket_t`

3. check that `ls_new()` return val != NULL
    - if NULL, stderr "Invalid Port\n" and exit(1)



## Accepting New Client Connection Stage:
1. Repeatedly accept connections made by clients to `port`
    - `ls_accept()` blocks execution until new connection is made
    - it accepts the socket from `ls_new()` as a parameter
    
    - `ls_accept()` returns new connection socket
    - this connection can be treated as int fd from `open()`



## Processing Connection Stage:
1. use `read()`/`write()` with connection socket as the fd arg
    - each connection will contain at most one valid request
    - if there are extra bytes after a valid request, just ignore

2. We support only 2 HTTP operations: `GET` and `PUT`



## Closing Connection Stage:
1. use `close()` on connection socket as fd



## HTTP Request Format

request :=  request-line    (required)
            header-list     (optional, repeated)
            empty-line      (required)
            message-body    (optional)


Size Limit:
    A valid request will not exceed 2048 characters before payload
        i.e.  request-line header-list empty-line



TODO :: finish HTTP Request Format section (1.3)
TODO :: analyze HTTP Responses Format section (1.4)
TODO :: analyze Method Semantics section (1.5)
TODO :: analyze Additional Functionality section (1.6)
TODO :: analyze Examples section for correctness (2)
TODO :: use Testing Tips section when ready (4.1)






## Design Process

### Scaffolding

1. parse command-line arguments
2. create a listener socket for given port

3. repeatedly:
    1. accept new connections on that socket/port
    2. parse the connections request
    3. perform the request
    4. close that connection 






