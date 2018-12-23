/*
Computer Networks Final Project
CNline client v0
Create Date: 2018.12.22
Update Date: 2018.12.22
Reference: 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#define ERR_EXIT(a) { perror(a); exit(1); }
// #define THREAD_NUM 4
#define DEBUG 0

struct timeval timeout;
int npacket;
int maxfd;
int time_argv_sec = 1, time_argv_usec = 0;
fd_set master;
fd_set readfds;

void *TCP_ping(void *args);
int userLogin (char *username, char *password, int sockfd);
int fileReceive(char *filename, int sockfd);

int main(int argc, char *argv[]) {
    int i, j;
    // Initialize global variables
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    maxfd = getdtablesize();


    // Initialize before each packet sent
    double rtt_msec = 0.0;
    struct timespec time_start, time_end;
    
    FD_ZERO(&master);
    FD_ZERO(&readfds);
    char hostname[10] = "127.0.0.1";
    char port[5] = "3000";


    // Get addrinfo
    struct addrinfo hints, *res;
    int status, flag = 0;
    void *addr;
    char ipstr[INET6_ADDRSTRLEN] = {0};
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;        // don't care about it is IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;    // TCP stream socket
    if ((status = getaddrinfo(&(hostname[0]), &(port[0]), &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo:%s\n", gai_strerror(status));
        return 1;
    }
    if (res->ai_family == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
        addr = &(ipv4->sin_addr);
    }
    inet_ntop(res->ai_family, addr, ipstr, sizeof(ipstr));    //turn unit bits to number
    #if DEBUG == 1
    fprintf(stderr, "%s: %s\n", ipstr, port);
    #endif

    // Build Socket
    int sockfd;
    if((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol))==-1) {
        ERR_EXIT("socket")
    }
    #if DEBUG == 1
        printf("socket fd = %d\n", sockfd);
    #endif
        
    // Set the socket Non-blocking
    
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags|O_NONBLOCK);
    
    // Connect and send message to server
        
    if((status = connect(sockfd, res->ai_addr, res->ai_addrlen)) == -1) {
        if (errno != EINPROGRESS) {
            #if DEBUG == 1
            fprintf(stderr, "connect error!\n");
            #endif
        }
    }

    // Use select as a timer, when the socket was successfully connected, select will return the fd is ready to write
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(sockfd, &writefds);
        
    if (select(sockfd + 1, NULL, &writefds, NULL, &timeout) == 1) {
        int so_error;
        socklen_t len = sizeof so_error;

        getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);

        if (so_error == 0) {    // connect success
            #if DEBUG == 1
            printf("%s: %s is open\n", ipstr, port);
            #endif
            fcntl(sockfd, F_SETFL, flags);  // Turn the socket back to Blocking mode
            // Registration
            char username[30], password[30];
            memset(&username, 0, sizeof(username));
            memset(&password, 0, sizeof(password));
            int login = userLogin(username, password, sockfd);
            if ( login < 0) {           // login fail
                fprintf(stdout, "login failed\n");
                return 1;
            } else if ( login == 2) {   // login success
                fprintf(stdout, "login success!! Hello, %s\n", username);
            } else {                    // newUser login = 1
                fprintf(stdout, "Sign up success!! Hello, %s\n", username);
            }
            int fr=fileReceive("user2.dat",sockfd);
            // Choose what to do
            fprintf(stdout, "What do you want to do?\n> Send message(S)\n> Send file(F)\n> Quit(Q)\n");
            char c;
            while(1) {
                fscanf(stdin, "%c", &c);
                int nbytes_send;
                if ( c == 'Q' || c == 'q') {
                    if ((nbytes_send = send(sockfd, "Q", 1, 0)) == -1) {
                        ERR_EXIT("send")
                    }
                    break;
                } else {
                    if ( c != '\n' ) {
                        fprintf(stdout, "you type the wrong format!please try again QQ\n");
                    }
                    continue;
                }
            }
        }
    }
    return 0;
}

int userLogin (char *username, char *password, int sockfd) {
    // Sign up and login interface
    char c;

    fprintf(stdout, "Welcome to CNline!\n> Login(L)\n> Sign up(R)\n> Cancel(C)\n");
    while (1) {
        fscanf(stdin, "%c", &c);
        if ( c == 'R' || c == 'r') {
            fprintf(stdout, "Sign up!\n");
            fprintf(stdout, "username:");
            fscanf(stdin, "%s", username);
            fprintf(stdout, "password:");
            fscanf(stdin, "%s", password);
        } else if ( c == 'L' || c == 'l'){
            fprintf(stdout, "Login!\n");
            fprintf(stdout, "username:");
            fscanf(stdin, "%s", username);
            fprintf(stdout, "password:");
            fscanf(stdin, "%s", password);
        } else if ( c == 'C' || c == 'c'){
            fprintf(stdout, "Bye!\n");
            return -1;
        } else {
            if ( c != '\n' ) {
                fprintf(stdout, "you type the wrong format!please try again QQ\n");
            }
            continue;
        }
        // Send user profile to server
        int nbytes_send;
        char message[512];
        message[0] = toupper(c);
        message[1] = '\0';
        strcat(message, " ");
        strcat(message, username);
        strcat(message, " ");
        strcat(message, password);
        
        #if DEBUG == 1
        fprintf(stderr, "%s\n", message);
        #endif

        if ((nbytes_send = send(sockfd, message, sizeof(message), 0)) == -1) {
            ERR_EXIT("send")
        }
        #if DEBUG == 1
        fprintf(stderr, "Client send %d bytes to server!\n", nbytes_send);
        #endif
        FD_SET(sockfd, &master);
        
        // Wait recv
        readfds = master;   // duplicate master fd_set
        int conn;

        if ((conn = select(maxfd + 1, &readfds, NULL, NULL, &timeout)) == -1) {
            ERR_EXIT("select")
        }
        else if (conn == 0) {       // Timeout or unreachable
            #if DEBUG == 1
            fprintf(stderr, "timeout!\n");
            #endif
            return -1;
        }
        if (FD_ISSET(sockfd, &readfds)) {
            char receiveMessage[512] = {};
            int nbytes_recv;
            if((nbytes_recv = recv(sockfd, receiveMessage, sizeof(receiveMessage), 0))<=0) {
                fprintf(stderr, "nothing recv\n");
            }
            else{
                #if DEBUG == 1
                fprintf(stderr,"message: %c\n%d\n", receiveMessage[0], nbytes_recv);
                #endif
                int ret = atoi(receiveMessage);
                if (ret == 1) {
                    fprintf(stdout, "Registration error: The username is existed, please change another username to sign up!\n> Sign up(R)\n> Login(L)\n> Cancel(C)\n");
                    continue;
                } else if (ret == 2) {  // Sign up success
                    return 1;           
                } else if (ret == 3) {  // Login success
                    return 2;           
                } else {                // Login fail
                    fprintf(stdout, "Login error: Somewhere wrong with username or password! Please try again!\n> Sign up(R)\n> Login(L)\n> Cancel(C)\n");
                    continue;
                }
                break;
            }
            // FD_CLR(sockfd, &master);
        }
    }
    return 0;
}

int fileReceive(char *filename, int sockfd){
    int fileSize,numbytes;
    char buf[1024];
    //receive filesize as the halt condition of non-blocking recv 
    FILE *fp;
    if((recv(sockfd, &fileSize, sizeof(int), 0))<=0) {
        fprintf(stderr, "file size 0\n");
    }
    #if DEBUG == 1
    printf("Size of the receive file=%d\n",fileSize);
    #endif
    
    //Open file
    if ( (fp = fopen(filename, "wb")) == NULL){
        ERR_EXIT("fopen")
    }
    int loopCount=1;
    int totalBytesRecv=0;
    //Receive file from server
    while(totalBytesRecv<fileSize){
        //numbytes = read(sockfd, buf, sizeof(buf));
        //set recv non-blocking
        numbytes =recv(sockfd, buf, sizeof(buf),MSG_DONTWAIT);
        //printf("read %d bytes, ", numbytes);
        if(numbytes > 0){
            numbytes = fwrite(buf, sizeof(char), numbytes, fp);
            totalBytesRecv += numbytes;
            printf("At loop %d, fwrite %d bytes\n", loopCount, numbytes);
        }     
        loopCount++;
    }
    printf("After %d loops, receive %d bytes from sockfd %d.", 
        loopCount, totalBytesRecv, sockfd);
    fclose(fp);
    
    return 1;
}

int userChooseTarget(char *userListFile){

    //print all users in user.dat now
    printf("Choose your target user:");
}
/*
int userSendMessage (int sockfd) {
    //after log in success, pass message-to-send to server
    struct sockaddr_in target;
    char ta_port[10];
    char ta_ip[30] = {'\0'};
    char str[300] = {'\0'};
    fprintf(stdout, "Target IP:");
    fscanf(stdin, "%s", ta_ip);
    fprintf(stdout, "Target port number:");
    fscanf(stdin, "%s", ta_port);
    target.sin_addr.s_addr = inet_addr(ta_ip);
    target.sin_port = htons(ta_port);
    target.sin_family = AF_INET;
    fprintf(stdout, "ENTER your message:");
    fgets(str,300,stdin);//fscanf?

    //message form:(target ip, target port,message)
    char message[512] = {'\0'};
    strcat(message, ta_ip);
    strcat(message, ",");
    strcat(message, ta_port);
    strcat(message, ",");
    strcat(message, str);

    int nbytes_send;
    if ((nbytes_send = send(sockfd, message, sizeof(message), 0)) == -1) {
        fprintf(stderr, "%d\n", nbytes_send);
        ERR_EXIT("send")
    }
    fprintf(stderr, "%d\n", nbytes_send);
    #if DEBUG == 1
    fprintf(stderr, "Client send %d bytes to server!\n", nbytes_send);
    #endif

}
*/