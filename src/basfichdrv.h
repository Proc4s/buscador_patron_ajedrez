// modulo : basfichdrv.h
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
#ifndef BASFICHDRV_H
#define BASFICHDRV_H

#include <stdint.h>
#include "ajedrez.h"

typedef struct {
	uint16_t		fileid;		// identificador de fichero (fecha yyyy*12+mm)
	uint16_t		particion;	// N. particion de max 1000000 de partidas.
	uint32_t		len;			// Longitud en bytes zona PARTIDASID de esta particion.
	uint64_t		offset;		// offset en PARTIDASID de esta particion.
} PARTFICH_t;

typedef struct {
	uint16_t		elomed;		// Valor de Elo medio de esta partida.
	uint8_t		ganador;		// Ganador partida (0=>inval,1=>blancas,2=>Negras,3=>tablas).
	uint8_t		flags;		// No usado de momento.
	uint64_t		offset;		// offset en DATA de la partida.
} __attribute__((packed)) PARTIDA_t;

typedef struct {
	int fdparticiones;
	PARTFICH_t *particiones;
	int lenparticiones;
	PARTIDA_t *partidas;
	int lenpartidas;
	int fdpartidas;
	int fddata;
//	FILE *fddata;
} BASFICH_t;

// Abre la base de datos para lectura.
extern int basfichOpenR(char *path,BASFICH_t *bd);
// Abre la base de datos para lectura-escritura (append).
extern int basfichOpenW(char *path,BASFICH_t *bd);
// cierra base de datos.
extern void basfichClose(BASFICH_t *bd);
// ordena el fichero de particiones por fileid, particion
extern void ordenaParticiones(BASFICH_t *bd);
// ordena las partidas de una determinada particion por elomed y ganador.
extern void ordenaPartidas(BASFICH_t *bd,int fileid,int particion);
// busca una particion en el fichero de particiones.
extern PARTFICH_t *buscaParticion(BASFICH_t *bd,int fileid,int particion);
// Carga en memoria los indices de partidas de una particion.
extern int cargaPartidas(BASFICH_t *bd,PARTFICH_t *particion);
// busca la primera partida que cumpla elomed, ganador en las partidas cargadas en memoria.
extern PARTIDA_t *buscaPartida(BASFICH_t *bd,int elomin,int gana);
// anhade una particion al fichero indices de particiones.
extern int anhadeParticion(BASFICH_t *bd,PARTFICH_t *particion);
// anhade una partida al fichero indice de partidas (campos).
extern int anhadePartida(BASFICH_t *bd,PARTIDA_t *partida);
// Lee los datos de una partida (cabpartida, movimientos).
extern void loadPartida(BASFICH_t *bd,PARTIDA_t *partida,CPARTIDA_t *cabpartida,MOVBIN_t *movimientos);

#endif // BASFICHDRV_H
