/**
 * netfs_server.h
 *
 * NetFS file server implementation.
 */

#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h> 
#include <netinet/in.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>

#include "common.h"
#include "logging.h"
#define MAX_REQ 1024
struct __attribute__((__packed__)) netfs_msg_header {
    uint64_t msg_len;
    uint16_t msg_type;
};

struct request_operations{
    int request_type;
    const char* request;
}request_op;

char client_request[MAX_REQ];
char *directory;
int port;
pid_t p_id;


/**
 * open file function
 *
 * this function is responsible for opening a file apon client request
 *
 * @param client_path | this is the file path that the client is asking to open and read
 *
 * @param server_path | the path that was initialized to start on the server
 *
  * @param socket_fd | the socket we set up for connection
 *
 * Does not envoke helper functions
 */
int open_send(const char * client_path,char * server_path, int socket_fd){
    int open_file; 
    size_t write_size;
    if ((strncmp(client_path,".",1) == 0 && strlen(client_path)== 1) || (strncmp(client_path,"./",2)==0 && strlen(client_path)>2)){
        open_file=open(client_path, O_WRONLY | O_APPEND);
        write_size= send(socket_fd,&open_file,sizeof(int),0); 

        if (open_file == -1){
            perror("selected file could not be opened");
            return 1;
        }
    
        if (write_size == -1){
            perror("sending request failed");
            return 1;
        }  

    } else{
        perror("path to directory does not exist");
        return 1;
    }
    return 0;

}


/**
 * get attribute function
 *
 * this function is responsible for getting the attributes of specified file
 *
 * @param client_path | this is the file path that the client is asking to to get attributes for
 *
 * @param server_path | the path that was initialized to start on the server
 *
  * @param socket_fd | the socket we set up for connection
 *
 * Does not envoke helper functions
 */
int getattr_send(const char * client_path,char * server_path, int socket_fd){

    struct stat status;

    if ((strncmp(client_path,".",1) == 0 && strlen(client_path)== 1) || (strncmp(client_path,"./",2)==0 && strlen(client_path)>2)){
        if(stat(client_path,&status)!=0){
            perror("could not find file properties");
            return 1;
        }
        // this makes file read only
        status.st_mode = (mode_t) (~0222 & status.st_mode);

        size_t write_size = send(socket_fd,&status,sizeof(struct stat),0);   
        if (write_size == -1){
            perror("sending request failed");
            return 1;
        }

    }

    return 0;
}

/**
 * read file function
 *
 * this function is responsible for opening a file on the server and reading its contents from an offset
 *
 * @param client_path | this is the file path that the client is asking to open and read
 *
 * @param server_path | the path that was initialized to start on the server
 *
  * @param socket_fd | the socket we set up for connection
 *
 * Does not envoke helper functions
 */
int readfile_send(const char * client_path,char * server_path, int socket_fd){
    if ((strncmp(client_path,".",1) == 0 && strlen(client_path)== 1) || (strncmp(client_path,"./",2)==0 && strlen(client_path)>2)){
        
        struct stat status;
        size_t requested_size;
        off_t requested_offset;
        int open_file; 
        int file_sent;

        //open selected file
        open_file=open(client_path, O_WRONLY | O_APPEND);
        if (open_file == -1){
            perror("selected file could not be opened");
            return 1;
        }

        //recieve the file stat of the specified file
        if(stat(client_path,&status)!=0){
            perror("could not find file properties");
            return 1;
        }

        // this makes file read only
        status.st_mode = (mode_t) (~0222 & status.st_mode);

        //recieve the size of read request from the client
        size_t recieve_size=recv(socket_fd,&requested_size, sizeof(size_t),0);
        if (recieve_size == -1){
            perror("unable to recieve request");
            return 1; 
        }
        //recieve the offset (read start) from the client
        recieve_size=recv(socket_fd,&requested_offset, sizeof(off_t),0);
        if (recieve_size == -1){
            perror("unable to recieve request");
            return 1; 
        }
        //if where we are reading from is out of file bounds
        if (requested_offset>status.st_size){
            requested_offset =0;
        }
        if (requested_size> status.st_size){
            requested_size=status.st_size-requested_offset;
        }
        //if the begining to the end of the file is out of bounds (we reset offset to be at the beginning)
        if (requested_size-requested_offset >status.st_size ){
            requested_offset=0;
        }
        //send what we have read to the client
        file_sent= sendfile(socket_fd, open_file, &requested_offset, requested_size);
        if (file_sent == -1){
            perror("could not send file data");
            return 1; 
        }
        //send the actual size of read preformed by the server
        size_t write_size= send(socket_fd,&requested_size,sizeof(int),0); 

        if (write_size == -1){
            perror("unable to send file size");
            return 1;
        }

    }
    return 0;
}

