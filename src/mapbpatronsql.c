// modulo : mapbpatronsql.c
// autor  : Antonio Pardo Redondo
//
// Este modulo implementa la busqueda de un patron de ajedrez en una base
// de partidas de ajedrez implementada en SQLITE.
//
// El modulo esta pensado para funcionar como 'mapper' en un sistema
// 'haddop mapreduce de apache' en su modalidad streaming.
//
// por su entrada estandar recibe peticiones de procesamiento del motor
// de mapreduce de hadoop en forma de lineas de texto que especifican
// el 'fileid','particion' y numero de la base donde se encuentra, separados
// por ','.
//
// por otra parte recibe por la variable de entorno $PATHAJEDREZ el path
// del arbol de carpetas que utiliza el sistema de busqueda. Dentro del
// arbol destacan:
//		-base => Carpeta con los enlaces simbolicos a las carpetas que contienen
//			|		las bases de datos SQLITE.
//			|
//			|____ base_x => Enlace simbolico a cada una de las bases.
//
//		-bin =>	Carpeta que contiene los ejecutables del sistema de busqueda.
//
//		-conf => Carpeta que contiene los ficheros de configuracion del sistema de busqueda.
//			|
//			|____ base.conf => configuracion de las bases SQLITE, numero y nombre.
//			|
//			|____ job.conf => configuracion del trabajo. Especifica el patron a buscar
//									y los criterios de restriccion de la busqueda asi como
//									el canal de comunicacion del progreso de la busqueda.
//
//		-data => Carpeta que contiene copia del fichero de entrada al 'mapreduce' y
//			|		carpeta donde se depositara el resultado de la busqueda de los distintos 'mapper'.
//			|
//			|____ entrada.txt =>  fichero con la lista de particiones a procesar.
//			|
//			|____ salida => Carpeta donde se deposita el resultado de los 'mapper'.
//						|
//						|____	xxxx => Una carpeta por cada 'fileid' procesado.
//									|
//									|____ yyyy => Un fichero por cada particion que contiene
//														los patrones hallados en dicha particion.
//
//	El modulo determina por los ficheros de configuracion la arquitectura del sistema de bases
// el patron a buscar y los criterios de busqueda.
// genera la salida con los datos de cada partida que cumple el patron y envia por una FIFO
// (cola de mensajes posix) datos con la informacion del progreso de la busqueda.
//
// Esta pensado para funcionar sin proceso 'reduce'.
// Cada proceso de este tipo procesa una particion a la vez, de forma que no se solapan
// las busquedas.
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
#include <mqueue.h>
#include "ajedrez.h"
#include "funaux.h"
#include "config.h"
#include "sqlitedrv.h"

// datos de la partida en curso de analisis.
CPARTIDA_t	cabpartida;				// cabecera de partida.
MOVBIN_t		movimientos[MAXMOV];	// lista de movimientos.

PATRON_t patronbin;	// patron compilado a buscar.

// acelerador de busqueda con la mascara de posiciones del tablero de interes.
// y contenido que deben tener dichas posiciones.
// son aquellas que deben tener un determinado contenido. Es lo mas facil de comprobar.
uint8_t 	mascara[64];	// mascara del tablero virtual 
uint8_t	spatron[64];	// contenido que debe poseer las posiciones de interes.

char basmaster[1000];	// path de la base master de SQLITE (la que contiene tabla de particiones y partidas)
CONF_BAS_t confbase;		// configuracion de las bases SQLITE.
CONF_JOB_t confjob;		// configuracion del trabajo de busqueda a realizar.

char *pathajedrez = NULL;	// PATH del sistema de busqueda.

//================ SQLITE ============================================
// conexion base.
sqlite3 *db = NULL; 	//base
sqlite3_stmt *stmt;	// cursor del query en curso

//-----------------------------------------------------------
// Funcion que carga la descripcion del patron a buscar y genera la mascara
// y contenido de interes del tablero para acelerar la busqueda. 
void iniPatron(char *filepatbin)
{
	int fdpat,i;
	RELAPIEZA_t *rela;
	int res;
	
	// lee el fichero del patron compilado sobre la estructura de descripcion del patron.
	if((fdpat=open(filepatbin,O_RDONLY)) < 0)
	{
		perror("Filepatbin\n");
		exit(2);
	}
	res = read(fdpat,&patronbin,sizeof(patronbin));
	close(fdpat);
	// formamos mascara de aceleracion.
	memset(mascara,0,sizeof(mascara));
	memset(spatron,0,sizeof(spatron));
	
	for(i=0,rela = &patronbin.relaand.relaciones[0];i<patronbin.relaand.nelementos;i++,rela++)
	{
		// No interesan las posiciones TABOO.
		if(rela->pieza_tar == TABOO)
			continue;
		// Tampoco interesan las relaciones a casilla.
		if((rela->pieza_ataque != NADA) && (rela->pieza_tar == NADA))
			continue;
		// indicaciones de posicion de piezas y relaciones a piezas.
		mascara[rela->pos] = 0xff;
		spatron[rela->pos] = rela->pieza_tar;	
	}
}


