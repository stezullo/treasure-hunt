#include "ProgettoLSO.h"

#define SOCKET_NAME "/tmp/my_socket"
#define MAX_LIM 10  		//Grandezza mappa di gioco (10x10).
#define BUFFER_LEN 256 		//Grandezza delle stringhe.
#define CONN_MAX 10         //Limite connessioni.

#define NUM_OSTACOLI 4  	//Numero di ostacoli nella mappa.

#define ACCESSO 0                         
#define DISCONNESSIONE 1

#define SIGNAL_DEACTIVED 0	//Segnale non attivato.
#define SIGNAL_ACTIVATED 1	//Segnale attivato.

typedef struct clientCollegati{	 //Lista Client che hanno richiesto una connessione al server e sono stati accettati.
	int nClient;                 //Numero che e' stato assegnato al client.
	int closed; 			//Flag: 1 client aperto 0 chiuso.
	struct clientCollegati *next;
}ListaClient;


typedef struct lista{       //Lista Client/Giocatori che hanno fatto il login.
	char nome[30];          //Username.
	int  numeroIndizi; 		//Numero Indizi trovato dal giocatore.
	int  ultimoIndizio;		//Flag: 1 se il giocatore ha trovato l'ultimo indizio 0 altrimenti.
	int  tesoroTrovato;		//Flag: 1 se il giocatore ha trovato il tesoro 0 altrimenti.
	struct lista *next;
}Lista;



typedef struct Indizio{     //Locazione degli indizi.
	int x;                          
	int y;				
}Indizio;


Indizio suggerimento[9];     //Viene utilizzata per i suggerimenti all'utente sulla posizione di un indizio.        
Indizio primoIndizio;

ListaClient *lClient = NULL;
Lista *giocatori = NULL;               


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;	//Inizializzo un mutex.
time_t inizio,attuale; 		//Variabili utilizzate da timer.
int ritrovamentoTesoro=1;	//Flag: 1 se il tesoro non e' stato trovato 0 altrimenti.				
char mappa[MAX_LIM][MAX_LIM];	//Mappa di gioco.			
int syncroVar=0; 					
int tracciaIndizi[9];		//Variabile utilizzata per tenere traccia degli Indizi.

ListaClient * inserisciClient(ListaClient *,int);  //Inserisce in una lista un client che e' stato accettato dal server.
ListaClient * rimuoviClient  (ListaClient *,int);  //Rimuove dalla lista un client che ha come numero client il secondo parametro.
ListaClient * ricercaClient  (ListaClient *,int); //Ricerca e restituisce il client che ha come numero client il secondo parametro.

void gestioneSegnali(int);  	//Signal Handler del server.

void * gestioneThread          (void *);	//Funzione di avvio per la gestione di ogni client.		
void * gestioneThreadClosedConn(void *);	//Funzione di avvio per la gestione della chiusura anomala di un client.

void effettuaLogin(int);				//Permette di registrare, loggare e far uscire un client
int verificaSegnale(char *,char *,int,int,int);		//Funzione che funge da Signal Handler che permette "l'invio" di segnali dal client al server.
void utentiOnline(char *);				//Salva i client che effettuano i login.
void aggiornaLogFile(char *, int);			//Utilizza un file per la memorizzazione di informazioni relative al login e alla disconnessione di client. 
void homeGioco(int, char *);				//Mostra l'interfaccia di un client loggato e permette di giocare, mostrare gli utenti online e uscire.
Indizio creaMappaDiGioco(char [MAX_LIM][MAX_LIM], Indizio *);	//Crea la mappa di gioco.
void gioco(int,char *, char [MAX_LIM][MAX_LIM]);	//Visualizza la mappa di gioco sul client, permette di muoversi all'interno di essa e fa altre operazioni come la visualizzazione dei giocator e l'uscita del giocatore dal gioco.
int numeroUtenti(Lista *);				//Restituisce il numero dei giocatori online.
void posizioneCorrente(char [MAX_LIM][MAX_LIM],int,int);	//Imposta la posizione di un giocatore.
void posizioneIniziale(int,int *,int *,char *);			//Imposta la posizione iniziale di un giocatore.
Lista *cancellaUtente(Lista *, char *);  			//Cancella un giocatore dalla lista degli online.
void esciDallaMappa(int *,int *);				//Permette di far uscire un giocatore dalla mappa di gioco.
void visualizzaUtenti(int,Lista *);				//Visualizza i giocatori connessi e il relativi indizi trovati.
void stampaMappaClient(int,char [MAX_LIM][MAX_LIM],int,int,int,char *);			//Visualizza la mappa di gioco sul client.
int aggiornaMappa(int,char [MAX_LIM][MAX_LIM],int,int,int,int,Indizio *,char *);	//Aggiorna la mappa in seguito ai spostamenti di un giocatore sulla mappa.
void aggiornaIndiziScovati(Lista *,char *);						//Aggiorna il numero di indizi trovati per singolo giocatore.
void aggiornaFileTesoro(char *);			//Gestisce un file in cui vengono scritte info di ritrovamento del tesoro relative al singolo giocatore.
int controllaUsername(char *);				//Restituisce 1 se trova il file del giocatore con il nome uguale al parametro 0 altrimenti.

