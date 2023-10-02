// programa de prueba para evaluar las prestaciones de cassandra en consulta.
//
// El programa lanza un QUERY seleccionando partidas que pertenezcan a una
// determinado fileid y particion y cumplan con los valores elo especificados
// y el ganador especificado.
//
// Por cada partida ejecuta en el tablero virtual los movimientos adecuados.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <cassandra.h>

uint8_t tabini[64] = {0x0c,0x0a,0x0b,0x0d,0x0e,0x0b,0x0a,0x0c,
							 0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,
							 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
							 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
							 0x04,0x02,0x03,0x05,0x06,0x03,0x02,0x04};

#define MAXMOV 1000	// maximo numero movimientos en una partida.							 

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
//
//================ CASSANDRA ============================================
// conexion base.
CassFuture* connect_future = NULL;
CassCluster* cluster = NULL;
CassSession* session = NULL;
CassFuture* close_future = NULL;
char* hosts = "127.0.0.1";

void print_error(CassFuture* future) {
  const char* message;
  size_t message_length;
  cass_future_error_message(future, &message, &message_length);
  fprintf(stderr, "Error: %.*s\n", (int)message_length, message);
}

void conectaCassandra()
{
	const char* message;
   size_t message_length;
   
   cluster = cass_cluster_new();
   session = cass_session_new();
	cass_cluster_set_contact_points(cluster, hosts);
	connect_future = cass_session_connect(session, cluster);
	if (cass_future_error_code(connect_future) == CASS_OK) {
		return;
	}
	else
	{
		print_error(connect_future);
		cass_future_free(connect_future);
		cass_cluster_free(cluster);
		cass_session_free(session);
		exit(1);
	}
}

void desconectaCassandra()
{
	close_future = cass_session_close(session);
   cass_future_wait(close_future);
   cass_future_free(close_future);
   cass_future_free(connect_future);
	cass_cluster_free(cluster);
	cass_session_free(session);
}

CassStatement* statement = NULL;
CassFuture* future = NULL;
CassResult* result = NULL;
CassIterator* iterator = NULL;

void lanzaQuery(int fileid,int particion,int elomed)
{
	CassError rc = CASS_OK;
	const char* query = "SELECT * FROM ajedrez.partidas where fileid = ? and particion = ? and elomed > ?";
	
	statement = cass_statement_new(query, 3);
	cass_statement_set_paging_size (statement,1000 );
	cass_statement_bind_int32(statement,0,fileid);
	cass_statement_bind_int16(statement,1,particion);
	cass_statement_bind_int16(statement,2,elomed);
	
	future = cass_session_execute(session, statement);
	if (cass_future_error_code(future) != 0) {
		print_error(future);
	}
	else
	{
		result = cass_future_get_result(future);
		iterator = cass_iterator_from_result(result);
		cass_future_free(future);
		future = NULL;
	}	
}

int nextPartida()
{
	uint8_t ganador;
	cass_bool_t has_more_pages = cass_false;
   union {
		uint32_t val32;
		MOVBIN_t	movimiento;
	} valtmp;
	
	if (cass_iterator_next(iterator) == 0)
	{
		has_more_pages = cass_result_has_more_pages(result);
		if (has_more_pages) {
			cass_statement_set_paging_state(statement, result);
		}
		cass_iterator_free(iterator);
		cass_result_free(result);
		if (has_more_pages) {
			future = cass_session_execute(session, statement);
			if (cass_future_error_code(future) != 0) {
				print_error(future);
				cass_future_free(future);
				cass_statement_free(statement);
				statement = NULL;
				return 0;
			}
			result = cass_future_get_result(future);
			iterator = cass_iterator_from_result(result);
			cass_future_free(future);
			if (cass_iterator_next(iterator) == 0)
			{
				cass_iterator_free(iterator);
				cass_result_free(result);
				cass_statement_free(statement);
				statement = NULL;
				return 0;
			}
		}
		else
		{
			return 0;
		}
	}

	{
		const CassValue* value = NULL;
		const CassRow* row = cass_iterator_get_row(iterator);
		CassIterator* items_iterator = NULL;

		value = cass_row_get_column(row, 2);
		cass_value_get_int16(value, &cabpartida.elomed);
		value = cass_row_get_column(row, 4);
		cass_value_get_int8(value, &ganador);
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
		value = cass_row_get_column(row, 3);
		cass_value_get_int32(value,&cabpartida.ind);
		cabpartida.nmov = 0;
		value = cass_row_get_column(row, 5);
		if(cass_value_item_count(value))
		{
	//   printf("\nNMOV=>%d, ELO=>%d, partid=>%d\n",cass_value_item_count(value),cabpartida.elomed,cabpartida.ind);     
			items_iterator = cass_iterator_from_collection(value);
			while (cass_iterator_next(items_iterator)) {
			  cass_value_get_int32(cass_iterator_get_value(items_iterator), &valtmp.val32);
			  movimientos[cabpartida.nmov] = valtmp.movimiento;
			  cabpartida.nmov++;
			}
		}
	 //  printf("CPARTMOV=>%d\n",cabpartida.nmov);
		cass_iterator_free(items_iterator);
		return 1;
	}
}

void liberaQuery()
{

	if(statement != NULL)
		cass_statement_free(statement);
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
	int fileid,partid,elomed;
	int partida,encontrados = 0;
	int ind;
	MOVBIN_t *mov;
	int i,len,nread;
	uint8_t tablero[64];
	
	if(argc != 4)
	{
		fprintf(stderr,"Usage: %s  <fileid> <partid> <elomed>\n",argv[0]);
		exit(1);
	}
	fileid = atoi(argv[1]);
	partid = atoi(argv[2]);
	elomed = atoi(argv[3]);
	conectaCassandra();
	lanzaQuery(fileid,partid,elomed);
	ind = 1;
	while(nextPartida())
	{
		iniciaJuego(tablero);
		mov = (MOVBIN_t *)movimientos;
		for(i=0;i<cabpartida.nmov;i++)
		{
			mueve(mov[i],tablero);

		}
	//	if((ind % 10000) == 0)
		{
			fprintf(stderr,"IND=>%d           \r",ind);
			fflush(stderr);
		}
		ind++;
	}
	fprintf(stderr,"IND=> %d\n",ind);
	liberaQuery();
	desconectaCassandra();
	exit(0);
}