// inicia partida.
// carga el tablero virtual con la situacion inicial de todas las piezas.
void iniciaJuego(uint8_t *tab)
{
	memcpy(tab,tablaini,sizeof(tablaini));
}

// Efectua un movimiento en el tablero virtual considerando una posible comida
// de peon al paso. En el resto de comidas no hay problema pues la pieza
// origen sustituye a la que se encuentra en la casilla destino.
void mueve(MOVBIN_t mov,uint8_t *tab)
{
	// se trata de un peon que se mueve en diagonal y la casilla destino esta vacia
	if(((mov.piezadest & 0x7) == PEON) && 			// peon come al paso
		((mov.origen %8) != (mov.destino %8)) &&
		(tab[mov.destino] != NADA))
	{
		// Eliminamos peon comido al paso.
		if(mov.piezadest & NEGRA)
			tab[mov.destino - 8] = NADA;
		else
			tab[mov.destino + 8] = NADA;
	}
	// movimiento propiamente dicho en el tablero.
	tab[mov.origen] = NADA;		// vaciamos la casilla origen
	tab[mov.destino] = mov.piezadest;	// sustituimos la casilla destino
}

// comprobacion de amenazas particulares.
//=======================================

// si existe la amenaza retorna 1, en caso contrario 0.

// hay un rey del color especificado en alguna de las 8 casillas
// que rodean a la ensayada.
int amenazaRey(uint8_t posicion,uint8_t color,uint8_t *tab)
{
	int x = posicion%8;
	int y = posicion/8;
	uint8_t pieza = REY | color;
	
	if((x<7) && (tab[posicion+1] == pieza))
		return 1;
	if((x>0) && (tab[posicion-1] == pieza))
		return 1;
	if((y<7) && (tab[posicion+8] == pieza))
		return 1;
	if((y>0) && (tab[posicion-8] == pieza))
		return 1;
	if((y<7) && (x<7) &&(tab[posicion+9] == pieza))
		return 1;
	if((y<7) && (x>0) && (tab[posicion+7] == pieza))
		return 1;
	if((y>0) && (x<7) && (tab[posicion-7] == pieza))
		return 1;
	if((y>0) && (x>0) && (tab[posicion-9] == pieza))
		return 1;
	return 0;	// no hay amenaza de rey.
}

