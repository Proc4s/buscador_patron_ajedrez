// Prueba de inserccion sqlite.
//
// carga el primer millon de partidas del fichero de entrada en formato
// pgn a la tabla partidas de la base sqlite.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <sqlite3.h>
#include <signal.h>
#include <time.h>

#define TIEMPO ((clock()*10)/CLOCKS_PER_SEC)

// codificacion de las piezas
#define NADA 		0
#define PEON		1
#define CABALLO 	2
#define ALFIL 		3
#define TORRE 		4
#define REINA 		5
#define REY 		6
#define NEGRA 		0x8
#define INVAL		0xf

// el tablero se define por 64 bytes cuyo valor es la pieza que contiene.
// esta es el contenido del tablero virtual al comienzo de una partida.
uint8_t tabini[64] = {0x0c,0x0a,0x0b,0x0d,0x0e,0x0b,0x0a,0x0c,
							 0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,
							 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
							 0x04,0x02,0x03,0x05,0x06,0x03,0x02,0x04};

// lista de piezas activas de un determinado color.
// su contenido es la posicion absoluta en el tablero (0:63) de cada pieza activa.	
typedef struct {
	uint8_t	rey;				// posicion rey.
	uint8_t	reina[9];		// posiciones reinas, varias debido a promociones.
	uint8_t	nreina;			// numero de reinas activas.
	uint8_t	torre[10];		// posiciones torres, puede ser mas de dos por promociones.
	uint8_t	ntorre;			// numero de torres activas.
	uint8_t	alfil[10];		// posiciones alfiles, puede ser mas de dos por promociones.
	uint8_t	nalfil;			// numero alfiles activos.
	uint8_t	caballo[10];	// posiciones caballos, puede ser mas de dos por promociones.
	uint8_t	ncaballo;		// numero caballos activos.
	uint8_t	peon[8];			// posiciones peones.
	uint8_t	npeon;			// numero peones activos.
} PIEZAS_t;

// Tablero. Contiene el tablero propiamente dicho
// y la lista de piezas blancas y negras activas
// para acelerar operaciones.
typedef struct {
	uint8_t	tab[64];		// tablero. Contenido pieza que ocupa esa casilla.
	PIEZAS_t blancas;		// lista piezas blancas.
	PIEZAS_t negras;		// lista piezas negras.
} TABLERO_t;

// Movimiento de una pieza.
typedef struct {
	uint8_t pieza;	// codigo de pieza en destino 
	uint8_t org;	// indice origen.(0-63)
	uint8_t dest;	// indice destino.(0-63)
} MOV_t;


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

//=========================================================================		

int partidas = 0;
int npgn;
char gpgn[100];
char gpgna[100];

char lineas[20][100*1024];
int nlineas;

//================ SQLITE ============================================
// conexion base.
sqlite3 *db = NULL;
int volatile transpend = 0;
int volatile baseopen = 0;
sqlite3_stmt *stmt = NULL;

void conectaSqlite(char *basename)
{
	int rc;
	sqlite3_stmt *stmt1;
	 char *error_message = 0;
	const char* pragasync = "PRAGMA synchronous = 0";
	const char* prgjournal = "PRAGMA journal_mode = MEMORY";
	
	rc = sqlite3_open(basename, &db);
	if( rc ){
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		exit(1);
	}
	rc = sqlite3_prepare(db, pragasync, -1, &stmt1, NULL);
	rc = sqlite3_step(stmt1);
	if (rc != SQLITE_DONE) {
        fprintf(stderr, "Error al cambiar pragma: %s\n", sqlite3_errmsg(db));
   }
   sqlite3_finalize(stmt1);
   
}

void beginTrans()
{
	int rc;
	sqlite3_stmt *stmt1;
	const char* btrans = "BEGIN TRANSACTION";
	const char *query = "INSERT INTO partidas(fileid,particion,elomed,ganador,partidaid,movimientos) VALUES(?,?,?,?,?,?)";
	
	rc = sqlite3_prepare(db, btrans, -1, &stmt1, NULL);
	rc = sqlite3_step(stmt1);
	if (rc != SQLITE_DONE) {
        fprintf(stderr, "Error BEGIN TRANSACTION: %s\n", sqlite3_errmsg(db));
   }
   sqlite3_finalize(stmt1);
	rc = sqlite3_prepare(db, query, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
	  fprintf(stderr, "Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
	  sqlite3_close(db);
	  exit(2);
	}	
   
}

void endTrans()
{
	int rc;
	sqlite3_stmt *stmt1;
	const char* etrans = "END TRANSACTION";
	
	rc = sqlite3_prepare(db, etrans, -1, &stmt1, NULL);
	rc = sqlite3_step(stmt1);
	if (rc != SQLITE_DONE) {
        fprintf(stderr, "Error END TRANSACTION: %s\n", sqlite3_errmsg(db));
   }
   sqlite3_finalize(stmt1);
	sqlite3_finalize(stmt);
}

void desconectaSqlite()
{
	sqlite3_close(db);
	db = NULL;
}

// Vuelca la partida procesada a la base de datos.
void vuelcaPart(int fileid)
{
	int i,rc;
	uint8_t ganador;
	int particion = partidas/1000000;


	if(cabpartida.nmov == 0)
		return;

   ganador = cabpartida.flags.ganablanca;
	ganador += cabpartida.flags.gananegra * 2;
	

	sqlite3_bind_int(stmt, 1, fileid);
	sqlite3_bind_int(stmt, 2, particion);
	sqlite3_bind_int(stmt, 3, cabpartida.elomed);
	sqlite3_bind_int(stmt, 4, ganador);
	sqlite3_bind_int(stmt, 5, cabpartida.ind);
	sqlite3_bind_blob(stmt, 6, (char *)movimientos, cabpartida.nmov * 4, SQLITE_STATIC);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
        fprintf(stderr, "Error al insertar fila: %s\n", sqlite3_errmsg(db));
   }
   sqlite3_clear_bindings(stmt);
   sqlite3_reset(stmt);

}

// funcion para anhadir un movimiento a la lista de movimientos recodificados
// para el registro de la base binaria.
void anadeMov(MOVBIN_t mov)
{
	movimientos[cabpartida.nmov] = mov;
	cabpartida.nmov++;
//	printf("ANHADE=>%d\n",cabpartida.nmov);
}

//==========================================================================
// inicia las estructuras de partida binaria.
void iniPart(void)
{
	memset(&cabpartida,0,sizeof(cabpartida));
	cabpartida.magic = MAGIC;
}

// funcion para mostrar una traza.
void debug(char *mensa,int nivel,MOV_t *mov,TABLERO_t *tab)
{
	int i;
	
	if(nivel < 2)
		return;
	
	printf("\n%s\n",mensa);
	printf("Part=>%d, MOV=>%d, %s-%s\n",partidas,npgn,gpgna,gpgn);
	printf("MORG=>%d, MDEST=>%d, PIEZA=>%d\n",mov->org,mov->dest,mov->pieza);
}

