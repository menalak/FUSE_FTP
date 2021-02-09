# Project 4: Network File System

See: https://www.cs.usfca.edu/~mmalensek/cs326/assignments/project-4.html
## About This Project
This projects is responsible for implementing the FUSE file system in a manner that establishes a linux implementation through TCP connection. 

NOTE: IMPLEMENTATIONS ONLY SUPPORT READ MODE

### How does our server side work?
Our server side takes in arguments that generate its main functionality; port number and file server. The server then listens to a connection from a client that requests connection through the running client.the server then waits for a command to execute from the client where it recieves the appropriate information and sends it through the send function. The commands that it handles are reading a file, reading a directory, getting file attributes, and opening a directory. 

### How does our client side work?
our client will take in three arguments, defined by: server name, port to connect to and file to mount. the client will then listen to executed commands on the terminal with our mounted file as its directory and will find the appropriate FUSE function interface to call in order to alert the server of the request through a TCP connection. The commands that it handles are reading a file, reading a directory, getting file attributes, and opening a directory. 

### Included Files
There are several files included. These are:
   - <b>Makefile</b>: For adjusting File specifics
   - <b>README.md</b>: it is a me, readme
   - <b>logging.h</b>: this is a file that holds our loging specifications and macros
   - <b>common.h</b>: this file contains the DEFULT attributes that the client and server share
   - <b>netfs_client.c</b>: this is the client side of our file system 
   - <b>netfs_server.c</b>: this is the server side of our file system 


## Testing

To check test cases, a check on a seleced mount file could be implemented where both the server and client should be running and commands should be executed within the mounted file.
