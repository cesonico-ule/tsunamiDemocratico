#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#define MAXUSR 15
#define MAXSOCIALACT 4


// - -- --- METODOS --- -- -
//Hilos
void *hiloUsuario (void *arg);
void *hiloAtendedorInvitacion (void *arg);
void *hiloAtendedorQR (void *arg);
void *hiloAtendedorPRO (void *arg);
void *hiloCoordinador(void *arg);

//Manejadoras
void manejadoraSolicitud(int sig);
void manejadoraTerminar();

//Auxiliares
void writeLogMessage(char *id, char *msg);
int calculaAleatorios(int min, int max);
int buscarEspacioSolicitud(); //Devuelve la posicion donde haya un hueco en la lista de solicitudes, si no lo encuentre devuelve un -1.


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
int condicionEmpezarActividad; //0 si no ha empezado la actividad, 1 si esta en curso
//Struct con los valores necesarios para el usuario
struct solicitudUsuario{
	int id;
	int atendido;
	int tipo;
};

struct solicitudUsuario *colaSolicitud;
struct solicitudUsuario *colaEventos;




int main (){

	srand(time(NULL));
	
	//Inicializamos los contadores
	contadorSolicitudes=0;
	contadorEventos=0;
	condicionEmpezarActividad=0;

	//Tratamos las senyales
	if(signal(SIGUSR1, manejadoraSolicitud)==SIG_ERR){
		perror("LLamada a manejadora fallida.");
		exit(-1);
	}
	if(signal(SIGUSR2, manejadoraSolicitud)==SIG_ERR){
		perror("LLamada a manejadora fallida.");
		exit(-1);
	}
	if(signal(SIGINT, manejadoraTerminar)==SIG_ERR){
		perror("LLamada a manejadora fallida.");
		exit(-1);
	}


	//Inicializamos las listas
	colaSolicitud = (struct solicitudUsuario*)malloc(MAXUSR*sizeof(struct solicitudUsuario));
	for(int i=0;i<MAXUSR;i++) {
		colaSolicitud[i].id=0;	
		colaSolicitud[i].atendido=0;
		colaSolicitud[i].tipo=0;
	}
	colaEventos = (struct solicitudUsuario*)malloc(MAXSOCIALACT*sizeof(struct solicitudUsuario));
	for(int i=0;i<MAXSOCIALACT;i++) {
		colaEventos[i].id=0;	
		colaEventos[i].atendido=0;
		colaEventos[i].tipo=0;
	}
	//Inicializamos los semaforos
	if (pthread_mutex_init(&semLog, NULL)!=0) exit(-1); 
	if (pthread_mutex_init(&semSolicitudes, NULL)!=0) exit(-1); 
	if (pthread_mutex_init(&semActividadSocial, NULL)!=0) exit(-1); 

	//Creamos el archivo de los log si no existe ya
	pthread_mutex_lock(&semLog);
	logFile= fopen("tsunamiDemocraticLog","w");
	writeLogMessage("Main","Ha empezado la aplicacion");
	pthread_mutex_unlock(&semLog);

 	//Creamos los hilos
	pthread_t atendInv, atendQR, atendPRO, coordinadorSocial;
	pthread_create (&atendInv, NULL, hiloAtendedorInvitacion, NULL);
	pthread_create (&atendQR, NULL, hiloAtendedorQR, NULL);
	pthread_create (&atendPRO, NULL, hiloAtendedorPRO, NULL);
	pthread_create (&coordinadorSocial, NULL, hiloCoordinador, NULL);
	
	printf("---Mandame senyales PID: %i ---\n",getpid());

	//Pause para que espere por las senyales
	while(1) {
		pause();
	}
}

//Llega una senyal y se crea una nueva solicitud de usuario

void manejadoraSolicitud(int sig){
	pthread_mutex_lock(&semSolicitudes); //Cerramos el mutex
	printf("Ha llegado una solicitud \n");
	int posicion=buscarEspacioSolicitud(); //Buscamos una posicion en la que haya hueco

	if(posicion!=-1) { //Si encontramos un hueco
		contadorSolicitudes++; //Incrementamos el contador de solicitudes
		colaSolicitud[posicion].id=contadorSolicitudes; //Le asignamos un id
		colaSolicitud[posicion].atendido=0; //Ponemos que no esta atendido ya que acaba de llegar
		if(sig==SIGUSR1) { //Si le llega la senyal SIGUSR1 será de tipo 1 
			colaSolicitud[posicion].tipo=1;
		}

		if(sig==SIGUSR2) { //Si le llega la senyal SIGUSR1 será de tipo 2
			colaSolicitud[posicion].tipo=2;
		}
		
		printf("Voy a crear el hilo \n");
		pthread_t user; //Declaro el usuario
		//Creo el hilo y le paso de argumentos la posicion de su numero de id.
		pthread_create(&user,NULL,hiloUsuario, (void *)colaSolicitud[posicion].id); 
	
	} else {
		printf("No hay espacio para solicitudes, se ignora la senyal \n");
	}
		
	pthread_mutex_unlock(&semSolicitudes); //Abrimos el mutex
		
}