/**
 * read directory function
 *
 * this function is responsible for opening a directory on the server and reading its files and folders
 *
 * @param client_path | this is the directory path that the client is asking to open and read
 *
 * @param server_path | the path that was initialized to start on the server
 *
  * @param socket_fd | the socket we set up for connection
 *
 * Does not envoke helper functions
 */
int readdir_send(const char * client_path,char * server_path, int socket_fd){


    DIR *dir;
    struct dirent *file;
    int size_path;


    if ((strncmp(client_path,".",1) == 0 && strlen(client_path)== 1) || (strncmp(client_path,"./",2)==0 && strlen(client_path)>2)){
        
        dir= opendir(client_path);
        if (dir == NULL){
            perror("directory not found");
            return 1;
        }
        while((file=readdir(dir)) != NULL){
            size_path= strlen(file->d_name);
            size_t write_size_length = send(socket_fd,&size_path,sizeof(int),0);   
            size_t write_size = send(socket_fd,&file->d_name,strlen(file->d_name)+1,0);   
            if (write_size == -1 || write_size_length==-1){
                perror("sending request failed");
                return 1;
            }
        }
    } 
    else{
    
        perror("client path is not relative or absolute to server path");
        return 1;
    }

    closedir(dir);
    return 0;
}


/**
 * main function
 *
 * this function is responsible for recieving request message and tranferring it to correct server handler
 *
 */
int main(int argc, char *argv[]) {

    if (argc == 3){

        directory = argv[1];
        port=atoi(argv[2]);

    } 
    else if (argc == 2){
        directory = argv[1];
        port=DEFAULT_PORT;

    } else{
        perror("inacurate insertion of argument count");
        return 1;
    }

    //check path provided 

    if (chdir(directory) == -1){
        perror("path provided is invalid");
        return 1;
    }

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("unable to create socket");
        return 1;
    }

    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(socket_fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("bind");
        return 1;
    }

    if (listen(socket_fd, 10) == -1) {
        perror("listen");
        return 1;
    }

    LOG("Listening on port %d\n", port);


    while (true) {
            /* Outer loop: this keeps accepting connection */
            
            //handling multiple processes
            p_id = fork();

            if (p_id == -1){
                perror("unable to create a new process");
                return 1;
            } 
            else if (p_id != 0){
                int wait_stat;
                waitpid(p_id, &wait_stat, 0);
            }

            struct sockaddr_in client_addr = { 0 };
            socklen_t slen = sizeof(client_addr);

            int client_fd = accept(
                    socket_fd,
                    (struct sockaddr *) &client_addr,
                    &slen);

            if (client_fd == -1) {
                perror("accept");
                return 1;
            }

            char remote_host[INET_ADDRSTRLEN];
            inet_ntop(
                    client_addr.sin_family,
                    (void *) &((&client_addr)->sin_addr),
                    remote_host,
                    sizeof(remote_host));
            LOG("Accepted connection from %s:%d\n", remote_host, client_addr.sin_port);

            size_t recieve_size=recv(socket_fd,&request_op, sizeof(struct request_operations),0);
            if (recieve_size == -1){
                perror("unable to recieve request");
                return 1; 
            }

            if (request_op.request_type == 1){
                readdir_send(request_op.request, directory,socket_fd);
            } 
            else if(request_op.request_type == 2){
                getattr_send(request_op.request, directory,socket_fd);
            } 
            else if(request_op.request_type == 3){
                open_send(request_op.request, directory,socket_fd);

            }
            else if(request_op.request_type == 4){
                readfile_send(request_op.request,directory,socket_fd);
            }
        }

        return 0; 

}
