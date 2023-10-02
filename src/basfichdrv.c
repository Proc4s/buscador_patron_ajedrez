// modulo : basfichdrv.c
// autor  : Antonio Pardo Redondo
//
// Driver de fichero de datos indexado para partidas de ajedrez.
// El identificador del fichero indexado es el path de la carpeta que contiene sus ficheros componentes. 
//
// La carpeta del fichero indexado contiene tres ficheros: 
//		-part.id => indices de particiones, contiene fileid (identificador de fichero), particion (trozo del fichero)
//						offset en campos.id donde comienza las patidas de esta particion y la longitud de los mismos.
//						Esta ordenado por fileid, particion.
//		-campos.id => indices de partidas. Campos para ordenar las partidas de una particion, contiene los datos
//						de ELOMED y ganador asi como el offset de la partida en el fichero de datos y su longitud.
//						Esta ordenado por ELOMED, ganador.
//		-data.bin => contiene los datos de cabecera de partida y movimientos de cada partida. Esta ordenado por orden de entrada.
//
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include "basfichdrv.h"
			
// Abre la base de datos para lectura.
int basfichOpenR(char *path,BASFICH_t *bd)
{
	char nomtmp[1000];
	int res;
	
	sprintf(nomtmp,"%s/part.id",path);	// Fichero de particiones.
	if((bd->fdparticiones=open(nomtmp,O_RDONLY)) >= 0)	// abrimos fichero particiones para lectura.
	{
		sprintf(nomtmp,"%s/campos.id",path);	// fichero de partidas.
		if((bd->fdpartidas=open(nomtmp,O_RDONLY)) >= 0)	// abrimos fichero de partidas para lectura.
		{
			sprintf(nomtmp,"%s/data.bin",path);	// fichero de datos.
		//	if((bd->fddata=fopen(nomtmp,"r")) != NULL)
			if((bd->fddata=open(nomtmp,O_RDONLY)) >= 0)	// abrimos fichero de datos para lectura.
			{
				bd->lenparticiones = lseek(bd->fdparticiones,0,SEEK_END); // tamanho fichero particiones.
				lseek(bd->fdparticiones,0,SEEK_SET);	// posicionamos principio fichero.
				bd->particiones = malloc(bd->lenparticiones);	// reservamos memoria para contener el fichero de particiones.
				res = read(bd->fdparticiones,bd->particiones,bd->lenparticiones); // volcamos a memoria fichero particiones.
				bd->partidas = NULL;
				bd->lenpartidas = 0;
				return 1;
			}
			else  // fallo de apertura fichero de datos.
			{
				close(bd->fdpartidas);
				close(bd->fdparticiones);
			}
		}
		else  // fallo de apertura fichero de partidas.
		{
			close(bd->fdparticiones);
		}
	}
	bd->fdparticiones = -1;
	bd->lenparticiones = 0;
	bd->fdpartidas = -1;
	bd->fddata = -1;
	bd->particiones = NULL;
	bd->partidas = NULL;
	bd->lenpartidas = 0;
	return 0;
}



// Abre la base de datos para lectura-escritura (append).
int basfichOpenW(char *path,BASFICH_t *bd)
{
	char nomtmp[1000];
	
	sprintf(nomtmp,"%s/part.id",path);	// Fichero de particiones.
	if((bd->fdparticiones=open(nomtmp,O_RDWR | O_CREAT,0666)) >= 0)	// abrimos fichero particiones para lectura-escritura.
	{
		bd->lenparticiones = lseek(bd->fdparticiones,0,SEEK_END); // tamanho fichero particiones y posicionado al final de este.
		sprintf(nomtmp,"%s/campos.id",path);	// fichero de partidas.
		if((bd->fdpartidas=open(nomtmp,O_RDWR | O_CREAT,0666)) >= 0)	// apertura para lectura-escritura.
		{
			lseek(bd->fdpartidas,0,SEEK_END);	// posicionamos final partidas.
			sprintf(nomtmp,"%s/data.bin",path);	// fichero de datos.
			if((bd->fddata=open(nomtmp,O_RDWR | O_CREAT,0666)) >= 0)	// apertura para lectura-escritura.
			{
				lseek(bd->fddata,0,SEEK_END);	// posicionamos final datos.
				bd->particiones = NULL;
				bd->partidas = NULL;
				bd->lenparticiones = 0;
				bd->lenpartidas = 0;
				return 1;
			}
			else // fallo apertura datos.
			{
				close(bd->fdpartidas);
				close(bd->fdparticiones);
			}
		}
		else  // fallo apertura partidas.
		{
			close(bd->fdparticiones);
		}
	}
	bd->fdparticiones = -1;
	bd->lenparticiones = 0;
	bd->fdpartidas = -1;
	bd->fddata = -1;
	bd->particiones = NULL;
	bd->partidas = NULL;
	bd->lenpartidas = 0;
	
	return 0;
}

