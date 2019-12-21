#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <limits.h> //Para añadir el valor de MAX_INT (mayor valor que cabe en un entero)

#define MAXUSR 15
#define MAXSOCIALACT 4


// - -- --- METODOS --- -- -
//Hilos
void *hiloUsuario (void *arg);
void *hiloAtendedor(void *arg);
void *hiloCoordinador(void *arg);

//Manejadoras
void manejadoraSolicitud(int sig);
void manejadoraTerminar();

//Auxiliares
void writeLogMessage(char *id, char *msg);
int calculaAleatorios(int min, int max);
int buscarEspacioSolicitud(); //Devuelve la posicion donde haya un hueco en la lista de solicitudes, si no lo encuentre devuelve un -1.
int buscarSolicitudTipo(int tipo);
int numSolicitudTipo(int tipo);
void atendiendo(int pos, int idUser, int atendedorId);


// - -- --- VARIABLES GLOBALES --- -- -


//Semaforo
pthread_mutex_t semaforo; 
pthread_mutex_t semLog;  //Mutex que controla la escritura en el log.
pthread_mutex_t semSolicitudes;   //Mutex que controla la aprobacion o no de las solicitudes.
pthread_mutex_t semActividadSocial;  //Mutex que controla la entrada a la actividad social.

//Condicion
pthread_cond_t condicion;

//Archivo de logs: logFile
FILE *logFile;

//Contadores
int contadorSolicitudes; //Numero unico para el id de la solicitud
int contadorUsuarios; //Numero de usuarios en la aplicacion
int contadorEventos; //Numero de usuarios en un evento
int condicionEmpezarActividad; //0 si no ha empezado la actividad, 1 si esta en curso
int fin; //Variable que controla el finald el programa
int terminarTrabajar; //Variable que controla que cuando se acabe el programa y los atendedores no esten ocupados puedan dejar de trabajar.


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
	fin=0;

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
	pthread_create (&atendInv, NULL, hiloAtendedor, (void*)1); //////////////////CREAR LOS OTROS 2
	pthread_create (&coordinadorSocial, NULL, hiloCoordinador, NULL);

	//Inicializamos la condición
	 if (pthread_cond_init(&condicion, NULL)!=0) exit(-1);

	
	printf("---Mandame senyales PID: %i ---\n",getpid());

	//Pause para que espere por las senyales
	while(1) {
		pause();
	}
}

//Llega una senyal y se crea una nueva solicitud de usuario

void manejadoraSolicitud(int sig){ //Cerramos el mutex
	printf("Ha llegado una solicitud \n");
	int posicion=buscarEspacioSolicitud(); //Buscamos una posicion en la que haya hueco

	if(posicion!=-1) { //Si encontramos un hueco
		
		pthread_mutex_lock(&semSolicitudes);
		contadorSolicitudes++; //Incrementamos el contador de solicitudes
		colaSolicitud[posicion].id=contadorSolicitudes; //Le asignamos un id
		colaSolicitud[posicion].atendido=0; //Ponemos que no esta atendido ya que acaba de llegar
		if(sig==SIGUSR1) { //Si le llega la senyal SIGUSR1 será de tipo 1 
			colaSolicitud[posicion].tipo=1;
		}

		if(sig==SIGUSR2) { //Si le llega la senyal SIGUSR1 será de tipo 2
			colaSolicitud[posicion].tipo=2;
		}
		
		pthread_mutex_unlock(&semSolicitudes); //Abrimos el mutex
		
		printf("Voy a crear el hilo \n");
		pthread_t user; //Declaro el usuario
		//Creo el hilo y le paso de argumentos la posicion de su numero de id.
		pthread_create(&user,NULL,hiloUsuario, (void *)colaSolicitud[posicion].id); 
	
	} else {
		printf("No hay espacio para solicitudes, se ignora la senyal \n");
	}
		
		
}