// inicia partida.
// Inicia las estructuras de posicion de las piezas y el tablero virtual.
void iniciaJuego(TABLERO_t *tab)
{
	memcpy(tab->tab,tabini,sizeof(tabini));
	tab->blancas.rey = 60;
	tab->blancas.reina[0] = 59;
	tab->blancas.nreina = 1;
	tab->blancas.torre[0] = 56;
	tab->blancas.torre[1] = 63;
	tab->blancas.ntorre = 2;
	tab->blancas.alfil[0] = 58;
	tab->blancas.alfil[1] = 61;
	tab->blancas.nalfil = 2;
	tab->blancas.caballo[0] = 57;
	tab->blancas.caballo[1] = 62;
	tab->blancas.ncaballo = 2;
	tab->blancas.peon[0] = 48;
	tab->blancas.peon[1] = 49;
	tab->blancas.peon[2] = 50;
	tab->blancas.peon[3] = 51;
	tab->blancas.peon[4] = 52;
	tab->blancas.peon[5] = 53;
	tab->blancas.peon[6] = 54;
	tab->blancas.peon[7] = 55;
	tab->blancas.npeon = 8;
	
	tab->negras.rey = 4;
	tab->negras.reina[0] = 3;
	tab->negras.nreina = 1;
	tab->negras.torre[0] = 0;
	tab->negras.torre[1] = 7;
	tab->negras.ntorre = 2;
	tab->negras.alfil[0] = 2;
	tab->negras.alfil[1] = 5;
	tab->negras.nalfil = 2;
	tab->negras.caballo[0] = 1;
	tab->negras.caballo[1] = 6;
	tab->negras.ncaballo = 2;
	tab->negras.peon[0] = 8;
	tab->negras.peon[1] = 9;
	tab->negras.peon[2] = 10;
	tab->negras.peon[3] = 11;
	tab->negras.peon[4] = 12;
	tab->negras.peon[5] = 13;
	tab->negras.peon[6] = 14;
	tab->negras.peon[7] = 15;
	tab->negras.npeon = 8;
}

// Elimina una pieza de las estructuras correspondientes a piezas activas.
void eliminaPieza(uint8_t pieza,uint8_t posicion,TABLERO_t *tab)
{
	int i;
	PIEZAS_t *piezas;
	
	if(pieza & NEGRA)
		piezas = &tab->negras;
	else
		piezas = &tab->blancas;
	switch(pieza & 0x7)
	{
		case REINA:
			for(i=0;i<piezas->nreina;i++)
			{
				if(piezas->reina[i] == posicion)
					break;
			}
			for(;i<(piezas->nreina - 1);i++)
				piezas->reina[i] = piezas->reina[i+1];
			piezas->nreina--;
			break;
		case TORRE:
			for(i=0;i<piezas->ntorre;i++)
			{
				if(piezas->torre[i] == posicion)
					break;
			}
			for(;i<(piezas->ntorre - 1);i++)
				piezas->torre[i] = piezas->torre[i+1];
			piezas->ntorre--;
			break;
		case CABALLO:
			for(i=0;i<piezas->ncaballo;i++)
			{
				if(piezas->caballo[i] == posicion)
					break;
			}
			for(;i<(piezas->ncaballo - 1);i++)
				piezas->caballo[i] = piezas->caballo[i+1];
			piezas->ncaballo--;
			break;
		case ALFIL:
			for(i=0;i<piezas->nalfil;i++)
			{
				if(piezas->alfil[i] == posicion)
					break;
			}
			for(;i<(piezas->nalfil - 1);i++)
				piezas->alfil[i] = piezas->alfil[i+1];
			piezas->nalfil--;
			break;
		case PEON:
			for(i=0;i<piezas->npeon;i++)
			{
				if(piezas->peon[i] == posicion)
					break;
			}
			for(;i<(piezas->npeon - 1);i++)
				piezas->peon[i] = piezas->peon[i+1];
			piezas->npeon--;
			break;
		default:
			break;
	}	
}

// Funcion para actualizar el tablero virtual con el nuevo movimiento.
void actTablero(MOV_t *mov,TABLERO_t *tab)
{
	tab->tab[mov->org] = NADA;
	if(tab->tab[mov->dest] != NADA)	// sustituye pieza.
	{
		eliminaPieza(tab->tab[mov->dest],mov->dest,tab);
	}
	else
	{
		// movimiento de comida de peon sin pieza en destino => come al paso.
		if(((mov->pieza & 0x7) == PEON) && (abs(mov->dest - mov->org) != 8) && (abs(mov->dest - mov->org) != 16))
		{
			debug("Come al paso",1,mov,tab);
			if(mov->pieza & NEGRA)
			{
				if(tab->tab[mov->dest - 8] == PEON)
				{
					eliminaPieza(tab->tab[mov->dest - 8],mov->dest -8,tab);
					tab->tab[mov->dest - 8] = NADA;
				}
				else
					debug("NO AL PASO",2,mov,tab);
			}
			else
			{
				if(tab->tab[mov->dest + 8] == PEON | NEGRA)
				{
					eliminaPieza(tab->tab[mov->dest + 8],mov->dest +8,tab);
					tab->tab[mov->dest + 8] = NADA;
				//	debug("fcome",10,mov,tab);
				}
				else
					debug("NO AL PASO1",2,mov,tab);
			}
		}
	}
	tab->tab[mov->dest] = mov->pieza;
}

// Actualiza las estructuras y tablero virtual por un movimiento de rey.
void mueveRey(MOV_t *mov,TABLERO_t *tab)
{
	PIEZAS_t *piezas;
	
	if(mov->pieza & NEGRA)
		piezas = &tab->negras;
	else
		piezas = &tab->blancas;	
	
	piezas->rey = mov->dest;
	actTablero(mov,tab);
}

// Actualiza las estructuras y tablero virtual por un movimiento de reina.
void mueveReina(MOV_t *mov,TABLERO_t *tab)
{
	int i;
	PIEZAS_t *piezas;
	
	if(mov->pieza & NEGRA)
		piezas = &tab->negras;
	else
		piezas = &tab->blancas;		

	for(i=0;i<piezas->nreina;i++)
	{
		if(piezas->reina[i] == mov->org)
		{
			piezas->reina[i] = mov->dest;
			break;
		}
	}
	if(i == piezas->nreina)
		debug("No encontrado reina ORG",3,mov,tab);
	actTablero(mov,tab);
}

// Actualiza las estructuras y tablero virtual por un movimiento de torre.
void mueveTorre(MOV_t *mov,TABLERO_t *tab)
{
	int i;
	PIEZAS_t *piezas;
	
	if(mov->pieza & NEGRA)
		piezas = &tab->negras;
	else
		piezas = &tab->blancas;		

	for(i=0;i<piezas->ntorre;i++)
	{
		if(piezas->torre[i] == mov->org)
		{
			piezas->torre[i] = mov->dest;
			break;
		}
	}
	if(i == piezas->ntorre)
		debug("No encontrado torre ORG",3,mov,tab);
	actTablero(mov,tab);
}

// Actualiza las estructuras y tablero virtual por un movimiento de alfil.
void mueveAlfil(MOV_t *mov,TABLERO_t *tab)
{
	int i;
	PIEZAS_t *piezas;
	
	if(mov->pieza & NEGRA)
		piezas = &tab->negras;
	else
		piezas = &tab->blancas;	

	for(i=0;i<piezas->nalfil;i++)
	{
		if(piezas->alfil[i] == mov->org)
		{
			piezas->alfil[i] = mov->dest;
			break;
		}
	}
	if(i == piezas->nalfil)
		debug("No encontrado alfil ORG",3,mov,tab);
	actTablero(mov,tab);
}

// Actualiza las estructuras y tablero virtual por un movimiento de caballo
void mueveCaballo(MOV_t *mov,TABLERO_t *tab)
{
	int i;
	PIEZAS_t *piezas;
	
	if(mov->pieza & NEGRA)
		piezas = &tab->negras;
	else
		piezas = &tab->blancas;	

	for(i=0;i<piezas->ncaballo;i++)
	{
		if(piezas->caballo[i] == mov->org)
		{
			piezas->caballo[i] = mov->dest;
			break;
		}
	}
	if(i == piezas->ncaballo)
		debug("No encontrado caballo ORG",3,mov,tab);
	actTablero(mov,tab);
}

// Actualiza las estructuras y tablero virtual por un movimiento de peon
void muevePeon(MOV_t *mov,TABLERO_t *tab)
{
	int i;
	PIEZAS_t *piezas;
	
	if(mov->pieza & NEGRA)
		piezas = &tab->negras;
	else
		piezas = &tab->blancas;	

	for(i=0;i<piezas->npeon;i++)
	{
		if(piezas->peon[i] == mov->org)
		{
			piezas->peon[i] = mov->dest;
			break;
		}
	}
	if(i == piezas->npeon)
	{
		debug("No encontrado peon org",3,mov,tab);
	}
	actTablero(mov,tab);
}