int main(int argc, char *argv[])
{
	//Gestione dei Segnali
	signal(SIGINT, gestioneSegnali);	//CTRL-C
	signal(SIGHUP,gestioneSegnali);
        signal(SIGTSTP,gestioneSegnali);	//CTRL-Z
        signal(SIGPWR,gestioneSegnali);			
	signal(SIGPIPE,SIG_IGN);	//SIGPIPE ignorato. (In caso di send/write su socket chiusa)
	
	srand((unsigned)time(NULL));   	//Generatore numeri random.

	system("clear");
	
	if(argc!=2)     //E' possibile passare un solo argomento: La porta.            
		write(2,"Errore - Numero Parametri errato!\n",sizeof("Errore - Numero Parametri errato!\n")), exit(1);
   
	const int porta = atoi(argv[1]);
	const char SERVER_FIRST_MESSAGE[] = "\t\t\t\tServer di \"Ricerca al Tesoro\"\n\t\t\t\t* di Gianluca Tulino e Stefano Zullo *\n\n";
	int serverDescriptor,clientDescriptor;	
	pthread_t tidClosedConn,tid;
  	struct sockaddr_in ServerAddress, ClientAddress;
	socklen_t clientLen;
	ServerAddress.sin_family=AF_INET;      //Scelgo ip di versione 4.
	ServerAddress.sin_addr.s_addr=INADDR_ANY;     //Scelgo qualsiasi indirizzo.

	if((serverDescriptor=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))<0)	//Creo un socket di tipo SOCK_STREAM affidabile.
		perror("Socket"), exit(1);
	
	unlink(SOCKET_NAME);                                                    

	ServerAddress.sin_port=htons(porta);     //Converto da Host Byte Order a Network Byte Order l'ordine dei byte dell'argomento.
  	
	
	if(bind(serverDescriptor, (struct sockaddr *)&ServerAddress, sizeof(ServerAddress))<0)  //Binding all'indirizzo scelto.
		perror("Bind"), exit(1);
	

	if(listen(serverDescriptor, CONN_MAX)<0)     //La socket si mette in ascolto in attesa di connessioni dai client. 
		perror("Listen"), exit(1);
		
	primoIndizio=creaMappaDiGioco(mappa,suggerimento);    			//Genero la mappa di gioco.
	time(&inizio);               				               
  	system("clear");
	write(1,SERVER_FIRST_MESSAGE,sizeof(SERVER_FIRST_MESSAGE));	
	while(1){
		  		
		clientLen = sizeof(ClientAddress);					
						
		if((clientDescriptor=accept(serverDescriptor, (struct sockaddr *)&ClientAddress,&clientLen))<0) 	//Accetto la connessione con il client e restituisco un file descriptor.
			perror("accept"), exit(1);								
		else{									
			printf("\nNuova connessione dal client %d!\n",clientDescriptor);				
			int *client = (int *) malloc(sizeof(int)),*clientDue = (int *) malloc(sizeof(int));
			*client = clientDescriptor;
			*clientDue = clientDescriptor;
			lClient=inserisciClient(lClient,clientDescriptor);						
			pthread_create(&tid, NULL, (void *) gestioneThread, (void *) client);				//Creo thread che gestisce tutte le operazioni del client.
			pthread_create(&tidClosedConn, NULL, (void *) gestioneThreadClosedConn, (void *)clientDue);	//Creo thread che gestisce le chiusure anomale del client.
		}			
			
	}
	close(clientDescriptor); 
  	close(serverDescriptor);
  	exit(0);
}

ListaClient * inserisciClient(ListaClient *top,int client){
	if(top==NULL){	
		top=(ListaClient *)malloc(sizeof(ListaClient));
		top->nClient = client;
		top->closed = 1;
		top->next=NULL;
	}
	else{
		ListaClient *tmp=(ListaClient *)malloc(sizeof(ListaClient));
		tmp->nClient = client;
		tmp->closed = 1;		
		tmp->next=top;		
		top=tmp;
	}
	return top;
}

ListaClient * rimuoviClient(ListaClient *top,int clientDaElim){
	if(top!=NULL){
		if(top->nClient==clientDaElim){
			ListaClient *tmp=(ListaClient *)malloc(sizeof(ListaClient));
			tmp=top;
			tmp->closed=0;
			top=top->next;
			free(tmp);
		}
		else
		    top->next=rimuoviClient(top->next,clientDaElim);
	}
    	return top;	
}

ListaClient * ricercaClient(ListaClient *top,int clientDaRicercare){
	if(top!=NULL){
		if(top->nClient==clientDaRicercare)
			return top;
		else
		    top->next=ricercaClient(top->next,clientDaRicercare);	
	}	
	return top;
}


void gestioneSegnali(int numSegnale)
{
	const char USCITA_DA_SEGNALE[] = "\n\nUscita da segnale! Alla Prossima!\n";
	write(1,USCITA_DA_SEGNALE,sizeof(USCITA_DA_SEGNALE));
	exit(1);
}

void * gestioneThread(void * arg)
{
	int sockDescriptor = *(int*)arg;
	effettuaLogin(sockDescriptor);
	pthread_exit(NULL);
}

void *gestioneThreadClosedConn(void * arg){
	int clientDaCercare = *(int*)arg;
	int flag=0,count=0;
	ListaClient *client;	
	char controllaConnessione;
	int byteOttenuti=0;	
	while(1){	
				
		flag=0;					
		if((client=ricercaClient(lClient,clientDaCercare))!=NULL){				
			do{			
				byteOttenuti = recv(client->nClient,(void *)(&controllaConnessione),0,0);					
				if(byteOttenuti<1){ 
					sleep(5);
					count++;													
				}
				else
					count=0;			
			}while(count!=20);				//100 secondi prima che si riconosce che il client e' stato chiuso in maniera anomala.		
			if(client->closed==1){			
				printf("Il client %d e' uscito in maniera anomala.\n",client->nClient);	
				client->closed=0;					
				lClient=rimuoviClient(lClient,client->nClient);		
				flag=1;					
				break;				
			}										
		}
		if(flag==1) break;
	}	
	pthread_exit(NULL);
}