void *hiloUsuario(void *arg) {

	//Cogemos el id pasado como argumento y lo almacenamos en una variable
	int id=(int)arg;
	//Buscamos la posicion que tiene el id del hilo en la lista
	int posicion=buscarPosicionEnLista(id);

	pthread_mutex_lock(&semLog);
	writeLogMessage("HiloUsuario","Ha sido creado"); //Hay que escribir su id, etc
	pthread_mutex_unlock(&semLog);
	printf("Se ha creado un nuevo hilo %i \n",id);
	
	sleep(4);  //Duerme 4 segundos
	int numeroAleatorio=-1;
	while(colaSolicitud[posicion].atendido==0) {
		//Si la solicitud es de tipo 1 (Invitacion) se calcula si se cansa de esperar
		if(colaSolicitud[posicion].tipo==1) { 
			numeroAleatorio=calculaAleatorios(1,10); //Calculamos un numero aleaorio
			if(numeroAleatorio==1) {
				printf("Se cancela la solicitud del usuario %i por cansarse de esperar\n",id);
				//Cerramos el mutex para excribir en el log
				pthread_mutex_lock(&semLog);
				writeLogMessage("HiloUsuario","Se cansa de esperar y se calcela la solicitud");
				pthread_mutex_unlock(&semLog);

				//Cerramos el mutex para borrar de la lista al usuario
				pthread_mutex_lock(&semSolicitudes);
				colaSolicitud[posicion].id=0;
				colaSolicitud[posicion].tipo=0;
				colaSolicitud[posicion].atendido=0;
				pthread_mutex_unlock(&semSolicitudes); 

				//Terminamos el hilo
				pthread_exit(NULL);
			}
			
		}

		//Si la solicitud es de tipo 2 (QR) se calcula si se va por no ser fiable
		if(colaSolicitud[posicion].tipo==2) {
			numeroAleatorio=calculaAleatorios(1,10); //Calculamos un numero aleaorio
			if(numeroAleatorio<=3) {
				printf("Se cancela la solicitud del usuario %i por no ser fiable\n",id);
				//Cerramos el mutex para excribir en el log
				pthread_mutex_lock(&semLog);
				writeLogMessage("HiloUsuario","No es fiable la solicitud y se cancelan");
				pthread_mutex_unlock(&semLog);

				//Cerramos el mutex para borrar de la lista al usuario
				pthread_mutex_lock(&semSolicitudes);
				colaSolicitud[posicion].id=0;
				colaSolicitud[posicion].tipo=0;
				colaSolicitud[posicion].atendido=0;
				pthread_mutex_unlock(&semSolicitudes); 

				//Terminamos el hilo
				pthread_exit(NULL);
			}
		
		}

		//Se calcula si le afecta que la aplicacion funcione mal a la solicitud
		numeroAleatorio=calculaAleatorios(1,100); //Calculamos un numero aleaorio
		if(numeroAleatorio<=15) {
			printf("Se cancela la solicitud del usuario %i por el mal funcionamiento de la aplicacion\n",id);
			//Cerramos el mutex para excribir en el log
			pthread_mutex_lock(&semLog);
			writeLogMessage("HiloUsuario","La aplicacion no funciona bien y cancela la solicitud");
			pthread_mutex_unlock(&semLog);

			//Cerramos el mutex para borrar de la lista al usuario
			pthread_mutex_lock(&semSolicitudes);
			colaSolicitud[posicion].id=0;
			colaSolicitud[posicion].tipo=0;
			colaSolicitud[posicion].atendido=0;
			pthread_mutex_unlock(&semSolicitudes); 
			//Terminamos el hilo
			pthread_exit(NULL);
		

		}

		sleep(4); //Duerme 4 segundos y vuelve a empezar el bucle.

	}


	while(colaSolicitud[posicion].atendido==1) { //Espera activa de que esta siendo atendido
		//Espera activa hasta que se le termine de atender
	}
	
	if(colaSolicitud[posicion].atendido==2) { //Ha sido atendido
		printf("Al usuario %i se le ha terminado de atender",id);
		int entradaActividad=0; //Variable que controla si ha entrado en la actividad
		int decision=calculaAleatorios(0,10);
		if(decision<5) {  //Intenta participar en una actividad social
			
		printf("El usuario %i decide entrar en una actividad social",id);
			while(entradaActividad=0) {
				pthread_mutex_lock(&semActividadSocial);
				int posicionActividad;
				posicionActividad=buscarEspacioActividadSocial();
				if(contadorEventos<MAXSOCIALACT) { //Puede entrar
					colaEventos[posicion].id=id;
					contadorEventos++;
					//Buscamos si no hay mas hueco en la actividad
					if(contadorEventos==MAXSOCIALACT) { 
						//Notifico para que se puede empezar la actividad
						condicionEmpezarActividad=1; 
					}
					//Desbloqueo el mutex
					pthread_mutex_unlock(&semActividadSocial);
					//Lo quitamos de la lista de solicitudes
					pthread_mutex_lock(&semSolicitudes);
					colaSolicitud[posicion].id=0;
					colaSolicitud[posicion].tipo=0;
					colaSolicitud[posicion].atendido=0;
					pthread_mutex_unlock(&semSolicitudes); 
					pthread_mutex_lock(&semLog);
					writeLogMessage("HiloUsuario","Esperando a que la actividad comience");
					pthread_mutex_unlock(&semLog);
					
					//Variable de condicion que espere para comenzar
					sleep(3);
					
					pthread_mutex_lock(&semLog);
					writeLogMessage("HiloUsuario","Deja la actividad");
					pthread_mutex_unlock(&semLog);


					entradaActividad=1;

				} else { //No hay hueco y se queda esperando

					//Desbloqueamos el mutex
					pthread_mutex_unlock(&semActividadSocial);
					sleep(3);  //Duerme 3 segundos y vuelve a intentar entrar
				}
		
			}

		} else {   //No decide participar
		
		printf("El usuario %i decide no entrar en una actividad social",id);
			//Lo quitamos de la lista de solicitudes
			pthread_mutex_lock(&semSolicitudes); 
			colaSolicitud[posicion].id=0;
			colaSolicitud[posicion].tipo=0;
			colaSolicitud[posicion].atendido=0;
			pthread_mutex_unlock(&semSolicitudes); 
			//Escribimos en el log
			pthread_mutex_lock(&semLog);
			writeLogMessage("HiloUsuario","decide no entrar en la actividad social");
			pthread_mutex_unlock(&semLog);

		}
		//Terminamos el hilo
		pthread_exit(NULL);
	}
			
}