// mueve una pieza en el tablero.
void mueve(MOV_t *mov,TABLERO_t *tab)
{
	MOVBIN_t movimiento;
	
	movimiento.piezaorg = mov->pieza;
	movimiento.piezadest = mov->pieza;
	movimiento.origen = mov->org;
	movimiento.destino = mov->dest;
	anadeMov(movimiento);
	
	switch(tab->tab[mov->org] & 0x7)
	{
		case REY:
			mueveRey(mov,tab);
			break;
		case REINA:
			mueveReina(mov,tab);
			break;
		case TORRE:
			mueveTorre(mov,tab);
			break;
		case ALFIL:
			mueveAlfil(mov,tab);
			break;
		case CABALLO:
			mueveCaballo(mov,tab);
			break;
		default:
			muevePeon(mov,tab);
			break;
	}
}

// transforma posicion geometrica a indice.
uint8_t transform(uint8_t x,uint8_t y)
{
	return((8+'0'-y)*8+(x-'a'));
}

// determina si el movimiento de la pieza indicada puede provocar jaque.
int posiblejaque(int rey,int pieza,int color,int indest,TABLERO_t *tab)
{
	int diffx,diffy;
	int fila,colum,filpieza,columpieza;
	int i,j;
	int tipo;
	int incx;
	int incy;
	int destx = indest%8;
	int desty = indest/8;
	int sobrepasa;
	
	if((npgn == 15) && (color > 0))
		i = 3;
	colum = rey % 8;
	fila = rey/8;
	columpieza = pieza % 8;
	filpieza = pieza/8;
	
	if(fila == filpieza) // pieza y rey en la misma fila.
	{
		if(fila == desty)
			return 0;
		if(colum > columpieza)
			incx = -1;
		else
			incx = 1;
		sobrepasa = 0;
		for(i=colum+incx;((i>= 0) && (i<8));i+=incx)
		{
			if(i==columpieza)
			{
				sobrepasa = 1;
				continue;
			}
			tipo = tab->tab[fila*8+i];
			if(tipo != NADA)
			{
				if(sobrepasa == 0)	// pieza interpuesta.
					return 0;
				if(color != (tipo & NEGRA))
				{
					tipo &= 0x7;
					if((tipo == REINA) || (tipo == TORRE))
						return 1;	// pieza amenaza.
					return 0;		// pieza interpuesta.
				}
				return 0;	// pieza interpuesta.
			}
		}
		return 0;	// no se encuentra amenaza.
	}
	
	if(colum == columpieza) // pieza y rey en la misma columna
	{
		if(colum == destx)
			return 0;
		if(fila > filpieza)
			incy = -1;
		else
			incy = 1;
		sobrepasa = 0;
		for(i=fila+incy;((i>= 0) && (i<8));i+= incy)
		{
			if(i==filpieza)
			{
				sobrepasa = 1;
				continue;
			}
			tipo = tab->tab[i*8+colum];
			if(tipo != NADA)
			{
				if(sobrepasa == 0)	// pieza interpuesta.
					return 0;
				if(color != (tipo & NEGRA))
				{
					tipo &= 0x7;
					if((tipo == REINA) || (tipo == TORRE))
						return 1;	// pieza amenaza.
					return 0;		// pieza interpuesta.
				}
				return 0;	// pieza interpuesta.
			}
		}
		return 0;	// no se encuentra amenaza.
	}

	diffx = abs(colum - columpieza);
	diffy = abs(fila - filpieza);
	
	
	if(diffx == diffy)	// pieza en diagonal al rey
	{
		incx = colum - columpieza;
		incy = fila - filpieza;
		
		if(abs(colum -destx) == abs(fila -desty))
		{
			if(incx > 0)
			{
				if((colum -destx) > 0)
				{
					if(incy > 0)
					{
						if((fila - desty) > 0)
							return 0;
					}
					else
					{
						if((fila - desty) < 0)
							return 0;
					}
				}
			}
			else
			{
				if((colum -destx) < 0)
				{
					if(incy > 0)
					{
						if((fila - desty) > 0)
							return 0;
					}
					else
					{
						if((fila - desty) < 0)
							return 0;
					}
				}
			}
		}
		
		// sentido de las diagonales.
		if(fila > filpieza)
		{
			incy = -1;
			if(colum > columpieza)
				incx = -1;
			else
				incx = 1;
		}
		else
		{
			incy = 1;
			if(colum > columpieza)
				incx = -1;
			else
				incx = 1;
		}
		sobrepasa = 0;
		// recorremos la diagonal en el sentido de la pieza.
		for(i=fila+incy,j=colum+incx;((i>=0) && (i<8) && (j>=0) && (j<8));i+=incy,j+=incx)
		{
			if((i==filpieza) && (j == columpieza))
			{
				sobrepasa = 1;
				continue;
			}
			tipo = tab->tab[i*8+j];
			if(tipo != NADA)
			{
				if(sobrepasa == 0)	// pieza interpuesta.
					return 0;
				if(color != (tipo & NEGRA))
				{
					tipo &= 0x7;
					if((tipo == REINA) || (tipo == ALFIL))
						return 1;	// pieza amenaza.
					return 0;		// pieza interpuesta.
				}
				return 0;	// pieza interpuesta.
			}
		}
		return 0; //No se encuentra amenaza.
	}
	return 0;
}

// Funciones para determinar el origen de una pieza que se mueve.
// si se trata de un rey el origen es la posicion actual anotada en la estructura.
uint8_t reymovorg(uint8_t color,TABLERO_t *tab)
{
	if(color == NEGRA)
		return(tab->negras.rey);
	else
		return(tab->blancas.rey);
}