void effettuaLogin(int sockDescriptor)
{
    const int USERNAME_TROVATO=1,LOGIN_OK=1,USER_IN_GAME=2,CRED_NO_VALIDE=0;
    int n=0,m=0,trovato=0,modeLogin=0,fileUsername=0,checkSegnale=0,j=0;
    Lista *listaApp=NULL;
    char buffer[BUFFER_LEN], buffer2[BUFFER_LEN], username[BUFFER_LEN], password[BUFFER_LEN], confrontaPassword[BUFFER_LEN],stringa[BUFFER_LEN];
    do{
	memset(stringa,0,BUFFER_LEN);
      	strcpy(stringa,"\n\t\t\t\tBenvenuto!\n\n\t\tCosa vuoi fare?\n\n\t\t\tREG - Registrazione\n\n\t\t\tLOG - Accedi\n\n\t\t\tQUIT - Uscire\n\n\t\t\t> ");
	write(sockDescriptor,stringa,sizeof(stringa));
      	memset(buffer,0,BUFFER_LEN);
      	n=read(sockDescriptor, buffer, BUFFER_LEN-1);
     	checkSegnale=verificaSegnale(buffer,"",-1,-1,sockDescriptor);
	if(checkSegnale==SIGNAL_ACTIVATED) return;
      	if(strcmp(buffer,"REG\n")==0){					//Registrazione.
	    pthread_mutex_unlock(&mutex);
            pthread_mutex_lock(&mutex);
       	    do{
		write(sockDescriptor, "\t\t- Username : ", sizeof("\t\t- Username : "));
           	memset(username,0,BUFFER_LEN);
           	n=read(sockDescriptor, username, BUFFER_LEN-1);
		checkSegnale=verificaSegnale(username,"",-1,-1,sockDescriptor);
		if(checkSegnale==SIGNAL_ACTIVATED)return;
           	trovato=controllaUsername(username); 
            	if(trovato==USERNAME_TROVATO){				//Se l'username e' stato trovato...	
               		memset(buffer,0,BUFFER_LEN);
              		strcpy(buffer, "\n\t\tUsername gia' in uso.\n\t\tPremi un tasto per provarne un altro!");
              		write(sockDescriptor, buffer, strlen(buffer));
                   	read(sockDescriptor, buffer, BUFFER_LEN);
			checkSegnale=verificaSegnale(buffer,"",-1,-1,sockDescriptor);
			if(checkSegnale == SIGNAL_ACTIVATED) return;
                }
            }while(trovato==USERNAME_TROVATO);				//Ripeti finche' non si utilizza un username nuovo
            write(sockDescriptor, "\t\t- Password : ", sizeof("\t\t- Password : "));
       	    memset(password,0,BUFFER_LEN);
       	    read(sockDescriptor, password, BUFFER_LEN-1);
	    checkSegnale=verificaSegnale(password,"",-1,-1,sockDescriptor);
	    if(checkSegnale==SIGNAL_ACTIVATED) return;
	    creat(username, S_IRWXU);
            fileUsername=open(username,O_WRONLY);
	    write(fileUsername,password,strlen(password));
	    close(fileUsername);
       	    memset(buffer,0,BUFFER_LEN);
            strcpy(buffer,"\t\tRegistrazione effettuata!\n\t\tPremere un tasto per continuare");
       	    write(sockDescriptor, buffer, strlen(buffer));
       	    memset(buffer,0,BUFFER_LEN);
       	    read(sockDescriptor, buffer, BUFFER_LEN);
	    checkSegnale=verificaSegnale(buffer,username,-1,-1,sockDescriptor);
	    if(checkSegnale==SIGNAL_ACTIVATED) return;
       	    close(fileUsername);
       	    pthread_mutex_unlock(&mutex);
     	}
    	else if(strcmp(buffer,"LOG\n")==0){			      //Login 
	    write(sockDescriptor, "\t\t- Username : ", sizeof("\t\t- Username : "));
            memset(username,0,BUFFER_LEN);
            read(sockDescriptor, username, BUFFER_LEN-1);
	    checkSegnale=verificaSegnale(username,username,-1,-1,sockDescriptor);
	    if(checkSegnale==SIGNAL_ACTIVATED) return;
	    for(listaApp=giocatori;listaApp!=NULL;listaApp=listaApp->next){
	    	if(strcmp(listaApp->nome,username)==0)
			break;
	    }
	    if(listaApp) modeLogin=USER_IN_GAME; //Username già in gioco
	    else{
	    	write(sockDescriptor, "\t\t- Password : ",sizeof("\t\t- Password : "));
	        memset(password,0,BUFFER_LEN);
	        read(sockDescriptor, password, BUFFER_LEN-1);
		checkSegnale=verificaSegnale(buffer,"",-1,-1,sockDescriptor);
		if(checkSegnale==SIGNAL_ACTIVATED) return;
	        if(n<0) perror("Lettura su socket fallita !"), exit(1);
		pthread_mutex_unlock(&mutex);	           	
		pthread_mutex_lock(&mutex);
	        fileUsername=open(username, O_RDWR);
		lseek(fileUsername, 0, SEEK_SET);
	        memset(confrontaPassword,0,BUFFER_LEN);
	        if(fileUsername>0){
	        	lseek(fileUsername, 0, SEEK_SET);
	           	while((m=read(fileUsername, buffer2, 1))>0){	// PRENDO LA PASSWORD DELL'USERNAME TROVATO
	                	if(buffer2[0]!='\n'){
	                        	confrontaPassword[j]=buffer2[0];
	                        	j++;
	                    	}
	                    	else{
					confrontaPassword[j]='\n';
	                        	if(strcmp(confrontaPassword,password)!=0) break; //LA PASSWORD NON COINCIDE
	                    		else if(strcmp(confrontaPassword, password)==0){ //LA PASSWORD COINCIDE	
	                       			utentiOnline(username);
	                        		modeLogin=LOGIN_OK;
	                        		break;
	                    		}	                            	
				}
	                }
	        }
	        close(fileUsername);
	        pthread_mutex_unlock(&mutex);
            }
            if(modeLogin==CRED_NO_VALIDE){  	// NOME UTENTE E PASSWORD NON SONO VALIDI
		memset(stringa,0,BUFFER_LEN);
		strcpy(stringa,"\t\tCredenziali invalide!\n\t\tPremere un tasto per continuare");
		n=write(sockDescriptor,stringa,sizeof(stringa));
               	memset(buffer,0,BUFFER_LEN);
               	read(sockDescriptor, buffer, BUFFER_LEN-1);
               	checkSegnale=verificaSegnale(buffer,username,-1,-1,sockDescriptor);
		if(checkSegnale==SIGNAL_ACTIVATED) return;
            }
            else if(modeLogin==USER_IN_GAME){   // UN UTENTE CON NOME IMMESSO E' GIA' IN PARTITA
		memset(stringa,0,BUFFER_LEN);
		strcpy(stringa,"\t\tUtente gia' loggato\n\t\tPremere [INVIO] per continuare");          	
		n=write(sockDescriptor,stringa, sizeof(stringa));
               	memset(buffer,0,BUFFER_LEN);
               	n=read(sockDescriptor, buffer, BUFFER_LEN-1);
               	checkSegnale=verificaSegnale(buffer,username,-1,-1,sockDescriptor);
		if(checkSegnale==SIGNAL_ACTIVATED) return;
	    }
            else{ //modeLogin == LOGIN_OK
		memset(stringa,0,BUFFER_LEN);
		strcpy(stringa,"\t\tLogin effettuato !\n\t\tPremere un tasto per continuare");   
              	n=write(sockDescriptor,stringa, sizeof(stringa));
              	memset(buffer,0,BUFFER_LEN);
              	n=read(sockDescriptor, buffer, BUFFER_LEN-1);
              	checkSegnale=verificaSegnale(buffer,username,-1,-1,sockDescriptor);
		if(checkSegnale==SIGNAL_ACTIVATED) return;
              	aggiornaLogFile(username,ACCESSO);
		printf("Il client %d si e' connesso.\n",sockDescriptor);           	
		homeGioco(sockDescriptor,username);
            }
	}
	else if(strcmp(buffer, "QUIT\n")==0){
      		printf("Il client %d e' uscito.\n",sockDescriptor);		
		lClient=rimuoviClient(lClient,sockDescriptor);	
		pthread_exit(NULL);	
	}
    }while(n>=0);
    return;
}



