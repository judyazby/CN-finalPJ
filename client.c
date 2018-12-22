/*
Computer Networks Project 1 v4
TCP Ping with multithread/ Non-blocking connection
Author: b04701232 Joanne Chen
Create Date: 2018.11.01
Update Date: 2018.11.04
Reference: 
1. https://www.geeksforgeeks.org/ping-in-c/
2. http://beej-zhtw.netdpi.net/
3. http://man7.org/linux/man-pages/man3/pthread_create.3.html
4. https://stackoverflow.com/questions/2597608/c-socket-connection-timeout
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

#define ERR_EXIT(a) { perror(a); exit(1); }
// #define THREAD_NUM 4
#define DEBUG 1

struct timeval timeout;
int npacket;
int maxfd;
int time_argv_sec = 1, time_argv_usec = 0;
fd_set master;
fd_set readfds;

void *TCP_ping(void *args);
int userLogin (char *username, char *password, int sockfd);


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
                fprintf(stderr, "login failed\n");
                return 1;
            } else if ( login == 0) {   // login success
                fprintf(stderr, "login success\nusername is %s, password is %s!\n", username, password);
            } else {                    // newUser
                fprintf(stderr, "newUser\nusername is %s, password is %s!\n", username, password);
            }
            // Send message to the server
            /*
            int nbytes_send;
            char message[512] = {"Hello Socket Programming!"};
            //clock_gettime(CLOCK_MONOTONIC, &time_start);
            if ((nbytes_send = send(sockfd, message, sizeof(message), 0)) == -1) {
                ERR_EXIT("send")
            }
            #if DEBUG == 1
            fprintf(stderr, "Client send %d bytes to server!\n", nbytes_send);
            #endif
            flag = 1;
            FD_SET(sockfd, &master);
            */
        }
    }
    /*
    // Sign up and login interface
    char c;
    fprintf(stdout, "Welcome to CNline!\nLogin?(L)\nSign up(S)\n");
    fscanf(stdin, "%c", &c);
    if ( c == 'S' || c == 's') {
        fprintf(stdout, "ya sign up!\n");
    } else if ( c == 'L' || c == 'l'){
        fprintf(stdout, "ya login!\n");
    }
    */
    
    /*
    pthread_t thread[tnum];
    int tcnt = 0;
    for ( i = 0; i < argc; i ++) {
        if (!host[i])   continue;
        else {
            if(pthread_create(&thread[tcnt], NULL, (void*)TCP_ping, argv[i])!=0){
				fprintf(stderr, "error: cannot create thread #%d\n", tcnt);
				break;
			}
			else    tcnt ++;
        }
    }
    for (i = 0; i < tnum; i ++) {
		if(pthread_join(thread[i], NULL)!=0){
			fprintf(stderr, "error: cannot join thread #%d\n", i);
		}
	}
    */
    return 0;
}

int userLogin (char *username, char *password, int sockfd) {
    // Sign up and login interface
    char c;

    fprintf(stdout, "Welcome to CNline!\n> Login(L)\n> Sign up(S)\n> Cancel(C)\n");
    while (1) {
        fscanf(stdin, "%c", &c);
        if ( c == 'S' || c == 's') {
            fprintf(stdout, "ya sign up!\n");
            fprintf(stdout, "username:");
            fscanf(stdin, "%s", username);
            fprintf(stdout, "password:");
            fscanf(stdin, "%s", password);
            break;
        } else if ( c == 'L' || c == 'l'){
            fprintf(stdout, "ya login!\n");
            fprintf(stdout, "username:");
            fscanf(stdin, "%s", username);
            fprintf(stdout, "password:");
            fscanf(stdin, "%s", password);
            break;
        } else if ( c == 'C' || c == 'c'){
            fprintf(stdout, "Bye!\n");
            return -1;
        } else {
            fprintf(stdout, "you type the wrong format!please try again QQ\n");
        }
    }
    
    // Send user profile to server
    int nbytes_send;
    char message[512];
    message[0] = c;
    message[1] = '\0';
    strcat(message, ",");
    strcat(message, username);
    strcat(message, ",");
    strcat(message, password);
    
    #if DEBUG == 1
    fprintf(stderr, "%s\n", message);
    #endif

    if ((nbytes_send = send(sockfd, message, sizeof(message), 0)) == -1) {
        fprintf(stderr, "%d\n", nbytes_send);
        ERR_EXIT("send")
    }
    fprintf(stderr, "%d\n", nbytes_send);
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
            fprintf(stderr,"message: '%s'\n", receiveMessage);
            #endif
            
        }
        FD_CLR(sockfd, &master);
    }
    return 0;
}