// Funcion para determinar la posicion origen del movimiento de una reina.
// tener en cuenta que puede haber hasta nueve reinas y pudiera darse la circunstancia
// de que hasta ocho puedan moverse a la posicion destino indicada. Hay que determinar
// cual es la unica que puede moverse a dicha posicion. Primero tiene que estar en la fila
// o columna indicadas si se proporcionan estas, luego tiene que estar en la misma fila, columna 
// o diagonal que el destino, no puede haber piezas interpuestas en su movimiento
// y por ultimo tiene que ser un movimiento legal, es decir no puede 
// provocar que su rey entre en jaque.
// orgx tiene valor distinto a -1 si en el movimiento se indica la columna origen.
// orgy tiene valor distinto a -1 si en el movimiento se indica la fila origen.
// la funcion retorna la posicion origen absoluta del movimiento.
uint8_t reinamovorg(uint8_t indest,uint8_t orgx,uint8_t orgy,uint8_t color,TABLERO_t *tab)
{
	PIEZAS_t *piezas;
	int i;
	int destx = indest%8;
	int desty = indest/8;
	int posible[10];
	int nposible = 0;
	int posjaque[10];
	int nposjaque = 0;
	
	if(color == NEGRA)
		piezas = &tab->negras;
	else
		piezas = &tab->blancas;
		
	if((orgx >= 0) && (orgx < 8)) // se proporciona columna.
	{
		if((orgy >= 0) && (orgy < 8)) // se proporciona fila.
			return(orgy*8 + orgx);
		else  // buscamos reina que esta en la misma columna.
		{
			for(i=0;i<piezas->nreina;i++)
			{
				if(piezas->reina[i]%8 == orgx)
				{
					int diffx = abs(destx - orgx);
					int diffy = abs(desty - piezas->reina[i]/8);
					
					if((diffx == 0) || (diffy == 0) || (diffx == diffy))
					{
						posible[nposible] = piezas->reina[i];
						nposible++;
					}
				}
			}
		}
	}
	else if((orgy >= 0) && (orgy < 8)) // se proporciona fila.
	{
		for(i=0;i<piezas->nreina;i++)
		{
			if(piezas->reina[i]/8 == orgy)
			{
				int diffx = abs(destx - piezas->reina[i]%8);
				int diffy = abs(desty - orgy);
				
				if((diffx == 0) || (diffy == 0) || (diffx == diffy))
				{
					posible[nposible] = piezas->reina[i];
					nposible++;
				}
			}
		}
	}
	else  // no se proporciona ni fila ni columna.
	{
		
		// tiene que estar en la misma columna o fila o diagonal del destino.
		for(i=0;i<piezas->nreina;i++)
		{
			int tmpx = piezas->reina[i]%8;
			int tmpy = piezas->reina[i]/8;
			int diffx = abs(destx - tmpx);
			int diffy = abs(desty - tmpy);
			
			if((diffx == 0) || (diffy == 0) || (diffx == diffy)) // posible torre hay que verificar que no hay piezas interpuestas.
			{
				posible[nposible] = piezas->reina[i];
				nposible++;
			}
		}
	}
	// eleccion entre las posibles.
	if(nposible == 0)	// pieza no encontrada.
		return -1;
	if(nposible == 1)	// solo uno posible.
		return(posible[0]);
	else  // varios posibles, hay que verificar piezas interpuestas.
	{
		for(i=0;i<nposible;i++)
		{
			int tmpx = posible[i]%8;
			int tmpy = posible[i]/8;
			int incx;
			int incy;
			int x,y;
			
			if(tmpx == destx)
			{
				incx = 0;
			}
			else if(tmpx > destx)
				incx = -1;
			else
				incx = 1;
			if(tmpy == desty)
				incy = 0;
			else if(tmpy > desty)
				incy = -1;
			else
				incy = 1;

			for(x=tmpx+incx,y=tmpy+incy;(x != destx) || (y != desty);x+=incx,y+=incy)
			{
				if(tab->tab[y*8+x] != NADA)
					break;
			}
			if((x == destx) && (y==desty))	// no ha encontrado piezas inter.
			{
				posjaque[nposjaque] = posible[i];
				nposjaque++;
			}
		}
		if(nposjaque == 0)
			return -1;
		if(nposjaque == 1)
			return(posjaque[0]);
		else   	// desambiguacion por jaque. simple, la primera que cumpla
		{
			for(i=0;i<nposjaque;i++)
			{
				if(posiblejaque(piezas->rey,posjaque[i],color,indest,tab) == 0)
					return(posjaque[i]);
			}
			return(posjaque[0]);	// si ninguna devolvemos la primera.
		}
	}
	return -1;	// pieza no encontrada !!!!!!!
}

// Funcion para determinar la posicion origen del movimiento de una torre.
// tener en cuenta que puede haber hasta diez torres y pudiera darse la circunstancia
// de que hasta cuatro puedan moverse a la posicion destino indicada. Hay que determinar
// cual es la unica que puede moverse a dicha posicion. Primero tiene que estar en la fila
// o columna indicadas si se proporcionan estas, luego tiene que estar en la misma fila o columna 
// que el destino, no puede haber piezas interpuestas en su movimiento y por ultimo tiene que 
// ser un movimiento legal, es decir no puede provocar que su rey entre en jaque.
// orgx tiene valor distinto a -1 si en el movimiento se indica la columna origen.
// orgy tiene valor distinto a -1 si en el movimiento se indica la fila origen.
// la funcion retorna la posicion origen absoluta del movimiento.
uint8_t torremovorg(uint8_t indest,uint8_t orgx,uint8_t orgy,uint8_t color,TABLERO_t *tab)
{
	PIEZAS_t *piezas;
	int i;
	int destx = indest%8;
	int desty = indest/8;
	int posible[10];
	int nposible = 0;
	int posjaque[10];
	int nposjaque = 0;
	
	if(color == NEGRA)
		piezas = &tab->negras;
	else
		piezas = &tab->blancas;
		
	if((orgx >= 0) && (orgx < 8)) // se proporciona columna.
	{
		if((orgy >= 0) && (orgy < 8)) // se proporciona fila.
			return(orgy*8 + orgx);
		else  // buscamos torre que esta en la misma columna.
		{
			for(i=0;i<piezas->ntorre;i++)
			{
				if(piezas->torre[i]%8 == orgx) // ademas debe estar en la misma fila o columna que el destino.
				{
					if((destx == orgx) || (desty == piezas->torre[i]/8))
					{
						posible[nposible] = piezas->torre[i];
						nposible++;
					}
				}
			}
		}
	}
	else if((orgy >= 0) && (orgy < 8)) // se proporciona fila.
	{
		for(i=0;i<piezas->ntorre;i++)
		{
			if(piezas->torre[i]/8 == orgy)
			{
				if((desty == orgy) || (destx == piezas->torre[i]%8))
				{
					posible[nposible] = piezas->torre[i];
					nposible++;
				}
			}
		}
	}
	else  // no se proporciona ni fila ni columna.
	{
		
		// tiene que estar en la misma columna o fila del destino.
		for(i=0;i<piezas->ntorre;i++)
		{
			int tmpx = piezas->torre[i]%8;
			int tmpy = piezas->torre[i]/8;
			
			if((tmpx == destx) || (tmpy == desty)) // posible torre hay que verificar que no hay piezas interpuestas.
			{
				posible[nposible] = piezas->torre[i];
				nposible++;
			}
		}
	}
	// eleccion entre las posibles.
	if(nposible == 0)
		return -1;
	if(nposible == 1)	// solo uno posible.
		return(posible[0]);
	else  // varios posibles, hay que verificar piezas interpuestas.
	{
		for(i=0;i<nposible;i++)
		{
			int tmpx = posible[i]%8;
			int tmpy = posible[i]/8;
			int incx;
			int incy;
			int x,y;
			
			if(tmpx == destx)
			{
				incx = 0;
				if(tmpy > desty)
					incy = -1;
				else
					incy = 1;
			}
			else
			{
				incy = 0;
				if(tmpx > destx)
					incx = -1;
				else
					incx = 1;
			}
			for(x=tmpx+incx,y=tmpy+incy;(x != destx) || (y != desty);x+=incx,y+=incy)
			{
				if(tab->tab[y*8+x] != NADA)
					break;
			}
			if((x == destx) && (y==desty))	// no ha encontrado piezas inter.
			{
				posjaque[nposjaque] = posible[i];
				nposjaque++;
			}
		}
		if(nposjaque == 0)
			return -1;
		if(nposjaque == 1)
			return(posjaque[0]);
		else   	// desambiguacion por jaque. simple, la primera que cumpla
		{
			for(i=0;i<nposjaque;i++)
			{
				if(posiblejaque(piezas->rey,posjaque[i],color,indest,tab) == 0)
					return(posjaque[i]);
			}
			return(posjaque[0]);	// si ninguna devolvemos la primera.
		}
	}
	return -1;	// pieza no encontrada !!!!!!!
}