int verificaSegnale(char *buffer,char *username,int a, int b,int client) {
	if(buffer[0]=='X'){                       //Carattere particolare di controllo per monitorare i segnali dal client.
		char stringa[50],nClient[15];
		memcpy(stringa,"",sizeof(""));
		memcpy(nClient,"",sizeof(""));			
		sprintf(nClient,"%d",client);		
		strcpy(stringa,"Il client ");		
		strcat(stringa,nClient);
		strcat(stringa," e' uscito.\n");		
		write(2,stringa,strlen(stringa));	   					
		if(username != NULL && strcmp(username,"")!=0 && strcmp(username,"\n")!=0 && strcmp(username,"X\n")!=0 && strcmp(username,"X")!=0 && strcmp(username,"\n")!=0){
			giocatori=cancellaUtente(giocatori,username);
			if(a!=-1 && b!=-1)
				esciDallaMappa(&a, &b);
			aggiornaLogFile(username,DISCONNESSIONE);
		}
		lClient=rimuoviClient(lClient,client);
		pthread_exit(NULL);
		return 1;
	}
	return 0;
}

void utentiOnline(char *username)
{
	if(giocatori==NULL){	
		giocatori=(Lista *)malloc(sizeof(Lista));
		strcpy(giocatori->nome,username);
		giocatori->numeroIndizi=0;
		giocatori->next=NULL;
	}
	else{
		Lista *tmp=(Lista *)malloc(sizeof(Lista));
		strcpy(tmp->nome, username);
		giocatori->numeroIndizi=0;
		giocatori->ultimoIndizio=0;
		giocatori->tesoroTrovato=0;
		tmp->next=giocatori;
		giocatori=tmp;
	}
	return;
}

void aggiornaLogFile(char *username,int MODE)
{
	int fd;
	char messaggio[BUFFER_LEN], orario[BUFFER_LEN];
	time_t ora;
	time(&ora);
	memset(messaggio,0,BUFFER_LEN);
	memset(orario,0,BUFFER_LEN);
	if(MODE == ACCESSO)	
		strcpy(messaggio,"Effettua l'accesso : ");
	else  //if MODE == DISCONNESSIONE
		strcpy(messaggio,"Si disconnette : ");    	
	sprintf(orario, "%s", ctime(&ora));   //Memorizzo data e ora dell'accesso o la disconnessione all'interno del file.
	strcat(messaggio,username);
    	strcat(messaggio,orario);
    	strcat(messaggio, "\n");
    	pthread_mutex_lock(&mutex);
    	fd=open("log.txt", O_WRONLY) ;
 	if(fd<0){
         creat("log.txt",S_IRWXU);
	 fd=open("log.txt",O_WRONLY);
      	}
    	lseek(fd,0,SEEK_END);
   	write(fd,messaggio,strlen(messaggio));
    	close(fd);
    	pthread_mutex_unlock(&mutex);
	return;
}

void homeGioco(int sockDescriptor, char *username)
{
	char scelta[BUFFER_LEN], buffer[BUFFER_LEN],app[BUFFER_LEN],client[sizeof(int)],str[BUFFER_LEN];
	int n=0;
	int checkSegnale=0;
  	do{
     		memset(buffer,0,BUFFER_LEN);
		strcpy(buffer,"\n\t\t\tCiao ");
		strcat(buffer,username);
		strcat(buffer,"\n\n\t\tCosa vuoi fare?\n\n\t\t\tGAME - Gioca\n\n\t\t\tUTENTI - Visualizza utenti online\n\n\t\t\tOFF - Disconnettiti\n\n\t\t\t>  ");
     		n=write(sockDescriptor, buffer, strlen(buffer)); 
     		memset(scelta,0,BUFFER_LEN);
     		n=read(sockDescriptor,scelta,BUFFER_LEN-1);
     		checkSegnale=verificaSegnale(scelta,username,-1,-1,sockDescriptor);
		if(checkSegnale==SIGNAL_ACTIVATED) return;
     		if(n<0)
			perror("Errore lettura da socket\n"), exit(1);
     		if(strcmp(scelta, "GAME\n")==0){			//Gioco.
			memcpy(client,"",sizeof(""));
			memcpy(str,"",sizeof(""));
			sprintf(client,"%d",sockDescriptor);
			strcpy(str,"Il client ");
			strcat(str,client);
			strcat(str," si trova in \"Gioco\"\n");
			write(1,str,strlen(str));
       			gioco(sockDescriptor,username,mappa);
      			break;
    		}
      		else if(strcmp(scelta, "UTENTI\n")==0){			//Visualizza utenti connessi.
			memcpy(client,"",sizeof(""));
			memcpy(str,"",sizeof(""));
			sprintf(client,"%d",sockDescriptor);
			strcpy(str,"Il client ");
			strcat(str,client);
			strcat(str," si trova in \"Lista Utenti\"\n");			
			write(1,str,strlen(str));
			visualizzaUtenti(sockDescriptor,giocatori);
			read(sockDescriptor ,app ,BUFFER_LEN-1);
			checkSegnale=verificaSegnale(app,username,-1,-1,sockDescriptor);
			if(checkSegnale==SIGNAL_ACTIVATED) return;
        	}
   		else if(strcmp(scelta, "OFF\n")==0){
			printf("Il client %d si e' disconnesso.\n",sockDescriptor);
			giocatori=cancellaUtente(giocatori,username);
			aggiornaLogFile(username,DISCONNESSIONE);
			effettuaLogin(sockDescriptor); 			//Effettuo chiamata al menu' principale per fare il logout.		
		}
        	else{
         		strcpy(buffer, "\n\t\tOperazione non valida. Premere un tasto per continuare.");
        		write(sockDescriptor, buffer, sizeof(buffer));
         		memset(buffer,0,BUFFER_LEN);
         		read(sockDescriptor, buffer, BUFFER_LEN);
         		checkSegnale=verificaSegnale(buffer,username,-1,-1,sockDescriptor);
			if(checkSegnale==SIGNAL_ACTIVATED) return;
        	}
  	}while(1);    	
	return;
}

