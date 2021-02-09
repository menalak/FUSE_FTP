/**
 * netfs_client.h
 *
 * Implementation of the netfs client file system. Based on the fuse 'hello'
 * example here: https://github.com/libfuse/libfuse/blob/master/example/hello.c
 */

#define FUSE_USE_VERSION 31

#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse3/fuse.h>
#include <netdb.h> 
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"
#include "logging.h"

#define TEST_DATA "hello world!\n"

/**
 *Command line options 
 */
static struct options {
    int show_help;
    int port;
    char* server;
} options;

#define OPTION(t, p) { t, offsetof(struct options, p), 1 }

/** 
 *Command line option specification. We can add more here. If we're interested
 * in a string, specify --opt=%s .
 */
static const struct fuse_opt option_spec[] = {
    OPTION("-h", show_help),
    OPTION("--server=%s", server),
    OPTION("--help", show_help),
    OPTION("--port=%d", port),
    FUSE_OPT_END 
};

/**
 *this is the message struct sent through the server-client connection
 * it has request type and the request specification
 */
struct request_operations{
    int request_type;
    const char* request;
}request_op;


/**
 * open connection method
 *
 * this function is responsible for setting up the connection on the client side
 *
 * @param server | this tells us the server name we are trying to open
 *
 * @param port | this tells us the port our socket lies in
 *
 * Does not envoke helper functions
*/
static int connect_open(char *server, int port){
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket");
        return 1;
    }
    struct hostent *server_host = gethostbyname(server);
    if (server_host == NULL) {
        fprintf(stderr, "Could not resolve host: %s\n", server);
        return 1;
    }

    struct sockaddr_in serv_addr = { 0 };
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr = *((struct in_addr *) server_host->h_addr);

    if (connect(
                socket_fd,
                (struct sockaddr *) &serv_addr,
                sizeof(struct sockaddr_in)) == -1) {

        perror("connect");
        return 1;
    }
    LOG("Connected to server %s:%d\n", server, port);
    return socket_fd;
}


/**
 * close connection method
 *
 * this function is responsible for closing the connection after it has been established
 *
 * @param connected_server | this is the server we have connected to
 *
 * Does not envoke helper functions
*/
static int connect_close(int connected_server){
    shutdown(connected_server, 0);
    close(connected_server);
    return 0;
}


/**
 * get attributes function
 *
 * this function is responsible for getting the file attributes of a path using its stats
 *
 * @param path | this is the path that we are trying to read its attributes
 *
 * @param stbuf | this is the stat structure that holds the path information
 *
 * @param fi | this is the file information provided by fuse
 *
 * Does not envoke helper functions
*/
static int netfs_getattr(
        const char *path, struct stat *stbuf, struct fuse_file_info *fi) {

    LOG("getattr: %s\n", path);

    int connection_return=connect_open(options.server, options.port);
    if ( connection_return== 1){
        perror("connect");
        return 1;
    }
    request_op.request_type=2;
    request_op.request=path;

    size_t write_size = send(connection_return,&request_op,sizeof(struct request_operations),0);   
    if (write_size == -1){
        perror("sending request failed");
        return 1;
    }
    size_t rec_size;
    rec_size=recv(connection_return,&stbuf,sizeof(struct stat),0);

    if (rec_size == -1){
        perror("unable to recieve open connection");
        return -1;
    }
    
    connect_close(connection_return);

    return 0;
}


/**
 * read directory function
 *
 * this function is responsible for opening a directory and reading its contents
 *
 * @param path | this is the path that we are trying to read into the directory 
 *
 * @param buf | the buffer we fill our mounted file system in
 *
 * @param filler | this fills our mounted file system
 *
 * @param offset | position in file
 *
 * @param fi | fuse file information
 *
 * @param flags | if any flags are provided
 *
 * Does not envoke helper functions
*/
static int netfs_readdir(
        const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
        struct fuse_file_info *fi, enum fuse_readdir_flags flags) {

    LOG("readdir: %s\n", path);
    int connection_return=connect_open(options.server, options.port);
    if ( connection_return== 1){
        perror("unable to establish connection");
        return 1;
    }
    request_op.request_type=1;
    request_op.request=path;
    
    size_t write_size = send(connection_return,&request_op,sizeof(struct request_operations),0);   
    if (write_size == -1){
        perror("sending request failed");
        return 1;
    }

    size_t recieve_size_buff;
    size_t recieve_size_length;
    char *rec_buff;
    int size;


    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    while (true){
        recieve_size_length =recv(connection_return,&size,sizeof(int),0);
        rec_buff= (char *)malloc(size);
        recieve_size_buff=recv(connection_return,&rec_buff,size,0);
        filler(buf, rec_buff, NULL, 0, 0);

        if (recieve_size_length == -1 || recieve_size_buff== -1){
            perror("error recieving file name");
            return 1;
        }

        if (rec_buff == NULL){
            break;
        }

    }
    connect_close(connection_return);

    return 0;
    
}