/*
void *TCP_ping(void *args){
    char *hostname = (char *)args;
    char *port;
    #if DEBUG == 1
    fprintf(stderr, "server: %s\n", hostname);
    #endif
    int i;
    int len = strlen(hostname);
    // Parse hostname
    for (i = 0; i < len; i ++) {
        if (hostname[i]==':') {
            port = &(hostname[i+1]);
            hostname[i] = '\0';
            break;
        }
    }
    #if DEBUG == 1
        fprintf(stderr, "hostname: %s\nport: %s\n", hostname, port);
    #endif
    int n = 1;

    while(n <= npacket || npacket == 0) {
        // Initialize before each packet sent
        double rtt_msec = 0.0;
        struct timespec time_start, time_end;
        fd_set master;
        fd_set readfds;
        FD_ZERO(&master);
        FD_ZERO(&readfds);
        timeout.tv_sec = time_argv_sec;
        timeout.tv_usec = time_argv_usec;
        
        // Get addrinfo
        struct addrinfo hints, *res;
        int status, flag = 0;
        void *addr;
        char ipstr[INET6_ADDRSTRLEN] = {0};
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;        // don't care about it is IPv4 or IPv6
        hints.ai_socktype = SOCK_STREAM;    // TCP stream socket
        if ((status = getaddrinfo(hostname, port, &hints, &res)) != 0) {
            fprintf(stderr, "getaddrinfo:%s\n", gai_strerror(status));
            return NULL;
        }
        if (res->ai_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
            addr = &(ipv4->sin_addr);
        }
        inet_ntop(res->ai_family, addr, ipstr, sizeof(ipstr));    //turn unit bits to number
        #if DEBUG == 1
        fprintf(stderr, "%s: %s\n", ipstr, port);
        #endif

        // Build socket
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

                // Send message to the server
                int nbytes_send;
                char message[512] = {"Hello Socket Programming!"};
                clock_gettime(CLOCK_MONOTONIC, &time_start);
                if ((nbytes_send = send(sockfd, message, sizeof(message), 0)) == -1) {
                    ERR_EXIT("send")
                }
                #if DEBUG == 1
                fprintf(stderr, "Client send %d bytes to server!\n", nbytes_send);
                #endif
                flag = 1;
                FD_SET(sockfd, &master);
            }
        }
        
        // Wait for recv
        readfds = master;   // duplicate master fd_set
        int conn;

        if ((conn = select(maxfd + 1, &readfds, NULL, NULL, &timeout)) == -1) {
            ERR_EXIT("select")
        }
        else if (conn == 0) {       // Timeout or unreachable
            #if DEBUG == 1
            fprintf(stderr, "packet number: %d timeout!\n", n);
            #endif
            fprintf(stdout, "timeout when connect to %s\n", ipstr);
            if (npacket != 0)   n++;
            close(sockfd);
            continue;
        }
        if (FD_ISSET(sockfd, &readfds)) {
            char receiveMessage[512] = {};
            int nbytes_recv;
            if((nbytes_recv = recv(sockfd, receiveMessage, sizeof(receiveMessage), 0))<=0) {
                fprintf(stderr, "nothing recv\n");
            }
            else{
                clock_gettime(CLOCK_MONOTONIC, &time_end);
                #if DEBUG == 1
                fprintf(stderr,"packet number: %d message: '%s'\n", n, receiveMessage);
                #endif
                if (flag) {
                    double timeElapsed = ((double)(time_end.tv_nsec - time_start.tv_nsec))/1000000.0;
                    rtt_msec = (time_end.tv_sec - time_start.tv_sec) * 1000.0 + timeElapsed;
                    fprintf(stdout, "recv from %s, RTT = %lf msec\n", ipstr, rtt_msec);
                }
            }
            FD_CLR(sockfd, &master);
        }
        close(sockfd);
        if (npacket == 0)   continue;
        else    n++;
    }
    return NULL;
}

*/