// Funcion para determinar la posicion origen del movimiento de alfil.
// tener en cuenta que puede haber hasta diez alfiles y pudiera darse la circunstancia
// de que hasta cuatro puedan moverse a la posicion destino indicada. Hay que determinar
// cual es el unico que puede moverse a dicha posicion. Primero tiene que estar en la fila
// o columna indicadas si se proporcionan estas, luego tiene que estar en una 
// diagonal del destino, no puede haber piezas interpuestas en su movimiento
// y por ultimo tiene que ser un movimiento legal, es decir no puede 
// provocar que su rey entre en jaque.
// orgx tiene valor distinto a -1 si en el movimiento se indica la columna origen.
// orgy tiene valor distinto a -1 si en el movimiento se indica la fila origen.
// la funcion retorna la posicion origen absoluta del movimiento.
uint8_t alfilmovorg(uint8_t indest,uint8_t orgx,uint8_t orgy,uint8_t color,TABLERO_t *tab)
{
	PIEZAS_t *piezas;
	int i;
	int destx = indest%8;
	int desty = indest/8;
	int posible[10];
	int nposible = 0;
	int posjaque[10];
	int nposjaque = 0;
	
	if(color == NEGRA)
		piezas = &tab->negras;
	else
		piezas = &tab->blancas;
		
	if((orgx >= 0) && (orgx < 8)) // se proporciona columna.
	{
		if((orgy >= 0) && (orgy < 8)) // se proporciona fila.
			return(orgy*8 + orgx);
		else  // buscamos alfil que esta en la misma columna.
		{
			for(i=0;i<piezas->nalfil;i++)
			{
				if(piezas->alfil[i]%8 == orgx)
				{
					int diffx = abs(destx - orgx);
					int diffy = abs(desty - piezas->alfil[i]/8);
					
					if(diffx == diffy)
					{
						posible[nposible] = piezas->alfil[i];
						nposible++;
					}
				}
			}
		}
	}
	else if((orgy >= 0) && (orgy < 8)) // se proporciona fila.
	{
		for(i=0;i<piezas->nalfil;i++)
		{
			if(piezas->alfil[i]/8 == orgy)
			{
				int diffx = abs(destx - piezas->alfil[i]%8);
				int diffy = abs(desty - orgy);
					
				if(diffx == diffy)
				{
					posible[nposible] = piezas->alfil[i];
					nposible++;
				}
			}
		}
	}
	else  // no se proporciona ni fila ni columna.
	{		
		// tiene que estar en una diagonal del destino.
		for(i=0;i<piezas->nalfil;i++)
		{
			int tmpx = piezas->alfil[i]%8;
			int tmpy = piezas->alfil[i]/8;
			int diffx = abs(tmpx - destx);
			int diffy = abs(tmpy - desty);
			
			if(diffx == diffy) // posible alfil hay que verificar que no hay piezas interpuestas.
			{
				posible[nposible] = piezas->alfil[i];
				nposible++;
			}
		}
	}
	// eleccion entre las posibles.
	if(nposible == 0)
		return -1;
	if(nposible == 1)	// solo uno posible.
		return(posible[0]);
	else  // varios posibles, hay que verificar piezas interpuestas y ambiguedad jaque
	{
		for(i=0;i<nposible;i++)
		{
			int tmpx = posible[i]%8;
			int tmpy = posible[i]/8;
			int incx;
			int incy;
			int x,y;
			
			if(tmpx > destx)
				incx = -1;
			else
				incx = 1;
			if(tmpy > desty)
				incy = -1;
			else
				incy = 1;
			for(x=tmpx+incx,y=tmpy+incy;x != destx;x+=incx,y+=incy)
			{
				if(tab->tab[y*8+x] != NADA)
					break;
			}
			if((x == destx) && (y == desty))	// no ha encontrado piezas inter en la diagonal.
			{
				posjaque[nposjaque] = posible[i];
				nposjaque++;
			}
		}
		if(nposjaque == 0)
			return -1;
		if(nposjaque == 1)
			return(posjaque[0]);
		else   	// desambiguacion por jaque. simple, la primera que cumpla
		{
			for(i=0;i<nposjaque;i++)
			{
				if(posiblejaque(piezas->rey,posjaque[i],color,indest,tab) == 0)
					return(posjaque[i]);
			}
			return(posjaque[0]);	// si ninguna devolvemos la primera.
		}
	}
	return -1;	// pieza no encontrada !!!!!!!
}

// Funcion para determinar la posicion origen del movimiento del caballo.
// tener en cuenta que puede haber hasta diez caballos y pudiera darse la circunstancia
// de que hasta ocho puedan moverse a la posicion destino indicada. Hay que determinar
// cual es el unico que puede moverse a dicha posicion. Primero tiene que estar en la fila
// o columna indicadas si se proporcionan estas, luego tiene que estar  
// a distancia de caballo y por ultimo tiene que ser un movimiento legal, es decir no puede 
// provocar que su rey entre en jaque.
// orgx tiene valor distinto a -1 si en el movimiento se indica la columna origen.
// orgy tiene valor distinto a -1 si en el movimiento se indica la fila origen.
// la funcion retorna la posicion origen absoluta del movimiento.
uint8_t caballomovorg(uint8_t indest,uint8_t orgx,uint8_t orgy,uint8_t color,TABLERO_t *tab)
{
	PIEZAS_t *piezas;
	int i;
	int destx,desty;
	int posjaque[10];
	int nposjaque = 0;
	
	destx = indest % 8;
	desty = indest/8;
	
	if(color == NEGRA)
		piezas = &tab->negras;
	else
		piezas = &tab->blancas;
		
	if((orgx >= 0) && (orgx < 8)) // se proporciona columna.
	{
		if((orgy >= 0) && (orgy < 8)) // se proporciona fila.
			return(orgy*8 + orgx);
		else  // buscamos caballo que esta en la misma columna y puede acceder a destino.
		{
			for(i=0;i<piezas->ncaballo;i++)
			{
				if(piezas->caballo[i]%8 == orgx)
				{
					int filcab = piezas->caballo[i]/8;
					int diffx = abs(orgx - destx);
					int diffy = abs(filcab - desty);
					
					if((diffx == 1) && (diffy == 2))
						return(piezas->caballo[i]);
					if((diffx == 2) && (diffy == 1))
						return(piezas->caballo[i]);
				}
			}
		}
	}
	else if((orgy >= 0) && (orgy < 8)) // se proporciona fila.
	{
		for(i=0;i<piezas->ncaballo;i++)
		{
			if(piezas->caballo[i]/8 == orgy)
			{
				int colcab = piezas->caballo[i]%8;
				int diffx = abs(colcab - destx);
				int diffy = abs(orgy - desty);
				
				if((diffx == 1) && (diffy == 2))
					return(piezas->caballo[i]);
				if((diffx == 2) && (diffy == 1))
					return(piezas->caballo[i]);
			}
		}
	}
	else  // no se proporciona ni fila ni columna.
	{
		for(i=0;i<piezas->ncaballo;i++)
		{
			int diffx = abs((piezas->caballo[i]%8) - destx);
			int diffy = abs((piezas->caballo[i]/8) - desty);
			
			// puede haber ambiguedad por jaque
			if((diffx == 1) && (diffy == 2))
			{
				posjaque[nposjaque] = piezas->caballo[i];
				nposjaque++;
			}
			else if((diffx == 2) && (diffy == 1))
			{
				posjaque[nposjaque] = piezas->caballo[i];
				nposjaque++;
			}
		}
		if(nposjaque == 0)
			return -1;
		if(nposjaque == 1)
			return(posjaque[0]);
		for(i=0;i<nposjaque;i++)
		{
			if(posiblejaque(piezas->rey,posjaque[i],color,indest,tab) == 0)
				return(posjaque[i]);
		}
		return(posjaque[0]);	// si ninguna devolvemos la primera.
	}
	return -1;
}