// hay una reina del color especificado en la diagonal, fila o
// columna de la posicion ensayada sin piezas interpuestas de otro color.
// o del propio que no supongan amenaza.
int amenazaReina(uint8_t posicion,uint8_t color,uint8_t *tab)
{
	int i;
	int x = posicion%8;
	int y = posicion/8;
	int postmp;
	
	// Examinamos las cuatro diagonales.
	// diagonal en cuadrante positivo +x+y
	// *
	//  *
	for(i=1;i<8;i++)
	{
		if(((x+i) > 7) || ((y+i) > 7))
			break;  // salimos del tablero.
		postmp = (y+i)*8 + x+i;
		if(tab[postmp] == (REINA | color))
			return 1;	// hay una reina en esta diagonal.
		if(tab[postmp] != NADA)	// pieza interpuesta.
		{
			if(tab[postmp] == (ALFIL | color)) // interpuesta mismo col supone amenaza.
				continue;
			else
				break;
		}
	}
	// diagonal en cuadrante +x-y
	//  *
	// *
	for(i=1;i<8;i++)
	{
		if(((x+i) > 7) || ((y-i) < 0))
			break;  // salimos del tablero.
		postmp = (y-i)*8 + x+i;
		if(tab[postmp] == (REINA | color))
			return 1;	// hay una reina en esta diagonal.
		if(tab[postmp] != NADA)	// pieza interpuesta.
		{
			if(tab[postmp] == (ALFIL | color)) // interpuesta mismo col supone amenaza.
				continue;
			else
				break;
		}
	}
	// diagonal en cuadrante -x-y
	//  *
	// *
	for(i=1;i<8;i++)
	{
		if(((x-i) < 0) || ((y-i) < 0))
			break;  // salimos del tablero.
		postmp = (y-i)*8 + x-i;
		if(tab[postmp] == (REINA | color))
			return 1;	// hay una reina en esta diagonal.
		if(tab[postmp] != NADA)	// pieza interpuesta.
		{
			if(tab[postmp] == (ALFIL | color)) // interpuesta mismo col supone amenaza.
				continue;
			else
				break;
		}
	}
	// diagonal en cuadrante -x+y
	//  *
	// *
	for(i=1;i<8;i++)
	{
		if(((x-i) < 0) || ((y+i) > 7))
			break;  // salimos del tablero.
		postmp = (y+i)*8 + x-i;
		if(tab[postmp] == (REINA | color))
			return 1;	// hay una reina en esta diagonal.
		if(tab[postmp] != NADA)	// pieza interpuesta.
		{
			if(tab[postmp] == (ALFIL | color)) // interpuesta mismo col supone amenaza.
				continue;
			else
				break;
		}
	}
	
	// fila y columna en ambos sentidos.
	
	// fila en x+
	for(i=1;i<8;i++)
	{
		if((x+i) > 7)
			break;  // salimos del tablero.
		postmp = y*8 + x+i;
		if(tab[postmp] == (REINA | color))
			return 1;	// hay una reina en esta diagonal.
		if(tab[postmp] != NADA)	// pieza interpuesta.
		{
			if(tab[postmp] == (TORRE | color)) // interpuesta mismo col supone amenaza.
				continue;
			else
				break;
		}
	}
	
	// fila en x-
	for(i=1;i<8;i++)
	{
		if((x-i) < 0)
			break;  // salimos del tablero.
		postmp = y*8 + x-i;
		if(tab[postmp] == (REINA | color))
			return 1;	// hay una reina en esta diagonal.
		if(tab[postmp] != NADA)	// pieza interpuesta.
		{
			if(tab[postmp] == (TORRE | color)) // interpuesta mismo col supone amenaza.
				continue;
			else
				break;
		}
	}
	
	// columna en y+
	for(i=1;i<8;i++)
	{
		if((y+i) > 7)
			break;  // salimos del tablero.
		postmp = (y+i)*8 + x;
		if(tab[postmp] == (REINA | color))
			return 1;	// hay una reina en esta diagonal.
		if(tab[postmp] != NADA)	// pieza interpuesta.
		{
			if(tab[postmp] == (TORRE | color)) // interpuesta mismo col supone amenaza.
				continue;
			else
				break;
		}
	}
	
	// columna en y-
	for(i=1;i<8;i++)
	{
		if((y-i) < 0)
			break;  // salimos del tablero.
		postmp = (y-i)*8 + x;
		if(tab[postmp] == (REINA | color))
			return 1;	// hay una reina en esta diagonal.
		if(tab[postmp] != NADA)	// pieza interpuesta.
		{
			if(tab[postmp] == (TORRE | color)) // interpuesta mismo col supone amenaza.
				continue;
			else
				break;
		}
	}
	
	return 0;	// No se ha encontrado reina amenazante.
}

// hay un ALFIL del color especificado en la diagonal
// de la posicion ensayada sin piezas interpuestas de otro color,
// o del propio que no supongan amenaza.
int amenazaAlfil(uint8_t posicion,uint8_t color,uint8_t *tab)
{
	int i;
	int x = posicion%8;
	int y = posicion/8;
	int postmp;
	
	// Examinamos las cuatro diagonales.
	// diagonal en cuadrante positivo +x+y
	// *
	//  *
	for(i=1;i<8;i++)
	{
		if(((x+i) > 7) || ((y+i) > 7))
			break;  // salimos del tablero.
		postmp = (y+i)*8 + x+i;
		if(tab[postmp] == (ALFIL | color))
			return 1;	// hay un alfil en esta diagonal.
		if(tab[postmp] != NADA)	// pieza interpuesta.
		{
			if(tab[postmp] == (REINA | color)) // interpuesta mismo col supone amenaza.
				continue;
			else
				break;
		}
	}
	// diagonal en cuadrante +x-y
	//  *
	// *
	for(i=1;i<8;i++)
	{
		if(((x+i) > 7) || ((y-i) < 0))
			break;  // salimos del tablero.
		postmp = (y-i)*8 + x+i;
		if(tab[postmp] == (ALFIL | color))
			return 1;	// hay un alfil en esta diagonal.
		if(tab[postmp] != NADA)	// pieza interpuesta.
		{
			if(tab[postmp] == (REINA | color)) // interpuesta mismo col supone amenaza.
				continue;
			else
				break;
		}
	}
	// diagonal en cuadrante -x-y
	//  *
	// *
	for(i=1;i<8;i++)
	{
		if(((x-i) < 0) || ((y-i) < 0))
			break;  // salimos del tablero.
		postmp = (y-i)*8 + x-i;
		if(tab[postmp] == (ALFIL | color))
			return 1;	// hay un alfil en esta diagonal.
		if(tab[postmp] != NADA)	// pieza interpuesta.
		{
			if(tab[postmp] == (REINA | color)) // interpuesta mismo col supone amenaza.
				continue;
			else
				break;
		}
	}
	// diagonal en cuadrante -x+y
	//  *
	// *
	for(i=1;i<8;i++)
	{
		if(((x-i) < 0) || ((y+i) > 7))
			break;  // salimos del tablero.
		postmp = (y+i)*8 + x-i;
		if(tab[postmp] == (ALFIL | color))
			return 1;	// hay un alfil en esta diagonal.
		if(tab[postmp] != NADA)	// pieza interpuesta.
		{
			if(tab[postmp] == (REINA | color)) // interpuesta mismo col supone amenaza.
				continue;
			else
				break;
		}
	}
	return 0;	// No se ha encontrado alfil.
}

