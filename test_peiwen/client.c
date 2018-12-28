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
#include <dirent.h>

#define ERR_EXIT(a) { perror(a); exit(1); }
// #define THREAD_NUM 4
#define DEBUG 1

char userList[300][32];
struct timeval timeout;
int npacket;
int maxfd;
int time_argv_sec = 1, time_argv_usec = 0;
fd_set master;
fd_set readfds;

int userLogin (char *username, char *password, int sockfd);
int fileReceive(char *filename, int sockfd);
void userReadOrSend(char *username, int sockfd);
void userChooseTarget(char *userListFile, char *buf);
int chooseToDo(char *receiver, int sockfd);
void findfile(char* pattern);
void showUnreadMsg(char* fileName, char* userName);

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
            int login = 0;
            while (login>=0){
                login = userLogin(username, password, sockfd);
                if ( login < 0) {           // login fail
                    fprintf(stdout, "login failed\n");
                    return 1;
                } else if ( login == 2) {   // login success
                    fprintf(stdout, "login success!! Hello, %s\n", username);
                } else {                    // newUser login = 1
                    fprintf(stdout, "Sign up success!! Hello, %s\n", username);
                }
                userReadOrSend(username, sockfd);
            }

            //目前signup還沒連到下面
            //int fr=fileReceive("user2.dat",sockfd);
            
            
            
            // Choose what to do
            /*
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
            */
        }
    }
    return 0;
}



