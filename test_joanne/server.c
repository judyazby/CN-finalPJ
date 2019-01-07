/*
Computer Networks Final Project
CNline server v0
Create Date: 2018.12.22
Update Date: 2019.01.06
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
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>

#define ERR_EXIT(a) { perror(a); exit(1); }
#define PKT_BUFSIZE 65536
#define DEBUG 1

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    int id;
    char username[32];
    char password[32];
    int fd;
    /* save the person chatting with now */
    int receiverId;
} user;

typedef struct {
    char filename[32];
    int fileSize;
    char *content;
} file;

server svr;  // server
user userList[300];
int maxfd;
int userCnt;
fd_set master;
fd_set readfds;

int registration ( char *params, int sockfd );
int userLogin ( char *params, int sockfd );
int logout ( int sockfd );
void fileTransfer(char *params, int sockfd);
int messaging(char *message, int sockfd);
void setReceiver(char *params, int sockfd);
void setUsernameByFd(char *userName, int sockfd);
int sendEntireFile(char *filepath, int sockfd);
int checkUnreadMsg(int sockfd);
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
    fcntl(svr.listen_fd, F_SETFL, O_NONBLOCK);//non-blocking
}

void init_user() {
    printf("init_user\n");
    FILE *fp = fopen("user.dat", "r");
    if (fp) {
        int i = 0;
        while(fscanf( fp, "%d %s %s", &userList[i].id, userList[i].username, userList[i].password) == 3) {
            userList[i].fd = -1;
            userList[i].receiverId = -1;
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
    char buf[PKT_BUFSIZE];
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
                            FD_CLR(i, &master);
                            close(i);
                        }
                        else {
                            ERR_EXIT("recv");
                        }
                    }
                    else {
                        #if DEBUG == 1
                        fprintf(stderr, "Server received %d bytes from client!\n", nbytes);
                        fprintf(stdout,"Contents:%s\n", buf);
                        #endif
                        char action;
                        char *params;
                        // int len = strlen(buf);
                        // Parse hostname
                        sscanf(buf,"%c", &action);
                        params = &(buf[2]);
                        int ret;
                        switch (action) {
                            case 'R':{      // registration
                                ret = registration(params, i);
                            }
                            break;
                            case 'U':{      // get user data
                                ret = sendEntireFile("./user.dat", i);
                            }
                            break;
                            case 'L':{      // user login
                                ret = userLogin(params, i);
                            }
                            break;
                            case 'S':{      // set receiver
                                setReceiver(params, i);                             
                            }
                            break;
                            case 'F':{      // file transfer
                                fileTransfer(params, i);                               
                            }
                            break;
                            case 'C':{      //check new message
                                ret = checkUnreadMsg(i);
                            }
                            break;
                            case 'M':{      // messaging
                                ret = messaging(params, i);
                            }
                            break;
                            case 'Q':{      // user logout
                                ret = logout(i);
                            }
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

void fileTransfer(char *params, int sockfd){
#if DEBUG == 1
    fprintf(stderr, "in fileTransfer\n");
#endif
    // init file info
    file f;
    int senderId, receiverId;
    char buf[PKT_BUFSIZE];
    sscanf(params, "%[^\n]", buf);
    sscanf(buf, "%s%d", f.filename, &f.fileSize);
    int l = strlen(buf);
    f.content = &(params[l+1]);

#if DEBUG == 1
    fprintf(stderr, "filename = %s fileSize = %d\n content = \n%s\n", f.filename, f.fileSize, f.content);
#endif
    // set up sender and receiverId by sockfd
    for (int i = 0; i < userCnt; i ++) {
        if (userList[i].fd == sockfd) {
            senderId = i;
            receiverId = userList[i].receiverId;
            break;
        }
    }
    int receiverSockfd = userList[receiverId].fd;

#if DEBUG == 1
    fprintf(stderr, "senderId = %d receiverId = %d userList[receiverId].receiverId = %d\n", senderId, receiverId, userList[receiverId].receiverId);
#endif

    // send file to receiver
    char *sendMsg;
    sendMsg = (char*) malloc(PKT_BUFSIZE);
    memset(sendMsg, 0, PKT_BUFSIZE);
    if ( userList[receiverId].receiverId != senderId) {
        // receiver is offline
        sprintf(sendMsg, "F 0 %s is offline or chatting with other while transfering \"%s\"", userList[receiverId].username, f.filename);
        fprintf(stderr, "sendMsg = %s", sendMsg);
        if (send(sockfd, sendMsg, strlen(sendMsg), 0) == -1) {
            ERR_EXIT("send");
        }
        free(sendMsg);
        return;
    }
    else {
        // receiver is online >> do file transfer
        int nbyte_send;
        sprintf(sendMsg, "F 1 %s\n", buf);
        strcat(sendMsg, f.content);
#if DEBUG == 1
        fprintf(stderr, "sendMsg = %s\n", sendMsg);
        fprintf(stderr, "receiverSockfd = %d\n", receiverSockfd);
#endif
        if ((nbyte_send = send(receiverSockfd, sendMsg, PKT_BUFSIZE, 0)) == -1) {
            ERR_EXIT("send");
        }
        free(sendMsg);
        fprintf(stderr, "nbyte_send = %d size = %d\n", nbyte_send, l);
    }
    return;
}

int sendEntireFile(char *filepath, int sockfd) {
#if DEBUG == 1
    fprintf(stderr, "in sendEntireFile\n");
    fprintf(stderr, "filepath: %s\n", filepath);
#endif

    FILE *pFile = fopen( filepath, "rb");
    char *buffer;
    
    if(pFile == NULL) {
        buffer = (char*) malloc (PKT_BUFSIZE);
        strcpy(buffer, "no such file");
        if (send(sockfd, buffer, PKT_BUFSIZE, 0) == -1) {
            ERR_EXIT("send");
        } else {
            free(buffer);
        }
        return -1;
    }
    long lSize;
    size_t result;
    // obtain file size:
    fseek (pFile , 0 , SEEK_END);
    lSize = ftell (pFile);
    rewind (pFile);

    // allocate memory to contain the whole file:
    buffer = (char*) malloc (sizeof(char)*lSize);
    memset(buffer, 0, sizeof(buffer));

    if (buffer == NULL) {fputs ("Memory error",stderr); exit (2);}

    // copy the file into the buffer:
    result = fread (buffer,1,lSize,pFile);
    if (result != lSize) {fputs ("Reading error",stderr); exit (3);}

#if DEBUG == 1
    fprintf(stderr, "read buffer = %s\n", buffer);
#endif

    if (send(sockfd, buffer, result, 0) == -1) {
        ERR_EXIT("send");
    } else {
        fclose(pFile);
        free (buffer);
    }
    /* the whole file is now loaded in the memory buffer. */

    // terminate
    return (int)result;
}

void setReceiver(char *params, int sockfd){
#if DEBUG == 1
    fprintf(stderr, "in setReceiver\n");
#endif
    char receiverName[32];

    // set reciver info of sender in userlist
    for (int i = 0; i < userCnt; i ++) {
        if (userList[i].fd == sockfd) {
            sscanf(params, "%s", receiverName);
            if (strcmp(receiverName, "closeWindow")==0){
                userList[i].receiverId = -1;
                break;
            }
            for (int j = 0; j < userCnt; j ++) {
                if(strcmp(userList[j].username, receiverName)==0){
                    userList[i].receiverId = j;
                }
            }
            char filepath[20], sender[4], receiver[4];
            memset(filepath, 0, sizeof(filepath));
            strcpy(filepath, "./history/");
            snprintf(sender, sizeof(sender), "%d", i);
            strcat(filepath, sender);
            strcat(filepath, "_");
            snprintf(receiver, sizeof(receiver), "%d", userList[i].receiverId);
            strcat(filepath, receiver);

#if DEBUG == 1
            fprintf(stderr, "history filepath:%s\n", filepath);
#endif

            int ret = sendEntireFile(filepath, sockfd);     //send history

#if DEBUG == 1
            fprintf(stderr, "ret=%d\n", ret);
#endif
            if(ret != -1) {
                FILE *fp = fopen( filepath, "r+");
                char msg[PKT_BUFSIZE];
                char read[4];
                int len = 0;
                while(fscanf(fp, "%[^\n]", msg) == 1){
                    len += strlen(msg)+1;
#if DEBUG ==1
                    fprintf(stderr, "%s %d\n", msg, len);
#endif

                    if (msg[strlen(msg)-1]=='0'){
#if DEBUG == 1
                        fprintf(stderr, "change read!%d\n", len);
#endif
                        int res = fseek(fp, len-2, SEEK_SET);
                        strcpy(read,"1");
                        if(res < 0)
                        {
                            ERR_EXIT("fseek");
                        }
                        fprintf(fp, "%s", read);
                    }
                    char c;
                    fscanf(fp, "%c", &c);
                } 
                fclose(fp);
            }
            
            break;
        }
    }
    return;
}

void setUsernameByFd(char *userName, int sockfd){
#if DEBUG == 1
    fprintf(stderr, "in setUsernameByFd\n");
#endif
    for (int i = 0; i < userCnt; i ++) {
        if (userList[i].fd == sockfd) {
            strcpy(userName, userList[i].username);
            break;
        }
    }
}

int messaging(char *message, int sockfd){
    int recvSockfd=-1;
    int senderId, receiverId;
    
#if DEBUG == 1
    fprintf(stderr, "in messaging\n");
    fprintf(stderr, "server receive message %s from socket %d\n", message, sockfd);
#endif
    
    // set senderId and receiverId by sockfd
    for (int i = 0; i < userCnt; i ++) {
        if (userList[i].fd == sockfd) {
            senderId = i;
            receiverId = userList[i].receiverId;
            break;
        }
    }
    
    // find the hitory file
    char filepath1[20], filepath2[20], sender[4], receiver[4];
    memset(filepath1, 0, sizeof(filepath1));
    memset(filepath2, 0, sizeof(filepath2));
    strcpy(filepath1, "./history/");
    strcpy(filepath2, "./history/");
    snprintf(sender, sizeof(sender), "%d", senderId);
    snprintf(receiver, sizeof(receiver), "%d", receiverId);
    strcat(filepath1, sender);
    strcat(filepath2, receiver);
    strcat(filepath1, "_");
    strcat(filepath2, "_");
    strcat(filepath1, receiver);
    strcat(filepath2, sender);
#if DEBUG == 1
    fprintf(stderr, "history filepath1:%s\n", filepath1);
    fprintf(stderr, "history filepath2:%s\n", filepath2);
#endif

    //find receiver sockfd
    for (int i = 0; i < userCnt; i ++) {
        if (receiverId==userList[i].id) {
            recvSockfd=userList[i].fd;
            break;
        }
    }
    
#if DEBUG == 1
    fprintf(stderr, "recvSockfd: %d\n", recvSockfd);
#endif

    // write message to history file
    FILE *fp1 = fopen( filepath1, "a");
    FILE *fp2 = fopen( filepath2, "a");
    fprintf(fp1, "%d %s 1\n", senderId, message);           // 1 read(don't care)
    
    if (recvSockfd != -1 && userList[receiverId].receiverId == senderId) {            // online
        fprintf(fp2, "%d %s 1\n", senderId, message);       // 1 read(online)
        char sendMsg[PKT_BUFSIZE];
        memset(sendMsg, 0, sizeof(sendMsg));
        strcpy(sendMsg, "M ");
        strcat(sendMsg, message);
        // if reciever is online and chat with sender, send the spot message to sockfd
        if (send(recvSockfd, sendMsg, sizeof(sendMsg)+1, 0) == -1) {
            ERR_EXIT("send");
        }
    }
    else {                           // offline
        fprintf(fp2, "%d %s 0\n", senderId, message);       // 0 unread(offline)
    }
    fclose(fp1);
    fclose(fp2);
}

int checkUnreadMsg ( int sockfd ) {
#if DEBUG == 1
    fprintf(stderr, "in checkUnreadMsg\n");
#endif
    int userId;
    for (int i = 0; i < userCnt; i ++) {
        if (userList[i].fd == sockfd) {
            userId = i;
            break;
        }
    }
    int check = 0;
    char unreadMsg[PKT_BUFSIZE];
    memset(unreadMsg, 0, PKT_BUFSIZE);
    for (int i = 0; i < userCnt; i ++) {
        int unreadCnt = 0;
        char filepath[20], sender[4], receiver[4];
        memset(filepath, 0, sizeof(filepath));
        strcpy(filepath, "./history/");
        snprintf(sender, sizeof(sender), "%d", userId);
        snprintf(receiver, sizeof(receiver), "%d", i);
        strcat(filepath, sender);
        strcat(filepath, "_");
        strcat(filepath, receiver);
        FILE *fp = fopen( filepath, "r+" );
        if (fp == NULL) continue;
        else{
            char unreadFromSender[PKT_BUFSIZE];
            memset(unreadFromSender, 0, PKT_BUFSIZE);
            char msg[PKT_BUFSIZE];
            memset(msg, 0, PKT_BUFSIZE);
            char read[4];
            int len = 0;
            while(fscanf(fp, "%[^\n]", msg) == 1){
                len += strlen(msg)+1;
#if DEBUG ==1
                fprintf(stderr, "%s\n", msg);
#endif
                if (msg[strlen(msg)-1]=='0'){
                    unreadCnt ++;
#if DEBUG == 1
                        fprintf(stderr, "change read!%d\n", len);
#endif
                        int res = fseek(fp, len-2, SEEK_SET);
                        strcpy(read,"1");
                        if(res < 0)
                        {
                            ERR_EXIT("fseek");
                        }
                        fprintf(fp, "%s", read);
                        msg[strlen(msg)-2] = '\0';
                        char senderInMsg[4];
                        sscanf(msg, "%s", senderInMsg);
                        //msg = &(msg[2]);
                        strcat(unreadFromSender, "\t");
                        strcat(unreadFromSender, &(msg[strlen(senderInMsg)+1]));
                        strcat(unreadFromSender, "\n");
                }
                char c;
                fscanf(fp, "%c", &c);
            } 
            fclose(fp);
            if (unreadCnt > 0) {
                check = 1;
                char senderInfo[1024];
                memset(senderInfo, 0, 1024);
                sprintf(senderInfo, "* %d unread messages from < %s >\n", unreadCnt, userList[i].username);
                strcat(unreadMsg, senderInfo);
                strcat(unreadMsg, unreadFromSender);
            }
        }
    }
    if(!check) {
        // no unread msg
        sprintf(unreadMsg, "There is no unread message.\n");
    }
    if (send(sockfd, unreadMsg, strlen(unreadMsg)+1, 0) == -1) {
        ERR_EXIT("send");
    }
    return 0;
}


int registration ( char *params, int sockfd ) {
#if DEBUG == 1
    fprintf(stderr, "in registration\n");
    printUser();
#endif
    char newUsername[32];
    char newPassword[32];
    sscanf(params, "%s %s", newUsername, newPassword);
    int i, check = 0;
    char returnMessage[PKT_BUFSIZE];
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
        userList[userCnt].id = userCnt;
        userList[userCnt].fd = sockfd;
        userList[userCnt].receiverId = -1;
        
        FILE *fp = fopen("user.dat", "a");
        fprintf(fp, "%d %s %s\n", userCnt, newUsername, newPassword);
        fclose(fp);
        strcpy(returnMessage, "2");
        userCnt++;
    }
    if (send(sockfd, returnMessage, strlen(returnMessage)+1, 0) == -1) {
        ERR_EXIT("send");
    }
    return 0;
}

int userLogin( char *params, int sockfd ) {
# if DEBUG == 1
    fprintf(stderr, "in userLogin: %s\n", params);
    printUser();
# endif
    char newUsername[32];
    char newPassword[32];
    sscanf(params, "%s %s", newUsername, newPassword);
    int i, check = 0;
    char returnMessage[PKT_BUFSIZE];
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
    return 1;
}

int logout(int sockfd) {
#if DEBUG == 1
    fprintf(stderr, "in logout\n");
#endif
    for (int i = 0; i < userCnt; i++) {
        if ( sockfd == userList[i].fd ) {
            userList[i].fd = -1;       // offline
            userList[i].receiverId = -1;
        }
    }
    return 0;
}

void printUser() {
    fprintf(stderr, "in printUser\n");
    for (int i= 0; i < userCnt; i++) {
        fprintf(stderr, "%s %s %d\n", userList[i].username, userList[i].password, userList[i].fd);
    }
    return;
}

