// modulo : gpatronbin.c
// autor  : Antonio Pardo Redondo
//
// Este modulo compila una definicion de patron en modo texto a un conjunto
// de estructuras binarias.
// La interpretacion del patron se realiza por palabras clave. Se admiten
// en mayusculas o minusculas, se permiten uno o varios blancos o tabuladores
// de separacion. Se permite definiciones multilinea y se pueden especificar comentarios.
// El compilador recibe el texto del patron por 'stdin' y genera la salida compilada
// por 'sdout'.
// se efectuan chequeos basicos de sintaxis y logica emitiendo informes por 'stderr'.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include "ajedrez.h"

char *nompieza[16] = {"EPT","P","N","B","R","Q","K","TB","EPT","p","n","b","r","q","k","TB"};

PATRON_t patronbin;

char listaand[MAXLINEA];
char listaor[MAXOR][MAXLINEA];
int nor;

uint8_t tablero[64];

// transforma posicion geometrica a indice.
uint8_t transform(uint8_t x,uint8_t y)
{
	return((8+'0'-y)*8+(x-'a'));
}

// Funcion para codificar una pieza dada por su inicial.
uint8_t codpieza(char pieza)
{
//	fprintf(stderr,"Pieza=>%c\n",pieza);
	switch(pieza)
	{
		case 'P':
			return(PEON);
			break;
		case 'p':
			return(PEON | NEGRA);
			break;
		case 'K':
			return(REY);
			break;
		case 'k':
			return(REY | NEGRA);
			break;
		case 'Q':
			return(REINA);
			break;
		case 'q':
			return(REINA | NEGRA);
			break;
		case 'R':
			return(TORRE);
			break;
		case 'r':
			return(TORRE | NEGRA);
			break;
		case 'B':
			return(ALFIL);
			break;
		case 'b':
			return(ALFIL | NEGRA);
			break;
		case 'N':
			return(CABALLO);
			break;
		case 'n':
			return(CABALLO | NEGRA);
			break;
		default:
			fprintf(stderr,"Pieza invalida=>%c\n",pieza);
			return(INVAL);
			break;
	}
}

// inicia el analizador, las estructuras de compilacion.
void iniAnalizador(void)
{
	int i;
	
	memset(tablero,0xff,sizeof(tablero));
	listaand[0] = 0;
	for(i=0;i<MAXOR;i++)
		listaor[i][0] = 0;
	nor = 0;
	memset(&patronbin,0,sizeof(patronbin));
}

// Funcion que chequea que la relacion especificada es compatible con
// indicaciones anteriores.
void chkpos(char *texto,RELAPIEZA_t elemento)
{
	if((tablero[elemento.pos] != 0xff) && (tablero[elemento.pos] != elemento.pieza_tar))
	{
		fprintf(stderr,"%s=>posicion incompatible con anterior=>%s\n",texto,nompieza[tablero[elemento.pos]]);
		exit(3);
	}
	tablero[elemento.pos] = elemento.pieza_tar;
}

