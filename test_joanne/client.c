/*
Computer Networks Final Project
CNline client v0
Create Date: 2018.12.22
Update Date: 2019.01.05
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
#include <dirent.h>
#include <libgen.h>

#define ERR_EXIT(a) { perror(a); exit(1); }
#define STDIN 0
#define PKT_BUFSIZE 65536
#define hostname "127.0.0.1"
#define port "3000"
#define DEBUG 0


typedef struct {
    int id;
    char username[32];
} user;


user userList[300];
int userCnt;
struct timeval timeout;
int maxfd;
int sockfd;
int time_argv_sec = 1, time_argv_usec = 0;
fd_set master;
fd_set readfds;

int userLogin (char *username, char *password, int sockfd);
int fileReceive(char *filename, int sockfd);
void userReadOrSend(char *username, int sockfd);
void userChooseTarget(char *userListFile, char *receiver, int sockfd);
int chooseToDo(char *sender, char *receiver, int sockfd, char *history);
void findfile(char* pattern);
void showUnreadMsg(char* fileName, char* userName);
void *sendEntireFile(void *args);
void recvEntireFile(char *params);

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
            int login = 0;
            while (login>=0){
                login = userLogin(username, password, sockfd);
                if ( login < 0) {           // login fail
                    //fprintf(stdout, "login failed\n");
                    return 1;
                } else if ( login == 2) {   // login success
                    fprintf(stdout, "login success!! Hello, %s\n", username);
                } else {                    // newUser login = 1
                    fprintf(stdout, "Sign up success!! Hello, %s\n", username);
                }
                userReadOrSend(username, sockfd);
            }
        }
    }
    return 0;
}



//send message or file to server with sockfd
int chooseToDo(char *sender, char *receiver, int sockfd, char *history){
    int i=0;
    char c;
    int senderId, read;
    char msg[PKT_BUFSIZE];
    char buf[PKT_BUFSIZE];
    memset(msg, 0, sizeof(msg));
    fprintf(stdout, "================================================================================\n");
    fprintf(stdout, "You are now chatting with %s!\nInstruction:\n> Send message -> Just type message that you want to send and press 'Enter'!\n> Send file(F)\n> Close this chat(C)\n", receiver);
    fprintf(stdout, "================================================================================\n");
    
    // set user info
    user sendUser, recvUser;
    for (i = 0; i < userCnt; i++) {
        if(strcmp(userList[i].username, sender) == 0) {
            sendUser.id = i;
            strcpy(sendUser.username, sender);
        }
        if(strcmp(userList[i].username, receiver) == 0) {
            recvUser.id = i;
            strcpy(recvUser.username, receiver);
        }
    }

    // print history
    if(strcmp(history, "no such file")!=0){
        while(sscanf(history, "%[^\n]", buf) >= 0){
            history = &(history[strlen(buf)+1]);
            sscanf(buf, "%d", &senderId);
            if (senderId == sendUser.id) {
                printf("< %s >:", sendUser.username);
            }
            else {
                printf("< %s >:", recvUser.username);
            }
            int l = strlen(buf);
            buf[l-2]='\0';
            printf("\t%s\n", &(buf[2]));
            //memset(msg, 0, sizeof(msg));
            memset(buf, 0, sizeof(buf));
        }
    }
    
    // listen to STDIN and sockfd to send and recv at the same time
    fprintf(stdout, "------------------------------------------------\n");
    fprintf(stdout, "(Type your message!)\n");
    FD_SET(STDIN, &master);
    FD_SET(sockfd, &master);
    int cnt = 0;

    while(1) {
        
        readfds = master;   // duplicate master fd_set
        int conn;

        if ((conn = select(maxfd + 1, &readfds, NULL, NULL, NULL)) == -1) {
            ERR_EXIT("select")
        }
        if (FD_ISSET(STDIN, &readfds)) {
            char inputMsg[PKT_BUFSIZE];
            memset(inputMsg, 0, sizeof(inputMsg));
            char c;
            fscanf(stdin, "%c", &c);
            fscanf(stdin, "%[^\n]", inputMsg);
            
            if(inputMsg[0]=='\n')   continue;
            memset(buf, 0, sizeof(buf));
            if (inputMsg[0]=='C') {
                int nbytes_send;
                strcpy(buf, "S closeWindow");
                fprintf(stdout, "\r============================================================\n");
                if ((nbytes_send = send(sockfd, buf, sizeof(buf), 0)) == -1) {
                    ERR_EXIT("send")
                }
                break;
            }
            else if(inputMsg[0]=='F'){      // file transfer
                // send and recv to see if receiver is online
                // if receiver is online, choose filepath and use multithread to send to server
                int fileCnt = 0;
                char filepath[3][32];
                fprintf(stdout, "Please enter the filepath:\n(You can at most choose 3 files to transfer at the same time, and seperate each filepath with a space.)\n");
                fscanf(stdin, "%c", &c);
                fscanf(stdin, "%[^\n]", buf);
                char *filepathPtr = buf;
                char filepathBuf[32] = {0};
                while(sscanf(filepathPtr, "%s", filepathBuf) == 1) {
                    filepathPtr = &(filepathPtr[strlen(filepathBuf)+1]);
                    if( access( filepathBuf, F_OK ) != -1 ) {       // check if the file exist
                        // file exists
#if DEBUG == 1
                        fprintf(stdout, "filepath = \"%s\" exist!\n", filepathBuf);
#endif
                        strcpy(filepath[fileCnt], filepathBuf);
                        fileCnt ++;
                        if ( fileCnt > 3 ) {
                            fprintf(stdout, "Warning: At most 3 files can be transfered at the same time!\n");
                            break;
                        }
                    } else {
                        // file doesn't exist
                        fprintf(stdout, "filepath = \"%s\" does not exist!\n", filepathBuf);
                    }
                }
#if DEBUG == 1
                fprintf(stderr, "fileCnt = %d\n", fileCnt);
#endif
                // create thread to send file to server samultaniously
                pthread_t thread[fileCnt];
                int tcnt;
                for ( tcnt = 0; tcnt < fileCnt; tcnt ++) {
                    if(pthread_create(&thread[tcnt], NULL, (void*)sendEntireFile, filepath[tcnt])!=0){
                        fprintf(stderr, "error: cannot create thread #%d\n", tcnt);
                        break;
                    }
                }
                for (tcnt = 0; tcnt < fileCnt; tcnt ++) {
                    if(pthread_join(thread[tcnt], NULL)!=0){
                        fprintf(stderr, "error: cannot join thread #%d\n", tcnt);
                    }
                }
            }
            else{
                // send message
                int nbytes_send;
                strcpy(buf, "M ");
                strcat(buf, inputMsg);
                if ((nbytes_send = send(sockfd, buf, sizeof(buf), 0)) == -1) {
                    ERR_EXIT("send")
                }
            }
                  
        }
        if (FD_ISSET(sockfd, &readfds)) {
            int nbytes_recv;
            char receiveMessage[PKT_BUFSIZE];
            memset(receiveMessage, 0, sizeof(receiveMessage));
#if DEBUG == 1
            fprintf(stderr, "in reading client sockfd!\n");
#endif
            if((nbytes_recv = recv(sockfd, receiveMessage, sizeof(receiveMessage), 0))<=0) {
                fprintf(stderr, "nothing recv\n");
            }
            else{
                // if the recv msg header is F
                // keep the message and packed into file
#if DEBUG == 1
                fprintf(stderr, "receiveMessage = %s %d\n", receiveMessage, nbytes_recv);
#endif
                if ( receiveMessage[0] == 'M' ) {
                    char *message = &(receiveMessage[2]);
                    fprintf(stdout, "\r< %s >:\t%s\n", receiver, message);
                }
                else if ( receiveMessage[0] == 'F' ) {
                    if (receiveMessage[2] == '0') {     // receiver is offline
                        char *errMsg = &(receiveMessage[4]);
                        fprintf(stdout, "\r< server >:\t%s\n", errMsg);
                    }
                    else {              // receiver is online
                        char *message = &(receiveMessage[4]);
#if DEBUG == 1
                        fprintf(stderr, "message:\n%s\n", message);
#endif
                        recvEntireFile(message);
                    }
                }
            }
        }
    }  
}

void *sendEntireFile(void *args){
#if DEBUG == 1
    fprintf(stderr, "in thread function!\n");
#endif
    char *filepath = (char *)args;
#if DEBUG == 1
    fprintf(stderr, "filepath:%s\n", filepath);
#endif
    FILE *pFile = fopen( filepath, "rb");
    char *buffer;
    
    if(pFile == NULL) {
        fprintf(stderr, "no such file!\n");
    }
    int lSize;
    size_t result;
    // obtain file size:
    fseek (pFile , 0 , SEEK_END);
    lSize = ftell (pFile);
    rewind (pFile);

    // allocate memory to contain the whole file:
    char *filename;
    char fileSize[10];
    filename = basename(filepath);
#if DEBUG == 1
    fprintf(stderr, "filename = %s %d\n", filename, lSize);
#endif
    
    buffer = (char*) malloc (sizeof(char)*lSize + PKT_BUFSIZE);
    memset(buffer, 0, sizeof(char)*lSize + PKT_BUFSIZE);
    strcat(buffer, "F ");
    strcat(buffer, filename);
    strcat(buffer, " ");
    snprintf(fileSize, sizeof(fileSize), "%d", lSize);
    strcat(buffer, fileSize);
    strcat(buffer, "\n");
    int l = strlen(buffer);
    if (buffer == NULL) {fputs ("Memory error",stderr); exit (2);}
    char *fileContent = &(buffer[l]);
    
    // copy the file into the buffer:
    result = fread (fileContent,1,lSize,pFile);
    if (result != lSize) {fputs ("Reading error",stderr); exit (3);}
#if DEBUG == 1
    fprintf(stderr, "readEntireFile\n%s\n", buffer);
#endif
    /* the whole file is now loaded in the memory buffer. */
    l = lSize + l;
    //fprintf(stderr, "l = %d\n", l);
    if (send(sockfd, buffer, l, 0) == -1) {
        ERR_EXIT("send");
    } else {
        fclose(pFile);
        free (buffer);
    }
    
    return NULL;
}

