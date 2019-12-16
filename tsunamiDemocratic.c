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
int contarMismoTipo(int idFacturador); //busca si hay solicitud del mismo tipo que el atendedor que esta esperando
int buscaMinId(int idAten); /* Busca el usuario de minimo ID */
void calculaEspera(int posUsuario); /* Calcula el tiempo de espera en la solicitud de  un usuario por el atendedor y devuelve si esta esta se acepta o no */

// - -- --- VARIABLES GLOBALES --- -- -


//Semaforo
pthread_mutex_t semaforo; 
pthread_mutex_t semLog;  //Mutex que controla la escritura en el log.
pthread_mutex_t semSolicitudes;   //Mutex que controla la aprobacion o no de las solicitudes.
pthread_mutex_t semActividadSocial;  //Mutex que controla la entrada a la actividad social.
pthread_mutex_t semaforoDeUsuarios; // Mutex que controla los usuarios de la aplicacion

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

//Variables
int trabajar; // Mantiene a los atendedores de solicitudes trabajando hasta que se cierre el aeropuerto 
int listaAte[2]; /* Lista donde los atendedores van a descansar */
int pasa; // Variable que indica si una solicitud pasa a una actividad cultural (0) o no (1);


int main (){

	srand(time(NULL));
	
	//Inicializamos los contadores
	contadorSolicitudes=0;
	contadorEventos=0;
	condicionEmpezarActividad=0;
	trabajar=0;
	listaAte[0] = 0;
	listaAte[1] = 0;
	listaAte[2] = 0;
	pasa =0; 

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
	if (pthread_mutex_init(&semaforoDeUsuarios, NULL)!=0) exit(-1); 

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
	int idFacturador = 1; //Inicializamos el id de facturador con 1 al tratarse de invitacion
	int pos;
	int uMinPos = -1;
	int atencion;
	int uAtendidos = 0; /* Cuenta usuarios atendidos */
	
	while((contarMismoTipo(1) + contarMismoTipo(2)) == 0){
		sleep(1); /* Espera */
	} //busca si hay solicitudes esperando y si nolas hay descansa un segundo
	
	if ((contarMismoTipo(idFacturador) > 0) && (listaAte[idFacturador-1]=0)){ //Busca si hay solicitudes de su mismo tipo y si es asi las trata
		uMinPos= buscaMinId(1);// Busca el usuario que mas lleva esperando
		pasa = calculaEspera(uMinPos);// Se ve si la solicitud sigue adelante y si es asi esperara a ver si se mete en una actividad cultural de lo contario se mata el hilo.
		uAtendidos = uAtendidos + 1; //incrementamos la variable que cuenta los que ha atendido
		
		if (pasa==0){ // si la solicitud avanza se cambia el numero de atendido
			
			pthread_mutex_lock(&semSolicitudes); 
			colaSolicitud[uMinPos].atendido=2;
			pthread_mutex_unlock(&semSolicitudes); 
		
}else {  // si no pasa se elimina la solicitud

			pthread_mutex_lock(&semSolicitudes); 
			colaSolicitud[uMinPos].id=0;
			colaSolicitud[uMinPos].tipo=0;
			colaSolicitud[uMinPos].atendido=0;
			pthread_mutex_unlock(&semSolicitudes); 
}

}else if((contarMismoTipo(idFacturador+1) > 0) && (listaAte[idFacturador-1]=0)){ //Al no haber solicitudes de su tipo busca del otro tipo para tratarlas
				   
		uMinPos= buscaMinId(1);// Busca el usuario que mas lleva esperando		
		pasa = calculaEspera(uMinPos);// Se ve si el programa
		uAtendidos = uAtendidos + 1; //incrementamos la variable que cuenta los que ha atendido

		if (pasa==0){ // si la solicitud avanza se cambia el numero de atendido
			
			pthread_mutex_lock(&semSolicitudes); 
			colaSolicitud[uMinPos].atendido=2;
			pthread_mutex_unlock(&semSolicitudes); 
		
}else { // si no pasa se elimina la solicitud

			pthread_mutex_lock(&semSolicitudes); 
			colaSolicitud[uMinPos].id=0;
			colaSolicitud[uMinPos].tipo=0;
			colaSolicitud[uMinPos].atendido=0;
			pthread_mutex_unlock(&semSolicitudes); 
}	

}

			if(uAtendidos == 5){
			
			printf("ME voy a tomar un descanso\n");
			listaAte[idFacturador-1]=1;
			sleep(10);
			listaAte[idFacturador-1]=0;
			uAtendidos = 0;
			
}

}

