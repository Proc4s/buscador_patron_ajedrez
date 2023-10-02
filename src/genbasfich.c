// modulo : genbasfich.c
// autor  : Antonio Pardo Redondo
//
// este programa covierte la información de una base de partidas de ajedrez descritas en formato PNG 
// a un formato binario mas tratable por medios informaticos.
// Cada partida en la base PGN esta descrita por una serie de metadatos con mucha informacion 
// sobre el lugar, fecha, contendientes, clasificación de los contendientes (puntos ELO) ganador
// terminacion etc y una linea que describe cada uno de los movimientos efectuados junto con anotaciones
// sobre cronometros y otras cosas.
// se trata de recoger solo la informacion que nos interesa de los metadatos como el elo medio de los
// contendientes, ganador de la partida, terminacion de esta e indice de la partida en el ficheo original PGN.
// El tratamiento mas critico se produce en el analisis y codificacion de los movimientos de la partida.
// La notacion PGN codifica el movimiento con el minimo numero de caracteres posible especificando en 
// muchos casos solo el tipo de pieza que se mueve y su destino, el origen del movimiento solo puede ser determinado
// si hay varias piezas con posibilidades, aplicando las reglas del ajedrez, analizando segun la forma de
// moverse cada pieza cuales tienen la posibilidad de moverse a la posicion destino, descartando las que
// tienen piezas interpuestas en su camino (escepto caballos) y las que al moverse dejan en jaque a su rey.
// 
// La idea es determinar en esta fase la posicion origen del movimiento, codificando en binario la pieza
// que se mueve, su posicion origen y su posición destino. En la explotacion de esta base se trata de recrear
// la partida movimiento a movimiento en un tablero virtual para comprobar la existencia de algun patron.
// La recreacion del movimiento con esta anotacion es inmediata, basta con vaciar la casilla origen y colocar
// en la casilla destino la pieza movida (salvo comida al paso).
//
// Ademas al codificar las cosas en binario compactamos la informacion y evitamos utilizar continuamente
// funciones para extraer la posicion numerica a partir de un string ASCII. Todo ello acelera considerablemente
// la recreacion informatica de una partida.
//
// En la notacion PGN el tablero se ve con las piezas blancas en la parte inferior y las negras en la superior.
// las filas se denotan con un numero (1:8) comenzando en la fila inferior y las columnas con una letra (a:h)
// comenzando en la columna de la izquierda.
// Las piezas en PGN se nombran por sus iniciales en ingles: K=rey, Q=reina, R=torre, N=caballo, B=alfil, P=peon.
// aunque los peones no se suelen indicar. Posibles anotaciones serian:
//
// Ra4 => una torre se mueve a la posicion 'a4'.
// Raa4 => la torre que esta en la columna 'a' se mueve a la posicion 'a4'
// R4a4 => la torre que esta en la fila 4 se mueve a posicion 'a4'.
// Rc4a4 => La torre que esta en 'c4' se mueve a la posicion 'a4.
// a4 => un peon se mueve a la posicion 'a4'.
// O-O => enroque corto.
// O-O-O => enroque largo.
// Rxa4 => una torre come al peon situado en 'a4'.
// a8=R => el peon se mueve a 'a8' y promociona a torre.
//
// En la base binaria las posiciones son absolutas (0:63) coincidiendo el '0' con 'a8' y 63 con 'h1'.
// de la posicion absoluta se puede deducir la fila = posicion/8. Y la columna como posicion%8. en nuestro
// sistema la dila cero es la 8 del PGN y la columna cero es la 'a' del PGN.
//
// En las diversas funciones de este proceso tambien se utilizan las posiciones absolutas indicadas.
//
// La informacion de salida se graba en un fichero indexado.
//
// La información de entrada se divide en particiones que constan de un maximo de un millon de partidas. En explotación
// cada partición se enviara a un proceso de tratamiento distinto. Se crea un fichero  indice con los campos de fileID
// y partición para poder hacer busquedas y selecciones por este concepto. Al final de la generacion este fichero esta
// ordenado por estos conceptos.
//
// Se crea otro fichero de indices (partidas) con un indice por cada partida que contiene los campos ELO-medio y ganador
// Estos indices se ordenan por cada particon por estos dos conceptos.
//
// El conjunto de los dos ficheros de indices permite buscar y seleccionar una partida por fileid, particion, ELOmed y ganador.
//
// El fichero de indices de particion indica por cada particion el offset en el fichero indice de partida donde
// comienzan las partidas de esta particion  y la longitud de esta.
//
// El fichero de indices de partidas indica el offset en el fichero de datos donde comienza cada partida y su longitud.
//
// El fichero de datos contiene en binario por cada partida la cabecera de partida y su lista de movimientos.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include "ajedrez.h"
#include "basfichdrv.h"


