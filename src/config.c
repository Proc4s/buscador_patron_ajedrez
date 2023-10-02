// modulo : config.c
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
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

// Funcion para limpiar por delante y por detras de blancos, tabuladores y finales
// de linea el texto pasado como parametro.
void limpia(char *texto)
{
	char textotmp[1000];
	char *pchar = texto;
	
	// limpia por delante blancos y tabuladores.
	while(*pchar != 0)
	{
		if((*pchar !=' ') && (*pchar !='\t'))
			break;
		pchar++;
	}
	strcpy(textotmp,pchar);
	// limpia por detras blancos, tabuladores y finales de linea.
	if(strlen(textotmp))
	{
		pchar = textotmp + strlen(textotmp) -1;
		while(pchar != textotmp)
		{
			if((*pchar !=' ') && (*pchar !='\t') && (*pchar !='\n') && (*pchar !='\r'))
				break;
			*pchar = 0;
			pchar--;
		}
	}
	strcpy(texto,textotmp);
}

// Funcion que basandose en el path del sistema de busqueda y en el fichero
// de configuracion de base, determina el path de la base a utilizar para la particion
// indicada.
char *getBasFromParticion(char *pathajz,CONF_BAS_t *cnfbas,int particion)
{
	static char nombastmp[1000];
	
	sprintf(nombastmp,"%s/base/base_%01d/%s",pathajz,particion%(cnfbas->numbases),cnfbas->nombase);
	return(nombastmp);
}

// Funcion para rellenar los campos de la estructura 'CONF_BAS_t' a partir
// del fichero de configuracion de base.
// retorna '1' si la lectura ha sido correcta y '0' en caso contrario.
int getConfBase(char *fileconf,CONF_BAS_t *cnfbas)
{
	FILE *fdtmp;
	static char nombase[1000];
//	static char basmaster[1000];
	char nametmp[1000];
	char linea[1000];
	char *pchar;
	
	nombase[0] = 0;
//	sprintf(nametmp,"%s/conf/base.conf",pathajz);
//	if((fdtmp=fopen(nametmp,"r")) == NULL)
	if((fdtmp=fopen(fileconf,"r")) == NULL)
	{
		perror("No puedo abrir fich. conf de base\n");
		exit(1);
	}
	while(fgets(linea,1000,fdtmp) != NULL)
	{
		if(strlen(linea) < 10)
			continue;
		pchar = strchr(linea,'=');
		if(pchar == NULL)
			continue;
		pchar++;
		if(strstr(linea,"BASENAME") != NULL)
		{
			strcpy(nombase,pchar);
			limpia(nombase);
			cnfbas->nombase = nombase;
		}
		else if(strstr(linea,"BASENUM") != NULL)
		{
			cnfbas->numbases = atoi(pchar);
		}
	}
	fclose(fdtmp);
//	sprintf(basmaster,"%s/base/base_0/%s",pathajz,nombase);
	cnfbas->basmaster = NULL;
	if((cnfbas->numbases > 0) && (strlen(nombase) > 0))
		return 1;
	return 0;
}

// Funcion para rellenar los campos de la estructura 'CONF_JOB_t' a partir
// del fichero 'job.conf'.
// retorna '1' si la lectura ha sido correcta y '0' en caso contrario.
int getConfJob(char *pathajz,CONF_JOB_t *cnfjob)
{
	FILE *fdtmp;
	static char patron[1000];
	static char fifo[1000];
	char nametmp[1000];
	char linea[1000];
	char *pchar;
	
	sprintf(nametmp,"%s/conf/job.conf",pathajz);
	if((fdtmp=fopen(nametmp,"r")) == NULL)
	{
		perror("No puedo abrir fich. conf de JOB\n");
		exit(1);
	}
	cnfjob->elomin = 0;
	cnfjob->elomax = 0;
	cnfjob->ganador = 0;
	cnfjob->formasal = 0;
	cnfjob->patron = NULL;
	while(fgets(linea,1000,fdtmp) != NULL)
	{
		if(strlen(linea) < 10)
			continue;
		pchar = strchr(linea,'=');
		if(pchar == NULL)
			continue;
		pchar++;
		if(strstr(linea,"ELOMIN") != NULL)
		{
			cnfjob->elomin = atoi(pchar);
		}
		else if(strstr(linea,"ELOMAX") != NULL)
		{
			cnfjob->elomax = atoi(pchar);
		}
		else if(strstr(linea,"GANADOR") != NULL)
		{
			cnfjob->ganador = atoi(pchar);
		}
		else if(strstr(linea,"FORMASAL") != NULL)
		{
			cnfjob->formasal = atoi(pchar);
		}
		else if(strstr(linea,"PATRON") != NULL)
		{
			strcpy(patron,pchar);
			limpia(patron);
			cnfjob->patron = patron;
		}
		else if(strstr(linea,"FIFO") != NULL)
		{
			strcpy(fifo,pchar);
			limpia(fifo);
			cnfjob->fifo = fifo;
		}
	}
	if((cnfjob->patron != NULL) && (cnfjob->fifo != NULL) && (cnfjob->elomax > 0) && (cnfjob->elomax >= cnfjob->elomin))
		return 1;
	return 0;
}

// Funcion para rellenar la estructura 'PARTICION-t' a partir de un string
// que indica el fileid, la particion y el indice de la base donde reside separadas por ','.
// Este string es el que suministra el sistema de busqueda al programa buscador.
// retorna '1' si la lectura ha sido correcta y '0' en caso contrario.
int getParticion(PARTICION_t *part,char *text)
{
	char *pchar;
	
	if((pchar = strtok(text,",")) == NULL)
		return 0;
	part->fileid = atoi(pchar);
	if((pchar = strtok(NULL,",")) == NULL)
		return 0;
	part->particion = atoi(pchar);
	if((pchar = strtok(NULL,",")) == NULL)
		return 0;
	part->base = atoi(pchar);
	return 1;
}