Indizio creaMappaDiGioco(char mappa[MAX_LIM][MAX_LIM], Indizio *suggerimento){
	char rappPrimoIndizio='1';
	const int NUM_INDIZI=9;
	int i=0,j=0,indice1=0,indice2=0,numIndiziTotali=NUM_INDIZI,numOstacoliTotali=NUM_OSTACOLI;
	
	for(i=0;i<numIndiziTotali;i++) 
		tracciaIndizi[i]=0;
	
	for(i=0;i<MAX_LIM;i++){					//Riempo la matrice con un valore di default.
		for(j=0;j<MAX_LIM;j++)
			mappa[i][j]='0';
	}

	i=0;
        //Posizionamento indizi
	primoIndizio.x=rand()%MAX_LIM;				//Gli indizi avranno coordinate random.
	primoIndizio.y=rand()%MAX_LIM;
	mappa[primoIndizio.x][primoIndizio.y]=rappPrimoIndizio; //Inserisco il primo indizio nella matrice.			 
	while(atoi(&rappPrimoIndizio)!=numIndiziTotali){	//Inserisco i restanti indizi nella matrice.
		indice1=rand()%MAX_LIM;				
		indice2=rand()%MAX_LIM;
		if(mappa[indice1][indice2]=='0'){		//Se la cella possiede un valore '0' vuol dire che e' libera.
		    	mappa[indice1][indice2]=rappPrimoIndizio+1;
			suggerimento[atoi(&rappPrimoIndizio)-1].x=indice1; //Modifico il vettore dei suggerimenti degli indizi.
			suggerimento[atoi(&rappPrimoIndizio)-1].y=indice2;
			rappPrimoIndizio++;
		}
	}
	i=0;
	do{							//Posiziono il tesoro.
		indice1=rand()%MAX_LIM;					
		indice2=rand()%MAX_LIM;
		if(mappa[indice1][indice2]=='0'){
            		mappa[indice1][indice2]='t';		//Il tesoro sara' mostrato come una 't'.
            		i++;
       		}
	}while(i!=1);
	suggerimento[NUM_INDIZI-1].x=indice1; 			//Il tesoro avra' coordinate uguali all'ultimo indizio.
	suggerimento[NUM_INDIZI-1].y=indice2;
	i=0;
        while(i!=numOstacoliTotali){				//Posiziono gli ostacoli.
		indice1=rand()%MAX_LIM;
		indice2=rand()%MAX_LIM;
		if(mappa[indice1][indice2]=='0'){
			mappa[indice1][indice2]='x';
			i++;
		}
	}
	return primoIndizio;
}


