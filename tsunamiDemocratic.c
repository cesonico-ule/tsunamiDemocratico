#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <limits.h> //Para añadir el valor de MAX_INT (mayor valor que cabe en un entero)



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
int buscarEspacioSolicitud();
int buscarPosicionEnLista(int idSolicitud);
int buscarEspacioActividadSocial();
int numSolicitudTipo(int tipo);
int usuarioMinId(int tipo); 
int calcularAtencion();
void atendiendo(int pos, int idUser, int atendedorId);



// - -- --- VARIABLES GLOBALES --- -- -


//Semaforo
pthread_mutex_t semaforo; 
pthread_mutex_t semLog;  //Mutex que controla la escritura en el log.
pthread_mutex_t semSolicitudes;   //Mutex que controla la aprobacion o no de las solicitudes.
pthread_mutex_t semActividadSocial;  //Mutex que controla la entrada a la actividad social.

//Condicion
pthread_cond_t condicion;
pthread_cond_t entrarActividad[4];
pthread_cond_t salirActividad;


//Archivo de logs: logFile
FILE *logFile;

//Contadores
int MAXUSR; //Numero maximo de solicitudes de usuario
int MAXSOCIALACT; //Numero maximo de personas en una actividad social
int MAXATEND; //Numero maximo de atendedores
int contadorSolicitudes; //Numero unico para el id de la solicitud
int contadorUsuarios; //Numero de usuarios en la aplicacion
int contadorEventos; //Numero de usuarios en un evento
int condicionEmpezarActividad; //0 si no ha empezado la actividad, 1 si esta en curso
int fin; //Variable que controla el finald el programa
int contadorFinAtendedores; //Cada vez que un atendedor termine se aumenta en 1



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
	MAXUSR=15; 
	MAXSOCIALACT=4;
	MAXATEND=3;
	contadorSolicitudes=0;
	contadorEventos=0;
	condicionEmpezarActividad=0;
	contadorFinAtendedores=0;
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
	pthread_create (&atendInv, NULL, hiloAtendedor, (void*)1);
	pthread_create (&atendQR, NULL, hiloAtendedor, (void*)2);
	pthread_create (&atendPRO, NULL, hiloAtendedor, (void*)3);
	pthread_create (&coordinadorSocial, NULL, hiloCoordinador, NULL);

	//Inicializamos la condición
	

	if (pthread_cond_init(&condicion, NULL)!=0) exit(-1);
	for(int i=0; i<4; i++) {
	    if (pthread_cond_init(&entrarActividad[i], NULL)!=0) exit(-1); 
	}


	if (pthread_cond_init(&salirActividad, NULL)!=0) exit(-1);

	
	printf("\t \t ---Mandame senyales para crear solicitudes PID: %i ---\n",getpid());

	//Pause para que espere por las senyales
	while(1) {
		pause();
	}
}

//Llega una senyal y se crea una nueva solicitud de usuario

void manejadoraSolicitud(int sig){ //Cerramos el mutex
	if(fin==0) { //Si se termina el programa se ignora la senyal
		printf("Ha llegado una solicitud");
		if(sig==SIGUSR1) {
			printf(" de tipo invitacion\n");
		} else {
			printf(" de tipo QR\n");
		}
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

			pthread_t user; //Declaro el usuario
			//Creo el hilo y le paso de argumentos la posicion de su numero de id.
			pthread_create(&user,NULL,hiloUsuario, (void *)colaSolicitud[posicion].id); 
		
		} else {
			printf("No hay espacio para solicitudes, se ignora la senyal \n");
		}
	}
}