void recvEntireFile(char *params) {
#if DEBUG == 1
    fprintf(stderr, "in recvEntireFile\n");
#endif
    int lSize;
    char filename[32];
    char *content;
    char buf[PKT_BUFSIZE];
    memset(buf, 0, sizeof(buf));
    sscanf(params, "%[^\n]", buf);
    sscanf(buf, "%s%d", filename, &lSize);
#if DEBUG == 1
    fprintf(stderr, "%s %d\n", filename, lSize);
#endif
    int l = strlen(buf);
    content = &(params[l+1]);

    FILE *pFile = fopen( filename, "wb" );
    size_t result;
    result = fwrite ( content , 1, lSize, pFile);
#if DEBUG == 1
    fprintf(stderr, "write to file = %d\n", (int)result);
#endif
    fclose(pFile);
    return;
}

/*
int fileReceive(char *filename, int sockfd){
    int fileSize, numbytes;
    char buf[PKT_BUFSIZE];
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
        if(numbytes > 0){
            numbytes = fwrite(buf, sizeof(char), numbytes, fp);
            totalBytesRecv += numbytes;
            #if DEBUG == 1
            printf("At loop %d, fwrite %d bytes\n", loopCount, numbytes);
            #endif
        }     
        loopCount++;
    }
    printf("After %d loops, receive %d bytes from sockfd %d\n.", 
        loopCount, totalBytesRecv, sockfd);
    fclose(fp);
    
    return 1;
}
*/

