#include <stdio.h>
#include <stdlib.h>

#define MAXUSR 15
#define MAXSOCIALACT 4


// - -- --- METODOS --- -- -
//Manejadoras
void manejadoraSolicitud(int sig);
void manejadoraTerminar();

//Auxiliares
void writeLogMessage(char *id, char *msg);
int calculaAleatorios(int min, int max);


// - -- --- VARIABLES GLOBALES --- -- -


//Semaforo
pthread_mutex_t semaforo; 
pthread_mutex_t semLog;  //Mutex que controla la escritura en el log.
pthread_mutex_t semSolicitudes;   //Mutex que controla la aprobacion o no de las solicitudes.
pthread_mutex_t semActividadSocial;  //Mutex que controla la entrada a la actividad social.

//Archivo de logs: logFile
FILE *logFile;

//Contadores
int contadorSolicitudes; //Numero unico para el id de la solicitud
int contadorUsuarios; //Numero de usuarios en la aplicacion
int contadorEventos; //Numero de usuarios en un evento

//Struct con los valores necesarios para el usuario
struct solicitudUsuario{
	int id;
	int atendido;
	int tipo;
} Usuario;

struct solicitudUsuario *colaSolicitud;
struct solicitudUsuario *colaEventos;




int main (){

	srand(time(NULL));

	if(signal(SIGUSR1, manejadoraInvitacion)==SIG_ERR){
		perror("LLamada a manejadora fallida.");
		exit(-1);
	}
	if(signal(SIGUSR2, manejadoraQr)==SIG_ERR){
		perror("LLamada a manejadora fallida.");
		exit(-1);
	}
	if(signal(SIGINT, manejadoraTerminar)==SIG_ERR){
		perror("LLamada a manejadora fallida.");
		exit(-1);
	}

	//Inicializamos las listas
	struct solicitudUsuario *colaSolicitud = (struct solicitudUsuario*)malloc(MAXUSR*sizeof(struct solicitudUsuario));
	for(int i=0;i<MAXUSR;i++) {
		solicitudUsuario[i].id=0;	
		solicitudUsuario[i].atendidot=0;
		solicitudUsuario[i].tipo=0
	}
	struct solicitudUsuario *colaEventos = (struct solicitudUsuario*)malloc(MAXSOCIALACT*sizeof(struct solicitudUsuario));
	for(int i=0;i<MAXSOCIALACT;i++) {
		solicitudUsuario[i].id=0;	
		solicitudUsuario[i].atendidot=0;
		solicitudUsuario[i].tipo=0
	}

	pause(); //Pause para que espere por las senyales
}

void manejadoraSolicitud(int sig){

}


void manejadoraTerminar(){

}

void writeLogMessage(char *id, char *msg) { 
	// Calculamos la hora actual 
	time_t now = time(0);
	struct tm *tlocal = localtime(&now); 
	char stnow[19]; strftime(stnow, 19, "%d/%m/%y %H:%M:%S", tlocal); 
	// Escribimos en el log 
	logFile = fopen(logFileName, "a"); 
	fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg); 
	fclose(logFile); 
}

int calculaAleatorios(int min, int max) {
	return rand() % (max-min+1) + min;
}