// hay una torre del color especificado en la misma fila o columna
// de la posicion ensayada sin piezas interpuestas de otro color.
// o del propio que no supongan amenaza.
int amenazaTorre(uint8_t posicion,uint8_t color,uint8_t *tab)
{
	int i;
	int x = posicion%8;
	int y = posicion/8;
	int postmp;
	
	// fila y columna en ambos sentidos.
	
	// fila en x+
	for(i=1;i<8;i++)
	{
		if((x+i) > 7)
			break;  // salimos del tablero.
		postmp = y*8 + x+i;
		if(tab[postmp] == (TORRE | color))
			return 1;	// hay una torre en esta fila.
		if(tab[postmp] != NADA)	// pieza interpuesta.
		{
			if(tab[postmp] == (REINA | color)) // interpuesta mismo col supone amenaza.
				continue;
			else
				break;
		}
	}
	
	// fila en x-
	for(i=1;i<8;i++)
	{
		if((x-i) < 0)
			break;  // salimos del tablero.
		postmp = y*8 + x-i;
		if(tab[postmp] == (TORRE | color))
			return 1;	// hay una torre en esta fila.
		if(tab[postmp] != NADA)	// pieza interpuesta.
		{
			if(tab[postmp] == (REINA | color)) // interpuesta mismo col supone amenaza.
				continue;
			else
				break;
		}
	}
	
	// columna en y+
	for(i=1;i<8;i++)
	{
		if((y+i) > 7)
			break;  // salimos del tablero.
		postmp = (y+i)*8 + x;
		if(tab[postmp] == (TORRE | color))
			return 1;	// hay una torre en esta fila.
		if(tab[postmp] != NADA)	// pieza interpuesta.
		{
			if(tab[postmp] == (REINA | color)) // interpuesta mismo col supone amenaza.
				continue;
			else
				break;
		}
	}
	
	// columna en y-
	for(i=1;i<8;i++)
	{
		if((y-i) < 0)
			break;  // salimos del tablero.
		postmp = (y-i)*8 + x;
		if(tab[postmp] == (TORRE | color))
			return 1;	// hay una torre en esta fila.
		if(tab[postmp] != NADA)	// pieza interpuesta.
		{
			if(tab[postmp] == (REINA | color)) // interpuesta mismo col supone amenaza.
				continue;
			else
				break;
		}
	}
	
	return 0;	// No se ha encontrado torre.
}

// hay un caballo del color especificado que puede alcanzar
// la posicion ensayada.
int amenazaCaballo(uint8_t posicion,uint8_t color,uint8_t *tab)
{
	int x = posicion%8;
	int y = posicion/8;
	uint8_t pieza = CABALLO | color;
	
	if((y>1) && (x<7) && (tab[posicion-15] == pieza)) // +1,-2
			return 1;
	if((y>0) && (x<6) && (tab[posicion-6] == pieza)) // +2,-1
			return 1;
	if((y<7) && (x<6) && (tab[posicion+10] == pieza)) // +2,+1
			return 1;
	if((y<6) && (x<7) && (tab[posicion+17] == pieza)) // +1,+2
			return 1;
	if((y<6) && (x>0) && (tab[posicion+15] == pieza)) // -1,+2
			return 1;
	if((y<7) && (x>1) && (tab[posicion+6] == pieza)) // -2,+1
			return 1;
	if((y>0) && (x>1) && (tab[posicion-10] == pieza)) // -2,-1
			return 1;
	if((y>1) && (x>0) && (tab[posicion-17] == pieza)) // -1,-2
			return 1;
	
	return 0;	// No hay caballo que pueda alcanzar la posicion.
}