// Funcion para determinar la posicion origen del movimiento un peon.
// si hay una comida puede haber hasta dos peones que pueden moverse al mismo destino.
// si no hay comida solo puede haber uno.
// orgx tiene valor distinto a -1 si en el movimiento se indica la columna origen.
// orgy tiene valor distinto a -1 si en el movimiento se indica la fila origen.
// la funcion retorna la posicion origen absoluta del movimiento.
uint8_t peonmovorg(uint8_t indest,uint8_t orgx,uint8_t come,uint8_t color,TABLERO_t *tab)
{
	PIEZAS_t *piezas;
	int posjaque[10];
	int nposjaque = 0;
	int i;
	
	// peon come, se mueve uno en diagonal.
	if(come)
	{
		if(color == NEGRA)
		{
			// la fila sera una menos que la fila destino.
			if((orgx >=0) && (orgx<8)) // se proporciona columna origen
				return(((indest/8)-1)*8 + orgx);
			else 
			{
				// no se proporciona columna origen, 2 posibilidades.
				// excepto que el destino sea un extremo (a,h) entonces
				// solo hay una posibilidad.
				if((indest%8) == 0)
					return(indest-7);
				if((indest%8) == 7)
					return(indest-9);
				// puede haber ambiguedad....
				if(tab->tab[indest -9] == PEON | NEGRA)
				{
					posjaque[nposjaque] = indest -9;
					nposjaque++;
				}
				else if(tab->tab[indest -7] == PEON | NEGRA)
				{
					posjaque[nposjaque] = indest -7;
					nposjaque++;
				}
			}
		}
		else  // blancas.
		{
			// la fila sera una mas que la fila destino.
			if((orgx >=0) && (orgx<8)) // se proporciona columna origen
				return(((indest/8)+1)*8 + orgx);
			else  // no se proporciona columna origen, 2 posibilidades.
			{
				// no se proporciona columna origen, 2 posibilidades.
				// excepto que el destino sea un extremo (a,h) entonces
				// solo hay una posibilidad.
				if((indest%8) == 0)
					return(indest+9);
				if((indest%8) == 7)
					return(indest+7);
				// puede haber ambiguedad.
				if(tab->tab[indest +9] == PEON)
				{
					posjaque[nposjaque] = indest +9;
					nposjaque++;
				}
				else if(tab->tab[indest +7] == PEON)
				{
					posjaque[nposjaque] = indest +7;
					nposjaque++;
				}
			}
		}
	}
	// peon no come, se mantiene en columna y estara 1 o dos posiciones
	// de distancia
	else 
	{
		if(color == NEGRA)
		{
			if(tab->tab[indest -8] == (PEON | NEGRA))
				return(indest -8);
			else
				return(indest -16);
		}
		else
		{
			if(tab->tab[indest +8] == PEON)
				return(indest +8);
			else
				return(indest +16);	
		}
	}
	if(nposjaque == 0)
		return -1;
	if(nposjaque == 1)
		return(posjaque[0]);
		
	if(color == NEGRA)
		piezas = &tab->negras;
	else
		piezas = &tab->blancas;
		
	for(i=0;i<nposjaque;i++)
	{
		if(posiblejaque(piezas->rey,posjaque[i],color,indest,tab) == 0)
			return(posjaque[i]);
	}
	return(posjaque[0]);
}

// calcula tipo, origen y destino del movimiento y mueve pieza.
void muevePieza(uint8_t *pgn, uint8_t color,int come,MOV_t *mov,TABLERO_t *tab)
{
	uint8_t orgx;
	uint8_t orgy;
	uint8_t pieza;
	int len;
	
	len = strlen(pgn);
	if((len > 5) || (len < 2)) // movimiento invalido.
	{
		debug("MOV-INVAL",4,mov,tab);
		mov->pieza = INVAL;
		return;
	}
	switch(pgn[0])
	{
		case 'K':
			pieza = REY | color;
			pgn++;
			break;
		case 'Q':
			pieza = REINA | color;
			pgn++;
			break;
		case 'R':
			pieza = TORRE | color;
			pgn++;
			break;
		case 'B':
			pieza = ALFIL | color;
			pgn++;
			break;
		case 'N':
			pieza = CABALLO | color;
			pgn++;
			break;
		default:
			pieza = PEON | color;
			len++;
			break;
	}
	mov->pieza = pieza;
	len--;
	switch(len)
	{
		case 2:
			mov->dest = transform(pgn[0],pgn[1]);
			orgx = -1;
			orgy = -1;
			break;
		case 3:
			mov->dest = transform(pgn[1],pgn[2]);
			if(pgn[0] >='a')
			{
				orgx = pgn[0] -'a';
				orgy = -1;
			}
			else
			{
				orgx = -1;
				orgy = 8+ '0'-pgn[0];
			}
			break;
		default:
			mov->dest = transform(pgn[2],pgn[3]);
			mov->org = transform(pgn[0],pgn[1]);
			orgx = mov->org%8;
			orgy = mov->org/8;
			break;
	}

	switch(pieza & 0x7)	// pieza sin color.
	{
		case REY :
			mov->org = reymovorg(pieza & NEGRA,tab);
			break;
		case REINA:
			mov->org = reinamovorg(mov->dest,orgx,orgy,pieza & 0x8,tab);
			break;
		case TORRE:
			mov->org = torremovorg(mov->dest,orgx,orgy,pieza & 0x8,tab);
			break;
		case ALFIL:
			mov->org = alfilmovorg(mov->dest,orgx,orgy,pieza & 0x8,tab);
			break;
		case CABALLO:
			mov->org = caballomovorg(mov->dest,orgx,orgy,pieza & 0x8,tab);
			break;
		default:
			mov->org = peonmovorg(mov->dest,orgx,come,pieza & 0x8,tab);
			break;
	}
	if(mov->org == -1)
	{
		debug("deterORG inval",5,mov,tab);
	}
	mueve(mov,tab);
}

// funcion que realiza un enrroque largo.
void enroquel(TABLERO_t *tab,uint8_t color)
{
	int i;
	MOVBIN_t movimiento;
	
	if(color == NEGRA)
	{
		movimiento.piezaorg = TORRE | NEGRA;
		movimiento.piezadest = TORRE | NEGRA;
		movimiento.origen = 0;
		movimiento.destino = 3;
		anadeMov(movimiento);
		movimiento.piezaorg = REY | NEGRA;
		movimiento.piezadest = REY | NEGRA;
		movimiento.origen = 4;
		movimiento.destino = 2;
		anadeMov(movimiento);
		
		tab->tab[4] = NADA;
		tab->tab[2] = REY | NEGRA;
		tab->tab[0] = NADA;
		tab->tab[3] = TORRE | NEGRA;
		tab->negras.rey = 2;
		for(i=0;i<tab->negras.ntorre;i++)
		{
			if(tab->negras.torre[i] == 0)
			{
				tab->negras.torre[i] = 3;
				return;
			}
		}
	}
	else
	{
		movimiento.piezaorg = TORRE;
		movimiento.piezadest = TORRE;
		movimiento.origen = 56;
		movimiento.destino = 59;
		anadeMov(movimiento);
		movimiento.piezaorg = REY;
		movimiento.piezadest = REY;
		movimiento.origen = 60;
		movimiento.destino = 58;
		anadeMov(movimiento);
		
		tab->tab[60] = NADA;
		tab->tab[58] = REY;
		tab->tab[56] = NADA;
		tab->tab[59] = TORRE;
		tab->blancas.rey = 58;
		for(i=0;i<tab->blancas.ntorre;i++)
		{
			if(tab->blancas.torre[i] == 56)
			{
				tab->blancas.torre[i] = 59;
				return;
			}
		}
	}
}

// funcion que realiza un enroque corto.
void enroquec(TABLERO_t *tab,uint8_t color)
{
	int i;
	MOVBIN_t movimiento;
	
	if(color == NEGRA)
	{
		movimiento.piezaorg = TORRE | NEGRA;
		movimiento.piezadest = TORRE | NEGRA;
		movimiento.origen = 7;
		movimiento.destino = 5;
		anadeMov(movimiento);
		movimiento.piezaorg = REY | NEGRA;
		movimiento.piezadest = REY | NEGRA;
		movimiento.origen = 4;
		movimiento.destino = 6;
		anadeMov(movimiento);
		
		tab->tab[4] = NADA;
		tab->tab[6] = REY | NEGRA;
		tab->tab[7] = NADA;
		tab->tab[5] = TORRE | NEGRA;
		tab->negras.rey = 6;
		for(i=0;i<tab->negras.ntorre;i++)
		{
			if(tab->negras.torre[i] == 7)
			{
				tab->negras.torre[i] = 5;
				return;
			}
		}
	}
	else
	{
		movimiento.piezaorg = TORRE;
		movimiento.piezadest = TORRE;
		movimiento.origen = 63;
		movimiento.destino = 61;
		anadeMov(movimiento);
		movimiento.piezaorg = REY;
		movimiento.piezadest = REY;
		movimiento.origen = 60;
		movimiento.destino = 62;
		anadeMov(movimiento);
		
		tab->tab[60] = NADA;
		tab->tab[62] = REY;
		tab->tab[63] = NADA;
		tab->tab[61] = TORRE;
		tab->blancas.rey = 62;
		for(i=0;i<tab->blancas.ntorre;i++)
		{
			if(tab->blancas.torre[i] == 63)
			{
				tab->blancas.torre[i] = 61;
				return;
			}
		}
	}
}

