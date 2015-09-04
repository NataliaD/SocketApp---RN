/*
 * client.c
 *
 *  Created on: 27.12.2014
 *      Author: Natalia Duske
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <ctype.h>
#include <wctype.h>
#include <wchar.h>
#include <errno.h>


#define	REQUETE			1
#define	REPONSE			2
#define	TEST_TEXTE		3
#define RESULTAT_TEST	4
#define ERREUR			5

#define TAILLE_TEXTE		684
#define TAILLE_CHILD        171
#define TAILLE_ID_CLIENT	44
#define TAILLE_RESULTAT		8	// Result can be "SUCCES !" or "ECHEC !!"

#define NUMERO_PORT_SERVEUR	42047	// Port number of server
#define SERVER_NAME "vega.info.univ-tours.fr" // "193.52.210.70" // Address of server

// Struct type for the inter-process message queue
typedef struct {
	long mType; // always needed for a message queue! Must not be included in a msgsnd / msgrcv size parameter!
	int  childId;
	char line[TAILLE_CHILD];
} message_t;

// Lock a semaphore with given ID
void sem1PO(int id_sem){
	struct sembuf op;
	op.sem_num=0;
	op.sem_op=-1;
	op.sem_flg=SEM_UNDO;
	semop(id_sem,&op,1);
}

// Release a semaphore with given ID
void sem1VO(int id_sem){
	struct sembuf op;
	op.sem_num=0;
	op.sem_op=1;
	op.sem_flg=SEM_UNDO;
	semop(id_sem,&op,1);
}

int *globalCounter;

int main()
{

	// Initialze a string for the result (filled with zero) and a message struct for the message queue
	char* resultString = calloc(TAILLE_TEXTE + 1, sizeof(char));
	message_t msg;
	msg.mType = 1; // The message queue message struct alyways has to have a long as first member with a value > 0!

	// Message types as declared in the specification of the task
	struct msg_requete {
		char vide[TAILLE_TEXTE];
	};

	struct msg_reponse {
		char lettre;
		int position;
		char vide[TAILLE_TEXTE];
	};

	struct msg_test_texte {
		char texte[TAILLE_TEXTE];
	};

	struct msg_resultat {
		char resultat[TAILLE_RESULTAT];
		char vide[TAILLE_TEXTE - TAILLE_RESULTAT];
	};

	struct msg_erreur {
		char texte[TAILLE_TEXTE];
	};

	struct msg_generic {
		char id_client[TAILLE_ID_CLIENT];
		int type_msg;
		union corps_msg {
			struct msg_requete requete;
			struct msg_reponse reponse;
			struct msg_test_texte test_texte;
			struct msg_resultat resultat_test;
			struct msg_erreur erreur;
		} corps;
	};

	// one structure for the sent and one for the received message
	struct msg_generic msg_send, msg_recv;

	// prepare the request message for the child processes
	bzero(msg_send.id_client, TAILLE_ID_CLIENT);
	strncpy(msg_send.id_client, "NataliaDuske", 12);
	msg_send.type_msg = REQUETE; // Set the message type of the request (REQUETE)

	// Prepare a message queue for exchanging the result string elements
	int idMsgQueue=msgget(IPC_PRIVATE,0660|IPC_CREAT);
	printf("Message Queue %i initialized\n", idMsgQueue);

	// Initialize a shared memory segment for the global counter
	int idShMem;
	struct shmid_ds nb_at;
	key_t sync_key=ftok("/sync_key", 2);
	nb_at.shm_nattch=10;
	idShMem=shmget(sync_key,sizeof(int),IPC_CREAT|0777);
	globalCounter = shmat(idShMem,NULL,0);
	shmctl(idShMem,IPC_STAT,&nb_at);

	// initialize a semaphore for shared memory access
	int idSem;
	idSem=semget(IPC_PRIVATE,1,0777);
	semctl(idSem,0,SETVAL,1);

	// Initialize shared memory value for the global counter with 0
	sem1PO(idSem);
	*globalCounter = 0;
	sem1VO(idSem);

	// Forking of processes:
	// fork child processes only if none of the child process IDs is 0.
	// process IDs are only 0 if the executing process itself is already a child process.
	// By only forking further if no forked process ID is 0, child processes don't create
	// "grand children". This way, there will be exactly one father process with four children.

	int p1 = 0;
	int p2 = 0;
	int p3 = 0;
	int p4 = 0;

	p1 = fork();
	if (p1 != 0) {
		p2 = fork();
	}
	if (p1 != 0 && p2 != 0) {
		p3 = fork();
	}
	if (p1 != 0 && p2 != 0 && p3 != 0) {
		p4 = fork();
	}

	int process = 0; // 0 = father process

	// 1 to 4 are the child processes, identified by how many child process have already
	// been created before a fork() statement.
	if (p1 == 0 && p2 == 0 && p3 == 0 && p4 == 0) {
		process = 1; // first child: no process IDs of children known, so the current process
		             // has to be the first one created by fork().
	} else if (p2 == 0 && p3 == 0 && p4 == 0) {
		process = 2; // second child: the first child's process ID is already set, the others are not.
		             // this process must have been created by the seconds fork().
	} else if (p3 == 0 && p4 == 0) {
		process = 3;
	} else if (p4 == 0) {
		process = 4;
	}

	// Make a connection to the server
	int client_socket;				// client socket ID
	struct sockaddr_in socketadr_server;	// Socket address structure
	socklen_t socket_server_size = sizeof(socketadr_server);
	struct hostent *server_adress;	// Structure for name resolution of an URL

	if ( (server_adress = gethostbyname(SERVER_NAME)) == NULL){
		printf("Unable to connect to server %s!\n", SERVER_NAME);
		return 1;
	}

	// copy the resolved server address data to the socket address struct
	bzero( (char *)&socketadr_server, sizeof(socketadr_server) );
	bcopy( server_adress->h_addr, (char *)&socketadr_server.sin_addr, server_adress->h_length);
	socketadr_server.sin_family = server_adress->h_addrtype;
	socketadr_server.sin_port = htons(NUMERO_PORT_SERVEUR); // Use the UNSIGNED value!
	client_socket = socket( AF_INET, SOCK_DGRAM, 0);

	// Set timeout for the socket
	struct timeval tv;
	tv.tv_sec  = 5;  /* 30 Secs Timeout */
	tv.tv_usec = 0;  // Not init'ing this can cause strange errors
	setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

	// Child processes:
	if(process != 0){ // only in child processes:

		// Each child has to read its own segment of the result string:
		int minPos = TAILLE_CHILD * (process -1);
		int maxPos = minPos + TAILLE_CHILD - 1;

		// Counters for the number of requests and the number of actually relevant reads
		// (inside the substring for the current child process and not already known characters)
		int counter = 0;
		int triesCounter = 0;

		// Loop until every needed character is sent from the server at least once.
		while (counter < TAILLE_CHILD){

			int sentRet = sendto(client_socket, &msg_send, sizeof(msg_send), 0, (struct sockaddr *)&socketadr_server, socket_server_size);
			int recvRet = recvfrom(client_socket, &msg_recv, sizeof(msg_recv), 0, (struct sockaddr *)&socketadr_server, &socket_server_size);

			if (sentRet < 0 || recvRet < 0) {
				printf("Error on sending or receiving from child %i: \nSend returncode = %i, Receive returncode = %i,\n",
						process, sentRet, recvRet);
			} else if(msg_recv.type_msg == REPONSE) {

				triesCounter++;

				if(msg_recv.corps.reponse.position >= minPos
						&& msg_recv.corps.reponse.position <= maxPos
						&& resultString[msg_recv.corps.reponse.position] != msg_recv.corps.reponse.lettre ) {

					// Copy the received character to its destination in the result string
					resultString[msg_recv.corps.reponse.position]=msg_recv.corps.reponse.lettre;

					// Increase the global counter while holding the semaphore for its shared memory
					sem1PO(idSem);
					(*globalCounter)++; //increment the global counter
					sem1VO(idSem);

					counter++;          // increment the child processes' internal counter

				} // if [new character]

				// Give some feedback about the process, but not too much spam =)
				if (triesCounter%50 == 0) {
					printf("Child %d received %d characters from %d requests. Global counter is now %d.\n",
							process,
							counter,
							triesCounter,
							(*globalCounter));
				}

			}

		} // end of "while (counter < TAILLE_CHILD)"

		// Report end of this child process and put its substring to the
		// message queue for the father process to merge it with the others.
		printf("Child process %d has finished after %i requests!\n", process, triesCounter);
		msg.childId = process;
		msg.mType = process;
		//sem1PO(idSem);
		memcpy(msg.line, resultString + (process-1) * TAILLE_CHILD, TAILLE_CHILD);
		int sendResult = msgsnd(idMsgQueue, &msg, TAILLE_CHILD * sizeof(char) + sizeof(int), 0);
		//sem1VO(idSem);
		printf("Child process %d sent message queue (result %i)!\n", process, sendResult);

	}

	else { // if process != 0

		// This is the father process: It does nothing more than wait for its
		// children to finish their work and afterwards combine the four substrings
		// to the solution, which is then sent to the server.

		// Wait for results...
		int received[4] = {0, 0, 0, 0};

		while(!received[0] || !received[1] || !received[2] || !received[3]) {
			printf("Father waiting for message queue...\n");
			int receiveResult = msgrcv(idMsgQueue,&msg,TAILLE_CHILD * sizeof(char) + sizeof(int),0,MSG_NOERROR);
			if (msg.childId < 1 || msg.childId > 4) {
				printf("Answer from invalid child process %i is ignored.\n", msg.childId);
			} else {
				printf("Father process received result part #%d:\n%s\n",
						msg.childId, msg.line);
				memcpy(resultString + (msg.childId - 1) * TAILLE_CHILD, msg.line, TAILLE_CHILD);
				received[msg.childId - 1] = 1;
			}
		}

		// Now the result is ready.
		/*
		int i = 0;
		for (i = 0; i < TAILLE_TEXTE; i++) {
			if (resultString[i] == 0) {
				resultString[i] = ' ';
			}
		}
		*/
		printf("Result is ready:\n%s\n", resultString);

		msg_send.type_msg = TEST_TEXTE;
		memcpy(msg_send.corps.test_texte.texte, resultString, TAILLE_TEXTE);
		int sentRet = sendto(client_socket, &msg_send, sizeof(msg_send), 0, (struct sockaddr *)&socketadr_server, socket_server_size);
		int recvRet = recvfrom(client_socket, &msg_recv, sizeof(msg_recv), 0, (struct sockaddr *)&socketadr_server, &socket_server_size);
		if (msg_recv.type_msg == RESULTAT_TEST) {
			printf("Server returned test result \"%s\"!\n", msg_recv.corps.resultat_test.resultat);
		} else {
			printf("Server did not return test result...\n");
		}

	}

	return 0;
} // main()