// funcion que anhade un elemento a la lista codificada de relaciones AND
// a partir del texto de su definicion.
void anhadeand(char *texto)
{
	char *pchar;
	RELAPIEZA_t elemento;
	
	if((strstr(texto,"taboo") != NULL) || (strstr(texto,"TABOO") != NULL))	// taboo
	{
		elemento.pieza_ataque = NADA;
		elemento.pieza_tar = TABOO;
		pchar = strchr(texto,'(');
		elemento.pos = transform(*(pchar+1),*(pchar+2));
		patronbin.relaand.relaciones[patronbin.relaand.nelementos] = elemento;
		patronbin.relaand.nelementos++;
	}
	else if((strstr(texto,"empty") != NULL) || (strstr(texto,"EMPTY") != NULL))	// empty
	{
		elemento.pieza_ataque = NADA;
		elemento.pieza_tar = NADA;
		pchar = strchr(texto,'(');
		elemento.pos = transform(*(pchar+1),*(pchar+2));
		patronbin.relaand.relaciones[patronbin.relaand.nelementos] = elemento;
		patronbin.relaand.nelementos++;
		chkpos(texto,elemento);
	}
	else if((strstr(texto,"structw") != NULL) || (strstr(texto,"STRUCTW") != NULL))	// structw
	{
		elemento.pieza_ataque = NADA;
		elemento.pieza_tar = PEON;
		pchar = strchr(texto,'(');
		while(*pchar != ')')
		{
			pchar++;
			elemento.pos = transform(*pchar,*(pchar+1));
			patronbin.relaand.relaciones[patronbin.relaand.nelementos] = elemento;
			patronbin.relaand.nelementos++;
			pchar += 2;
			chkpos(texto,elemento);
		}
	}
	else if((strstr(texto,"structb") != NULL) || (strstr(texto,"STRUCTB") != NULL))	// structb
	{
		elemento.pieza_ataque = NADA;
		elemento.pieza_tar = PEON | NEGRA;
		pchar = strchr(texto,'(');
		while(*pchar != ')')
		{
			pchar++;
			elemento.pos = transform(*pchar,*(pchar+1));
			patronbin.relaand.relaciones[patronbin.relaand.nelementos] = elemento;
			patronbin.relaand.nelementos++;
			pchar += 2;
			chkpos(texto,elemento);
		}
	}
	else if(strchr(texto,'(') != NULL)	// relacion.
	{
		elemento.pieza_ataque = codpieza(texto[0]);
		if(elemento.pieza_ataque == INVAL)
			return;
	//	if((texto[2] >= 'a') && (texto[2] <= 'h'))
		if(texto[4] == ')')
		{
			elemento.pieza_tar = NADA;
			pchar = texto +2;
		}
		else
		{
			elemento.pieza_tar = codpieza(texto[2]);
			if(elemento.pieza_tar == INVAL)
				return;
			pchar = texto +3;
		}
		elemento.pos = transform(*pchar,*(pchar+1));
		patronbin.relaand.relaciones[patronbin.relaand.nelementos] = elemento;
		patronbin.relaand.nelementos++;
		chkpos(texto,elemento);
	}
	else   		// posicion
	{
		elemento.pieza_ataque = NADA;
		elemento.pieza_tar = codpieza(texto[0]);
		if(elemento.pieza_tar == INVAL)
			return;
		pchar = texto +1;
		elemento.pos = transform(*pchar,*(pchar+1));
		patronbin.relaand.relaciones[patronbin.relaand.nelementos] = elemento;
		patronbin.relaand.nelementos++;
		chkpos(texto,elemento);
	}
}

// funcion que anhade un elemento a la lista codificada actual de relaciones OR
// a partir del texto de su definicion.
void anhadeor(char *texto)
{
	char *pchar;
	RELAPIEZA_t elemento;
	if((strstr(texto,"taboo") != NULL) || (strstr(texto,"TABOO") != NULL))	// taboo
	{
		elemento.pieza_ataque = NADA;
		elemento.pieza_tar = TABOO;
		pchar = strchr(texto,'(');
		elemento.pos = transform(*(pchar+1),*(pchar+2));
		patronbin.relaor[patronbin.nrelaor].relaciones[patronbin.relaor[patronbin.nrelaor].nelementos] = elemento;
		patronbin.relaor[patronbin.nrelaor].nelementos++;
	}
	else if((strstr(texto,"empty") != NULL) || (strstr(texto,"EMPTY") != NULL))	// empty
	{
		elemento.pieza_ataque = NADA;
		elemento.pieza_tar = NADA;
		pchar = strchr(texto,'(');
		elemento.pos = transform(*(pchar+1),*(pchar+2));
		patronbin.relaor[patronbin.nrelaor].relaciones[patronbin.relaor[patronbin.nrelaor].nelementos] = elemento;
		patronbin.relaor[patronbin.nrelaor].nelementos++;
	}
	else if((strstr(texto,"structw") != NULL) || (strstr(texto,"STRUCTW") != NULL))	// structw
	{
		fprintf(stderr,"STRUCW en relacion OR\n");
		// Invalido. Estructura de peones definida en relacion OR.
	}
	else if((strstr(texto,"structb") != NULL) || (strstr(texto,"STRUCTB") != NULL))	// structb
	{
		fprintf(stderr,"STRUCB en relacion OR\n");
		// Invalido. Estructura de peones definida en relacion OR.
	}
	else if(strchr(texto,'(') != NULL)	// relacion.
	{
		elemento.pieza_ataque = codpieza(texto[0]);
		if(elemento.pieza_ataque == INVAL)
			return;
	//	if((texto[2] >= 'a') && (texto[2] <= 'h'))
		if(texto[4] == ')')
		{
			elemento.pieza_tar = NADA;
			pchar = texto +2;
		}
		else
		{
			elemento.pieza_tar = codpieza(texto[2]);
			if(elemento.pieza_tar == INVAL)
				return;
			pchar = texto +3;
		}
		elemento.pos = transform(*pchar,*(pchar+1));
		patronbin.relaor[patronbin.nrelaor].relaciones[patronbin.relaor[patronbin.nrelaor].nelementos] = elemento;
		patronbin.relaor[patronbin.nrelaor].nelementos++;
	}
	else   		// posicion
	{
		elemento.pieza_ataque = NADA;
		elemento.pieza_tar = codpieza(texto[0]);
		if(elemento.pieza_tar == INVAL)
			return;
		pchar = texto +1;
		elemento.pos = transform(*pchar,*(pchar+1));
		patronbin.relaor[patronbin.nrelaor].relaciones[patronbin.relaor[patronbin.nrelaor].nelementos] = elemento;
		patronbin.relaor[patronbin.nrelaor].nelementos++;
	}
}