// funcion de promocion de pieza. El peon ya ha movido
// solo sustituir el peon en mov->dest por pieza de promocion mov->pieza.
void promoPieza(MOV_t *mov,TABLERO_t *tab)
{
	int i;
	PIEZAS_t *piezas;
	MOVBIN_t movimiento;
	
	cabpartida.nmov--; // anulamos ultimo movimiento anotado.
	movimiento.piezaorg = PEON | (mov->pieza & NEGRA);
	movimiento.piezadest = mov->pieza;
	movimiento.origen = mov->org;
	movimiento.destino = mov->dest;
	anadeMov(movimiento);
	
	// sustituir pieza en el tablero.
	tab->tab[mov->dest] = mov->pieza;
	// dar de baja peon involucrado.
	if(mov->pieza & NEGRA)
		piezas = &tab->negras;
	else
		piezas = &tab->blancas;
		
	for(i=0;i<piezas->npeon;i++)
	{
		if(piezas->peon[i] == mov->dest)
			break;
	}
	for(i++;i<piezas->npeon;i++)
	{
		piezas->peon[i-1] = piezas->peon[i];
	}
	piezas->npeon--;
	
	// damos de alta la pieza sustituida.	
	switch(mov->pieza & 0x7)
	{
		case REINA:
			piezas->reina[piezas->nreina] = mov->dest;
			piezas->nreina++;
			break;
		case TORRE:
			piezas->torre[piezas->ntorre] = mov->dest;
			piezas->ntorre++;
			break;
		case ALFIL:
			piezas->alfil[piezas->nalfil] = mov->dest;
			piezas->nalfil++;
			break;
		case CABALLO:
			piezas->caballo[piezas->ncaballo] = mov->dest;
			piezas->ncaballo++;
			break;
		default:
			debug("Promociona pieza invalida",6,mov,tab);
	}
}


// analiza string de movimiento y procesa movimientos especiales
// como enrroque y promocion.
// filtra signos especiales y efectua movimiento de la pieza.
void determov(uint8_t *pgn, uint8_t color,MOV_t *mov,TABLERO_t *tab)
{
	uint8_t pgntmp[20],pgntmp1[20];
	int come;
	int pieza;
	int i,j;
	
	// procesamiento de enrroque.
	if(strchr(pgn,'O') != NULL)
	{
		if(strcmp(pgn,"O-O-O") == 0)
			enroquel(tab,color);
		else
			enroquec(tab,color);
		return;
	}
	// anotacion pieza comida y supresion indicacion ('x')de string.
	if(strchr(pgn,'x') != NULL)
	{
		come = 1;
		for(i=0,j=0;pgn[i] != 0;i++)
		{
			if(pgn[i] != 'x')
			{
				pgntmp[j] = pgn[i];
				j++;
			}
		}
		pgntmp[j] = 0;
	}
	else
	{
		come = 0;
		strcpy(pgntmp,pgn);
	}
	// procesamiento promocion.
	if(strchr(pgn,'=') != NULL)
	{
		for(i=0;i<strlen(pgntmp);i++)
		{
			if(pgntmp[i] == '=')
			{
				pgntmp1[i] = 0;
				break;
			}
			else
				pgntmp1[i] = pgntmp[i];
		}
		switch(pgntmp[i+1])
		{
			case 'Q':
			pieza = REINA | color;
			break;
		case 'R':
			pieza = TORRE | color;
			break;
		case 'B':
			pieza = ALFIL | color;
			break;
		case 'N':
			pieza = CABALLO | color;
			break;
		default:
			debug("Determov pieza invalida",7,mov,tab);
		}
		muevePieza(pgntmp1,color,come,mov,tab);
		mov->pieza = pieza;
		promoPieza(mov,tab);
		return;
	}
	muevePieza(pgntmp,color,come,mov,tab);
}



void cargaTab(TABLERO_t *tab,uint8_t *descripcion)
{
	int i,j;
	int linea = 0;
	int colum = 0;
	
	// carga tablero con descripciones.
	for(i=0;i<strlen(descripcion);i++)
	{
		switch(descripcion[i])
		{
			case 'K':
				tab->tab[linea*8+colum] = REY | NEGRA;
				colum++;
				break;
			case 'k':
				tab->tab[linea*8+colum] = REY;
				colum++;
				break;
			case 'Q':
				tab->tab[linea*8+colum] = REINA | NEGRA;
				colum++;
				break;
			case 'q':
				tab->tab[linea*8+colum] = REINA;
				colum++;
				break;
			case 'B':
				tab->tab[linea*8+colum] = ALFIL | NEGRA;
				colum++;
				break;
			case 'b':
				tab->tab[linea*8+colum] = ALFIL;
				colum++;
				break;
			case 'N':
				tab->tab[linea*8+colum] = CABALLO | NEGRA;
				colum++;
				break;
			case 'n':
				tab->tab[linea*8+colum] = CABALLO;
				colum++;
				break;
			case 'R':
				tab->tab[linea*8+colum] = TORRE | NEGRA;
				colum++;
				break;
			case 'r':
				tab->tab[linea*8+colum] = TORRE;
				colum++;
				break;
			case 'P':
				tab->tab[linea*8+colum] = PEON | NEGRA;
				colum++;
				break;
			case 'p':
				tab->tab[linea*8+colum] = PEON;
				colum++;
				break;
			case '/':
				linea++;
				colum = 0;
				break;
			default:
				for(j=0;j<(descripcion[i] - '0');j++)
				{
					tab->tab[linea*8+colum] = NADA;
					colum++;
				}
				break;
		}
	}
	// elabora listas de blancas y negras a partir de tablero.
	tab->blancas.nreina = 0;
	tab->blancas.ntorre = 0;
	tab->blancas.ncaballo = 0;
	tab->blancas.nalfil = 0;
	tab->blancas.npeon = 0;
	
	tab->negras.nreina = 0;
	tab->negras.ntorre = 0;
	tab->negras.ncaballo = 0;
	tab->negras.nalfil = 0;
	tab->negras.npeon = 0;
	
	for(i=0;i<64;i++)
	{
		PIEZAS_t *piezas;
		
		if(tab->tab[i] & NEGRA)
			piezas = &tab->negras;
		else
			piezas = &tab->blancas;
		
		switch(tab->tab[i] & 0x7)
		{
			case REY :
				piezas->rey = i;
				break;
			case REINA :
				piezas->reina[piezas->nreina] = i;
				piezas->nreina++;
				break;
			case TORRE :
				piezas->torre[piezas->ntorre] = i;
				piezas->ntorre++;
				break;
			case ALFIL :
				piezas->alfil[piezas->nalfil] = i;
				piezas->nalfil++;
				break;
			case CABALLO :
				piezas->caballo[piezas->ncaballo] = i;
				piezas->ncaballo++;
				break;
			case PEON :
				piezas->peon[piezas->npeon] = i;
				piezas->npeon++;
				break;
			default:
				break;
		}
	}
}



void kill_handler(int sig)
{
	fprintf(stderr,"\nMata..\n");
	if(transpend)
	{
		endTrans();
		transpend = 0;
	}
	fprintf(stderr,"Final Trans\n");
	if(baseopen)
		desconectaSqlite();
		baseopen = 0;
	fprintf(stderr,"Base closed\n");
	fprintf(stderr,"\nAbortado====Partidas=>%d\n",partidas);
	exit(1);
}

void exit_handler(void)
{

	fprintf(stderr,"\nSalida..\n");
	kill_handler(0);
}