// cierre de la base.
void basfichClose(BASFICH_t *bd)
{
	// libera las memorias utilizadas.
	if(bd->particiones != NULL)
		free(bd->particiones);
	if(bd->partidas != NULL)
		free(bd->partidas);
	// cierra ficheros abiertos.
	if(bd->fdparticiones >= 0)
		close(bd->fdparticiones);
	if(bd->fdpartidas >= 0)
		close(bd->fdpartidas);
	if(bd->fddata >= 0)
		close(bd->fddata);
	bd->fdparticiones = -1;
	bd->lenparticiones = 0;
	bd->fdpartidas = -1;
	bd->fddata = -1;
	bd->particiones = NULL;
	bd->partidas = NULL;
	bd->lenpartidas = 0;
}

// funcion de comparacion para QSORT para ordenar particiones por
// fileid, particion.
int compaParticion(const void *uno,const void *otro)
{
	int res;
	
	res = ((PARTFICH_t *)uno)->fileid - ((PARTFICH_t *)otro)->fileid;
	if(res != 0)
		return res;
	res = ((PARTFICH_t *)uno)->particion - ((PARTFICH_t *)otro)->particion;
	return res;
}

// Funcion para ordenar los indices de particiones por fileid, particion.
void ordenaParticiones(BASFICH_t *bd)
{
	uint8_t *particiones;
	int res;
	
	// cargamos en memoria el fichero de particiones.
	bd->lenparticiones = lseek(bd->fdparticiones,0,SEEK_END);	// longitud fichero.
	particiones = malloc(bd->lenparticiones);	// reserva de memoria.
	lseek(bd->fdparticiones,0,SEEK_SET);		// posicionado al principio fichero.
	res = read(bd->fdparticiones,particiones,bd->lenparticiones);	// volcado a memoria.
	qsort(particiones,bd->lenparticiones/sizeof(PARTFICH_t),sizeof(PARTFICH_t),compaParticion);	// ordenacion.
	lseek(bd->fdparticiones,0,SEEK_SET);	// posicionado al principio fichero.
	res = write(bd->fdparticiones,particiones,bd->lenparticiones);	// volcado de memoria al fichero.
	free(particiones);	// liberacion memoria reservada.
}

// funcion de comparacion para QSORT para ordenar partidas por
// elomed, ganador.
int compaPartidas(const void *uno,const void *otro)
{
	int res;
	
	res = ((PARTIDA_t *)uno)->elomed - ((PARTIDA_t *)otro)->elomed;
	if(res != 0)
		return res;
	res = ((PARTIDA_t *)uno)->ganador - ((PARTIDA_t *)otro)->ganador;
	return res;
}

