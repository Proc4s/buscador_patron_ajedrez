// modulo : funaux.c
// autor  : Antonio Pardo Redondo
//
// Modulo para generar la imagen representativa de la situacion actual
// del tablero o la representacion FEN del mismo. Para representar el
// resultado de la busqueda de patron.
//
#include <stdio.h>
#include <fcntl.h>
#include "funaux.h"

// Funcion que imprime la letra representativa de la pieza en la
// posicion actual del tablero o string FEN.
void printPieza(FILE *fd,uint8_t pieza)
{
	switch(pieza & 0x7)
	{
		case REY:
			if(pieza & NEGRA)
				fprintf(fd,"k");
			else
				fprintf(fd,"K");
			break;
		case REINA:
			if(pieza & NEGRA)
				fprintf(fd,"q");
			else
				fprintf(fd,"Q");
			break;
		case TORRE:
			if(pieza & NEGRA)
				fprintf(fd,"r");
			else
				fprintf(fd,"R");
			break;
		case ALFIL:
			if(pieza & NEGRA)
				fprintf(fd,"b");
			else
				fprintf(fd,"B");
			break;
		case CABALLO:
			if(pieza & NEGRA)
				fprintf(fd,"n");
			else
				fprintf(fd,"N");
			break;
		case PEON:
			if(pieza & NEGRA)
				fprintf(fd,"p");
			else
				fprintf(fd,"P");
			break;
		default:
			fprintf(fd,".");
			break;
	}
}

// Genera en texto una imagen representativa del tablero.
// en la parte superior indica el nombre de las columnas
// en el lado derecha el numero de las filas estandar.
// en el lado izquierdo el numero de casilla absoluta de la columna
// de la izquierda. La ausencia de pieza se indica con '.', las
// blancas pro la inicial de la pieza en ingles en mayusculas. Las
// negras por la inicial de la pieza en ingles en minusculas.
void showtab(FILE *fd,uint8_t *tab)
{
	int i;
	
	fprintf(fd,"\n    a b c d e f g h\n");
	
	fprintf(fd,"\n 0  ");
	for(i=0;i<8;i++)
	{
		printPieza(fd,tab[i]);
		fprintf(fd," ");
	}
	fprintf(fd," 8\n");
	
	fprintf(fd," 8  ");
	for(i=8;i<16;i++)
	{
		printPieza(fd,tab[i]);
		fprintf(fd," ");
	}
	fprintf(fd," 7\n");
	
	fprintf(fd,"16  ");
	for(i=16;i<24;i++)
	{
		printPieza(fd,tab[i]);
		fprintf(fd," ");
	}
	fprintf(fd," 6\n");
	
	fprintf(fd,"24  ");
	for(i=24;i<32;i++)
	{
		printPieza(fd,tab[i]);
		fprintf(fd," ");
	}
	fprintf(fd," 5\n");
	
	fprintf(fd,"32  ");
	for(i=32;i<40;i++)
	{
		printPieza(fd,tab[i]);
		fprintf(fd," ");
	}
	fprintf(fd," 4\n");
	
	fprintf(fd,"40  ");
	for(i=40;i<48;i++)
	{
		printPieza(fd,tab[i]);
		fprintf(fd," ");
	}
	fprintf(fd," 3\n");
	
	fprintf(fd,"48  ");
	for(i=48;i<56;i++)
	{
		printPieza(fd,tab[i]);
		fprintf(fd," ");
	}
	fprintf(fd," 2\n");
	
	fprintf(fd,"56  ");
	for(i=56;i<64;i++)
	{
		printPieza(fd,tab[i]);
		fprintf(fd," ");
	}
	fprintf(fd," 1\n\n");
}

// Escribe en el fichero indicado un string FEN correspondiente al
// tablero actual.
void showFEN(FILE *fd,uint8_t color,CASTLING_t castling,uint8_t pasa,
							uint8_t hmov,uint8_t fmov,uint8_t *tab)
{
	int i,j;
	int vacios;
	
	// posicion de las piezas.
	for(i=0;i<8;i++)
	{
		vacios = 0;
		for(j=0;j<8;j++)
		{
			if(tab[(i*8)+j] != 0)
			{
				if(vacios)
				{
					fprintf(fd,"%01d",vacios);
					vacios = 0;
				}
				printPieza(fd,tab[(i*8)+j]);
			}
			else
				vacios++;
		}
		if(vacios)
			fprintf(fd,"%01d",vacios);
		if(i<7)
			fprintf(fd,"/");
	}
	// color que juega.
	if(color)
		fprintf(fd," b ");
	else
		fprintf(fd," w ");
	// posibilidad de enrroque.
	vacios = 0;
	if(castling.reyw)
	{
		vacios = 1;
		fprintf(fd,"K");
	}
	if(castling.reinaw)
	{
		vacios = 1;
		fprintf(fd,"Q");
	}
	if(castling.reyb)
	{
		vacios = 1;
		fprintf(fd,"k");
	}
	if(castling.reinab)
	{
		vacios = 1;
		fprintf(fd,"q");
	}
	if(vacios == 0)
		fprintf(fd,"-");
	// peon con posible captura al paso, viene en coordenada absoluta,0=>no peon.
	if(pasa)
	{
		uint8_t fila = 8 - (pasa/8);
		uint8_t columna = pasa%8;
		fprintf(fd," %c%c ",columna+'a',fila+'0');
	}
	else
		fprintf(fd," - ");
	// medios movimientos desde ultima captura o peon.
	fprintf(fd,"%0d ",hmov);
	// movimientos totales
	fprintf(fd,"%0d\n",fmov);
}
