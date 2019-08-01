#include "ProgettoLSO.h"

#define BUFFER_LEN 1000
#define MESSAGE_LEN 512

int sock;

void gestisciSegnale();

int main(int argc, char *argv[])
{
	//Gestione dei Segnali
	signal(SIGINT, gestisciSegnale);	//CTRL-C
	signal(SIGHUP, gestisciSegnale);
	signal(SIGTSTP,gestisciSegnale);	//CTRL-Z
	signal(SIGPIPE,gestisciSegnale);	//SIGPIPE intercettato in caso di server offline.
	signal(SIGPWR, gestisciSegnale);
    
	system("clear");

	if(argc!=3)						    //E' possibile passare due argomenti: Ip e porta. 
		write(2,"\t\tErrore - Numero Parametri errato!\n",sizeof("\t\tErrore - Numero Parametri errato!\n")), exit(1);   

    	const char *ip =argv[1];
    	const int porta=atoi(argv[2]);
    
    	char buffer[BUFFER_LEN];
	char messaggio[MESSAGE_LEN];
    
    	if((sock=socket(AF_INET, SOCK_STREAM, 0))<0)
		perror("Client Socket"), exit(1);
	
   	struct hostent *server = gethostbyname(ip);    
    	
	struct sockaddr_in serv_addr;
    	
	memset((char *) &serv_addr,0,sizeof(serv_addr));
    
	serv_addr.sin_family=AF_INET;
    
	memcpy((char *) server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);
    	
	serv_addr.sin_port=htons(porta);
    	if(connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr))<0) //Connessione al server.
        	perror("Connect"), exit(1);
	while(1){
		memset(buffer,0, BUFFER_LEN);
        	read(sock, &buffer, BUFFER_LEN);
        	printf("%s", buffer);
		memset(messaggio,0,MESSAGE_LEN);
		fgets(messaggio, MESSAGE_LEN, stdin);
        	write(sock, messaggio, strlen(messaggio));       	
		system("clear");        	
		if(strcmp(messaggio,"QUIT\n")==0){
			sleep(1);
			write(1,"\t\t* Disconnessione effettuata! *\n",sizeof("\t\t* Disconnessione effettuata! *\n"));
			break;
		}
   	}
   	close(sock);
   	exit(0);
}

void gestisciSegnale(int numSegnale)
{
       write(sock,"X",sizeof("X"));	 //Carattere particolare di controllo da inviare al server.
       if(numSegnale==SIGINT || numSegnale==SIGTSTP)
           write(1,"\nUscita da segnale! Alla Prossima!\n",sizeof("\nUscita da segnale! Alla Prossima!\n"));
       else if(numSegnale==SIGPIPE)
		write(1,"Il Server e' andato offline!\n",sizeof("Il Server e' andato offline!\n"));	
       exit(1);
}

