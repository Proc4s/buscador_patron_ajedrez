// modulo : config.h
// autor  : Antonio Pardo Redondo
//
// Este modulo provee las funciones necesarias para interpretar los ficheros
// de configuración del sistema de busqueda de patrones de ajedrez.
//
// los ficheros de configuración se encuentran en la carpeta 'conf' del
// arbol de busqueda. Actualmente consta de dos ficheros:
//	-'base.conf' que especifica la configuracion de la base de datos.
//	-'job.conf' que especifica los criterios de busqueda, el patron a buscar
//					el formato de salida y el canal de comunicaciones para reportar
//					el progreso de la busqueda.
//
// En estos ficheros se especifica una propiedad por linea con el formato:
//		clave = valor
//
// El fichero de configuracion de base 'base.conf' define los siguientes parametros:
//		-BASENAME= 'nombre de la base'
//		-BASENUM = Numero de instancias de la base.
//
// El fichero 'job.conf' define los siguientes parametros:
//		-ELOMIN= valor minimo de ELOMED a considerar en la busqueda.
//		-ELOMAX= valor maximo de ELOMED a considerar en la busqueda.
//		-GANADOR= ganador de las partidas a considerar (0=Cualquiera, 1=blancas, 2=Negras, 3=Tablas)
//		-FORMASAL= Formato del resultado de la busqueda (0= Imagen, 1= FEN) 
//		-PATRON='Path al fichero de patron a buscar compilado'.
//		-FIFO= 'nombre canal de comunicaciones para notificar progreso de la busqueda'.
//
#ifndef CONFIG_H
#define CONFIG_H

#include "ajedrez.h"

// Estructura de configuracion de base de datos.
typedef struct {
		int numbases;		// Numero de bases.
		char *nombase;		// Nombre de las bases.
		char *basmaster;	// Nombre de la base master (base_0).
	} CONF_BAS_t;

// Estructura de configuracion del trabajo.
typedef struct {
		int elomin;		// elomed minimo a buscar.
		int elomax;		// elomed maximo a buscar.
		int ganador;	// ganador de la partida.
		int formasal;	// formato de salida del resultado.
		char *patron;	// Path al patron compilado a buscar.
		char *fifo;		// Nombre canal de comunicaciones progreso.
	} CONF_JOB_t;

// Estructura de particion a procesar.	
typedef struct {
	int fileid;			// identificador de fichero original (anho*12+mes).
	int particion;		// N. de particion a procesar.
	int base;			// indice de la base de datos a utilizar.
} PARTICION_t;

// Funcion que basandose en el path del sistema de busqueda y en el fichero
// de configuracion de base, determina el path de la base a utilizar para la particion
// indicada.		
extern char *getBasFromParticion(char *pathajz,CONF_BAS_t *cnfbas,int particion);

// Funcion para rellenar los campos de la estructura 'CONF_BAS_t' a partir
// del fichero de configuracion de base.
// retorna '1' si la lectura ha sido correcta y '0' en caso contrario.
extern int getConfBase(char *pathajz,CONF_BAS_t *cnfbas);

// Funcion para rellenar los campos de la estructura 'CONF_JOB_t' a partir
// del fichero 'job.conf'.
// retorna '1' si la lectura ha sido correcta y '0' en caso contrario.
extern int getConfJob(char *pathajz,CONF_JOB_t *cnfjob);

// Funcion para rellenar la estructura 'PARTICION-t' a partir de un string
// que indica el fileid, la particion y el indice de la base donde reside separadas por ','.
// Este string es el que suministra el sistema de busqueda al programa buscador.
// retorna '1' si la lectura ha sido correcta y '0' en caso contrario.
extern int getParticion(PARTICION_t *part,char *text);


#endif //CONFIG_H
