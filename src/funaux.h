// modulo : funaux.h
// autor  : Antonio Pardo Redondo
//
// Modulo para generar la imagen representativa de la situacion actual
// del tablero o la representacion FEN del mismo. Para representar el
// resultado de la busqueda de patron.
//
#ifndef FUNAUX_H
#define FUNAUX_H

#include "ajedrez.h"

// Genera en texto sobre el fichero indicado una imagen
// representativa del tablero.
extern void showtab(FILE *fd,uint8_t *tab);

// Escribe en el fichero indicado un string FEN correspondiente al
// tablero actual con los parametros adicionales dados.
extern void showFEN(FILE *fd,uint8_t color,CASTLING_t castling,
					uint8_t pasa,uint8_t hmov,uint8_t fmov,uint8_t *tab);
#endif //FUNAUX_H