void gioco(int sockDescriptor,char *username, char mappa[MAX_LIM][MAX_LIM]){
	int cor1=0,cor2=0,valueMax=INT_MIN,flag=0,vincitori=0,checkSegnale=0;
	const int TIME_LIMIT=60,OUT_OF_MAT_LIM=5;	
	char scelta[BUFFER_LEN],continua[BUFFER_LEN],buffer[1000],nomeVincitore[20];
    	Lista *g;

    	posizioneIniziale(sockDescriptor, &cor1, &cor2,username);
    	posizioneCorrente(mappa, cor1, cor2);
    	stampaMappaClient(sockDescriptor,mappa,-1,cor1,cor2,username);
    	while(1){ 									//Fino a quando non si trova il tesoro o si vince per maggior numero indizi trovati non si esce.
       		flag=0;
     		memset(scelta,0,BUFFER_LEN);
     		memset(buffer,0,1000);
     		read(sockDescriptor, scelta, BUFFER_LEN-1);
     		checkSegnale=verificaSegnale(scelta,username,cor1,cor2,sockDescriptor);
		if(checkSegnale==SIGNAL_ACTIVATED) return;
											
     		if(ritrovamentoTesoro==0 || difftime(time(&attuale), inizio)>TIME_LIMIT){
		 	if(difftime(time(&attuale),inizio)>TIME_LIMIT&&ritrovamentoTesoro!=0){ 	//Controllo per verificare se e' finito o meno il tempo.
				for(g=giocatori;g!=NULL;g=g->next){
					if(g->numeroIndizi>valueMax){
						valueMax=g->numeroIndizi;
						strcpy(nomeVincitore,g->nome);
					}
				}
				strcat(buffer, "\n\t\tTempo Terminato!!\n");
				for(g=giocatori;g!=NULL;g=g->next)
					if(g->numeroIndizi==valueMax)
						vincitori++;
				if(vincitori>1){
					strcat(buffer,"\t\t\tPareggio, Vincono :\n"); 		//Piu' vincitori.
					for(g=giocatori;g!=NULL;g=g->next){
						if(g->numeroIndizi==valueMax){        
							strcat(buffer,"\t\t\t");
							strcat(buffer, g->nome);
							strcat(buffer,"\n");
						}
					}
				}
				else{ 								//Unico vincitore.
					strcat(buffer,"\t\tVince :\n"); 
					strcat(buffer,"\t\t\t");					
					strcat(buffer,nomeVincitore);
				}
				vincitori=0;
				valueMax=INT_MIN;
			}
			else{
				strcat(buffer,"\n\t\t** Il tesoro e' stato trovato !\n\t\tVince :\n");
				for(g=giocatori;g!=NULL;g=g->next)
					if(g->tesoroTrovato==1){
						strcat(buffer,"\t\t\t");							
						strcat(buffer,g->nome);
					}
			}
			pthread_mutex_lock(&mutex);
			syncroVar++;
			if(syncroVar==1)
	     			primoIndizio=creaMappaDiGioco(mappa,suggerimento);
     			pthread_mutex_unlock(&mutex);
     			strcat(buffer,"\n\t\tAttendi altri giocatori...\n");
     			write(sockDescriptor, buffer, strlen(buffer));
     			while(syncroVar<numeroUtenti(giocatori) && syncroVar!=0){ 
				read(sockDescriptor, continua, BUFFER_LEN-1);
				checkSegnale=verificaSegnale(continua,username,cor1,cor2,sockDescriptor);
				if(checkSegnale==SIGNAL_ACTIVATED) return;
			}
     			posizioneIniziale(sockDescriptor, &cor1, &cor2,username);
     			posizioneCorrente(mappa, cor1, cor2);
			for(g=giocatori;g!=NULL;g=g->next){
				g->numeroIndizi=0;
				g->ultimoIndizio=0;
				g->tesoroTrovato=0;
     			}
			ritrovamentoTesoro=1;
			syncroVar=0;
			flag=-1;
			time(&inizio);
     		}
		else{
	     		if(strcmp(scelta,"A\n")==0||strcmp(scelta,"a\n")==0){ 			//Spostamento a sinistra.
	     			if((cor2-1)>-1){
	     				flag=aggiornaMappa(sockDescriptor,mappa,cor1,cor2-1,cor1,cor2,suggerimento,username);
	     				if(flag==0 && ritrovamentoTesoro==1){
	     					posizioneCorrente(mappa,cor1,cor2-1);
	               				cor2=cor2-1;
	              			}
	               			else if(flag>=10)
	               				cor2=cor2-1;
	        		}
	        		else
	        			flag=OUT_OF_MAT_LIM; 					//Movimento fuori matrice.

	        	}
      			else if(strcmp(scelta,"W\n")==0||strcmp(scelta,"w\n")==0){         	//Spostamento sopra.
				if((cor1-1)>-1){
	      				flag=aggiornaMappa(sockDescriptor,mappa,cor1-1,cor2,cor1,cor2,suggerimento,username);
	      				if(flag==0 && ritrovamentoTesoro==1){
	      					posizioneCorrente(mappa,cor1-1,cor2);
                        			cor1=cor1-1;
	                		}
	               			else if(flag>=10){
	               				cor1=cor1-1;
	               			}
                		}
               			else
                    			flag=OUT_OF_MAT_LIM;
			}
     			else if(strcmp(scelta,"S\n")==0||strcmp(scelta,"s\n")==0){        	//Spostamento in basso.
     				if((cor1+1)<10){
     					flag=aggiornaMappa(sockDescriptor,mappa,cor1+1,cor2,cor1,cor2,suggerimento,username);
     					if(flag==0 && ritrovamentoTesoro==1){
     						posizioneCorrente(mappa,cor1+1,cor2);
               					cor1=cor1+1;
              	 			}
	               			else if(flag>=10)
	               				cor1=cor1+1;
            			}
	            		else
	            			flag=OUT_OF_MAT_LIM;
			}
			else if(strcmp(scelta, "D\n")==0 || strcmp(scelta, "d\n")==0){    	//Spostamento a destra.
				if((cor2+1)<10){
					flag=aggiornaMappa(sockDescriptor,mappa,cor1,cor2+1,cor1,cor2,suggerimento,username);
					if(flag==0 && ritrovamentoTesoro==1){
						posizioneCorrente(mappa,cor1,cor2+1);
               					cor2=cor2+1;
               				}
               				else if(flag>=10)
               					cor2=cor2+1;
                		}
                		else
                			flag=OUT_OF_MAT_LIM;
          		}
			else if(strcmp(scelta, "OFF\n")==0  || strcmp(scelta, "off\n")==0){	//Disconnessione.
      				printf("Il client %d si e' disconnesso.\n",sockDescriptor);
      				esciDallaMappa(&cor1,&cor2);
      				homeGioco(sockDescriptor,username);
      			}
      			else if(strcmp(scelta, "UTENTI\n")==0 || strcmp(scelta, "utenti\n")==0) //Lista Giocatori.
      				visualizzaUtenti(sockDescriptor,giocatori);
      		}
      		stampaMappaClient(sockDescriptor, mappa, flag,cor1,cor2,username);
 	}
	return;
}

int numeroUtenti(Lista *giocatori)
{
	if(giocatori==NULL)
       		return 0;
	else
        	return 1+numeroUtenti(giocatori->next);
}

void posizioneCorrente(char mappa[MAX_LIM][MAX_LIM], int x, int y)
{
	mappa[x][y]='o';
	return;
}

void posizioneIniziale(int sockDescriptor,int *cor1, int *cor2,char *username)
{
	int checkSegnale;
	char buffer[BUFFER_LEN], scelta[BUFFER_LEN];
    	memset(buffer,0,BUFFER_LEN);
     	pthread_mutex_lock(&mutex);   
    	do{
		*cor1=rand()%10;
     		*cor2=rand()%10;
    	}while(mappa[*cor1][*cor2]!='0' && mappa[*cor1][*cor2]!='c');
    	strcpy(buffer, "\t\t\tSei stato posizionato\n\t\t\tPremere [INVIO] per continuare");
    	write(sockDescriptor, buffer, sizeof(buffer));
    	read(sockDescriptor, scelta,BUFFER_LEN-1);
    	checkSegnale=verificaSegnale(scelta,username,*cor1,*cor2,sockDescriptor);
	if(checkSegnale==SIGNAL_ACTIVATED) return;
    	pthread_mutex_unlock(&mutex);
    	memset(buffer,0,BUFFER_LEN);
	return;
}

