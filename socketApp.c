//Autoren: Natalia Duske
//Melanie Remmels

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <time.h>

#define SRV_PORT 7777

void client(int argc, char** argv);
void server(char** argv);
int receiveFile(char* fileName, int socket, int display);
int sendFile(char* fileName, int socket);
int createSocket(const char* dest, const char* type);



/**
 * Entry point, here should be chosen how to start an application
 * as server or client
 */
int main(int argc, char** argv) {
    
    setbuf(stdout, NULL); // No Buffer for output, every printf is displayed directly
    
    //Making a choice to start an application with:
    // -S --> as Server
    // -C --> as Client
    if (argc != 3 || !(strcmp(argv[1], "-C") == 0 || strcmp(argv[1], "-S") == 0)) {
        printf("First argument has to be -C for client or -S for Server! \"%s\"\n", argv[1]);
        exit(1);
    }
    if (strcmp(argv[1], "-C") == 0) {
        printf("Starting as client\n\n");
        client(argc, argv);
    } else {
        printf("Starting as server\n\n");
        server(argv);
    }
    
    return 0;
}

/**
 * Create a socket for Target IP oder name dest
 */
int createSocket(const char* dest, const char* type){
    
    int resSocket = -1;
    char portStr[10]; // Buffer to hold the port Number as a string
    // Address information variables
    struct addrinfo *result, *rp, *sockAddr = NULL;
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints); // empty struct memory
    hints.ai_socktype = SOCK_STREAM; //TCP socket
    hints.ai_family = AF_UNSPEC; //can use IPv4 or IPv6
    char ipstr[INET6_ADDRSTRLEN];

    printf("Resolving Server %s\n", dest);
    // Copy the #define'd integer port number into a string
    sprintf(portStr, "%d", SRV_PORT);
    
    //fill the addrinfo structure for the socket connection
    int status = getaddrinfo(dest, portStr, &hints, &result); 
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }//if(status)
    
    // iterate over the addrinfo structs produced by getaddrinfo, 
    // to find a valid IPv4 address

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        struct sockaddr* sa = (struct sockaddr*) rp->ai_addr;
        // get the pointer to the address itself
       	
	 if (rp->ai_family == AF_INET) { // IPv4
            sockAddr = rp;
            //convert IP address from binary to string
            inet_ntop( rp->ai_family, (void*)&((struct sockaddr_in*)sa)->sin_addr, ipstr, sizeof(ipstr) );
	        break;
        } else if (rp->ai_family == AF_INET6) {
            sockAddr = rp;
            //convert IP address from binary to string
            inet_ntop( rp->ai_family, (void*)&((struct sockaddr_in6*)sa)->sin6_addr, ipstr, sizeof(ipstr) );
		        break;
        }
        printf("Address %s found\n", ipstr);

    // Use any identified IPv4 address
    }//for()

    if (sockAddr == NULL) {
        // If no address was found, sockAddr stays NULL => exit.
        printf("No valid address found!\n");
        exit(1);
    } else {
        //if address was found, use it and print it
        printf("Using address %s\n", ipstr);
    }
	    //create socket
    if ((resSocket = socket(sockAddr->ai_family,
                         sockAddr->ai_socktype,
                         sockAddr->ai_protocol)) < 0) {
        perror("TCP Socket");
        exit(1);
    }

    printf("Socket created.\n");
    printf("Inhalt von type: %s\n", type);

    if (strcmp(type, "CLIENT") == 0) {
        //try to connect with server
	printf("Client Socket\n");
        if (connect(resSocket, sockAddr->ai_addr, sockAddr->ai_addrlen) < 0) {
            printf("\n");
            perror("Connect");
            exit(1);
        }
    } else if (strcmp(type, "SERVER") == 0) {    
	printf("Server Socket!\n");
        // Try to setup the server socket
        if (bind(resSocket,  sockAddr->ai_addr, sockAddr->ai_addrlen) < 0) {
            perror("bind");
            exit(1);
        }
        if (listen(resSocket, 5) < 0) {
            perror("listen");
            close(resSocket);
            exit(1);
        }
    }

    // Free ressources of address lookup 
    freeaddrinfo(result);
    
    return resSocket;
}

/**
 * Create the client
 */