// hay un peon del color especificado que puede alcanzar la posicion
// especificada.
// solo hay dos casillas posibles segun el color del peon.
int amenazaPeon(uint8_t posicion,uint8_t color,uint8_t *tab)
{
	int x = posicion%8;
	int y = posicion/8;
	uint8_t pieza = PEON | color;
	
	if(color) // negra=> y-1, x+-1
	{
		if((y>0) && (x<7) && (tab[posicion-7] == pieza))
			return 0;
		if((y>0) && (x>0) && (tab[posicion-9] == pieza))
			return 1;
	}
	else     	// blanca=> y+1,x+-1
	{
		if((y<7) && (x<7) && (tab[posicion+9] == pieza))
			return 1;
		if((y<7) && (x>0) && (tab[posicion+7] == pieza))
			return 1;
	}
	return 0;
}


// Funciones para verificar TABOO.
//================================
// una posicion es taboo si el rey de color indicado
// puede acceder a ella (distancia 1) pero la casilla esta ocupada
// por una pieza de su color o amenazada por alguna pieza contraria.
int veriTaboo(uint8_t posicion,uint8_t color,uint8_t *tab)
{
	int x = posicion%8;
	int y = posicion/8;
	uint8_t colamenaza;
	int postmp;
	int i;
	
	// Primero comprobamos si la casilla es accesible por el rey
	// del color indicado.
	if(amenazaRey(posicion,color,tab) == 0)
		return 0;	// El rey no puede alcanzar la casilla.
	// La casilla esta ocupada por una pieza del mismo color.
	if((tab[posicion] != NADA) && ((tab[posicion] & NEGRA) == color))
		return 1;	// taboo por casilla ocupada por pieza mismo color.
	
	// La menaza tiene que ser de color contrario.
	if(color)
		colamenaza = 0;
	else
		colamenaza = NEGRA;
	if(amenazaReina(posicion,colamenaza,tab))
		return 1;
	if(amenazaAlfil(posicion,colamenaza,tab))
		return 1;
	if(amenazaTorre(posicion,colamenaza,tab))
		return 1;
	if(amenazaCaballo(posicion,colamenaza,tab))
		return 1;
	if(amenazaPeon(posicion,colamenaza,tab))
		return 1;
	if(amenazaRey(posicion,colamenaza,tab))
		return 1;
	return 0;
}

