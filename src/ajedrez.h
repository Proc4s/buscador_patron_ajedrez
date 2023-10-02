// modulo : basfichdrv.h
// autor  : Antonio Pardo Redondo
//
// Modulo de definicion de estructuras y constantes utilizados en el procesamiento de partidas
// de ajedrez para buscar patrones.
#ifndef AJEDREZ_H
#define AJEDREZ_H

#include <stdint.h>
#include <time.h>

// codificacion de las piezas
#define NADA 		0
#define PEON		1		// 'P'
#define CABALLO 	2		// 'N'
#define ALFIL 		3		// 'B'
#define TORRE 		4		// 'R'
#define REINA 		5		// 'Q'
#define REY 		6		// 'K'
#define TABOO		7		// indicacion de posicion TABOO.
#define NEGRA 		0x8
#define INVAL		0xf

// La codificacion de una pieza ocupa tres bits y en el cuarto se codifica el color (NEGRA o no).
// por lo tanto una pieza con su color se codifica en 4 bits.

// el tablero se define por 64 bytes cuyo valor es la pieza que contiene.
// La casilla 'a8' estandar corresponde con la posicion cero del array
// mientras que la casilla 'h1' corresponde con la posicion 63 del array.
// esta es el contenido del tablero virtual al comienzo de una partida.
static uint8_t tablaini[64] = {0x0c,0x0a,0x0b,0x0d,0x0e,0x0b,0x0a,0x0c,
							 0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,
							 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
							 0x04,0x02,0x03,0x05,0x06,0x03,0x02,0x04};

//============ Estructuras Interpretacion PGN =============================

// lista de piezas activas de un determinado color.
// su contenido es la posicion absoluta en el tablero (0:63) de cada pieza activa.
// Se utiliza para acelerar la localizacion de piezas en el tablero.
// Su contenido debe corresponder en todo momento con el tablero virtual.	
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


//================== Estructuras para trabajar con informacion ya codificada ======
#define MAXMOV 1000	// maximo numero movimientos en una partida.

// estructura que detalla un movimiento en binario.
// La pieza origen puede ser distinta de la pieza destino en una promocion.
typedef struct {
	uint8_t	piezaorg;	// pieza origen
	uint8_t	piezadest; 	// pieza destino.
	uint8_t	origen;		// posicion origen absoluta de la pieza (0:63).
	uint8_t	destino;		// posicion destino absoluta de la pieza (0:63). 
} MOVBIN_t;

// Codificacion del byte de flags de la partida
typedef struct {
	uint8_t	ganablanca : 1;	// indicacion de que ganan las blancas.
	uint8_t	gananegra : 1;		// indicacion de que ganan las negras.
	uint8_t	tnormal : 1;		// la partida ha terminado normalmente.
	uint8_t	reser : 5;			// reservado para futuro uso.
} FLAGS_t;

// Estructura de cabecera de partida en binario	
typedef struct {
	uint16_t	magic;
	uint8_t	reser;
	FLAGS_t	flags;	// flags de la partida.
	uint16_t	nmov;		// numero de movimientos de la partida en lista de movimientos.
	uint16_t	elomed;	// elo media de los jugadores,
	uint32_t	ind;		// indice de la partida en el fichero PGN original.
} CPARTIDA_t;

// Estructura de posibilidad de enroque.
typedef struct {
	uint8_t reinaw : 1;	// posibilidad enroque lado reina blancas.
	uint8_t reyw : 1;		// posibilidad enroque lado rey blancas.
	uint8_t reinab : 1;	// posibilidad enroque lado reina negras.
	uint8_t reyb : 1;		// posibilidad enroque lado rey negras.
	uint8_t reser : 4;
} CASTLING_t;

//======== Estructuras de definicion de patrones codificados ===============
// Patrones.
#define MAXOR				8		// numero maximo de secciones OR
#define MAXRELA			32		// numero maximo de relaciones.
#define MAXLINEA			1000	// tamanho maximo linea definicion patron.

// estructura que define una relación, posicion, casilla vacia o taboo.
// Si la pieza de ataque es de tipo NADA se trata de una definicion
// de posicion, casilla vacia o taboo :
// 	-si pieza_tar es de tipo NADA se trata de una definicion de vacio.
// 	-si pieza_tar es de tipo TABOO, se trata de una definicion de taboo.
// 	- si pieza_tar es de cualquier otro tipo se trata de una definicion de posicion.
// Si la pieza de ataque es de un tipo determinado:
//		-si pieza_tar es de tipo NADA se trata de un ataque o defensa a una posicion.
//		si pieza_tar tiene tipo se trata de un ataque o defensa a una pieza concreta.
// Las estructuras de peones se codifican como una lista de posiciones.
typedef struct {
	uint8_t	pieza_ataque;	// pieza que ataca o defiende.
	uint8_t	pieza_tar;		// pieza objeto de ataque o defensa.
	uint8_t	pos;				// posicion en tablero pieza_tar.
	} RELAPIEZA_t;


// Estructura que define una lista de relaciones o posiciones.
typedef struct {
		uint8_t nelementos;	// numero de elemntos activos en la lista.
		RELAPIEZA_t relaciones[MAXRELA];	// lista de relaciones.
	} LISTARELA_t;
	
// Estructura que define un patron.
// Relaciones logicas simples con preferencia de operacion OR
// es decir: el patron de define como una secuencia de operaciones AND
// con terminos que pueden resultar a su vez de operaciones OR.
// No puede utilizarse operaciones OR en La estructura de posicion de peones y esta
// Las estructuras de peones se codifican como listas de posiciones de peones con relacion AND.
// se integra en la lista posand.

typedef struct {
	uint8_t			color;			// color que juega.
	uint8_t			nrelaor;			// Numero de listas de relaciones OR.
	LISTARELA_t		relaand;			// lista de relaciones AND.
	LISTARELA_t		relaor[MAXOR];	// Array de listas de relaciones OR.
} PATRON_t;

// 5 tics por segundo para trazas
#define TIEMPO ((clock()*5)/CLOCKS_PER_SEC)

#endif // AJEDREZ_H
