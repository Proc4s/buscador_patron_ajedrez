// modulo : fich2sqlite.c
// autor  : Antonio Pardo Redondo
//
// Utilidad para cargar un conjunto de bases SQLITE indicadas por el fichero
// de configuración de base, con los datos de partidas almacenadas en el
// fichero indexado cuyo path se indica.
//
// La ventaja de cargar la base de datos de esta forma consiste en que los
// datos ya se insertan ordenados por los criterios de busqueda, acelerando
// enormemente la velocidad de consulta de los mismos.
//
// Las particiones se graban en la base correspondiente al numero de 
// particion modulo el numero de bases. Normalmente el sistema se configura
// para que exista una base de datos por cada disco de la maquina, de esta
// forma se multiplica el ancho de banda dispoible de la informacion almacenada.
// repartiendo las particiones entre las distintas bases.
//
// La tabla de particiones global reside en la base_0 (master) de sqlite.
// Esta tabla indica a cada fileid-particion en que numero de base se encuentra.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include "ajedrez.h"
#include "funaux.h"
#include "config.h"
#include "sqlitedrv.h"
#include "basfichdrv.h"

int transpend = 0;	// transaccion pendiente.
int baseopen = 0; 	// base abierta.

void main(int argc, char *argv[])
{
	CONF_BAS_t cnfbas;
	char basmaster[1000];
	BASFICH_t 	bdfch;
	CPARTIDA_t	cabpartida;
	MOVBIN_t		movimientos[MAXMOV];
	PARTFICH_t 	particioncur;
	PARTFICH_t *partfch;
	PARTIDA_t *partidafch;
	sqlite3 		*dbsq3 = NULL;
	sqlite3_stmt *stmt;
	int i = 0;
	clock_t slot;
	char nombastmp[1000];
	
	if(argc != 4)
	{
		fprintf(stderr,"Usage: %s <pathbasfich> <carpetabases sqlite> <base.conf>\n",argv[0]);
		exit(1);
	}
	// abrimos fichero indexado.
	if(basfichOpenR(argv[1],&bdfch ) == 0)
	{
		fprintf(stderr,"No puedo abrir basfich\n");
		exit(1);
	}
	// cargamos configuracion de bases.
	if(getConfBase(argv[3],&cnfbas) == 0)
	{
		fprintf(stderr,"Configuracion base invalida\n");
		exit(1);
	}
	sprintf(basmaster,"%s/base_0/%s",argv[2],cnfbas.nombase);
	cnfbas.basmaster = basmaster;
	
	
//	conectaSqlite(&dbsq3,argv[2]);
//	beginTransW(dbsq3,&stmt);
	
	// iteramos por particiones.
	for(partfch=bdfch.particiones;((uint8_t *)partfch - (uint8_t *)(bdfch.particiones)) < bdfch.lenparticiones;partfch++)
	{
		cargaPartidas(&bdfch,partfch);
		
		// cerramos posible transaccion y base abierta,anotamos particion en tabla particiones de la base master
		// abrimos la base correspondiente a la nueva particion y comenzamos nueva transaccion. 
		if(transpend)
		{
			endTransW(dbsq3,stmt);
			transpend = 0;
		}
		if(baseopen)
		{
			desconectaSqlite(dbsq3);
			baseopen = 0;
		}
		conectaSqlite(&dbsq3,cnfbas.basmaster);
		baseopen = 1;
		insertaParticion(dbsq3,partfch->fileid,partfch->particion,partfch->particion % cnfbas.numbases);
		desconectaSqlite(dbsq3);
		baseopen = 0;
		sprintf(nombastmp,"%s/base_%01d/%s",argv[2],partfch->particion % cnfbas.numbases,cnfbas.nombase);
		conectaSqlite(&dbsq3,nombastmp);
		baseopen = 1;
		beginTransW(dbsq3,&stmt);
		transpend = 1;
		// iteramos por las partidas de la particion en curso.
		for(partidafch=bdfch.partidas;((uint8_t *)partidafch - (uint8_t *)(bdfch.partidas)) < bdfch.lenpartidas;partidafch++)
		{
			loadPartida(&bdfch,partidafch,&cabpartida,movimientos);
			vuelcaPart(dbsq3,stmt,&cabpartida,movimientos,partfch->fileid,partfch->particion);
			i++;
			// mostramos periodicamente el progreso.
			if(TIEMPO != slot)
			{
				slot = TIEMPO;
				printf("PART=>%d\r",i);
				fflush(stdout);
			}
		}
	//	endTransW(dbsq3,stmt);
	//	beginTransW(dbsq3,&stmt);
	}
	// finalizamos la ultima transaccion y cerramos las bases.
	endTransW(dbsq3,stmt);
	desconectaSqlite(dbsq3);
	basfichClose(&bdfch);
}