#define MAGIC	0x55AA
	
int particion;							// N. particion actual.
int particionant;						// N. Particion anterior.
PARTFICH_t particioncur;			// datos particion en curso

// Registro de partida. Consiste en una cabecera de partida seguida por un array de movimientos
// El numero de movimientos en el array esta indicado en cabpartida.nmov.
CPARTIDA_t	cabpartida;
MOVBIN_t		movimientos[MAXMOV];

int partidas = 0;		// Indice de partida en curso.
int npgn;				// numero de movimiento en partida.
char gpgn[100];		// texto movimiento
char gpgna[100];		// texto movimiento anterior.

// inicia las estructuras de partida binaria.
void iniPart(void)
{
	memset(&cabpartida,0,sizeof(cabpartida));
	cabpartida.magic = MAGIC;
}

// funcion para anhadir un movimiento a la lista de movimientos recodificados
// para el registro de la base binaria.
void anadeMov(MOVBIN_t mov)
{
	movimientos[cabpartida.nmov] = mov;
	cabpartida.nmov++;
}

// funcion para volcar al fichero indexado la partida en curso ya recodificada.
void vuelcaPart(BASFICH_t *bd,int fileid)
{
	PARTIDA_t partmp;
	off_t offtmp;
	
	if(particion != particionant)	// Cambio de particion
	{
		offtmp = lseek(bd->fdpartidas,0,SEEK_END);
		if(particionant >= 0)		// No primera particion
		{
			particioncur.len = offtmp - particioncur.offset;	// Anota longitud particion
			anhadeParticion(bd,&particioncur);						// anhade particion
			ordenaPartidas(bd,fileid,particioncur.particion);	// ordena partidas de esta particion
		}
		// inicia nueva particion.
		particioncur.fileid = fileid;
		particioncur.particion = particion;
		particioncur.offset = offtmp;
		particionant = particion;
	}
	// codifica ganador partida.
	partmp.ganador = 0;
	if(cabpartida.flags.ganablanca)
	{
		if(cabpartida.flags.gananegra)
			partmp.ganador = 3;
		else
			partmp.ganador = 1;
	}
	else
		partmp.ganador = 2;
	// Rellena datos indice partida actual.
	partmp.elomed = cabpartida.elomed;
	partmp.flags = 0;
	partmp.offset = lseek(bd->fddata,0,SEEK_END);
	// Escribe datos partida y movimientos.
	if(write(bd->fddata,&cabpartida,sizeof(cabpartida)) != sizeof(cabpartida))
	{
		fprintf(stderr,"Fallo escritura cabpartida\n");
		exit(4);
	}
	if(write(bd->fddata,movimientos,sizeof(MOVBIN_t) * cabpartida.nmov) != (sizeof(MOVBIN_t) * cabpartida.nmov))
	{
		fprintf(stderr,"Fallo escritura movimientos\n");
		exit(4);
	}
	anhadePartida(bd,&partmp);	// Anhade partida a indices partidas.
}

// Funcion para mostrar trazas de debug.
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
	memcpy(tab->tab,tablaini,sizeof(tablaini));
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
	
	// determina color.
	if(pieza & NEGRA)
		piezas = &tab->negras;
	else
		piezas = &tab->blancas;
		
	// Modifica la lista correspondiente a la pieza eliminada.
	switch(pieza & 0x7)
	{
		case REINA:
			// localiza la pieza a eliminar en la lista.
			for(i=0;i<piezas->nreina;i++)
			{
				if(piezas->reina[i] == posicion)
					break;
			}
			// la elimina desplazando el resto de piezas de la lista una posicion a la izqu.
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
	tab->tab[mov->org] = NADA;			// La pieza desaparece de la posicion origen
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
	tab->tab[mov->dest] = mov->pieza; // pone pieza en la casilla destino.
}

