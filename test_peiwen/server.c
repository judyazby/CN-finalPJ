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
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#define ERR_EXIT(a) { perror(a); exit(1); }
#define DEBUG 1

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char username[32];
    char password[32];
    int fd;
    /* save the person chatting with now */
    char receiver[32];
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
void fileTransfer(char *filename, int sockfd);
int messaging(char *params, int sockfd);
void findfile(char* pattern);
void setReceiver(char *params, int sockfd);
void setUsernameByFd(char *userName, int sockfd);

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
                            case 'R':{
                                ret = registration(params, i);
                                /*
                                if(ret){                                    
                                    fileTransfer("user.dat",i);
                                }
                                */
                            }
                            break;
                            case 'L':{
                                ret = userLogin(params, i);
                                /*
                                if(ret){                                    
                                    fileTransfer("user.dat",i);
                                }
                                */
                            }
                            break;
                            case 'S':{
                            	setReceiver(params, i);                            	
                            	//如果一直在messaging loop, 就必須想怎麼塞入select
                            	//否則中間有人登入, server就無法處理
                            }
                            break;
                            case 'C':{//check new message
                            	/*
                            	fprintf(stdout,"Your unread messages from:\n");
                            	char pattern[32];
                            	setUsernameByFd(pattern, i);
                            	findfile(pattern);
                            	*/
                            }
                            break;
                            case 'M':{
                            	ret = messaging(params, i);/* params hold sender&receiver name*/
                            }
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

void fileTransfer(char *filename, int sockfd){
    struct stat filestat;
    int numbytes;
    char buf[1024];

    int flags = fcntl(sockfd, F_GETFL, 0);
    

    FILE *fp;
    if ( lstat(filename, &filestat) < 0){
        exit(1);
    }
    fp = fopen(filename, "rb");//跟開啟檔案時不一樣，用b可能會有問題？
    int fileSize = (int)filestat.st_size;
    #if DEBUG == 1
    //printf("The file size is %llu\n", (unsigned long long)filestat.st_size);
    printf("The file size is %d\n", fileSize);
    #endif
    if(fp==NULL)
        ERR_EXIT("open file:")
    if (send(sockfd, &fileSize, sizeof(fileSize), 0) == -1) {
        ERR_EXIT("Null file");
    }

    fcntl(sockfd, F_SETFL, flags|O_NONBLOCK);//non-blocking

    int loopCount=1;
    //read data from file and send it
    while(1){
        printf("%d loop,",loopCount);
        numbytes = fread(buf, sizeof(char), sizeof(buf), fp);
        printf("fread %d bytes, ", numbytes);
        //if read successfully, send data
        if (numbytes>0){
            numbytes = send(sockfd, buf, numbytes,0);
            //numbytes = write(sockfd, buf, numbytes);
            printf("Sending %d bytes\n",numbytes);
        }
        if (numbytes<1024){
            if(feof(fp)){
                fclose(fp);
                printf("End of file\n");
                printf("File transfer to sockfd %d completed.", sockfd);
                fcntl(sockfd, F_SETFL, flags);//turn back to blocking mode
            }
            if(ferror(fp)){
                printf("Error reading\n");
            }
            break;
        }
        loopCount++;
    }
    printf("%d loop,",loopCount);
}

void setReceiver(char *params, int sockfd){
	char receiver[32];
	sscanf(params, "%s", receiver);

    //set reciver info of sender in userlist
    for (int i = 0; i < userCnt; i ++) {
        if (userList[i].fd == sockfd) {
        	strcpy(userList[i].receiver,receiver);
        	break;
        }
    }
}

void setUsernameByFd(char *userName, int sockfd){	
	for (int i = 0; i < userCnt; i ++) {
        if (userList[i].fd == sockfd) {
        	strcpy(userName, userList[i].username);
        	break;
        }
    }
}

int messaging(char *params, int sockfd){
	int recvSockfd=-1;
	char receiver[32];
	char sender[32];
    char message[512];

    sscanf(params, "%s %[^\n]*c", receiver, message);
	//sscanf(params, "%s %[^\n]*c", receiver, message);

    //find sender by sockfd
    for (int i = 0; i < userCnt; i ++) {
        if (userList[i].fd == sockfd) {
        	strcpy(sender, userList[i].username);
        	break;
        }
    }    


	//create a C(n,2) dat chat records
	char fileName[100] = "chat_";
	//fileName: alphabet order
	int cmp=strcmp(sender,receiver);
	//allow self messaging
	if (cmp>=0){//sender字母比較後面
		strcat(fileName, receiver);
		strcat(fileName, "_");
		strcat(fileName, sender);	        	
	} else {
		strcat(fileName, sender);
		strcat(fileName, "_");
		strcat(fileName, receiver);	        		
	}
	strcat(fileName, ".dat");
	FILE *fp= fopen(fileName, "a");
	char msgBuf[512];
	//append message

	//find receiver sockfd
	for (int i = 0; i < userCnt; i ++) {
        if (strcmp(receiver, userList[i].username) == 0) {
        	recvSockfd=userList[i].fd;
        	break;
        }
    }

    # if DEBUG == 1
	fprintf(stdout,"targetSockfd=%d\n",recvSockfd);
	# endif
	
	//check if the receiver is online
	if(recvSockfd>0){
		//還沒做:client在messaging同時要能recv

		msgBuf[0] = 'R';//stands for read
	} else {
		msgBuf[0] = 'U';//stands for unread
	}
	
	strcat(msgBuf, " ");
	strcat(msgBuf, sender);
	strcat(msgBuf, " to ");
	strcat(msgBuf, receiver);
	strcat(msgBuf, ": ");
	strcat(msgBuf, message);
	strcat(msgBuf, "\n\0");
	int results = fputs(msgBuf, fp);
	if (results == EOF) {
	    fprintf(stdout,"Fail to write msg.\n");
	    // Failed to write do error code here.
	}
	fclose(fp);//感覺要BLOCK一下?
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
    return 1;//待改。目前不管有沒有登入成功，都會傳送檔案
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
//http://titania.ctie.monash.edu.au/handling-files/find-file.c
void findfile(char* pattern)
{
    DIR* dir;			/* pointer to the scanned directory. */
    struct dirent* entry;	/* pointer to one directory entry.   */
    char cwd[500];	/* current working directory.        */
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
	    	printf("%s/%s\n", cwd, entry->d_name);
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