Lista *cancellaUtente(Lista *giocatori, char *username){
	if(giocatori!=NULL){
		if(strcmp(giocatori->nome,username)==0){
			Lista *elem=(Lista *)malloc(sizeof(Lista));
			elem=giocatori;
			giocatori=giocatori->next;
			free(elem);
		}
		else
		    giocatori->next=cancellaUtente(giocatori->next, username);
	}
    	return giocatori;
}

void esciDallaMappa(int *x, int *y)
{
	if(mappa[*x][*y]=='o')
		mappa[*x][*y]='0';
	else
		*x=-1, *y=-1;
	return;
}

void visualizzaUtenti(int sockDescriptor, Lista *giocatori){
	char buffer[500],num_Indizi[3],nomeUtente[20];
	int i=0;
	memset(buffer,0,500);
	if(giocatori==NULL)
		write(sockDescriptor,"\t\tNon ci sono utenti connessi.\n",sizeof("\t\tNon ci sono utenti connessi.\n"));
	else{
		Lista *g=giocatori;
		strcat(buffer,"\n\t\tUsername | Numero di Indizi\n");
		while(g!=NULL){
			for(i=0;g->nome[i]!='\n';i++)
				nomeUtente[i]=g->nome[i];
			nomeUtente[i]='\0';
			strcat(buffer,"\t\t");
			strcat(buffer,nomeUtente);
			strcat(buffer," | ");
			sprintf(num_Indizi,"%d",g->numeroIndizi);
			strcat(buffer,num_Indizi);
			strcat(buffer,"\n");
			g=g->next;
	        }
		strcat(buffer,"\t\t");
		write(sockDescriptor,buffer,strlen(buffer));
	}
	return;
}

void stampaMappaClient(int sockDescriptor,char mappa[MAX_LIM][MAX_LIM],int flag,int cor1,int cor2,char *username){
	//flag| 0 : Movimento consentito, 1 : Movimento consentito, 1<=flag-9<=8 : Indizio, 18 : Ultimo Indizio, 20 : Tesoro|//	
	int i=0,j=0,ultimoInd=0;
	const int OUT_OF_MAT_LIM=5,MOV_NON_CONSENTITO=1,INIT_GAME=-1,MOV_CONSENTITO=0,TESORO=20,ULTIMO_INDIZIO=18;
	char buffer[10000],n_tes[5],buf[2],app[5];	
	Lista *g;

	memset(buffer,0,10000);
	
	for(g=giocatori;g!=NULL;g=g->next){
		if(strcmp(g->nome,username)==0)
			ultimoInd=g->ultimoIndizio;
	}
	
	if(flag==MOV_CONSENTITO||flag==TESORO){
		strcat(buffer,"\t\t\tMovimento consentito");
		strcat(buffer,"\n\n");
	}
	else if(flag==INIT_GAME){
		strcat(buffer,"\t\t\t> Inizio del gioco <");
		strcat(buffer," \n\n");
		strcat(buffer,"\t\t\tIl primo indizio si trova in posizione : ");
		sprintf(app,"%d",primoIndizio.x);
		strcat(buffer,app);
		strcat(buffer,",");
		sprintf(app,"%d",primoIndizio.y);
		strcat(buffer,app);
		strcat(buffer,"\n\n");
	}
	else if(flag==OUT_OF_MAT_LIM){
		strcat(buffer,"\t\t\t> Movimento oltre matrice non consentito! <");
		strcat(buffer," \n\n");
	}
	else if(flag==MOV_NON_CONSENTITO){
		strcat(buffer,"\t\t\t\t> Movimento non consentito <");
		strcat(buffer," \n\n");
	}
	else{
		if(flag!=TESORO){
			if(flag==ULTIMO_INDIZIO)
				strcat(buffer,"\t\t\t(!) L'ultimo indizio e' stato trovato! Il tesoro si trova in posizione : ");
			else //flag == INDIZIO
				strcat(buffer,"\t\t\t(!) Hai trovato un indizio! Il prossimo si trova in posizione : ");
			sprintf(n_tes,"%d",suggerimento[(flag-10)].x);
			strcat(buffer,n_tes);
			strcat(buffer,",");
			sprintf(n_tes,"%d",suggerimento[(flag-10)].y);
			strcat(buffer,n_tes);
			strcat(buffer,"\n\n");
		}
	}
	strcat(buffer,"\t\t");
	for(i=0;i<MAX_LIM;i++){
		sprintf(app,"%d\t",i);
		strcat(buffer,app);
	}
	strcat(buffer,"\n\t   ");
	for(i=0;i<MAX_LIM-2;i++)
		strcat(buffer,"_________");
	strcat(buffer,"_______");	
	strcat(buffer,"\n\n");
	for(i=0;i<MAX_LIM;i++){
		sprintf(app,"\t%d  |\t",i);
		strcat(buffer,app);
     		for(j=0;j<MAX_LIM;j++){							   
            		if(mappa[i][j]=='$'){
				if(ultimoInd==1)
     		   			strcat(buffer,"$\t");	 //Tesoro.
     		   		else{
     		   			if(cor1==i && cor2==j)
     		   				strcat(buffer, "@\t");  //Utente.   			
					else
						strcat(buffer, ".\t");	 //Casella Esplorata.
				}
            		}
            		else if(mappa[i][j]=='/')	 //Ostacolo.
     		  		strcat(buffer,"/\t");
            		else if(mappa[i][j]=='x'||mappa[i][j]=='t'||mappa[i][j]=='0')  //Caselle da esplorare.
                		strcat(buffer,"*\t");
            		else if(mappa[i][j]>='1'&&mappa[i][j]<='9'){ 	 //Indizi.
            			buf[0]=mappa[i][j];
				buf[1]='\0';
				if(tracciaIndizi[atoi(buf)-1]==0)
					strcat(buffer,"*\t");
				else{
					if(cor1==i && cor2==j) //Cambio il carattere dell'utente da '@' a '#' in caso di indizio trovato.
						strcat(buffer,"#\t");
					else{
						strcat(buffer,buf);
						strcat(buffer,"\t");
					}
				}
			}
            		else if(mappa[i][j]=='c')          
                		strcat(buffer,".\t");
            		else if(mappa[i][j]=='o') //Posizione attuale dell'utente.
               			strcat(buffer,"@\t");
        	}
        	strcat(buffer,"\n");
    }
    strcat(buffer,"\t   ");
    for(i=0;i<MAX_LIM-2;i++)
		strcat(buffer,"_________");
    strcat(buffer,"_______");
    strcat(buffer,"\n\n");
    sprintf(app,"%.0lf",difftime(time(&attuale),inizio));
    strcat(buffer,"\t\tTempo trascorso : ");
    strcat(buffer,app);
    strcat(buffer,"s\n\n");
	
    if(ritrovamentoTesoro!=0)
    	strcat(buffer,"\n\t\t* Movimento con [A/W/S/D] *\n\t\t* Uscire con [OFF] *\n\t\t* Stampare la lista utenti con [UTENTI] *\n\t\t\t> ");
    else{ 
    	strcat(buffer,"\t\tIl tesoro e' stato trovato!!!\n\t\tUna nuova mappa e' stata generata!\n\t\tPremi [INVIO] per continuare");
	ritrovamentoTesoro=0;
    }
    write(sockDescriptor, buffer, strlen(buffer));
    memset(buffer,0,10000);
    return;
}