void client(int argc, char** argv) {

    if (argc < 3) {
        printf("Client mode requires an additional parameter! Usage:\n");
        printf("%s -C [servername or address]\n", argv[0]);
        exit(1);
    }

    int s_tcp; // socket descriptor
    int doLoop = 1; // switch for terminating the command loop
    // define and initialize a buffer for all string operations
    char* buffer; 
    int bufferSize = 65535;
    buffer = malloc(bufferSize * sizeof (char));
   
    char cmd[256], cmdArg[256]; // Command used (get, put, quit) and argument for command
    char* serverAddress = argv[2]; // argv is taken from main method
    
    // create a socket for the connection to server serverAdress
    s_tcp = createSocket(serverAddress, "CLIENT");

    // PART TWO: Main client loop////
    /////////////////////////////////
    printf(" OK!\nPlease choose one of the following commands:\n");
    printf("get [filename]   read file from server\n", argv[3]);
    printf("put [filename]   write local file to server\n", argv[3]);
    printf("quit             close client application\n\n", argv[3]);
    
    while (doLoop) {
        //parse input from terminal
        //there are 3 possibilities:
        //GET, PUT and QUIT
        memset(buffer, 0, bufferSize);//clear buffer
        memset(cmd, 0, sizeof(cmd));  //clear command buffer
        memset(cmdArg, 0, sizeof(cmdArg)); //clear command arguments buffer
        //read the whole input until linefeed
        fgets (buffer, bufferSize, stdin);
        //split fgets() result into command and argument
        sscanf(buffer, "%s %s", cmd, cmdArg);
        
        //if input is QUIT, then just EXIT
        if (strcmp(cmd, "quit") == 0) {
            doLoop = 0;
        //if input is GET
        } else if (strcmp(cmd, "get") == 0) {
            //buffer should be cleared again
            memset(buffer, 0, bufferSize);
            strcpy(buffer, "@SEND ");
            strcat(buffer, cmdArg);
            send(s_tcp, buffer, strlen(buffer), 0);
            
            memset(buffer, 0, bufferSize);
            // read the header info and print received caracters
            recv(s_tcp, buffer, bufferSize, 0);
            printf("%s", buffer);
            // Send a simple "OK"
            send(s_tcp, "OK", 4, 0);
            // write and display the file
            receiveFile(cmdArg, s_tcp, 0);

        } else if (strcmp(cmd, "put") == 0) {
            // Send an info, that a file is being sent
            memset(buffer, 0, bufferSize);
            strcpy(buffer, "@RECEIVE ");
            strcat(buffer, cmdArg);
            //send server information about the file to receive
            send(s_tcp, buffer, strlen(buffer), 0);
            // Wait for an answer from server
            memset(buffer, 0, bufferSize);
            recv(s_tcp, buffer, bufferSize, 0);
            // Send file lines
            sendFile(cmdArg, s_tcp);
            // Wait for answer
            memset(buffer, 0, bufferSize);
            recv(s_tcp, buffer, bufferSize, 0);
            printf("Completion Answer from Server: %s\n", buffer);
            
        } else {
            //if there were unknown input:
            printf("Command %s unknown\n", cmd);
        }
        printf("\n");
        
    }//while (doLoop)
    
    printf("Closing connection.\n");
    close(s_tcp);
}

/**
 * Create server socket
 */
void server(char** argv) {
    
    int s_tcp, s_client; // socket descriptors
    struct sockaddr_in6 sa_client; // socket address structures
    int sa_len = sizeof (struct sockaddr_in), n;
	
    char* serverAddress = argv[2]; // argv is taken from main method
    char* buffer; //buffer for all string operations
    int bufferSize = 1024;
    buffer = malloc(bufferSize * sizeof (char));

    char fileName[256], hostName[64];
    char ipstr[INET6_ADDRSTRLEN];
    struct sockaddr_storage addrInfo;
    socklen_t addrInfoLen = sizeof(addrInfo);
   
    // create a socket for the server
    s_tcp = createSocket(serverAddress, "SERVER");
        
    ///////////Main server loop///////////
    //////////////////////////////////////
    //main loop can be interrupted only by using "CTRL + C"
    while (1) {
        printf("> Waiting for TCP connections ... \n");
        if ((s_client = accept(s_tcp, (struct sockaddr*) &sa_client, &sa_len)) < 0) {
            perror("accept");
            close(s_tcp);
            exit(1);
        }
        printf("accepted\n");
        n = 1;
	 getsockname(s_client, (struct sockaddr *)&addrInfo, &addrInfoLen);
	//getpeername(sa_client, (struct sockaddr*)&addrInfo, &addrInfoLen);
        
        //as long as client sends data, server receives and evaluates it
        //if client sends 0 Bytes, the connection will be closed and the
        //server will return to the main loop and wait for new connections
        while (n > 0) {
            memset(buffer, 0, bufferSize); // Prepare receive buffer with zeros
            n = recv(s_client, buffer, bufferSize, 0);
            if (n > 0) {
                // interpretation of the received command, only two options: 
                // "@RECEIVE [filename]" means, the client will send a file
                //                       named [filename]
                // "@SEND [filename]"    means, the client requests the contents
                //                       of file [filename].
                if (strncmp(buffer, "@RECEIVE", 8) == 0) {
                    strcpy(fileName, buffer+9);
                    // Tell client that the command is accepted
                    send(s_client, "OK", 3, 0);
                    // create the file
                    receiveFile(fileName, s_client, 1);
                    //convert client address from binary to string
                    //inet_ntop(sa_client.sin6_family, &(sa_client.sin6_addr), ipstr, sizeof(ipstr));
		    struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addrInfo; 
      		    inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof(ipstr));
		    
                    memset(buffer, 0, bufferSize);
                    //write into buffer the result after receiving and send to client
                    //after that server is waiting for the next order
                    sprintf(buffer, "OK <%s>:<%d>", ipstr, ntohs(sa_client.sin6_port));
                    send(s_client, buffer, bufferSize, 0);
                } else if (strncmp(buffer, "@SEND", 5) == 0) {
                    strcpy(fileName, buffer+6);
                    // Return message
                    // <Date + Time>
                    // <Serverhostname>
                    // <used Server-IP-Address>
                    // contents of <filename>
                    gethostname(hostName, sizeof(hostName)); //get pc name
                    //convert client address from binary to string
                    //inet_ntop(sa_client.sin6_family, &(sa_client.sin6_addr), ipstr, sizeof(ipstr));
		    struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addrInfo; 
      		    inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof(ipstr));
                    time_t tmpTime;
                    time(&tmpTime);
                    struct tm* curLocalTime = localtime(&tmpTime);
                    memset(buffer, 0, bufferSize);
                    sprintf(buffer, "<%02d.%02d.%d %02d:%02d:%02d>\n<%s>\n<%s>\n", 
                                  curLocalTime->tm_mday, 
                                  curLocalTime->tm_mon + 1, 
                                  curLocalTime->tm_year + 1900,
                                  curLocalTime->tm_hour,
                                  curLocalTime->tm_min,
                                  curLocalTime->tm_sec,
                                  hostName,
                                  ipstr);
                    send(s_client, buffer, bufferSize, 0);//send Header

                    recv(s_client, buffer, bufferSize, 0);
                    // send the file
                    sendFile(fileName, s_client);
                    
                }//else if (@SEND)
            }//if(n>0)
        }//while(n>0)
    }//while(1)
    free(buffer);
    close(s_tcp);
}//server ()