void *hiloAtendedorQR(void *arg) {
	int idFacturador = 2; //Inicializamos el id de facturador con 2 al tratarse de QR
	int pos;
	int uMinPos = -1;
	int atencion;
	int uAtendidos = 0; /* Cuenta usuarios atendidos */
	
	while((contarMismoTipo(1) + contarMismoTipo(2)) == 0){
		sleep(1); /* Espera */
	} //busca si hay solicitudes esperando y si nolas hay descansa un segundo
	
	if ((contarMismoTipo(idFacturador) > 0) && (listaAte[idFacturador-1]=0)){ //Busca si hay solicitudes de su mismo tipo y si es asi las trata
		uMinPos= buscaMinId(2);// Busca el usuario que mas lleva esperando
		pasa = calculaEspera(uMinPos);// Se ve si la solicitud sigue adelante y si es asi esperara a ver si se mete en una actividad cultural de lo contario se mata el hilo.
		uAtendidos = uAtendidos + 1; //incrementamos la variable que cuenta los que ha atendido
		
		if (pasa==0){ // si la solicitud avanza se cambia el numero de atendido
			
			pthread_mutex_lock(&semSolicitudes); 
			colaSolicitud[uMinPos].atendido=2;
			pthread_mutex_unlock(&semSolicitudes); 
		
}else {  // si no pasa se elimina la solicitud

			pthread_mutex_lock(&semSolicitudes); 
			colaSolicitud[uMinPos].id=0;
			colaSolicitud[uMinPos].tipo=0;
			colaSolicitud[uMinPos].atendido=0;
			pthread_mutex_unlock(&semSolicitudes); 
}

}else if((contarMismoTipo(idFacturador-1) > 0) && (listaAte[idFacturador-1]=0)){ //Al no haber solicitudes de su tipo busca del otro tipo para tratarlas
				   
		uMinPos= buscaMinId(1);// Busca el usuario que mas lleva esperando		
		pasa = calculaEspera(uMinPos);// Se ve si el programa
		uAtendidos = uAtendidos + 1; //incrementamos la variable que cuenta los que ha atendido

		if (pasa==0){ // si la solicitud avanza se cambia el numero de atendido
			
			pthread_mutex_lock(&semSolicitudes); 
			colaSolicitud[uMinPos].atendido=2;
			pthread_mutex_unlock(&semSolicitudes); 
		
}else { // si no pasa se elimina la solicitud

			pthread_mutex_lock(&semSolicitudes); 
			colaSolicitud[uMinPos].id=0;
			colaSolicitud[uMinPos].tipo=0;
			colaSolicitud[uMinPos].atendido=0;
			pthread_mutex_unlock(&semSolicitudes); 
}	

}

			if(uAtendidos == 5){
			
			printf("ME voy a tomar un descanso\n");
			listaAte[idFacturador-1]=1;
			sleep(10);
			listaAte[idFacturador-1]=0;
			uAtendidos = 0;
			
}

}

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

/*
 * Funcion que cuenta usuarios del mismo tipo que el facturador
*/
int contarMismoTipo(int idFacturador){

	int i;
	int contador = 0;
	for(i = 0; i < MAXUSR;i++){
		pthread_mutex_lock(&semaforoDeUsuarios);
		if(solicitudUsuario[i].tipo == idFacturador && solicitudUsuario[i].atendido == 0){
			contador++;
		}
		pthread_mutex_unlock(&semaforoDeUsuarios);		
	}
return contador;
} /* Cerrada */