// Actualiza las estructuras y tablero virtual por un movimiento de rey.
void mueveRey(MOV_t *mov,TABLERO_t *tab)
{
	PIEZAS_t *piezas;
	
	// determina la estructura a actualizar en funcion del color de la pieza.
	if(mov->pieza & NEGRA)
		piezas = &tab->negras;
	else
		piezas = &tab->blancas;	
	
	piezas->rey = mov->dest;	// modifica la posicion del rey.
	actTablero(mov,tab);			// actualiza tablero virtual.
}

// Actualiza las estructuras y tablero virtual por un movimiento de reina.
void mueveReina(MOV_t *mov,TABLERO_t *tab)
{
	int i;
	PIEZAS_t *piezas;
	
	// determina la estructura a actualizar en funcion del color de la pieza.
	if(mov->pieza & NEGRA)
		piezas = &tab->negras;
	else
		piezas = &tab->blancas;		

	// localiza la pieza que se mueve
	for(i=0;i<piezas->nreina;i++)
	{
		if(piezas->reina[i] == mov->org)
		{
			piezas->reina[i] = mov->dest;	// actualiza posicion de la pieza
			break;
		}
	}
	if(i == piezas->nreina)
		debug("No encontrado reina ORG",3,mov,tab);
	actTablero(mov,tab);	// actualiza tablero virtual.
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
	MOVBIN_t movimiento;	// Estructura binaria de movimiento.
	
	// actualiza movimiento binario.
	movimiento.piezaorg = mov->pieza;
	movimiento.piezadest = mov->pieza;
	movimiento.origen = mov->org;
	movimiento.destino = mov->dest;
	anadeMov(movimiento);	// anhade movimiento binario a la lista de movimientos de salida.
	
	// mueve pieza en estructuras de piezas activas y tablero virtual.
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

// determina si el movimiento de la pieza indicada puede hacer entrar a su rey en jaque
// lo que indica que esta pieza no puede ser el origen del movimiento.
// se utiliza para resolver la ambiguedad cuando dos piezas pueden ser el origen del
// movimiento pero una de ellas lo tiene impedido por que pondria a su rey en jaque.
//
// rey => pos rey,pieza=> pos pieza, color=> color pieza, indest=> destino de la pieza.
// Retorna cero si no y uno si el movimiento provoca jaque.
int posiblejaque(int rey,int pieza,int color,int indest,TABLERO_t *tab)
{
	int diffx,diffy;
	int fila,colum,filpieza,columpieza;
	int i,j;
	int tipo;
	int incx;
	int incy;
	// posicion destino pieza que se mueve.
	int destx = indest%8;
	int desty = indest/8;
	
	int sobrepasa;
	
	// determina fila y columna en que se encuentra el rey
	if((npgn == 15) && (color > 0))
		i = 3;
	colum = rey % 8;
	fila = rey/8;
	
	// determina fila y columna en la que se encuentra la pieza.
	columpieza = pieza % 8;
	filpieza = pieza/8;
	
	// si la pieza que se mueve y el rey estan en la misma fila y el destino
	// 	de la pieza es tambien la misma fila, no hay cambio de amenaza.
	// 	si el destino es otra fila y hay una pieza interpuesta tampoco supone amenaza.
	//		si hay una pieza detras de la que se mueve (en la misma fila) y es de distinto
	//		color que el rey y se trata de una torre o una reyna hay amenaza.
	if(fila == filpieza) // pieza y rey en la misma fila.
	{
		if(fila == desty) // si la pieza se mueve en la misma fila no genera nueva amenaza.
			return 0;
		if(colum > columpieza)	// El rey se encuentra a la derecha de la pieza
			incx = -1;
		else
			incx = 1;
		sobrepasa = 0;
		
		// examinamos las posiciones desde el rey en direccion a la pieza
		// dentro de los limites del tablero.
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
	
	// si la pieza que se mueve y el rey estan en la misma columna y el destino
	// 	de la pieza es tambien la misma columna, no hay cambio de amenaza.
	// 	si el destino es otra columna y hay una pieza interpuesta tampoco supone amenaza.
	//		si hay una pieza detras de la que se mueve (en la misma columna) y es de distinto
	//		color que el rey y se trata de una torre o una reyna hay amenaza.
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
	
	// si la pieza que se mueve y el rey estan en la misma diagonal y el destino
	// 	de la pieza es tambien la misma diagonal, no hay cambio de amenaza.
	// 	si el destino es fuera de la diagonal y hay una pieza interpuesta tampoco supone amenaza.
	//		si hay una pieza detras de la que se mueve (en la misma diagonal) y es de distinto
	//		color que el rey y se trata de un alfil o una reyna hay amenaza.
	if(diffx == diffy)	// pieza en diagonal al rey
	{
		// determina incx e incy para moverse por la diagonal, segun la diagonal que se trate.
		incx = colum - columpieza;
		incy = fila - filpieza;
		
		// el destino esta tambien en una diagonal.
		if(abs(colum -destx) == abs(fila -desty))
		{
			if(incx > 0)
			{
				if((colum -destx) > 0)
				{
					if(incy > 0)
					{
						if((fila - desty) > 0)
							return 0;	// pieza se mueve en la misma diagonal, no hay cambio de amenaza
					}
					else
					{
						if((fila - desty) < 0)
							return 0;	// pieza se mueve en la misma diagonal, no hay cambio de amenaza
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
							return 0;	// pieza se mueve en la misma diagonal, no hay cambio de amenaza
					}
					else
					{
						if((fila - desty) < 0)
							return 0;	// pieza se mueve en la misma diagonal, no hay cambio de amenaza
					}
				}
			}
		}
		
		//  calculo de los incrementos para seguir sentido de las diagonales.
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
		// recorremos la diagonal desde el rey en el sentido de la pieza.
		for(i=fila+incy,j=colum+incx;((i>=0) && (i<8) && (j>=0) && (j<8));i+=incy,j+=incx)
		{
			if((i==filpieza) && (j == columpieza))
			{
				sobrepasa = 1;	// se alcanza la pieza.
				continue;
			}
			tipo = tab->tab[i*8+j]; // pieza en la casilla considerada
			if(tipo != NADA)			// hay una pieza
			{
				if(sobrepasa == 0)	// pieza interpuesta.
					return 0;			// no hay amenaza por que hay una pieza interpuesta.
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
	return 0;	// la pieza que se mueve no esta ni en fila ni en columna ni en diagonal al rey => no supone amenaza.
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
	int destx = indest%8;	// columna destino
	int desty = indest/8;	// fila destino.
	int posible[10];		// lista de posibles candidatas.
	int nposible = 0;		// numero de posibles candidatas.
	int posjaque[10];		// lista posibles tras comprobacion jaque
	int nposjaque = 0;	// numero de posibles tras comprobar jaque.
	
	// segun el color selecciona las estructuras de posicion a utilizar.
	if(color == NEGRA)
		piezas = &tab->negras;
	else
		piezas = &tab->blancas;
		
	if((orgx >= 0) && (orgx < 8)) // se proporciona columna.
	{
		if((orgy >= 0) && (orgy < 8)) // se proporciona fila.
			return(orgy*8 + orgx); 		// la orden de movimiento incluye el origen del mismo.
		else  // buscamos reina que esta en la misma columna.
		{
			for(i=0;i<piezas->nreina;i++)
			{
				if(piezas->reina[i]%8 == orgx)
				{
					int diffx = abs(destx - orgx);
					int diffy = abs(desty - piezas->reina[i]/8);
					
					if((diffx == 0) || (diffy == 0) || (diffx == diffy)) // reina en misma columna, anotamos como posible
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
				
				if((diffx == 0) || (diffy == 0) || (diffx == diffy)) // reina en la misma fila, anotamos como posible
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
		// y no tiene que haber piezas interpuestas entre origen y destino.
		for(i=0;i<piezas->nreina;i++)
		{
			int tmpx = piezas->reina[i]%8;	// columna de la reina que consideramos.
			int tmpy = piezas->reina[i]/8;	// fila de la reina que consideramos.
			int diffx = abs(destx - tmpx);	// diferencia de columna entre la reina considerada y el destino.
			int diffy = abs(desty - tmpy);	// diferencia de fila entre la reina considerada y el destino.
			
			// posible reina pues esta en la misma fila o columna o en una diagonal
			// despues hay que verificar que no hay piezas interpuestas.
			if((diffx == 0) || (diffy == 0) || (diffx == diffy)) // anotamos como posible.
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
			int tmpx = posible[i]%8; // columna pieza ensayada
			int tmpy = posible[i]/8; // fila pieza ensayada
			int incx;
			int incy;
			int x,y;
			
			// determinamos el sentido de los incrementos para movernos hacia el destino.
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
			if((x == destx) && (y==desty))	// no ha encontrado piezas inter. la anotamos para verificar que no provoca jaque de su rey al moverse
			{
				posjaque[nposjaque] = posible[i];
				nposjaque++;
			}
		}
		if(nposjaque == 0)
			return -1;		// no se encuentra ninguna con posibilidades.
		if(nposjaque == 1)
			return(posjaque[0]);	// encontrada solo una como posible.
		else   	// desambiguacion por jaque. simple, la primera que cumpla
		{
			for(i=0;i<nposjaque;i++)
			{
				if(posiblejaque(piezas->rey,posjaque[i],color,indest,tab) == 0) // pieza no incurre en jaque de su rey al moverse.
					return(posjaque[i]);
			}
			return(posjaque[0]);	// si ninguna devolvemos la primera. NO DEBERIA OCURRIR
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
	int destx = indest%8;	// columna destino
	int desty = indest/8;	// fila destino
	int posible[10];			// lista de posibles piezas origen.
	int nposible = 0;			// numero de posibles piezas origen.
	int posjaque[10];			// lista posibles piezas a comprobar jaque propio
	int nposjaque = 0;		// numero de piezas a comprobar jaque propio.
	
	
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
						posible[nposible] = piezas->torre[i];	// anotamos posible candidata.
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
				if((desty == orgy) || (destx == piezas->torre[i]%8)) // misma fila, anotamos posible candidata.
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
			int tmpx = piezas->torre[i]%8;   // columna de la candidata
			int tmpy = piezas->torre[i]/8;	// fila de la candidata.
			
			if((tmpx == destx) || (tmpy == desty)) // posible torre hay que verificar que no hay piezas interpuestas.
			{
				posible[nposible] = piezas->torre[i];	// anotamos posible candidata.
				nposible++;
			}
		}
	}
	// eleccion entre las posibles.
	if(nposible == 0)
		return -1;	// no se ha encontrado ninguna posible candidata.
	if(nposible == 1)	// solo uno posible.
		return(posible[0]);
	else  // varios posibles, hay que verificar piezas interpuestas.
	{
		for(i=0;i<nposible;i++)
		{
			int tmpx = posible[i]%8;	// columna de la candidata
			int tmpy = posible[i]/8;	// fila de la candidata.
			int incx;
			int incy;
			int x,y;
			
			// determinamos los incrementos a aplicar para movernos desde el destino a la pieza a ensayar.
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
			// nos movemos desde el destino a la pieza ensayada buscando posibles piezas interpuestas.
			for(x=tmpx+incx,y=tmpy+incy;(x != destx) || (y != desty);x+=incx,y+=incy)
			{
				if(tab->tab[y*8+x] != NADA)
					break;
			}
			if((x == destx) && (y==desty))	// no ha encontrado piezas inter.
			{
				posjaque[nposjaque] = posible[i];	// anotamos como posible para comprobar mov ilegal por jaque.
				nposjaque++;
			}
		}
		if(nposjaque == 0)	// no se ha encontrado ninguna pieza posible.
			return -1;
		if(nposjaque == 1)	// se ha encontrado solo una posible.
			return(posjaque[0]);
		else   	// desambiguacion por jaque. simple, la primera que cumpla
		{
			for(i=0;i<nposjaque;i++)
			{
				if(posiblejaque(piezas->rey,posjaque[i],color,indest,tab) == 0) // no provoca jaque propio al moverse
					return(posjaque[i]);
			}
			return(posjaque[0]);	// si ninguna devolvemos la primera. NO DEBERIA OCURRIR
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
			return(posjaque[0]);	// si ninguna devolvemos la primera. NO ES POSIBLE
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
	if(nposjaque == 0) // ninguno cumple condiciones
		return -1;
	if(nposjaque == 1) // solo uno cumple condiciones.
		return(posjaque[0]);
		
	if(color == NEGRA)
		piezas = &tab->negras;
	else
		piezas = &tab->blancas;
	
	// comprobacion de que el movimiento es legal		
	for(i=0;i<nposjaque;i++)
	{
		if(posiblejaque(piezas->rey,posjaque[i],color,indest,tab) == 0)
			return(posjaque[i]);
	}
	return(posjaque[0]);
}

// calcula tipo, origen y destino del movimiento y mueve pieza.
// pgn => string de movimiento.
// color => color del movimiento.
// come => indicacion de que este movimiento come una pieza.
// mv => movimiento decodificado resultado de la funcion.
// tab => situacion actual de las piezas.
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
	switch(pgn[0])  // El primer caracter indica el tipo de pieza, excepto en peon
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
		case 2:	// solo se indica posicion destino
			mov->dest = transform(pgn[0],pgn[1]);
			orgx = -1;
			orgy = -1;
			break;
		case 3:	// se indica fila o columna destino
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
		default: // se indica origen y destino
			mov->dest = transform(pgn[2],pgn[3]);
			mov->org = transform(pgn[0],pgn[1]);
			orgx = mov->org%8;
			orgy = mov->org/8;
			break;
	}
	// calculamos el origen en funcion de la pieza, destino, color e indicaciones de origen
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
	if(mov->org == -1)	// no se ha encontrado origen valido
	{
		debug("deterORG inval",5,mov,tab);
	}
	mueve(mov,tab);	// mueve la pieza.
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
		movimiento.piezaorg = REY ;
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
	// anotamos movimiento sustitucion
	movimiento.piezaorg = (PEON | (mov->pieza & NEGRA));
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
		
	// Eliminacion del peon.	
	for(i=0;i<piezas->npeon;i++)	// localizamos peon involucrado
	{
		if(piezas->peon[i] == mov->dest)
			break;
	}
	for(i++;i<piezas->npeon;i++)	// desplazamos el resto de peones de la lista
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
		muevePieza(pgntmp1,color,come,mov,tab); // mueve peon
		mov->pieza = pieza;
		promoPieza(mov,tab);	// promociona peon
		return;
	}
	muevePieza(pgntmp,color,come,mov,tab);
}


void main(int argc, char *argv[])
{
	TABLERO_t tablero;
	MOV_t	mov;
	uint8_t *linea;
	uint8_t *lineain;
	int color;
	uint64_t  movimientos = 0;
	char *punte,*punte1,*punte2;
	int elo1,elo2;
	int fileid;
	int npartida;
	off_t offtmp;
	BASFICH_t bd;
	int estado;
	int nivelpar;

	// la invocacion es con el nombre del comando, el path a la carpeta del fichero
	// indexado de salida y el identificador de fichero origen (fecha=>yyyy*12+mm).
	// la entrada de datos se supone que se efectua por STDIN que proviene de una PIPE.
	// ejemplo:
	//  zstdcat file_png.zst | ./genbasfich base_fich 24277
	if(argc != 3)
	{
		fprintf(stderr,"Usage: %s <fichbas> <fileid> < fileorg\n",argv[0]);
		exit(1);
	}
	// apertura del fichero indexado para escritura.
	if(basfichOpenW(argv[1],&bd) == 0)
	{
		perror("Falla open fichbase");
		exit(2);
	}
	fileid = atoi(argv[2]);
	particionant = -1;
	particion = 0;
	partidas = 0;
	npartida = 0;
	linea = (uint8_t *)malloc(100*1024);
	lineain = (uint8_t *)malloc(100*1024);
	
	// Los datos de entrada se reciben en lineain. Una vez se detecta las lineas
	// de movimiento, se filtran y se copian a linea.
	// al final, en linea tenemos una unica linea de movimientos filtrada.
	while(fgets(lineain,100*1024,stdin) != NULL)	// lectura linea a linea hasta que no haya mas.
	{
		if(memcmp(lineain,"[Event ",7) == 0)	// linea comienzo de partida.
		{
			partidas++;

			iniciaJuego(&tablero);	// inicia las estructuras del tablero virtual
			iniPart();					// inicia las estructuras binarias de salida.
			while(fgets(lineain,100*1024,stdin) != NULL)	// sigue leyendo lineas.
			{
				if(memcmp(lineain,"[Event ",7) == 0)	// comienzo de nueva partida.
				{
					elo1 = 0;
					elo2 = 0;
					
					partidas++;
				}
				if(lineain[0] != '1')	//no se trata de la linea de movimientos.interpretamos meta-datos.
				{
					if(memcmp(lineain,"[WhiteElo",9) == 0)	// linea de ELO de blancas.
					{
						punte = strchr(lineain,'"');	// el dato esta entre comillas.
						elo1 = atoi(punte+1);
					}
					else if(memcmp(lineain,"[BlackElo",9) == 0)	// linea de ELO de negras.
					{
						punte = strchr(lineain,'"'); // el dato esta entre comillas.
						elo2 = atoi(punte+1);
					}
					else if(memcmp(lineain,"[Result",7) == 0)	// linea de resultado de la partida.
					{
						if(strstr(lineain,"1-0") != NULL)	// ganan blancas.
						{
							cabpartida.flags.ganablanca = 1;
							cabpartida.flags.gananegra = 0;
						}
						else if(strstr(lineain,"0-1") != NULL)	// ganan negras.
						{
							cabpartida.flags.ganablanca = 0;
							cabpartida.flags.gananegra = 1;
						}
						else if(strstr(lineain,"1/2") != NULL)	// tablas, anotamos que ganan ambos.
						{
							cabpartida.flags.ganablanca = 1;
							cabpartida.flags.gananegra = 1;
						}
					}
					else if(memcmp(lineain,"[Termination",7) == 0)	// Terminacion de la partida.
					{
						if(strstr(lineain,"Normal") != NULL)	// teminacion normal
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
					if(fgets(punte,100*1024,stdin) == NULL) // leemos siguiente linea.
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
					while((punte1 = strchr(punte,'{')) != NULL)	// buscamos comienzo anotaciones
					{
						*punte1 = 0;	// termina el string en el comienzo de anotaciones.
						strcat(linea,punte);	// copiamos a linea filtrada.
						
						punte = punte1;
						punte1 = strchr(punte1+1,'}'); // buscamos final anotaciones
						if(punte1 != NULL)
						{
							punte = punte1+1;	// siguiente movimiento.
							continue;
						}
						else
							break; // no hay mas movimientos.
					}
					strcat(linea,punte); // copiamos cola.
					punte1 = &linea[strlen(linea)];	// apuntamos al final de la linea.
				}
				else  		// hay parentesis o $ metodo largo.
				{
					// recorremos la linea de entrada.
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
				
				// recorremos la linea filtrada movimiento a movimiento.
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
						npgn = atoi(punte);	// Numero de movimiento
						// determinamos color del movimiento.(blancas=>'.' Negras=>'...')
						if(strstr(punte,"...") != NULL)
							color = NEGRA;
						else
							color = 0;
						punte1++;
						// quitamos blancos anteriores al movimiento
						while(*punte1 == ' ') punte1++;
						punte = punte1;		// principio del movimiento.
					}
					else  		// No hay numero de movimiento(notacion Nm Mbl Mneg), juegan negras.
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
					// filtrado de anotaciones al final del movimiento. Simbolos de jaque y similares.
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
					strcpy(gpgna,gpgn);	// salvamos movimiento anterior (para debug).
					strcpy(gpgn,punte);	// copiamos movimiento actual filtrado.
					movimientos++;
					
					// determina totalmente movimiento, anota en tablero virtual y en salida.
					determov(gpgn,color,&mov,&tablero);
					
					// trazas de progreso.
					if((partidas%100)== 0)
					{
						printf("ind=>%d, mov=>%ju         \r",partidas,movimientos);
						fflush(stdout);
					}
					punte = punte1;
				}
				break;
			}
			npartida++;
			
			if(npartida >= 1000000)	// Nueva particion.
			{
				particion++;
				npartida = 0;
			}
			vuelcaPart(&bd,fileid);	// Vuelca partida traducida a fichero indexado.
		}
	}
	
	// anota longitud en indices de actual particion.
	offtmp = lseek(bd.fdpartidas,0,SEEK_END);
	particioncur.len = offtmp - particioncur.offset;
	anhadeParticion(&bd,&particioncur);	// anahade particion actual.
	ordenaPartidas(&bd,fileid,particioncur.particion);	// ordena partidas de particion actual.
	basfichClose(&bd);	// cierra fichero indexado.
	fprintf(stderr,"\nFINAL====Partidas=>%d, MOV=>%ju\n",partidas,movimientos);
//	close(fd);
	printf("\n");
}