void main(int argc, char *argv[])
{
	TABLERO_t tablero;
	MOV_t	mov;
	uint8_t *linea;
	uint8_t *lineain;
	int color;
	int fileid;
	uint64_t  movimientos = 0;
	char *punte,*punte1,*punte2;
	int elo1,elo2;
	int estado;
	int filas;
	int nivelpar;
	time_t tiempo;

	if(argc != 3)
	{
		fprintf(stderr,"Usage: %s <base> <fileid> < fileorg\n",argv[0]);
		exit(1);
	}
	signal(SIGINT,kill_handler);
	atexit(exit_handler);
	fileid = atoi(argv[2]);
	conectaSqlite(argv[1]);
	baseopen = 1;
	partidas = 0;
	beginTrans();
	transpend = 1;
	filas = 0;
	linea = (uint8_t *)malloc(100*1024);
	lineain = (uint8_t *)malloc(100*1024);

	tiempo = TIEMPO;
	
	while(fgets(lineain,100*1024,stdin) != NULL)
	{
		if(memcmp(lineain,"[Event ",7) == 0)
		{
			partidas++;

			iniciaJuego(&tablero);
			iniPart();
			while(fgets(lineain,100*1024,stdin) != NULL)
			{
				if(memcmp(lineain,"[Event ",7) == 0)
				{
					elo1 = 0;
					elo2 = 0;
					
					partidas++;
				}
				if(lineain[0] != '1')	// interpretamos meta-datos.
				{
					if(memcmp(lineain,"[WhiteElo",9) == 0)
					{
						punte = strchr(lineain,'"');
						elo1 = atoi(punte+1);
					}
					else if(memcmp(lineain,"[BlackElo",9) == 0)
					{
						punte = strchr(lineain,'"');
						elo2 = atoi(punte+1);
					}
					else if(memcmp(lineain,"[Result",7) == 0)
					{
						if(strstr(lineain,"1-0") != NULL)
						{
							cabpartida.flags.ganablanca = 1;
							cabpartida.flags.gananegra = 0;
						}
						else if(strstr(lineain,"0-1") != NULL)
						{
							cabpartida.flags.ganablanca = 0;
							cabpartida.flags.gananegra = 1;
						}
						else if(strstr(lineain,"1/2") != NULL)
						{
							cabpartida.flags.ganablanca = 1;
							cabpartida.flags.gananegra = 1;
						}
					}
					else if(memcmp(lineain,"[Termination",7) == 0)
					{
						if(strstr(lineain,"Normal") != NULL)
							cabpartida.flags.tnormal = 1;
					}
					continue;
				}
				// Linea de movimientos..
				cabpartida.ind = partidas;
				cabpartida.elomed = (elo1+elo2)/2;

				// fusionamos lineas hasta encontrar linea en blanco.
				punte = lineain;
				while(1)
				{
					if(strlen(punte) < 5) // linea en blanco finaliza mov.
						break;
					// limpiamos terminadores.
					punte = strchr(punte,'\n');
					if(punte == NULL)
						break;
					while((*punte == '\n') || (*punte == '\r'))
					{
						*punte = 0;
						punte--;
					}
					if(*punte != ' ')	// si no termina en blanco la terminamos en blanco.
					{
						punte++;
						*punte = ' ';
					}
					punte++;
					if(fgets(punte,100*1024,stdin) == NULL)
						break;
				}
				
				// filtramos comentarios y anotaciones '{' y '('. copiando
				// datos de lineain a linea.
				// los parentesis pueden tener anidamiento. 
				
				// acelerador si no hay parentesis ni $
				if(strpbrk(lineain,"()$") == NULL)
				{
					punte = lineain;
					linea[0] = 0;
					while((punte1 = strchr(punte,'{')) != NULL)
					{
						*punte1 = 0;
						strcat(linea,punte);
						punte = punte1;
						punte1 = strchr(punte1+1,'}');
						if(punte1 != NULL)
						{
							punte = punte1+1;
							continue;
						}
						else
							break;
					}
					strcat(linea,punte);
					punte1 = &linea[strlen(linea)];
				}
				else  		// hay parentesis o $ metodo largo.
				{
					for(punte=lineain,punte1=linea,estado = 0; *punte != 0;punte++)
					{
						switch(estado)
						{
							case 0:	// estado normal.
								if(*punte == '(')	// comienza zona '('.
								{
									estado = 1;
									nivelpar = 1;
									break;
								}
								if(*punte == '{')	// comienza zona '{'.
								{
									estado = 2;
									break;
								}
								if(*punte == '$')	// comienza zona '$'.
								{
									estado = 3;
									break;
								}
								*punte1 = *punte;
								punte1++;
								break;
							case 1:	// zona '(' esperamos cierre ')'.
								if(*punte == ')')	// final zona '('.
								{
									nivelpar--;
									if(nivelpar == 0)
										estado = 0;
									break;
								}
								if(*punte == '(')	// comienza nueva zona '('.
								{
									nivelpar++;
									break;
								}
								break;
							case 2:	// zona '{' esperamos cierre '}'.
								if(*punte == '}')	// final zona '{'.
								{
									estado = 0;
									break;
								}
								break;
							case 3: // zona '$' esperamos cierre ' '.
								if(*punte == ' ')	// final zona '$'.
								{
									estado = 0;
									break;
								}
								break;
							default:
								fprintf(stderr,"Estado invalido\n");
								exit(3);
						}
					}
				}
				*punte1 = 0;
					
				punte1 = linea;
				while(1)
				{
					punte = punte1;
					// al final punte1 debe apuntar al siguiente movimiento
					// tmp contiene el movimiento actual.
					// punte contiene el principio del movimiento y punte2 el final.
					if((*punte1 >= '0') && (*punte1 <= '9'))	// indicacion de numero de movimiento.
					{
						punte1 = strchr(punte,' ');	// blanco separa numero de movimiento.
						if(punte1 == NULL)
							break;
						*punte1 = 0;
						npgn = atoi(punte);
						if(strstr(punte,"...") != NULL)
							color = NEGRA;
						else
							color = 0;
						punte1++;
						while(*punte1 == ' ') punte1++;
						punte = punte1;		// principio del movimiento.
					}
					else  		// No hay numero de movimiento, juegan negras.
					{
						color = NEGRA;
					}
					punte1 = strchr(punte1,' ');	// siguiente blanco final mov.
					if(punte1 == NULL)
						break;
					*punte1 = 0;
					punte2 = punte1;		// punte2 final movimiento.
					punte1++;
					while(*punte1 == ' ') punte1++; 	// principio siguiente movimiento.
					// filtrado de anotaciones al final del movimiento.
					for(punte2 = punte2-1;punte2 != punte;punte2--)
					{
						if((*punte2 >= '0') && (*punte2 <= '9'))
							break;
						if((*punte2 == 'O') || (*punte2 == 'Q') || (*punte2 == 'R') | (*punte2 == 'N') || (*punte2 == 'B'))
							break;
						*punte2 = 0;
					}
					if(punte2 == punte)
						break;
					strcpy(gpgna,gpgn);
					strcpy(gpgn,punte);
					movimientos++;

					determov(gpgn,color,&mov,&tablero);

				//	if(TIEMPO != tiempo)
					{
						printf("ind=>%d, mov=>%ju          \r",partidas,movimientos);
						fflush(stdout);
				//		tiempo = TIEMPO;
					}
					punte = punte1;
				}
				break;
			}
			vuelcaPart(fileid);
			filas++;
			if(filas >= 100000)
			{
				endTrans();
				transpend = 0;
				filas = 0;
				beginTrans();
				transpend = 1;
			}
		if(partidas >= 1000000)
			break;
		//	vuelcaPart(fd);
		//	showtab(&tablero);
		}
	}
	endTrans();
	transpend = 0;
	fprintf(stderr,"\nFINAL====Partidas=>%d, MOV=>%ju\n",partidas,movimientos);
	desconectaSqlite();
	baseopen = 0;
//	close(fd);
	printf("\n");
	exit(0);
}