/*
* Funcion para buscar el ID minimo
*/
int buscaMinId(int idAten){

	int i;
	int posicion; /* Variable donde se va a guardar la posicion del usuario que se va a atender */
	
	pthread_mutex_lock(&semaforoDeUsuarios);
	int antMin = MAXUSR; /* Se inicializa con un valor de una variable global*/
		for(i = 0; i<MAXUSR;i++){
			if(solicitudUsuario[i].id != 0 && solicitudUsuario[i].atendido == 0 && solicitudUsuario[i].tipo == idAten && solicitudUsuario[i].id <= antMin){
			posicion = i;
			antMin = solicitudUsuario[i].id;
			}
		}
		pthread_mutex_unlock(&semaforoDeUsuarios);

return posicion;
} //cerramos

void calculaEspera(int posUsuario){
	int atencion = calculaAleatorios(1,10); /* Calcula los porcentajes para ver si el usuario lo tiene todo en regla */
	int denegado = 0; // Numero que indica si el usuario se va a unir a una actividad cultural o no ya sea porque no ha pasado el control o porque la ha rechazado
	int tiempoAtencion;
			pthread_mutex_lock(&semaforoDeUsuarios);
	int idUsuario = listaUsuarios[posUsuario].id;
		pthread_mutex_unlock(&semaforoDeUsuarios);
	int sigue=0;
		//Logs
	pthread_mutex_lock(&semLog);

		char idLog[10];
		char usuario[50] = "Usuario ";

		sprintf(idLog, "%d", idUsuario);/* */
		strcat(usuario, idLog); /* Concatenamos lo que dice y el id */

		
		pthread_mutex_unlock(&semLog);
		
		if (atencion<=7){ //El 70% de los usuarios lo tienen todo en regla
		
		tiempoAtencion=calculaAleatorios(1,4);
		sleep(tiempoAtencion);
			printf("Usuario %d : Tiene todo los documentos en regla.\n",idUsuario);
		//Al estar todo en orden se pasa a decidir si quiere o no ir a una actividad cultural
			//Logs
			pthread_mutex_lock(&semaforoLog);
			char tiempo[10];
			char msg[100] = "El usuario ha pasado el control con exito en el tiempo de ";
			sprintf(tiempo, "%d", tiempoAtencion); /* Convierte a char el tiempo de atencion */
			strcat(msg,tiempo);
			escribirLog(usuario,msg);
		pthread_mutex_unlock(&semaforoLog);
		//Fin del log

		}else if((atencion==8) || (atencion==9)){ //El 20% tiene problemas con sus datos personales
			tiempoAtencion=calculaAleatorios(2,6);		
			sleep(tiempoAtencion);
		printf("Usuario %d : Tiene errores en sus datos personales pero pasa el control sin problemas.\n",idUsuario);
// Aunque hay errores en sus datos personales el usuario pasa el control
			
			//Logs
			pthread_mutex_lock(&semaforoLog);
			char tiempo[10];
			char msg[100] = "El usuario ha pasado el control pese a que habia errores en sus datos en el tiempo de ";
			sprintf(tiempo, "%d", tiempoAtencion); /* Convierte a char el tiempo de atencion */
			strcat(msg,tiempo);
			escribirLog(usuario,msg);
		pthread_mutex_unlock(&semaforoLog);
		//Fin del log

} else { //Hay un 10% de solicitudes que no pasan el control porque tienen antecedentes
			tiempoAtencion=calculaAleatorios(6,10);		
			sleep(tiempoAtencion);
		printf("Usuario %d : No pasa el control porque tiene antecendentes penales\n",idUsuario);
		denegado=1;
// Aunque hay errores en sus datos personales el usuario pasa el control
			
			//Logs
			pthread_mutex_lock(&semaforoLog);
			char tiempo[10];
			char msg[100] = "El usuario ha pasado el control pese a que habia errores en sus datos en el tiempo de ";
			sprintf(tiempo, "%d", tiempoAtencion); /* Convierte a char el tiempo de atencion */
			strcat(msg,tiempo);
			escribirLog(usuario,msg);
		pthread_mutex_unlock(&semaforoLog);
		//Fin del log



}
 
		return denegado;

} 