// Funcion general de compilacion del patron.
void compilaPatron()
{
	char *pchar,*pchar1;
	int i;
	
	// analizamos texto de la linea de relaciones AND.
	// pchar apunta al token actual, pchar1 al separador que le sigue.
	pchar = listaand;
	while((pchar1 = strpbrk(pchar,",\n")) != NULL)
	{
		if(strncmp(pchar,"struc",5) == 0)	// structura de peones. Anhadimos la lista completa
		{
			pchar1 = strchr(pchar,')') + 1;
		}
		*pchar1 = 0;
		if(patronbin.relaand.nelementos >= MAXRELA)
		{
			fprintf(stderr,"AND=>Sobrepasado MAXRELA\n");
			exit(2);
		}
		anhadeand(pchar);	// anhadimos token a lista AND.
		pchar = pchar1+1;
	}
	// analizamos el array de lineas de texto OR.
	// pchar apunta al token actual, pchar1 al separador que le sigue.
	// Aqui no puede haber lista de peones.
	for(i=0;i<nor;i++)
	{
		pchar = listaor[i];
		while((pchar1 = strpbrk(pchar,",\n")) != NULL)
		{
			if(patronbin.relaor[i].nelementos >= MAXRELA)
			{
				fprintf(stderr,"OR=>Sobrepasado MAXRELA\n");
				exit(2);
			}
			*pchar1 = 0;
			anhadeor(pchar); // anhade a lista OR actual.
			pchar = pchar1+1;
		}
		patronbin.nrelaor++; // incrementa lista OR activa.
	}
}

// limpia blancos y tabuladores por delante, por detras e intercalados.
void limpia(char *texto)
{
	char res[100];
	int i,posres;
	
	for(i=0,posres=0;i<strlen(texto);i++)
	{
		if((texto[i] == ' ') || (texto[i] == '\t'))
			continue;
		res[posres] = texto[i];
		posres++;
	}
	res[posres] = 0;
	strcpy(texto,res);
	
}

// Limpia la informacion de entrada eliminando blancos y distinguiendo
// entre elementos de la cadena AND principal y listas de relaciones OR
// parciales.
// La linea que llega aqui ya esta fusionada y eliminados comentarios.
void preAnaLinea(char *linea)
{
	char *psepa,*pcadena,*plimpio;
	int modoor = 0;
	char tmp[100];
	
	pcadena = linea;
	while((psepa=strpbrk(pcadena,",;\n")) != NULL)
	{
		if(*psepa == '\n')
		{
			*psepa = 0;
			strcpy(tmp,pcadena);
			limpia(tmp);
			if(modoor)
			{
				if(nor == MAXOR)
				{
					fprintf(stderr,"Sobrepasado Nor\n");
					exit(1);
				}
				strcat(listaor[nor],tmp);
				nor++;
			}
			else
				strcat(listaand,tmp);
			break;
		}
		if(modoor)
		{
			if(*psepa == ',')		// campo and en zona or => final zona or.
			{
				if(nor == MAXOR)
				{
					fprintf(stderr,"Sobrepasado Nor\n");
					exit(1);
				}
				*psepa = 0;
				strcpy(tmp,pcadena);
				limpia(tmp);
				strcat(listaor[nor],tmp);
				pcadena = psepa+1;
				nor++;
				modoor = 0;
			}
			else  					// campo OR en zona OR.
			{
				*psepa = 0;
				strcpy(tmp,pcadena);
				limpia(tmp);
				strcat(listaor[nor],tmp);
				strcat(listaor[nor],",");
				pcadena = psepa+1;
			}
		}
		else
		{
			if(*psepa == ',')		// campo and en zona and
			{
				*psepa = 0;
				strcpy(tmp,pcadena);
				limpia(tmp);
				strcat(listaand,tmp);
				strcat(listaand,",");
				pcadena = psepa+1;
			}
			else  					// campo OR en zona AND=> comienza OR.
			{
				*psepa = 0;
				strcpy(tmp,pcadena);
				limpia(tmp);
				strcat(listaor[nor],tmp);
				strcat(listaor[nor],",");
				pcadena = psepa+1;
				modoor = 1;
			}
		}
	}
}