int aggiornaMappa(int sockDescriptor,char mappa[MAX_LIM][MAX_LIM],int cor1,int cor2,int oldcor1,int oldcor2,Indizio *suggerimento,char *username){
	Lista *g;
	const int MOV_NON_CONSENTITO=1,MOV_CONSENTITO=0,TESORO=20;  
	int ultimoInd=0,flag=0;
	char buf[2];
	for(g=giocatori;g!=NULL;g=g->next)
		if(strcmp(g->nome,username)==0)
			ultimoInd=g->ultimoIndizio;
	if(mappa[cor1][cor2]=='t'||mappa[cor1][cor2]=='$'){ //Se trovo il tesoro.
		mappa[cor1][cor2]='$';			
		if(ultimoInd==1){  										
			ritrovamentoTesoro=0;
			for(g=giocatori;g!=NULL;g=g->next)
				if(strcmp(g->nome,username)==0)
					g->tesoroTrovato=1;
			aggiornaFileTesoro(username);
		}
		if(!(mappa[oldcor1][oldcor2]>='1'&&mappa[oldcor1][oldcor2]<='9'))		       
        		mappa[oldcor1][oldcor2]='c';
        	flag=TESORO;
	}
	else if(mappa[cor1][cor2]>='1'&&mappa[cor1][cor2]<='9'){  	//Se trovo un indizio.
     		if(!(mappa[oldcor1][oldcor2]>='1'&&mappa[oldcor1][oldcor2]<='9') ){
        		if (mappa[oldcor1][oldcor2]!='$')
        			mappa[oldcor1][oldcor2]='c';
    		}
		buf[0]=mappa[cor1][cor2];
		buf[1]='\0';
		flag=9+atoi(buf);
		if(tracciaIndizi[atoi(buf)-1]==0)
			aggiornaIndiziScovati(giocatori,username);
		tracciaIndizi[atoi(buf)-1]=1;
		if(mappa[cor1][cor2]=='9'){	//Trovato ultimo indizio.								
			for(g=giocatori;g!=NULL;g=g->next)
				if(strcmp(g->nome,username)==0)
					g->ultimoIndizio=1;
		}
	}
	else if(mappa[cor1][cor2]=='x')  		//Se trovo un ostacolo.
     		mappa[cor1][cor2]='/', flag=MOV_NON_CONSENTITO;
    	else if(mappa[cor1][cor2]=='/'||mappa[cor1][cor2]=='o'||mappa[cor1][cor2]=='O') //Ostacolo già scovato o altro giocatore.
     		flag=MOV_NON_CONSENTITO;
    	else if(mappa[cor1][cor2]=='0'||mappa[cor1][cor2]=='c'){  //Caselle Inesplorate o esplorate ma non significative.
     		if(!(mappa[oldcor1][oldcor2]>='1'&&mappa[oldcor1][oldcor2]<='9')){
           		if(mappa[oldcor1][oldcor2]!='$')
				mappa[oldcor1][oldcor2]='c';
        	}
     		flag=MOV_CONSENTITO;
    	}
	return flag;
}

void aggiornaIndiziScovati(Lista *giocatori,char *username){
	if(strcmp(giocatori->nome, username)==0)
        	giocatori->numeroIndizi=giocatori->numeroIndizi+1;
	else
        	aggiornaIndiziScovati(giocatori->next,username);
	return;
}

void aggiornaFileTesoro(char *username){
	int fd;
      	char messaggio[BUFFER_LEN], orario[BUFFER_LEN];
      	time_t ora;
      	time(&ora);
      	memset(messaggio,0,BUFFER_LEN);
      	memset(orario,0,BUFFER_LEN);
      	strcpy(messaggio,"Ritrovamento Tesoro : ");
      	sprintf(orario,"%s",ctime(&ora));
      	strcat(messaggio,username);
      	strcat(messaggio,orario);
      	strcat(messaggio,"\n");
      	pthread_mutex_lock(&mutex);
      	fd=open("logTesoro.txt",O_WRONLY);
      	if(fd<0){
        	creat("logTesoro.txt",S_IRWXU);
		fd=open("logTesoro.txt",O_WRONLY);
      	}
      	lseek(fd,0,SEEK_END);
      	write(fd,messaggio,strlen(messaggio));
      	close(fd);
     	pthread_mutex_unlock(&mutex);
	return;
}


int controllaUsername(char *username){
     int fd;
     fd=open(username, O_RDONLY);
     if(fd<0)
         return 0;
     else
     	close(fd);
     return 1;
}
