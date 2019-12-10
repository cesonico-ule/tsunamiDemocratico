#include <stdio.h>
#include <stdlib.h>

#define MAXUSR 15
#define MAXPART 4


// - -- --- METODOS --- -- -
//Manejadoras
void manejadoraInvitacion();
void manejadoraQr();
void manejadoraTerminar();

//Auxiliares


// - -- --- VARIABLES GLOBALES --- -- -

//Listas de pids
pid_t *listaUsuarios;
pid_t *actividadesSociales;

//Semaforo
pthread_mutex_t semaforo; 

//Archivo de logs: logFile
FILE *logFile;

//Contadores
int contadorSolicitudes;
int contadorUsuarios;
int contadorEventos;

//Struct con los valores necesarios para el usuario
struct solicitudUsuario{
	int id;
	int atendido;
	int tipo;
};

struct solicitudUsuario *colaSolicitud;
struct solicitudUsuario *colaEventos;




int main (){
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
	
return 0;
}

void manejadoraInvitacion(){

}

void manejadoraQr(){

}

void manejadoraTerminar(){

}



