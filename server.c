/*
Computer Networks Final Project
CNline server v0
Create Date: 2018.12.22
Update Date: 2018.12.22
Reference: 

*/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/file.h>
#include <fcntl.h>

#define ERR_EXIT(a) { perror(a); exit(1); }
#define DEBUG 0

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char username[32];
    char password[32];
    int fd;
} user;

server svr;  // server
user userList[300];
int maxfd;
int userCnt;
fd_set master;
fd_set readfds;

int registration ( char *params, int sockfd );
int userLogin ( char *params, int sockfd );
int logout ( int sockfd );
void printUser();       // for debug


static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }
    #if DEBUG == 1
    fprintf(stderr, "init_server success!\n");
    #endif
}

void init_user() {
    FILE *fp = fopen("user.dat", "r");
    if (fp) {
        int i = 0;
        while(fscanf( fp, "%s %s", userList[i].username, userList[i].password) == 2) {
            userList[i].fd = -1;
            i++;
        }
        userCnt = i;
        fclose(fp);
    }
}

int main(int argc, char *argv[]) {
    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    // Initialize server
    maxfd = getdtablesize();
    userCnt = 0;
    init_server((unsigned short) atoi(argv[1]));
    init_user();
    #if DEBUG == 1
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);
    #endif

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;
    int conn_fd;  // fd for a new connection with client
    char buf[512];
    int buf_len;

    
    FD_ZERO(&master);
    FD_ZERO(&readfds);
    FD_SET(svr.listen_fd, &master);
    while(1) {
        readfds = master;   // duplicate master fd_set
        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) == -1) {
            ERR_EXIT("select")
        }
        int i;
        for (i = 0; i < maxfd; i ++) {
            if (FD_ISSET(i, &readfds)) {
                if (i==svr.listen_fd) {
                    clilen = sizeof(cliaddr);
    			    conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
    			    if (conn_fd < 0) {
       				    if (errno == EINTR || errno == EAGAIN) continue;  // try again
            			if (errno == ENFILE) {
                		    (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                		    continue;
            		    }
            			ERR_EXIT("accept")
        			}
                    else {
                        FD_SET(conn_fd, &master);
                        #if DEBUG == 1
                        fprintf(stderr, "recv from %s:%d\n", inet_ntoa(cliaddr.sin_addr), (int)cliaddr.sin_port);
                        fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, inet_ntoa(cliaddr.sin_addr));
                        #endif
                    }
                }
                else {  // recv client
                    int nbytes;
                    #if DEBUG == 1
                    fprintf(stderr, "i=%d\n", i);
                    #endif
                    if ((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0) {
                        if (nbytes == 0) {  // connection closed by client
                            fprintf(stderr, "selectserver: socket %d hung up\n", i);
                            logout(i);
                        }
                        else {
                            ERR_EXIT("recv");
                        }
                    }
                    else {
                        #if DEBUG == 1
                        fprintf(stderr, "Server received %d bytes from client!\n", nbytes);
                        #endif
                        char action;
                        char *params;
                        // int len = strlen(buf);
                        // Parse hostname
                        sscanf(buf,"%c", &action);
                        params = &(buf[2]);
                        int ret;
                        switch (action) {
                            case 'R':
                                ret = registration(params, i);
                            break;
                            case 'L':
                                ret = userLogin(params, i);
                            break;
                            case 'Q':
                                ret = logout(i);
                            break;
                            default:
                                fprintf(stderr, "do nothing!\n");
                        }

                    }
                }
            }
        }
    }
    return 0;
}


int registration ( char *params, int sockfd ) {
    printUser();
    char newUsername[32];
    char newPassword[32];
    sscanf(params, "%s %s", newUsername, newPassword);
    int i, check = 0;
    char returnMessage[512];
    memset(&returnMessage, 0, sizeof(returnMessage));
    for ( i = 0; i < userCnt; i ++) {

        if (strcmp(newUsername, userList[i].username) == 0) {          // registeration duplicate username
            check = 1;
        }
    }

    if (check) {          // registeration duplicate username
        strcpy(returnMessage, "1");
    } else {                        // registration success
        strcpy(userList[userCnt].username, newUsername);
        strcpy(userList[userCnt].password, newPassword);
        userList[userCnt].fd = sockfd;
        userCnt++;
        FILE *fp = fopen("user.dat", "a");
        fprintf(fp, "%s %s\n", newUsername, newPassword);
        fclose(fp);
        strcpy(returnMessage, "2");
    }
    if (send(sockfd, returnMessage, strlen(returnMessage)+1, 0) == -1) {
        ERR_EXIT("send");
    }
    return 0;
}

int userLogin( char *params, int sockfd ) {
    # if DEBUG == 1
    printUser();
    fprintf(stderr, "in userLogin: %s\n", params);
    # endif
    char newUsername[32];
    char newPassword[32];
    sscanf(params, "%s %s", newUsername, newPassword);
    int i, check = 0;
    char returnMessage[512];
    memset(&returnMessage, 0, sizeof(returnMessage));
    for ( i = 0; i < userCnt; i ++) {

        if (strcmp(newUsername, userList[i].username) == 0) {
            if (strcmp(newPassword, userList[i].password) == 0) {      // login success
                userList[i].fd = sockfd;
                strcpy(returnMessage, "3");
            } else {                                                   // wrong password
                strcpy(returnMessage, "4");
            }
            check = 1;
        }
    }
    if (!check) {
        strcpy(returnMessage, "4");
    }

    if (send(sockfd, returnMessage, sizeof(returnMessage), 0) == -1) {
        ERR_EXIT("send");
    }
}

int logout(int sockfd) {
    int i;
    for ( i = 0; i < userCnt; i++) {
        if ( sockfd == userList[i].fd ) {
            userList[i].fd = -1;       // offline
        }
    }
    FD_CLR(sockfd, &master);
    close(sockfd);
    return 0;
}

void printUser() {
    int i;
    fprintf(stderr, "userprint\n");
    for ( i= 0; i < userCnt; i++) {
        fprintf(stderr, "%s %s %d\n", userList[i].username, userList[i].password, userList[i].fd);
    }
    return;
}