//====================================================================
// Funcion para comprobar si el tablero virtual actual cumple con el patron
// especificado. Retorna '1' si cumple y '0' si no cumple.
int compruebaPatron(uint8_t color,uint8_t *tab)
{
	int i,j,res=0;
//	uint8_t pieza;
	uint32_t *ptab,*pmasc,*pspat;
	RELAPIEZA_t *rela;
	uint32_t tmp;
	uint8_t colortab;
	
	if(color == patronbin.color)	// no ha movido el color esperado por el patron.
		return 0;
		
	// comprobacion posiciones patron.
	// comprobamos mascara aceleracion.
	for(i=0,ptab=(uint32_t *)tab,pmasc=(uint32_t *)mascara,pspat = (uint32_t *)spatron;i<16;i++)
	{
		tmp = (*ptab & *pmasc)^*pspat;
		if(tmp)
		{
			return 0;	// no cumple mascara de aceleracion.
		}
		ptab++;
		pmasc++;
		pspat++;
	}
	// posiciones AND, las amenazas a posicion no deben tener una pieza del mismo color.
	for(i=0,rela = &patronbin.relaand.relaciones[0];i<patronbin.relaand.nelementos;i++,rela++)
	{
		if(rela->pieza_tar == TABOO)
			continue;
		if((rela->pieza_ataque != NADA) && (rela->pieza_tar == NADA) && (tab[rela->pos] != NADA) && ((tab[rela->pos] & NEGRA) == (rela->pieza_ataque & NEGRA)))
			return 0;	// No cumple amenazas AND.
	}
	// comprobacion posiciones OR, al menos debe cumplirse una por cada lista de OR
	// En las amenazas a posicion esta no deb tener una pieza del mismo color.
	for(i=0;i<patronbin.nrelaor;i++)
	{
		rela = &patronbin.relaor[i].relaciones[0];
		for(j=0;j<patronbin.relaor[i].nelementos;j++,rela++)
		{
			if(rela->pieza_tar == TABOO)
				break;
			if(rela->pieza_tar == tab[rela->pos])
				break;
			// amenaza aposicion.
			else if((rela->pieza_ataque != NADA) && (rela->pieza_tar == NADA) && (tab[rela->pos] != NADA) && ((tab[rela->pos] & NEGRA) != (rela->pieza_ataque & NEGRA)))
				break;
		}
		if(j == patronbin.relaor[i].nelementos)	// No verifica ninguna.
			return 0;
	}
	// verifica posiciones, comprobamos relaciones y TABOO.
	
	// comprobacion relaciones y TABOO  AND.
	for(i=0,rela = &patronbin.relaand.relaciones[0];i<patronbin.relaand.nelementos;i++,rela++)
	{
		if(rela->pieza_ataque != NADA)
		{
			switch(rela->pieza_ataque & 0x7)
			{
				case REY:
					if(amenazaRey(rela->pos,rela->pieza_ataque & NEGRA,tab) == 0)
						return 0;
					break;
				case REINA:
					if(amenazaReina(rela->pos,rela->pieza_ataque & NEGRA,tab) == 0)
						return 0;
					break;
				case TORRE:
					if(amenazaTorre(rela->pos,rela->pieza_ataque & NEGRA,tab) == 0)
						return 0;
					break;
				case ALFIL:
					if(amenazaAlfil(rela->pos,rela->pieza_ataque & NEGRA,tab) == 0)
						return 0;
					break;
				case CABALLO:
					if(amenazaCaballo(rela->pos,rela->pieza_ataque & NEGRA,tab) == 0)
						return 0;
					break;
				case PEON:
					if(amenazaPeon(rela->pos,rela->pieza_ataque & NEGRA,tab) == 0)
						return 0;
					break;
				default:
					return 0;
			}
		}
		else if(rela->pieza_tar == TABOO)
		{
			if(patronbin.color)
				colortab = 0;
			else
				colortab = NEGRA;
			if(veriTaboo(rela->pos,colortab,tab) == 0)
				return 0;
		}
	}
	
	// comprobacion relaciones y posiciones TABOO OR, aqui hay que verificar que alguna se cumpla	
	for(i=0;i<patronbin.nrelaor;i++)
	{
		rela = &patronbin.relaor[i].relaciones[0];
		res = 0;
		for(j=0;j<patronbin.relaor[i].nelementos;j++,rela++)
		{
			if(rela->pieza_ataque != NADA)
			{
				switch(rela->pieza_ataque & 0x7)
				{
					case REY:
						if(amenazaRey(rela->pos,rela->pieza_ataque & NEGRA,tab) != 0)
							res++;
						break;
					case REINA:
						if(amenazaReina(rela->pos,rela->pieza_ataque & NEGRA,tab) != 0)
							res++;
						break;
					case TORRE:
						if(amenazaTorre(rela->pos,rela->pieza_ataque & NEGRA,tab) != 0)
							res++;
						break;
					case ALFIL:
						if(amenazaAlfil(rela->pos,rela->pieza_ataque & NEGRA,tab) != 0)
							res++;
						break;
					case CABALLO:
						if(amenazaCaballo(rela->pos,rela->pieza_ataque & NEGRA,tab) != 0)
							res++;
						break;
					case PEON:
						if(amenazaPeon(rela->pos,rela->pieza_ataque & NEGRA,tab) != 0)
							res++;
						break;
					default:
						res=0;
				}
				if(res)
					break;
			}
			else if(rela->pieza_tar == TABOO)
			{
				if(patronbin.color)
					colortab = 0;
				else
					colortab = NEGRA;
				if(veriTaboo(rela->pos,colortab,tab) != 0)
					break;
			}
			else
			{
				if(rela->pieza_tar == tab[rela->pos])
					break;
			}
		}
		if(j == patronbin.relaor[i].nelementos)	// No verifica ninguna.
			return 0;
	}
	return 1;	// cumple patron.
}