void *hiloAtendedorInvitacion(void *arg) {
	printf("Hilo atendedor invitacion inicializado\n");

}

void *hiloAtendedorQR(void *arg) {
	printf("Hilo atendedor QR inicializado\n");

}

void *hiloAtendedorPRO(void *arg) {
	printf("Hilo atendedor PRO inicializado\n");


}

void *hiloCoordinador(void *arg) {
	printf("Hilo coordinador inicializado\n");

	//Variable de condicion que espera a que se llene la actividad

	pthread_mutex_lock(&semActividadSocial); //Lock al mutex
	writeLogMessage("HiloCoordinador","La actividad social va a comenzar");
	sleep(3); //Actividad de duracion de tres segundos
	writeLogMessage("HiloCoordinador","La actividad social ha terminado");
	pthread_mutex_unlock(&semActividadSocial); //Unlock al mutex

	//Cierra el hilo
	pthread_exit(NULL);

}

void manejadoraTerminar(){

}

void writeLogMessage(char *id, char *msg) { 
	// Calculamos la hora actual 
	time_t now = time(0);
	struct tm *tlocal = localtime(&now); 
	char stnow[19]; strftime(stnow, 19, "%d/%m/%y %H:%M:%S", tlocal); 
	// Escribimos en el log 
	logFile = fopen("tsunamiDemocraticLog", "a"); 
	fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg); 
	fclose(logFile); 
}

int calculaAleatorios(int min, int max) {
	return rand() % (max-min+1) + min;
}

//Busca un espacio disponible para la siguiente solicitud y devuelve la posicion si lo encuentra
//Si no lo encuentra devuelve -1

int buscarEspacioSolicitud() {
	printf("Buscando espacio\n");
	int posicion=-1;
	int posEncontrada=0;

	for(int i=0;i<MAXUSR && posEncontrada==0;i++) { //Buscamos donde hay un hueco libre
		if(colaSolicitud[i].id==0) { //Si lo encuentra guardamos su posicion
			posicion=i;
			posEncontrada=1;
		}
	}
	
	printf("Terminado de buscar espacio\n");
	return posicion; //Si encontramos un hueco devuelve su posicion, si no devuelve -1
}

//Busca la posicion de un usuario por el id argumentado en la lista de solicitudes
int buscarPosicionEnLista(int idSolicitud) {
	int posicion=0;
	int posEncontrada=0;
	int i=0;

	while(i<MAXUSR && posEncontrada==0) { //Buscamos la posicion que contiene el id argumentado
		if(colaSolicitud[i].id==idSolicitud) {
			posicion=i;
			posEncontrada=1;
		}
		i++;
	}
	return posicion; //Devolvemos la posicion en la lista
}

//Busca un espacio disponible para la entrada a la lista de actividades si lo encuentra
//Si no lo encuentra devuelve -1
int buscarEspacioActividadSocial() {
	printf("Buscando espacio Actividad Social\n");
	int posicion=-1;
	int posEncontrada=0;

	for(int i=0;i<MAXSOCIALACT && posEncontrada==0;i++) { //Buscamos donde hay un hueco libre
		if(colaEventos[i].id==0) { //Si lo encuentra guardamos su posicion
			posicion=i;
			posEncontrada=1;
		}
	}
	
	printf("Terminado de buscar espacio Actividad Social\n");
	return posicion; //Si encontramos un hueco devuelve su posicion, si no devuelve -1
}