/**
 * open file function
 *
 * this function is responsible for opening a file
 *
 * @param path | this is the path that we are trying to read into the directory 
 *
 * @param fi | fuse file information
 *
 * Does not envoke helper functions
*/
static int netfs_open(const char *path, struct fuse_file_info *fi) {

    LOG("open: %s\n", path);

    int connection_return=connect_open(options.server, options.port);
    if ( connection_return== 1){
        perror("connect");
        return -1;
    }
    request_op.request_type=3;
    request_op.request=path;

     size_t write_size = send(connection_return,&request_op,sizeof(struct request_operations),0);   
    if (write_size == -1){
        perror("sending request failed");
        return -1;
    }
    int succ_output;
    int rec_size;

    rec_size=recv(connection_return,&succ_output,sizeof(int),0);
    if (rec_size == -1 || succ_output == -1){
        perror("unable to retrieve open file request");
        return -1;
    }
    
    connect_close(connection_return);

    return succ_output;
}


/**
 * read file function
 *
 * this function is responsible for opening a file then reading its contents
 *
 * @param path | this is the path that we are trying to read into the directory 
 *
 * @param buf | the buffer we fill our mounted file system in
 *
 * @param size | the size of the buffer we are trying to read
 *
 * @param offset | position in file
 *
 * @param fi | fuse file information
 *
 * Does not envoke helper functions
*/
static int netfs_read(
        const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi) {

    LOG("read: %s\n", path);
 
    int connection_return=connect_open(options.server, options.port);
    if ( connection_return== 1){
        perror("unable to establish connection");
        return -1;
    }
    request_op.request_type=4;
    request_op.request=path;

    //send the type of request we are sending to the server
    size_t write_size = send(connection_return,&request_op,sizeof(struct request_operations),0);   
    if (write_size == -1){
        perror("sending request failed");
        return -1;
    }
    //send the size of read requested
    write_size = send(connection_return,&size,sizeof(size_t),0);   
    if (write_size == -1){
        perror("sending request failed");
        return -1;
    } 
    //send the offset we are starting from
    write_size = send(connection_return,&offset,sizeof(off_t),0);   
    if (write_size == -1){
        perror("sending request failed");
        return -1;
    } 

    //recieve the buffer read from offset to the selected size if available
    size_t rec_size=recv(connection_return,&buf,sizeof(char *),0);
    if (rec_size == -1){
        perror("unable to retrieve file buffer");
        return -1;
    }
    //recive the size that was read to send it back 
    rec_size=recv(connection_return,&size,sizeof(int),0);
    if (rec_size == -1){
        perror("unable to retrieve size read");
        return -1;
    }
    connect_close(connection_return);

    //return the size read by the server which we recieved
    return size;
}



/** 
 *This struct maps file system operations to our custom functions defined
 * above. 
 */
static struct fuse_operations netfs_client_ops = {
    .getattr = netfs_getattr,
    .readdir = netfs_readdir,
    .open = netfs_open,
    .read = netfs_read,
};

/** 
 *this is the string output of selecting the help or -h flag
 *
 */
static void show_help(char *argv[]) {
    printf("usage: %s [options] <mountpoint>\n\n", argv[0]);
    printf("File-system specific options:\n"
            "    --port=<n>          Port number to connect to\n"
            "                        (default: %d)"
            "\n", DEFAULT_PORT);
}

/**
 * main function
 *
 * this function is responsible for onvoking and setting up the fuse file system
 *
 */
int main(int argc, char *argv[]) {
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    /* Parse options */
    if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1) {
        return 1;
    }

    if (options.show_help) {
        show_help(argv);
        assert(fuse_opt_add_arg(&args, "--help") == 0);
        args.argv[0] = (char*) "";
    }

    return fuse_main(args.argc, args.argv, &netfs_client_ops, NULL);
}