// Funcion para traducir un movimiento a formato PGN.
char * mov2pgn(MOVBIN_t *mov) 
{
	int filaorg = 8 - mov->origen/8;
	int colorg = mov->origen%8;
	int filadest = 8 - mov->destino/8;
	int coldest = mov->destino%8;
	char pieza;
	static char tmp[20];
	
	switch(mov->piezaorg & 0x7)
	{
		case REY:
			pieza = 'K';
			break;
		case REINA:
			pieza = 'Q';
			break;
		case TORRE:
			pieza = 'R';
			break;
		case ALFIL:
			pieza = 'B';
			break;
		case CABALLO:
			pieza = 'N';
			break;
		default:
			pieza = ' ';
	}
	if(mov->piezaorg & NEGRA)
		sprintf(tmp,"... %c%c%c%c%c",pieza,colorg + 'a',filaorg+'0',coldest+'a',filadest+'0');
	else
		sprintf(tmp,". %c%c%c%c%c",pieza,colorg + 'a',filaorg+'0',coldest+'a',filadest+'0');
	return tmp;	
}

void main()
{
//	int fileid,partid,elomed,gana;
//	int partida,encontrados = 0;
	int ind;
	int incparticion;
	int incpartidas;
	int inchallados;
	char msg[1000];
	mqd_t fdmq;
	
	FILE *fdsal;
	MOVBIN_t *mov;
	PARTICION_t part;
	int i,len,nread;
	int movpartida;
	int hmov;
	int paso;
	CASTLING_t castling;
	int ultcolor;
	uint8_t tablero[64];
	clock_t slot;
	char linea[1000];
	
	// Cargamos el PATH al sistema de busqueda.
	if((pathajedrez=getenv("PATHAJEDREZ")) == NULL)
	{
		fprintf(stderr,"PATHAJEDREZ no definido\n");
		exit(1);
	}
	// cargamos configuracion base de datos.
	sprintf(linea,"%s/conf/base.conf",pathajedrez);
	if(getConfBase(linea,&confbase) == 0)
	{
		fprintf(stderr,"Configuracion Base invalida\n");
		fprintf(stderr,"PATH=>%s NUM=>%d, NOM=>%s\n",pathajedrez,confbase.numbases,confbase.nombase);
		exit(1);
	}
	sprintf(basmaster,"%s/base/base_0/%s",pathajedrez,confbase.nombase);
	confbase.basmaster = basmaster;
	
	// Cargamos configuracion de trabajo.
	if(getConfJob(pathajedrez,&confjob) == 0)
	{
		fprintf(stderr,"Configuracion JOB invalido\n");
		exit(1);
	}
//	printf("ELOMIN=>%d,ELOMAX=>%d,GANA=>%d,FORMA=>%d,PATRON=>%s\n",confjob.elomin,
//				confjob.elomax,confjob.ganador,confjob.formasal,confjob.patron);

//	fprintf(stderr,"Apertura msg\n");
	// abrimos canal de comunicacion de progreso.
	if((fdmq=mq_open(confjob.fifo,O_WRONLY | O_NONBLOCK)) == (mqd_t)-1)
	{
		perror("No puedo abrir FIFO");
		exit(1);
	}

	// Cargamos patron de busqueda.
	iniPatron(confjob.patron);
	ind = 0;
	
	// Leemos lineas con los datos de las particiones a tratar.
	// las lineas contienen: "fileid,particion,nbase\n"
	while(fgets(linea,1000,stdin) != NULL)
	{
		if(strlen(linea) < 4)	// linea invalida.
			continue;
		if(getParticion(&part,linea) == 0)
		{
			fprintf(stderr,"Particion invalida\n");
			continue;
		}
		// creamos fichero de resultados de esta particion con
		// posible creacion de la carpeta fileid si no existe.
		sprintf(linea,"%s/data/salida/%d",pathajedrez,part.fileid);
		if(mkdir(linea,0777) < 0)
		{
			if(errno != EEXIST)
			{
				perror(linea);
				continue;
			}
		}
		sprintf(linea,"%s/data/salida/%d/%d",pathajedrez,part.fileid,part.particion);
		if((fdsal = fopen(linea,"w")) == NULL)
		{
			perror(linea);
			continue;
		}
		
		// conectamos la base que contiene la particion a procesar.
		conectaSqlite(&db,getBasFromParticion(pathajedrez,&confbase,part.particion));
		// lanzamos QUERY con las restricciiones de la busqueda.
		lanzaQueryR(db,&stmt,part.fileid,part.particion,confjob.elomin,confjob.elomax,confjob.ganador);

		// indicaciones de progreso.
		slot = TIEMPO;
		incparticion = 1;	// una particion procesada
		incpartidas = 0;
		inchallados = 0;
		
		// iteramos por las partidas resultado del QUERY.
		while(nextPartida(db,stmt,&cabpartida,movimientos))
		{
			iniciaJuego(tablero);	// iniciamos tablero virtual.
			mov = (MOVBIN_t *)movimientos;
			// Iniciamos indicadores para FEN.
			ultcolor = NEGRA;
			movpartida = 0;
			hmov = 0;
			castling.reinaw = 1;
			castling.reyw = 1;
			castling.reinab = 1;
			castling.reyb = 1;
			paso = 0;
			incpartidas++;
			ind++;
			// iteramos por los movimientos de la partida.
			for(i=0;i<cabpartida.nmov;i++)
			{
				// actualizamos indicadores FEN.
				if((mov[i].piezadest & NEGRA) == 0)
				{
					if( ultcolor == NEGRA)
					{
						movpartida++;
						hmov++;
					}
				}
				else
				{
					if(ultcolor != NEGRA)
						hmov++;
				}
				ultcolor = mov[i].piezadest & NEGRA;
				if(confjob.formasal == 1)	// salida FEN
				{
					// mueve peon o come pieza.
					if((tablero[mov[i].destino] != NADA) || ((mov[i].piezaorg & 0x7) == PEON))
						hmov = 0;
					// salida de peon posible come al paso.
					if(((mov[i].piezaorg & 0x7) == PEON) && (abs(mov[i].origen - mov[i].destino) == 16))
					{
						if(mov[i].origen > mov[i].destino)
							paso = mov[i].origen -8;
						else
							paso = mov[i].origen +8;
					}
					else
						paso = 0;
					// castling.
					if((*((uint8_t *)&castling) & 0xf) != 0)	// aun queda alguno por resolver.
					{
						if((mov[i].piezaorg & 0x7) == REY)
						{
							if(mov[i].piezaorg & NEGRA)
							{
								castling.reinab = 0;
								castling.reyb = 0;
							}
							else
							{
								castling.reinaw = 0;
								castling.reyw = 0;
							}
						}
						else if((mov[i].piezaorg & 0x7) == TORRE)
						{
							if(mov[i].piezaorg & NEGRA)
							{
								if(mov[i].origen == 0)
									castling.reinab = 0;
								else if(mov[i].origen == 7)
									castling.reyb = 0;
							}
							else
							{
								if(mov[i].origen == 56)
									castling.reinaw = 0;
								else if(mov[i].origen == 63)
									castling.reyw = 0;
							}
						}
					}
				}
				else
					hmov = 0;
				// efectua el movimiento en el tablero virtual.	
				mueve(mov[i],tablero);
				// comprueba si cumple el patron.
				if(compruebaPatron(mov[i].piezadest & NEGRA,tablero))
				{
					// genera linea de info resultado.
					inchallados++;
				//	encontrados++;
					fprintf(fdsal,"[FileId=%d,Particion=%d,PartId=%d,Mov=%d,Elomed=>%d,Flags=>%d,NextMov=>%s]\n",
							part.fileid,part.particion,cabpartida.ind,movpartida,cabpartida.elomed,*((uint8_t *)&cabpartida.flags),mov2pgn(&mov[i+1]));
					// genera imagen o FEN segun configuracion.
					if(confjob.formasal == 0)	// salida IMG
						showtab(fdsal,tablero);
					else
						showFEN(fdsal,ultcolor,castling,paso,hmov,movpartida,tablero);
					break;
				}
			}
			
			// la indicacion de progreso se realiza por tiempo.
			// consiste en una linea con el siguiente contenido.
			// particiones procesadas desde indicacion anterior.
			// partidas procesadas desde indicacion anterior.
			// patrones hallados desde indicacion anterior.
			// Los datos van separados por ',' y terminados en '\n'.
			// se envian por el canal de mensajes posix (FIFO).
			if(TIEMPO != slot)
			{
				slot = TIEMPO;
				sprintf(msg,"%d,%d,%d\n",incparticion,incpartidas,inchallados);
			//	printf("%d,%d,%d\n",incparticion,incpartidas,inchallados);
			//	fflush(stdout);
				if(mq_send(fdmq,msg,strlen(msg),0) == 0)
				{
					// si se han enviado con exito se inician los contadores.
					// de lo contrario se sigue acumulando.
					incparticion = 0;
					incpartidas = 0;
					inchallados = 0;
				}
			}
		}
		// final de particion, se envia informe de progreso final.
		sprintf(msg,"%d,%d,%d\n",incparticion,incpartidas,inchallados);
		mq_send(fdmq,msg,strlen(msg),0);
		// cirre del fichero de salida y de la base de datos.
		fclose(fdsal);
		liberaQuery(stmt);
		desconectaSqlite(db);
	}
	// cierra canal de comunicaciones.	
	mq_close(fdmq);
	fprintf(stderr,"IND=> %d\n",ind);
	exit(0);
}