void userChooseTarget(char *userListFile, char *receiver, int sockfd){
#if DEBUG == 1
    fprintf(stderr, "in userChooseTarget\n");
#endif
    //print all users in user.dat now
    fprintf(stdout, "----------------User List----------------\n");
    
    char buf[30];
    int nbytes_send, nbytes_recv;
    char userData[PKT_BUFSIZE];
    memset(userData, 0, PKT_BUFSIZE);

    strcpy(buf, "U");
    if ((nbytes_send = send(sockfd, buf, strlen(buf)+1 , 0)) == -1) {
        ERR_EXIT("send")
    }
    if((nbytes_recv = recv(sockfd, userData, sizeof(userData), 0))<0) {
        fprintf(stderr, "recv error\n");
    }
    memset(buf, 0, 30);
    int i=0;
    userCnt = 0;
    char *user = userData;
    while(sscanf(user, "%[^\n]", buf) >= 0){
        user = &(user[strlen(buf)+1]);
        sscanf(buf, "%d%s%*s", &userList[i].id, userList[i].username);
        printf("  %d:\t%s\n", userList[i].id, userList[i].username);
        memset(buf, 0, 30);
        i++;
    }
    userCnt = i;
    printf("--------------------------------------------------\n");
    fprintf(stdout, "Choose someone to chat by number:");
    int target;
    fscanf(stdin,"%d", &target);
    strcpy(receiver, userList[target].username);
    return;
}