void *hiloUsuario(void *arg) {

	//Cogemos el id pasado como argumento y lo almacenamos en una variable
	int id=(int *)arg;
	//Buscamos la posicion que tiene el id del hilo en la lista
	int posicion=buscarPosicionEnLista(id);

	pthread_mutex_lock(&semLog);
	writeLogMessage("HiloUsuario","Ha sido creado"); //Hay que escribir su id, etc
	pthread_mutex_unlock(&semLog);
	printf("Se ha creado un nuevo hilo %i \n",id);
	
	sleep(4);  //Duerme 4 segundos
	int numeroAleatorio=0;
	while(colaSolicitud[posicion].atendido==0) {
		printf("Hilo usuario %i esta esperando\n",id);
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
		numeroAleatorio=calculaAleatorios(1,100); //Calculamos un numero aleatorio
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
		printf("Al usuario %i se le ha terminado de atender \n",id);
		int entradaActividad=0; //Variable que controla si ha entrado en la actividad
		int decision=calculaAleatorios(0,10);
		if(decision<5) {  //Intenta participar en una actividad social
			
		printf("El usuario %i decide entrar en una actividad social \n",id);
			while(entradaActividad==0) { ///////////////////////////////////cambiarlo a 1 para probar los atendedores
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
						pthread_cond_signal(&condicion);
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
		
		printf("El usuario %i decide no entrar en una actividad social \n",id);
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
		
	}
	if(colaSolicitud[posicion].atendido==3) {
		printf("El usuario %i ha sido atendido pero tiene antecedentes policiales \n",id);
		pthread_mutex_lock(&semSolicitudes); 
		colaSolicitud[posicion].id=0;
		colaSolicitud[posicion].tipo=0;
		colaSolicitud[posicion].atendido=0;
		pthread_mutex_unlock(&semSolicitudes); 
	}
	//Terminamos el hilo
		printf("El usuario %i sale de la aplicacion \n",id);
		pthread_mutex_lock(&semLog);
		writeLogMessage("HiloUsuario","Sale de la aplicacion");
		pthread_mutex_unlock(&semLog);
		pthread_exit(NULL);
			
}


void *hiloAtendedor(void *arg) {
	int idAtendedor=(int *)arg;
	
	printf("Hilo atendedor %i inicializado \n",idAtendedor);
	int numAtendidos=0;
	int ocupado=0;
	int posUsuario;


	while(terminarTrabajar==0) {
		while(numSolicitudTipo(1)==0 && numSolicitudTipo(2)==0) { //No hay ninguna solicitud, por lo que espera	
			sleep(1);
		}
	

		if(numSolicitudTipo(idAtendedor)>0 || idAtendedor==3) { //Si hay atendedores de su tipo buscamos un usuario para atender
			posUsuario=usuarioMinId(idAtendedor); //Buscamos el usuario del mismo tipo con el menor id
		
		} else { //No hay usuario de su tipo por lo que buscamos de otro
			
			if(idAtendedor==1) { 
				posUsuario=usuarioMinId(2);
			} 
			if(idAtendedor==2) {
				posUsuario=usuarioMinId(1);
			}
		}

		if(colaSolicitud[posUsuario].id!=0 && colaSolicitud[posUsuario].atendido==0) {
			pthread_mutex_lock(&semSolicitudes);
			colaSolicitud[posUsuario].atendido=1; //Cambiamos el flag de atendido
			pthread_mutex_unlock(&semSolicitudes);
			atendiendo(posUsuario, colaSolicitud[posUsuario].id,idAtendedor);
		
			numAtendidos++;
		}
		
			
		
		//Mira si le toca tomar cafe
		if(numAtendidos%5==0 && numAtendidos!=0) {
			pthread_mutex_lock(&semLog); 
			writeLogMessage("Atendedor 1","Se va a su descanso a tomar cafe");
			pthread_mutex_unlock(&semLog);
			sleep(10);
			pthread_mutex_lock(&semLog); 
			writeLogMessage("Atendedor 1","Vuelve de su descanso");
			pthread_mutex_unlock(&semLog);
		}

	}

}


void *hiloCoordinador(void *arg) {
	printf("Hilo coordinador inicializado\n");
	pthread_cond_wait(&condicion, &semActividadSocial);

	//Variable de condicion que espera a que se llene la actividad
	pthread_mutex_lock(&semActividadSocial); //Lock al mutex de la actividad social
	pthread_mutex_lock(&semLog); 
	writeLogMessage("HiloCoordinador","La actividad social va a comenzar");
	pthread_mutex_unlock(&semLog);

	sleep(3); //Actividad de duracion de tres segundos

	pthread_mutex_lock(&semLog); 
	writeLogMessage("HiloCoordinador","La actividad social ha terminado");
	pthread_mutex_unlock(&semLog);
	pthread_mutex_unlock(&semActividadSocial); //Unlock al mutex

	//Cierra el hilo
	pthread_exit(NULL);
}

void manejadoraTerminar(){
	fin=1;
	//esperar a que se termine de atender a usuarios
	terminarTrabajar=1;

	//No pasará mientras el contador indique que se puede producir un evento
	while(contadorEventos == 4 ){ }
	pthread_exit(NULL); 

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

	
	pthread_mutex_lock(&semSolicitudes);
	while(i<MAXUSR && posEncontrada==0) { //Buscamos la posicion que contiene el id argumentado
		if(colaSolicitud[i].id==idSolicitud) {
			posicion=i;
			posEncontrada=1;
		}
		i++;
	}

	pthread_mutex_unlock(&semSolicitudes);
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

//Cuenta el numero de solicitudes que hay del tipo argumentado
int numSolicitudTipo(int tipo) {
	int num=0;
	pthread_mutex_lock(&semSolicitudes); 
	for(int i=0;i<MAXUSR;i++) {
		if(colaSolicitud[i].tipo==tipo && colaSolicitud[i].atendido==0) {
			num++;
		}
	}
	pthread_mutex_unlock(&semSolicitudes); 

	return num;
}

//Busca el usuario con el minimo id de un tipo.
int usuarioMinId(int tipo) {

	int minId= INT_MAX;
	int position;
	
	pthread_mutex_lock(&semSolicitudes); 
	if(tipo>=3) {
		for(int i=0;i<MAXUSR;i++) {
			if(colaSolicitud[i].atendido==0) {
				if(colaSolicitud[i].id<minId) {
					minId=colaSolicitud[i].id;
					position=i;
				}
			}
		}

	} else {
		for(int i=0;i<MAXUSR;i++) {
			if(colaSolicitud[i].tipo==tipo && colaSolicitud[i].atendido==0) {
				if(colaSolicitud[i].id<minId) {
					minId=colaSolicitud[i].id;
					position=i;
				}
			}
		}
	}
	
	pthread_mutex_unlock(&semSolicitudes); 
	return position;
}


int calcularAtencion() {
	int numAleatorio=calculaAleatorios(1,10);
	int atencion=-1;
	
	if(numAleatorio<=7 && numAleatorio>=1) {
		atencion=0;

	} else if(numAleatorio==8 || numAleatorio==9) {
		atencion=1;

	} else if(numAleatorio==10) {
		atencion=2;
	}
return atencion;

}

//Calcula si el usuario tiene todo en regla, hay error con sus datos personales o tiene expediente policial y lo registra en el log

void atendiendo(int pos, int idUser, int atendedorId) {
	int atencion;
	printf("Atendedor %i atiende al usuario %i \n",atendedorId,idUser);
	atencion=calcularAtencion(); //Calculamos la atencion
	printf("Atendedor %i atiende al usuario %i y atencion %i \n",atendedorId,idUser,atencion);
	//Escribimos en el log
	pthread_mutex_lock(&semLog); 
	writeLogMessage("Atendedor 1","Atiende al usuario -insertar numero-");
	pthread_mutex_unlock(&semLog);
	if(atencion==0) {
		sleep(calculaAleatorios(1,4));
		pthread_mutex_lock(&semLog);
		writeLogMessage("Atendedor ","Termina de atender al usuario -insertar num-");
		writeLogMessage("Atendedor ","Atencion correcta del usuario -insertar num-");
		pthread_mutex_unlock(&semLog);
	} else if(atencion==1) {
		sleep(calculaAleatorios(2,6));
		pthread_mutex_lock(&semLog);
		writeLogMessage("Atendedor ","Termina de atender al usuario -insertar num-");
		writeLogMessage("Atendedor ","Algun error en datos personales del usuario -insertar num-");
		pthread_mutex_unlock(&semLog);
	} else if(atencion==2) {
		sleep(calculaAleatorios(6,10));
		pthread_mutex_lock(&semLog);
		writeLogMessage("Atendedor ","Termina de atender al usuario -insertar num-");
		writeLogMessage("Atendedor ","Encontrada ficha policial con antecedentes del usuario -insertar num-");
		pthread_mutex_unlock(&semLog);
	}
	//Cambiamos el flag de atendido
	pthread_mutex_lock(&semSolicitudes);
	if(atencion==0 || atencion==1) {
		colaSolicitud[pos].atendido=2;
	} else if(atencion==2) {
		colaSolicitud[pos].atendido=3; //No podra unirse a una actividad social
	}
	pthread_mutex_unlock(&semSolicitudes);
	printf("Atendedor %i deja de antender al usuario %i \n",atendedorId,idUser);
}

		
		
	