/**
 * receive file from socket and store it locally
 */
int receiveFile(char* fileName, int socket, int display) {
    
    char* buffer; //buffer for received data chars
    int bufferSize = 65535;
    buffer = malloc(bufferSize * sizeof (char));
    int eof = 0;
    FILE* inFile = NULL;

    // Ask the other side, if the requested file was found
    memset(buffer, 0, bufferSize);
    recv(socket, buffer, bufferSize, 0);
    if (strncmp(buffer, "@OK", 3) != 0) {
        eof = 1; // don't start writing
	if(display == 0){
            printf("%s\n", buffer);
	}
    } else {
        eof = 0;
        // create the file or overwrite if already exists
        inFile = fopen(fileName, "w+");
    }
    // Dummy answer
    send(socket, "@OK", 4, 0);

    // receive file lines
    while(eof == 0) {
        memset(buffer, 0, bufferSize);
        //fill the buffer with data from socket
        recv(socket, buffer, bufferSize, 0);
        //write the data from buffer into outfile
        fputs (buffer, inFile);
	if(display == 0){
            printf("%s", buffer);
	}
        // check if file is complete
        //at the end of the filedata there is an appended string "@EOF" after "\0"
        if (strcmp((buffer + strlen(buffer) + 1), "@EOF") == 0) {
            //printf("File finished\n");
            eof=1;
        }
    }//while(eof == 0)
    
    free(buffer);
    if (inFile != NULL) {
        fclose(inFile);
    }
    return 0;

}

/**
 * sends requested file
 * if such file is absent, an error message is returned instead
 */
int sendFile(char* fileName, int socket) {
    
    char* buffer; // Another local character buffer
    int bufferSize = 65535;
    buffer = malloc(bufferSize * sizeof (char));
    int returnValue = 0;
    FILE* outFile = fopen(fileName, "r");
    //if the file isn't readable or doesn't exist
    //send error-message and return error code
    if (outFile == NULL) {
        memset(buffer, 0, bufferSize);
        //write to socket buffer
        sprintf(buffer, "File \"%s\" not found.\n", fileName);
        printf("%s", buffer);
        // Tell the other side, if the requested file was found
        send(socket, "File not Found!", 16, 0);
        recv(socket, buffer, bufferSize, 0);
        returnValue = 1;
    } else {
        // Tell the other side, if the requested file was found
        send(socket, "@OK", 4, 0);
        recv(socket, buffer, bufferSize, 0);
        // Send file lines
        while (!feof(outFile)) {
            memset(buffer, 0, bufferSize);
            //read from file line by line and send it
            fgets (buffer, bufferSize, outFile);
            send(socket, buffer, strlen(buffer), 0);
        }
        memset(buffer, 0, bufferSize);
        //to mark the end of the file append extra "@EOF" after "\0"
        strcpy(buffer+1 , "@EOF");
        send(socket, buffer, 5, 0);
    } 
    
    free(buffer);
    return returnValue;
}
