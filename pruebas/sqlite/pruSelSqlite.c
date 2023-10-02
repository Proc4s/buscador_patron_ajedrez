// programa de prueba para evaluar las prestaciones de sqlite en consulta.
//
// El programa lanza un QUERY seleccionando partidas que pertenezcan a una
// determinado fileid y particion y cumplan con los valores elo especificados
// y el ganador especificado.
//
// Por cada partida ejecuta en el tablero virtual los movimientos adecuados.
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <sqlite3.h>
#include <time.h>

#define TIEMPO ((clock()*10)/CLOCKS_PER_SEC)

#define NADA 		0
#define PEON		1
#define CABALLO 	2
#define ALFIL 		3
#define TORRE 		4
#define REINA 		5
#define REY 		6
#define TABOO		7		// indicacion de posicion TABOO.
#define NEGRA 		0x8
#define INVAL		0xf


uint8_t tabini[64] = {0x0c,0x0a,0x0b,0x0d,0x0e,0x0b,0x0a,0x0c,
							 0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,
							 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
							 0x04,0x02,0x03,0x05,0x06,0x03,0x02,0x04};

#define MAXMOV 1000	// maximo numero movimientos en una partida.
#define MAGIC	0x55AA

typedef struct {
	uint8_t	piezaorg;
	uint8_t	piezadest;
	uint8_t	origen;
	uint8_t	destino;
} MOVBIN_t;

typedef struct {
	uint8_t	ganablanca : 1;
	uint8_t	gananegra : 1;
	uint8_t	tnormal : 1;
	uint8_t	reser : 5;
} FLAGS_t;
	
typedef struct {
	uint16_t	magic;
	uint8_t	reser;
	FLAGS_t	flags;
	uint16_t	nmov;
	uint16_t	elomed;
	uint32_t	ind;
} CPARTIDA_t;

CPARTIDA_t	cabpartida;
MOVBIN_t		movimientos[MAXMOV];

//================ SQLITE ============================================
// conexion base.
sqlite3 *db = NULL;
sqlite3_stmt *stmt;

void conectaSqlite(char *basename)
{
	int rc;
	
	rc = sqlite3_open(basename, &db);
	if( rc ){
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		exit(1);
	}
}

void desconectaSqlite()
{
	sqlite3_close(db);
	db = NULL;
}

void lanzaQuery(int fileid, int particion,int elomed,int gana)
{
	int rc;
	const char* query = "SELECT * FROM partidas WHERE fileid = ? and particion = ? and elomed > ?  and ganador = ?";
	const char* queryr = "SELECT * FROM partidas WHERE fileid = ? and particion = ? and elomed > ? ";
	
	if(gana == 0)
		rc = sqlite3_prepare(db, queryr, -1, &stmt, NULL);
	else
		rc = sqlite3_prepare(db, query, -1, &stmt, NULL);
	sqlite3_bind_int(stmt, 1, fileid);
	sqlite3_bind_int(stmt, 2, particion);
	sqlite3_bind_int(stmt, 3, elomed);
	if(gana != 0)
		sqlite3_bind_int(stmt, 4, gana);
}

int nextPartida()
{
	int rc;
	uint8_t ganador;
	
	memset(&cabpartida,0,sizeof(cabpartida));
	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
	  cabpartida.elomed = sqlite3_column_int(stmt, 2);
	  cabpartida.ind = sqlite3_column_int(stmt, 4);
	  ganador = sqlite3_column_int(stmt, 3);
	  const void *datos_resultado = sqlite3_column_blob(stmt, 5);
	  int tamano_resultado = sqlite3_column_bytes(stmt, 5);
	  cabpartida.nmov = tamano_resultado/4;
	  memcpy(movimientos,datos_resultado,tamano_resultado);
	  switch(ganador)
      {
			case 1:
				cabpartida.flags.ganablanca = 1;
				cabpartida.flags.gananegra = 0;
				break;
			case 2:
				cabpartida.flags.ganablanca = 0;
				cabpartida.flags.gananegra = 1;
				break;
			case 3:
				cabpartida.flags.ganablanca = 1;
				cabpartida.flags.gananegra = 1;
				break;
			default:
				cabpartida.flags.ganablanca = 0;
				cabpartida.flags.gananegra = 0;
				break;
		}
		return 1;
    } else {
        return 0;
    }
}

void liberaQuery()
{
	sqlite3_finalize(stmt);
}
//-----------------------------------------------------------

// inicia partida.
void iniciaJuego(uint8_t *tab)
{
	memcpy(tab,tabini,sizeof(tabini));
}

void mueve(MOVBIN_t mov,uint8_t *tab)
{
	tab[mov.origen] = NADA;
	tab[mov.destino] = mov.piezadest;
}


void main(int argc, char *argv[])
{
	int fileid,partid,elomed,gana;
	int partida,encontrados = 0;
	int ind;
	MOVBIN_t *mov;
	int i,len,nread;
	int movpartida;
	int ultcolor;
	uint8_t tablero[64];
	clock_t slot;
	
	if((argc != 5) && (argc != 6))
	{
		fprintf(stderr,"Usage: %s <base> <fileid> <particion> <elomed> [<gana>} \n",argv[0]);
		exit(1);
	}
	fileid = atoi(argv[2]);
	partid = atoi(argv[3]);
	elomed = atoi(argv[4]);
	if(argc == 5)
		gana = 0;
	else
		gana = atoi(argv[5]);

	conectaSqlite(argv[1]);
	lanzaQuery(fileid,partid,elomed,gana);
	ind = 1;
	slot = TIEMPO;
	
	while(nextPartida())
	{
		iniciaJuego(tablero);
		mov = (MOVBIN_t *)movimientos;
		ultcolor = NEGRA;
		movpartida = 0;
		for(i=0;i<cabpartida.nmov;i++)
		{
			if((mov[i].piezadest & NEGRA) == 0)
			{
				if( ultcolor == NEGRA)
					movpartida++;
			}
			ultcolor = mov[i].piezadest & NEGRA;
			mueve(mov[i],tablero);
		}
	//	if((ind % 10000) == 0)
	//	if(TIEMPO != slot)
		{
	//		slot = TIEMPO;
			fprintf(stderr,"IND=>%d           \r",ind);
			fflush(stderr);
		}
		ind++;
	}
	fprintf(stderr,"IND=> %d\n",ind);
	liberaQuery();
	desconectaSqlite();
	exit(0);
}