//send message or file to server with sockfd
int chooseToDo(char *receiver, int sockfd){
    int i=0;
    char c;
    //送訊息之前，如果對方也送訊息給自己，如何即時顯示?
    //fprintf(stdout,"What do you want to do?\n> Send message(S)\n> Send file(F)\n> Close this chat(C)\n");   
    while(1) {
        getchar();    
        fprintf(stdout,"What do you want to do?\n> Send message(M)\n> Send file(F)\n> Close this chat(C)\n");
        //scanf("%c", &c);
        fscanf(stdin, "%c", &c);
        int nbytes_send;
        char message[512];
        if ( c == 'M' || c == 'm') {            
        	char typeMessage[300];
            memset(&typeMessage, 0, sizeof(typeMessage));
            int nbytes_send;
            char *line = NULL;
            size_t len = 0;
            ssize_t read; 
            getchar();
            fprintf(stdout, "Type your message:");
            //getchar();

        	//printf("Type your message:");
            fscanf (stdin,"%[^\n]*c", typeMessage);
            //strcpy(typeMessage, line);
            
            /*
            if(getline(&line,&len,stdin)) {
                strcpy(typeMessage, line);//read a single line message
                //break;
            }
        */
            
            //concade ('s',receiver,message)
            message[0] = toupper(c);
            message[1] = '\0';
            strcat(message, " ");
            strcat(message, receiver);
            strcat(message, " ");
            strcat(message, typeMessage);
            //#if DEBUG == 1
                fprintf(stderr, "%s\n", message);
            //#endif
            
            
            if ((nbytes_send = send(sockfd, message, sizeof(message), 0)) == -1) {
                ERR_EXIT("send")
            }
            #if DEBUG == 1
            fprintf(stderr, "Send Message %d bytes to server!\n", nbytes_send);
            #endif
            //try to read online message

            
            i++;
        } else if ( c == 'C' || c == 'c'){
            message[0] = toupper(c);
            message[1] = '\0';
            if ((nbytes_send = send(sockfd, message, sizeof(message), 0)) == -1) {
                ERR_EXIT("send")
            }
            fprintf(stdout, "Close window...\n");
            return 0;
        } else {
            if ( c != '\n' ) {
                fprintf(stdout, "you type the wrong format!please try again QQ\n");
            }
            continue;
        }
    }
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

void userChooseTarget(char *userListFile, char *buf){

    //print all users in user.dat now
    printf("------------User List------------\n");

    char *line = NULL;
    size_t len = 0;
    ssize_t read; 
  
    // Open file 
    FILE *fptr = fopen(userListFile, "r"); 
    if (fptr == NULL) 
    { 
        printf("Cannot open file \n"); 
        exit(1); 
    } 
    // 需要開一個array儲存現有的user list
    int i=0;
    while ((read = getline(&line, &len, fptr)) != -1) {
        char *data = strtok(line," ");
        strcpy(userList[i], data);
        i++;
    }
    for(int j=0;j<i;j++){
    	printf( "%d：\t%s\n",j,userList[j]);
    }//待解問題：還沒下線前，又有新加進來的user怎麼辦？
    fclose(fptr); 
    printf("------------------------------------\n");
    fprintf(stdout,"Choose someone to chat by number:");
    int target;
    fscanf(stdin,"%d", &target);
    strcpy(buf, userList[target]);
}

void userReadOrSend(char *username, int sockfd){//return when log out
    char c;
    while (1) {
        getchar();
        fprintf(stdout, "What do you want to do?\n> Check new message(C)\n> Send new message(S)\n> Logout(L)\n");
        fscanf(stdin, "%c", &c);
        if ( c == 'C' || c == 'c') {
            fprintf(stdout,"Your unread messages from:\n");
            findfile(username);

            continue;//還沒做
        } else if ( c == 'S' || c == 's'){
            char message[512];
            char receiver[32];
            int nbytes_send;

            memset(&receiver, 0, sizeof(receiver));
            userChooseTarget("user.dat",receiver);
            printf("Your choose to chat with:%s\n",receiver);
            message[0] = toupper(c);
            strcat(message, " ");
            strcat(message, receiver);
            strcat(message, "\0");
            if ((nbytes_send = send(sockfd, message, sizeof(message), 0)) == -1) {
                ERR_EXIT("send")
            }
            #if DEBUG == 1
                fprintf(stderr, "Send Message %d bytes to server!\n", nbytes_send);
            #endif
            
            int todo=chooseToDo(receiver, sockfd);
            if (todo == 0){//close this window and choose another target
                continue;
            }
        } else if ( c == 'L' || c == 'l'){
            fprintf(stdout, "Logout!\n");
            return;
        } else {
            if ( c != '\n' ) {
                fprintf(stdout, "you type the wrong format!please try again QQ\n");
            }
            continue;
        }
    }
}

//ref: http://titania.ctie.monash.edu.au/handling-files/find-file.c
void findfile(char* pattern)
{
    DIR* dir;           /* pointer to the scanned directory. */
    struct dirent* entry;   /* pointer to one directory entry.   */
    char cwd[500];  /* current working directory.        */
    struct stat dir_stat;       /* used by stat().                   */

    /* first, save path of current working directory */
    if (!getcwd(cwd, 500)) {
    perror("getcwd:");
    return;
    }


    /* open the directory for reading */
    dir = opendir(".");
    if (!dir) {
    fprintf(stderr, "Cannot read directory '%s': ", cwd);
    perror("");
    return;
    }

    /* scan the directory, traversing each sub-directory, and */
    /* matching the pattern for each file name.               */
    while ((entry = readdir(dir))) {
    /* check if the pattern matchs. */
        if (entry->d_name && strstr(entry->d_name, pattern)) {
            //printf("%s/%s\n", cwd, entry->d_name);
            #if DEBUG == 1   
            printf("%s\n",entry->d_name);
            #endif
            showUnreadMsg(entry->d_name, pattern);
        }
        /* check if the given entry is a directory. */
        if (stat(entry->d_name, &dir_stat) == -1) {
            perror("stat:");
            continue;
        }
        /* skip the "." and ".." entries, to avoid loops. */
        if (strcmp(entry->d_name, ".") == 0)
            continue;
        if (strcmp(entry->d_name, "..") == 0)
            continue;
    }
}

//ref:https://codeforwin.org/2018/02/c-program-replace-specific-line-a-text-file.html
void showUnreadMsg(char* fileName, char* userName){
    /*  Open all required files */
    FILE * fPtr = fopen(fileName, "r");
    FILE * fTemp = fopen("replace.tmp", "w");

    /* fopen() return NULL if unable to open file in given mode. */
    if (fPtr == NULL || fTemp == NULL)
    {
        /* Unable to open file hence exit */
        printf("\nUnable to open file.\n");
        printf("Please check whether file exists and you have read/write privilege.\n");
        exit(EXIT_SUCCESS);
    }
    
    char buffer[32];
    char read;
    char sender[32];
    char receiver[32];//會多冒號
    char message[512];

    /*
     * Read line from source file and write to destination 
     */
    while ((fgets(buffer, 512, fPtr)) != NULL)
    {
        sscanf(buffer, "%c %s %s %[^\n]*c", &read, sender, receiver, message);
        if (read=='U' || read=='u'){
            if (strcmp(sender, userName)!=0) {
                printf("From %s:%s\n",sender,message);
                buffer[0]='R';
            }
        }
        fputs(buffer, fTemp);         
    }


    /* Close all files to release resource */
    fclose(fPtr);
    fclose(fTemp);

    /* Delete original source file */
    remove(fileName);

    /* Rename temporary file as original file */
    rename("replace.tmp", fileName);

    printf("All messages are read!\n");
}