// El patron de texto se recibe por 'stdin'.
// El patron binario se envia por 'stdout'.
// se permiten definiciones multilinea y el patron finaliza cuando se
// detecta un token que comienza con '1' (juagada propuesta) este token
// define quien juega segun vaya seguido por "." (blancas) o "..." (negras).
// En una primera fase, se eliminan comentarios y se fusionan
// lineas en una sola. Si una linea intermedia no finaliza con indicador
// de operacion logica se presupone que es AND ','.
// Posteriormente se invoca a la funcion de preanalisis.
void main()
{
	char lineain[MAXLINEA];
	char linea[MAXLINEA];
	int colorjuega = 0;
	char *pchar;
	uint8_t *pmem;
	int i,j;
	int len;
	int final = 0;
	
	iniAnalizador();
	linea[0] = 0;
	while( fgets(lineain,MAXLINEA,stdin) != NULL)
	{
		// eliminamos comentarios.
		pchar = strchr(lineain,'/');
		if(pchar != NULL)
			*pchar = 0;
		// borramos blancos tabuladores y final de linea por el final.
		for(i=strlen(lineain)-1;i>0;i--)
		{
			if((lineain[i] != ' ') && (lineain[i] != '\n') && (lineain[i] != '\t') && (lineain[i] != '\r'))
				break;
			lineain[i] = 0;
		}
		// miramos si la linea termina en ',' o ';' caso contrario terminamos en ','.
		if((lineain[i] != ',') && (lineain[i] != ';'))
		{
			lineain[i+1] = ',';
			lineain[i+2] = 0;
		}
		pchar = strstr(lineain,"1.");	// deteccion final de patron.
		if(pchar != NULL)
		{
			final = 1;
			if(strstr(pchar,"1...") != NULL)
				colorjuega = NEGRA;
			*pchar = 0;
		}
		len = strlen(lineain);
		if((strlen(linea) + len) >= MAXLINEA)
		{
			fprintf(stderr,"OVERRUN buffer...\n");
			exit(2);
		}
		strcat(linea,lineain);
		if(final)
			break;
	}
	if(!final)
	{
		fprintf(stderr,"No marcado color juega..\n");
		exit(2);
	}
	strcat(linea,"\n");	// terminamos linea fusionada.
	fprintf(stderr,"LIN=>%s",linea);
	fflush(stdout);
	preAnaLinea(linea);
	// terminamos lineas resultado preanalisis.
	if(listaand[strlen(listaand)-1] == ',')
		listaand[strlen(listaand)-1] = 0; // eliminamos ultimo separador.
	strcat(listaand,"\n");	// terminamos linea.
	for(i=0;i<nor;i++)
		strcat(listaor[i],"\n");
	
	fprintf(stderr,"AND=>%s\n",listaand);
	for(i=0;i<nor;i++)
	{
		fprintf(stderr,"OR(%d)=>%s\n",i,listaor[i]);
	}
	
	compilaPatron();
	patronbin.color = colorjuega;
/*	
	for(i=0,pmem = (uint8_t *)&patronbin;i<(sizeof(patronbin)-32);i+=32)
	{
		for(j=0;j<32;j++)
		{
			printf("%02x ",*pmem);
			pmem++;
		}
		printf("\n");
	}
*/
	len = write(1,&patronbin,sizeof(patronbin));
	exit(0);
}