// ordena las partidas de una determinada particion por elomed y ganador.
void ordenaPartidas(BASFICH_t *bd,int fileid,int particion)
{
	PARTFICH_t *partmp;
	uint8_t *particiones;
	uint8_t *partidas;
	int i;
	int res;
	
	bd->lenparticiones = lseek(bd->fdparticiones,0,SEEK_END); // longitud particiones.
	lseek(bd->fdparticiones,0,SEEK_SET);	// posicionamos principio particiones.
	particiones = malloc(bd->lenparticiones);	// reservamos memoria para particiones.
	res = read(bd->fdparticiones,particiones,bd->lenparticiones);	// volcamos a memoria las particiones.
	// buscamos el fileid, particion indicada. NOTA: podria acelerarse con busqueda dicotomica.
	for(i=0,partmp =(PARTFICH_t *)particiones;i<bd->lenparticiones/sizeof(PARTFICH_t);i++,partmp++)
	{
		if((partmp->fileid == fileid) && (partmp->particion == particion))
			break;
	}
	partidas = malloc(partmp->len);	// reservamos memoria para partidas.
	lseek(bd->fdpartidas,partmp->offset,SEEK_SET);	// posicionamos en primera partida de la particion.
	res = read(bd->fdpartidas,partidas,partmp->len);		// volcamos partidas a memoria.
	qsort(partidas,partmp->len/sizeof(PARTIDA_t),sizeof(PARTIDA_t),compaPartidas);	// ordenamos partidas.
	lseek(bd->fdpartidas,partmp->offset,SEEK_SET);	// situamos en primera partida de la particion
	res = write(bd->fdpartidas,partidas,partmp->len);	// volcamos partidas ordenadas a fichero.
	free(particiones);	// liberamos memoria particiones.
	free(partidas);		// liberamos memoria partidas.
}

// busca una particion en el fichero de indices de particiones.
PARTFICH_t *buscaParticion(BASFICH_t *bd,int fileid,int particion)
{
	int i;
	PARTFICH_t *partmp;
	
	for(i=0,partmp = bd->particiones;i<(bd->lenparticiones/sizeof(PARTFICH_t));i++,partmp++)
	{
		if((partmp->fileid == fileid) && (partmp->particion == particion))
			return(partmp);
	}
	return NULL;
}

// funcion para cargar en memoria las partidas de una particion.
int cargaPartidas(BASFICH_t *bd,PARTFICH_t *particion)
{
	int res;
	
	if(bd->partidas != NULL)
		free(bd->partidas);
	bd->partidas = malloc(particion->len);
	lseek(bd->fdpartidas,particion->offset,SEEK_SET);
	res = read(bd->fdpartidas,bd->partidas,particion->len);
	bd->lenpartidas = particion->len;
	return 1;
}

// busca la primera partida que cumpla con elomin y ganador en las partidas de una particion cargada.
PARTIDA_t *buscaPartida(BASFICH_t *bd,int elomin,int gana)
{
	int i;
	PARTIDA_t *partmp;
	
	for(i=0,partmp = bd->partidas;i<(bd->lenpartidas/sizeof(PARTIDA_t));i++,partmp++)
	{
		if((partmp->elomed == elomin) && (partmp->ganador == gana))
			return(partmp);
	}
	return NULL;
}

// Funcion que anhade una particion al final del fichero de indices de particiones.
int anhadeParticion(BASFICH_t *bd,PARTFICH_t *particion)
{
	int res;
	
	lseek(bd->fdparticiones,0,SEEK_END);
	res = write(bd->fdparticiones,particion,sizeof(PARTFICH_t));
}

// funcion que ahade una partida al final del fichero de indices de partidas.
int anhadePartida(BASFICH_t *bd,PARTIDA_t *partida)
{
	int res;
	
	lseek(bd->fdpartidas,0,SEEK_END);
	res = write(bd->fdpartidas,partida,sizeof(PARTIDA_t));
}

// Funcion para leer los datos de una determinada partida (cabpartida y movimientos).
void loadPartida(BASFICH_t *bd,PARTIDA_t *partida,CPARTIDA_t *cabpartida,MOVBIN_t *movimientos)
{
	int res;
	
	lseek(bd->fddata,partida->offset,SEEK_SET);
	res = read(bd->fddata,cabpartida,sizeof(CPARTIDA_t));
	res = read(bd->fddata,movimientos,cabpartida->nmov * sizeof(MOVBIN_t));
}