void *hiloUsuario(void *arg) {

	//Cogemos el id pasado como argumento y lo almacenamos en una variable
	int id=(int *)arg;
	//Buscamos la posicion que tiene el id del hilo en la lista
	int posicion=buscarPosicionEnLista(id);
	char identificacion[100]; //Identificacion del usuario para cuando se escriban los mensajes en el log
	
	sprintf(identificacion, "Usuario con ID %i y tipo %i",id,colaSolicitud[posicion].tipo);
	pthread_mutex_lock(&semLog);
	writeLogMessage(identificacion,"Ha sido creado"); //Hay que escribir su id, etc
	pthread_mutex_unlock(&semLog);
	printf("Se ha iniciado una nueva solicitud con ID %i \n",id);
	
	sleep(4);  //Duerme 4 segundos
	int numeroAleatorio=0;
	while(colaSolicitud[posicion].atendido==0) { //Mientras este el usuario y aun el programa no haya sido terminado
		//Si la solicitud es de tipo 1 (Invitacion) se calcula si se cansa de esperar
		if(colaSolicitud[posicion].tipo==1) { 
			numeroAleatorio=calculaAleatorios(1,10); //Calculamos un numero aleaorio
			if(numeroAleatorio==1) {
				printf("Se cancela la solicitud del usuario %i por cansarse de esperar\n",id);
				//Cerramos el mutex para excribir en el log
				pthread_mutex_lock(&semLog);
				writeLogMessage(identificacion,"Se cansa de esperar y se calcela la solicitud");
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
				writeLogMessage(identificacion,"No es fiable la solicitud y se cancelan");
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
			writeLogMessage(identificacion,"La aplicacion no funciona bien y cancela la solicitud");
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
		sleep(1);//Espera activa hasta que se le termine de atender
	}
	
	if(colaSolicitud[posicion].atendido==2) { //Ha sido atendido
		printf("Al usuario %i se le ha terminado de atender \n",id);
		pthread_mutex_lock(&semLog);
			writeLogMessage(identificacion,"Se le ha terminado de atender");
		pthread_mutex_unlock(&semLog);
		int entradaActividad=0; //Variable que controla si ha entrado en la actividad
		int decision=calculaAleatorios(1,10);
		if(decision<=5 && fin==0) {  //Intenta participar en una actividad, mientras no se haya acabado el programa puede entrar
			printf("El usuario %i decide entrar en una actividad social \n",id);
			pthread_mutex_lock(&semLog);
			writeLogMessage(identificacion,"Decide entrar en una actividad social");
			pthread_mutex_unlock(&semLog);
			while(entradaActividad==0) { 
				pthread_mutex_lock(&semActividadSocial);
				int posicionActividad;
				posicionActividad=buscarEspacioActividadSocial();
				if(contadorEventos<MAXSOCIALACT && condicionEmpezarActividad==0) { //Puede entrar
					//Desbloqueo el mutex

					colaEventos[posicionActividad].id=id;
					pthread_mutex_unlock(&semActividadSocial);
					entradaActividad=1;

					contadorEventos++;


					//Lo quitamos de la lista de solicitudes

					pthread_mutex_lock(&semSolicitudes);
					colaSolicitud[posicion].id=0;
					colaSolicitud[posicion].tipo=0;
					colaSolicitud[posicion].atendido=0;
					pthread_mutex_unlock(&semSolicitudes); 

					pthread_mutex_lock(&semLog);
					writeLogMessage(identificacion,"Esperando a que la actividad comience");
					pthread_mutex_unlock(&semLog);

					//Buscamos si no hay mas hueco en la actividad
					if(contadorEventos==4) { 
						//Notifico para que se puede empezar la actividad
						printf("Llamada a que empiece la actividad\n");
						pthread_mutex_lock(&semLog);
						writeLogMessage(identificacion,"Notifica de que se puede empezar la actividad");
						pthread_mutex_unlock(&semLog);
						condicionEmpezarActividad=1; 
						pthread_cond_signal(&condicion);
					}
					//Espero a que pueda empezar la actividad
					pthread_mutex_lock(&semActividadSocial);
					
					int signal=contadorEventos;
	
					pthread_cond_wait(&entrarActividad[contadorEventos-1], &semActividadSocial);

					sleep(3); //Despues de esperar 3 segundo se va de la actividad
					colaEventos[posicionActividad].id=0; //Borramos su id de la posicion de la lista
					pthread_mutex_lock(&semLog);
					writeLogMessage(identificacion,"Deja la actividad");
					pthread_mutex_unlock(&semLog);
					
					contadorEventos=contadorEventos-1;
					
					
					printf("Usuario %i va a salir de la actividad: contador eventos %i \n",id,contadorEventos);
					pthread_cond_signal(&entrarActividad[signal]);
					if(contadorEventos==0) { //El ultimo que quede por salir le manda una senyal al coordinador
											pthread_mutex_lock(&semLog);
					writeLogMessage(identificacion,"Notifica de que es el ultimo usuario en salir de la actividad");
					pthread_mutex_unlock(&semLog);
						pthread_cond_signal(&salirActividad);
						condicionEmpezarActividad==0; //Termina la actividad
					}
					pthread_mutex_unlock(&semActividadSocial);
					printf("Usuario %i deja la actividad social \n",id);

					
				} else { //No hay hueco y se queda esperando
					pthread_mutex_unlock(&semActividadSocial); //Desbloqueamos el mutex
					sleep(3);  //Duerme 3 segundos y vuelve a intentar entrar
				}
		
			}

		} else {   //Decide no participar
		
		printf("El usuario %i no entra en una actividad social \n",id);
		//Lo quitamos de la lista de solicitudes
		pthread_mutex_lock(&semSolicitudes); 
		colaSolicitud[posicion].id=0;
		colaSolicitud[posicion].tipo=0;
		colaSolicitud[posicion].atendido=0;
		pthread_mutex_unlock(&semSolicitudes); 
		//Escribimos en el log
		pthread_mutex_lock(&semLog);
		writeLogMessage(identificacion,"no entra en la actividad social");
		pthread_mutex_unlock(&semLog);

		}
		
	}
	if(colaSolicitud[posicion].atendido==3) { //Usuario que no puede entrar a una actividad social
		printf("El usuario %i ha sido atendido pero tiene antecedentes policiales \n",id);
		pthread_mutex_lock(&semSolicitudes); 
		pthread_mutex_lock(&semLog);
		writeLogMessage(identificacion,"ha sido atendido pero tiene antecedentes policiales");
		pthread_mutex_unlock(&semLog);
		colaSolicitud[posicion].id=0;
		colaSolicitud[posicion].tipo=0;
		colaSolicitud[posicion].atendido=0;
		pthread_mutex_unlock(&semSolicitudes); 
	}
	//Terminamos el hilo
		printf("El usuario %i sale de la aplicacion \n",id);
		pthread_mutex_lock(&semLog);
		writeLogMessage(identificacion,"Sale de la aplicacion");
		pthread_mutex_unlock(&semLog);
		pthread_exit(NULL);
			
}


void *hiloAtendedor(void *arg) {
	int idAtendedor=(int *)arg;	
	int terminarTrabajar=0; //Variable que controla que cuando se acabe el programa y los atendedores no esten ocupados puedan dejar de trabajar.
	int numAtendidos=0;
	int ocupado=0;
	int posUsuario;
	char identificacion[100]; //Identificacion del usuario para cuando se escriban los mensajes en el log
	
	if(idAtendedor==1) {
		sprintf(identificacion, "Atendedor %i de tipo Invitacion| ",idAtendedor);
	} else if(idAtendedor==2) {
		sprintf(identificacion, "Atendedor %i de tipo QR| ",idAtendedor);
	} else {
		sprintf(identificacion, "Atendedor %i de tipo PRO| ",idAtendedor);
	}
	


	while(1) {
		while(numSolicitudTipo(1)==0 && numSolicitudTipo(2)==0) { //No hay ninguna solicitud, por lo que espera
			if(fin==1) {//Si se termina el programa y no hay nadie que atender
				terminarTrabajar=1; 
				break;
			}
			sleep(1); //Salimos del bucle
		}
		
		if(terminarTrabajar==1) { //Si la variable de terminar de trabajar es =1
			break; //Salimos del bucle y termina el hilo
		}
	

		if(numSolicitudTipo(idAtendedor)>0 || idAtendedor>=3) { //Si hay atendedores de su tipo buscamos un usuario para atender
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
			writeLogMessage("Atendedor ","Se va a su descanso a tomar cafe");
			pthread_mutex_unlock(&semLog);
			sleep(10);
			pthread_mutex_lock(&semLog); 
			writeLogMessage("Atendedor ","Vuelve de su descanso");
			pthread_mutex_unlock(&semLog);
		}

	}
	contadorFinAtendedores++;
	pthread_mutex_lock(&semLog);
	writeLogMessage(identificacion,"ha terminado su trabajo");
	pthread_mutex_unlock(&semLog);
	printf("\t \t //-Terminado atendedor %i-//\n",idAtendedor);
	pthread_exit(NULL);

}


void *hiloCoordinador(void *arg) {
	while(fin==0) {
		pthread_mutex_lock(&semActividadSocial); //Cerramos el mutex
		//Esperamos a que haya 4 usuarios en la actividad social
		pthread_cond_wait(&condicion, &semActividadSocial);
		condicionEmpezarActividad=1; //Empieza la actividad
		printf("\t \t ---Empezando Actividad---\n");

		pthread_mutex_lock(&semLog); 
		writeLogMessage("Hilo coordinador","La actividad social va a comenzar");
		pthread_mutex_unlock(&semLog);
		pthread_cond_signal(&entrarActividad[0]);




		printf("\t \t ---En curso Actividad---\n");
		pthread_cond_wait(&salirActividad, &semActividadSocial); //Espera a que el ultimo participante le avise de que ha termido
		
		pthread_mutex_lock(&semLog); 
		writeLogMessage("Hilo coordinador","La actividad social ha terminado");
		pthread_mutex_unlock(&semLog);
		
		pthread_mutex_unlock(&semActividadSocial);  //Abrimos el mutex
		
		condicionEmpezarActividad=0; //Termina la actividad, pueden entrar personas a la actividad
		printf("\t \t ---Terminada actividad---\n"); 
	}
		printf("\t \t //-Terminado hilo coordinador-//\n");
		pthread_exit(NULL);

	//Cierra el hilo
}

void manejadoraTerminar(){
	
	if(fin==0) {
		printf("\t \t //-Ha llegado la senyal de terminar, terminando la aplicacion-//\n");
		fin=1;

		while(contadorFinAtendedores!=MAXATEND){ //Esperamos a que terminen los atendedores de gestionar las solicitudes
			sleep(1);
		}
		
		while(condicionEmpezarActividad==1) { //Esperamos a que termine la actividad social si hay una en curso
			sleep(1);
		}
		pthread_mutex_lock(&semLog);
		writeLogMessage("Manejadora terminar programa ","termina el programa despues de que los atendedores terminen su trabajo");
		pthread_mutex_unlock(&semLog);
		printf("\t \t // -Se he terminado de atender y han acabado su trabajo los atendedores- //\n");
		printf("\t \t // -Terminando aplicacion- //\n");
		exit(0);
	}
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
	int posicion=-1;
	int posEncontrada=0;

	for(int i=0;i<MAXUSR && posEncontrada==0;i++) { //Buscamos donde hay un hueco libre
		if(colaSolicitud[i].id==0) { //Si lo encuentra guardamos su posicion
			posicion=i;
			posEncontrada=1;
		}
	}
	
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
	int posicion=-1;
	int posEncontrada=0;

	for(int i=0;i<MAXSOCIALACT && posEncontrada==0;i++) { //Buscamos donde hay un hueco libre
		if(colaEventos[i].id==0) { //Si lo encuentra guardamos su posicion
			posicion=i;
			posEncontrada=1;
		}
	}
	return posicion; //Si encontramos un hueco devuelve su posicion, si no devuelve -1
}

//Cuenta el numero de solicitudes que hay del tipo argumentado y lo devuelve
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

//Busca el usuario con el minimo id de un tipo y devuelve su posicion
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

//Funcion que devuelve un valor de atencion aleatoriamente. 
//Si el numero generado aleatoriamente esta entre el 1 y el 7 devolvera 0 (todo correcto)
//Si es el 8 o el 9 devolver un 1 (Error en datos personales)
//Si es el 10 devolvera un 2  (Usuario con antecedentes policiales)
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

//Revisa si el usuario tiene todo en regla, hay error con sus datos personales o tiene expediente policial y lo registra en el log

void atendiendo(int pos, int idUser, int atendedorId) {
	int atencion;
	char identificacion[100];
	char mensajeAtencion[200];
	char mensajeTiempo[100];
	int tiempo;
	sprintf(identificacion, "Atendedor %i",atendedorId);
	printf("Atendedor %i atiende al usuario %i \n",atendedorId,idUser);
	atencion=calcularAtencion(); //Calculamos la atencion
	//Escribimos en el log
	pthread_mutex_lock(&semLog); 
	writeLogMessage(identificacion,"Atiende al usuario -insertar numero-");
	pthread_mutex_unlock(&semLog);
	if(atencion==0) {
		tiempo=calculaAleatorios(1,4);
		sleep(tiempo);
		
		printf("Atendedor %i termina de atender al usuario %i \n",atendedorId,idUser);
		printf("Atencion correcta del usuario %i \n",idUser);
		sprintf(mensajeAtencion, "Atendedor %i termina de atender al usuario %i, atencion correcta",atendedorId,idUser);
		sprintf(mensajeTiempo, "El usuario %i ha esperado %i segundo(s) siendo atendido",idUser,tiempo);
		pthread_mutex_lock(&semLog);
		writeLogMessage(identificacion,mensajeAtencion);
		writeLogMessage(identificacion,mensajeTiempo);
		pthread_mutex_unlock(&semLog);
	} else if(atencion==1) {
		printf("Atendedor %i termina de atender al usuario %i \n",atendedorId,idUser);
		
		printf("Error en datos personales del usuario %i \n",idUser);
		tiempo=calculaAleatorios(2,6);
		sleep(tiempo);
		sprintf(mensajeAtencion, "Atendedor %i termina de atender al usuario %i, error en datos personales",atendedorId,idUser);
		sprintf(mensajeTiempo, "El usuario %i ha esperado %i segundo(s) siendo atendido",idUser,tiempo);
		pthread_mutex_lock(&semLog);
		writeLogMessage(identificacion,mensajeAtencion);
		writeLogMessage(identificacion,mensajeTiempo);
		pthread_mutex_unlock(&semLog);
	} else if(atencion==2) {
		printf("Atendedor %i termina de atender al usuario %i \n",atendedorId,idUser);
		
		printf("Encontrada ficha policial del usuario %i \n",idUser);

		tiempo=calculaAleatorios(6,10);
		sleep(tiempo);

		sprintf(mensajeAtencion, "Atendedor %i termina de atender al usuario %i, encontrados antecedentes policiales del usuario",atendedorId,idUser);
		sprintf(mensajeTiempo, "El usuario %i ha esperado %i segundo(s) siendo atendido",idUser,tiempo);
		pthread_mutex_lock(&semLog);
		writeLogMessage(identificacion,mensajeAtencion);
		writeLogMessage(identificacion,mensajeTiempo);
		pthread_mutex_unlock(&semLog);
	}
	//Cambiamos el flag de atendido
	pthread_mutex_lock(&semSolicitudes);
	if(atencion==0 || atencion==1) {
		colaSolicitud[pos].atendido=2; //Se le termina de atender
	} else if(atencion==2) {
		colaSolicitud[pos].atendido=3; //No podra unirse a una actividad social
	}
	pthread_mutex_unlock(&semSolicitudes);
}

		
		
	