void userReadOrSend(char *username, int sockfd){    //return when log out
#if DEBUG == 1
    fprintf(stderr, "in userReadOrSend\n");
#endif
    char c;
    while (1) {
        getchar();
        fprintf(stdout, "What do you want to do?\n> Check new message(C)\n> Send new message(S)\n> Logout(Q)\n");
        fscanf(stdin, "%c", &c);
        if ( c == 'C' || c == 'c') {
            fprintf(stdout,"Your unread messages from:\n");
            //findfile(username);

            continue;//還沒做
        } else if ( c == 'S' || c == 's'){
            char message[PKT_BUFSIZE];
            char receiver[32];
            int nbytes_send;
            memset(receiver, 0, sizeof(receiver));
            memset(message, 0, sizeof(message));
            userChooseTarget("user.dat", receiver, sockfd);      // Choose user user data need to be send by server
            printf("You choose to chat with:%s\n",receiver);
            message[0] = toupper(c);
            strcat(message, " ");
            strcat(message, receiver);
            strcat(message, "\0");
            if ((nbytes_send = send(sockfd, message, strlen(message)+1 , 0)) == -1) {
                ERR_EXIT("send")
            }
#if DEBUG == 1
                fprintf(stderr, "Send Message %d bytes to server!%s\n", nbytes_send, message);
#endif
            char history[PKT_BUFSIZE];
            memset(&history, 0, sizeof(history));
            int nbytes_recv;
            if((nbytes_recv = recv(sockfd, history, sizeof(history), 0))<0) {
                fprintf(stderr, "recv error\n");
            }
            else {
#if DEBUG == 1
                    fprintf(stderr, "Recv Message %d bytes from server!\n", nbytes_recv);
                    fprintf(stderr, "Recv Message %s\n", history);
#endif
                if(nbytes_recv == 0 || strcmp(history, "no such file")==0) {
                    fprintf(stdout, "New friend!! Say hi to him or her!!\n");
                }
                int todo = chooseToDo(username, receiver, sockfd, history);
                if (todo == 0){//close this window and choose another target
                    continue;
                }
            }
        } else if ( c == 'Q' || c == 'q'){
            fprintf(stdout, "Logout!\n");
            char message[PKT_BUFSIZE];
            int nbytes_send;
            strcpy(message, "Q");
            if ((nbytes_send = send(sockfd, message, strlen(message)+1 , 0)) == -1) {
                ERR_EXIT("send")
            }
            return;
        } else {
            if ( c != '\n' ) {
                fprintf(stdout, "you type the wrong format!please try again QQ\n");
            }
            continue;
        }
    }
}


int userLogin (char *username, char *password, int sockfd) {
#if DEBUG == 1
    fprintf(stderr, "in userLogin\n");
#endif
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
                fprintf(stdout, "You type the wrong format!Please try again QQ\n");
            }
            continue;
        }
        // Send user profile to server
        int nbytes_send;
        char message[PKT_BUFSIZE];
        message[0] = toupper(c);
        message[1] = '\0';
        strcat(message, " ");
        strcat(message, username);
        strcat(message, " ");
        strcat(message, password);
        
#if DEBUG == 1
        fprintf(stderr, "%s\n", message);
#endif

        if ((nbytes_send = send(sockfd, message, strlen(message)+1 , 0)) == -1) {
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
            char receiveMessage[PKT_BUFSIZE] = {};
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
        }
    }
    return 0;
}
