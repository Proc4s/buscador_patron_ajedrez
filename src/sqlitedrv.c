// modulo : funaux.c
// autor  : Antonio Pardo Redondo
//
// Modulo que implementa las funciones para interactuar con la base de datos SQLITE.
// En concreto implementa las funciones para conectarse a la base, lanzar un QUERY,
// obtener el resultado del mismo, comenzar una transaccion, finalizar esta y efectuar
// una inserccion en una tabla de la base.
//
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "sqlitedrv.h"

// funcion para conectar con la base de datos indicada por su path.
// pone la base en modo asincrono para ganar velocidad.
void conectaSqlite(sqlite3 **db,char *basename)
{
	int rc;
	sqlite3_stmt *stmt1;
	char *error_message = 0;
	const char* pragasync = "PRAGMA synchronous = 0";
	
	rc = sqlite3_open(basename, db);
	if( rc ){
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(*db));
		sqlite3_close(*db);
		exit(1);
	}
	rc = sqlite3_prepare(*db, pragasync, -1, &stmt1, NULL);
	rc = sqlite3_step(stmt1);
	if (rc != SQLITE_DONE) {
        fprintf(stderr, "Error al cambiar pragma: %s\n", sqlite3_errmsg(*db));
   }
   sqlite3_finalize(stmt1);
}

// Funcion para desconectar de la base de datos indicada por su descriptor.
void desconectaSqlite(sqlite3 *db)
{
	sqlite3_close(db);
	db = NULL;
}

// Funcion para indicar el comienzo de una transaccion de escritura en la base.
// en la tabla de partidas.
// se indican el descriptor de la base y el descriptor del cursor a usar en las insercciones.
void beginTransW(sqlite3 *db,sqlite3_stmt **stmt)
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
	rc = sqlite3_prepare(db, query, -1, stmt, NULL);
	if (rc != SQLITE_OK) {
	  fprintf(stderr, "Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
	  sqlite3_close(db);
	  exit(2);
	}	
}

// Funcion para finalizar una transaccion de escritura.
// se indican el descriptor de la base y el descriptor del cursor usado en las insercciones.
void endTransW(sqlite3 *db,sqlite3_stmt *stmt)
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

// Funcion para volcar una partida indicada por su cabecera y lista de movimientos al cursor de inserccion
// actual en la base de datos. Se indican el descriptor de la base, el cursor de inserccion, el identificador
// del fichero de partidas original y el numero de particion en proceso.
void vuelcaPart(sqlite3 *db,sqlite3_stmt *stmt,CPARTIDA_t *cabpar,MOVBIN_t *mov,int fileid,int particion)
{
	int i,rc;
	uint8_t ganador;


	if(cabpar->nmov == 0)
		return;
	
	// codificacion de ganador (0=NADA, 1=>blancas, 2=>negras, 3=tablas).
   ganador = cabpar->flags.ganablanca;
	ganador += cabpar->flags.gananegra * 2;
	
	// rellenamos cursor de inserccion.
	sqlite3_bind_int(stmt, 1, fileid);
	sqlite3_bind_int(stmt, 2, particion);
	sqlite3_bind_int(stmt, 3, cabpar->elomed);
	sqlite3_bind_int(stmt, 4, ganador);
	sqlite3_bind_int(stmt, 5, cabpar->ind);
	sqlite3_bind_blob(stmt, 6, (char *)mov, cabpar->nmov * 4, SQLITE_STATIC);
	rc = sqlite3_step(stmt);	// efectua inserccion en base.
	if (rc != SQLITE_DONE) {
        fprintf(stderr, "Error al insertar fila: %s\n", sqlite3_errmsg(db));
   }
   // reset del cursor usado.
   sqlite3_clear_bindings(stmt);
   sqlite3_reset(stmt);
}

// Funcion para lanzar un QUERY de consulta a la base, se indica fileid, particion, elomin, elomax y ganador
// como criterios de busqueda. Se indica ademas el descriptor de la base y el cursor a usar para el resultado
// del QUERY.
void lanzaQueryR(sqlite3 *db,sqlite3_stmt **stmt,int fileid, int particion,int elomin,int elomax,int gana)
{
	int rc;
	const char* query = "SELECT * FROM partidas WHERE fileid = ? and particion = ? and elomed > ?  and elomed < ? and ganador = ?";
	const char* queryr = "SELECT * FROM partidas WHERE fileid = ? and particion = ? and elomed > ? and elomed < ?";
	
	if(gana == 0)	// si el ganador no importa reducimos el QUERY.
		rc = sqlite3_prepare(db, queryr, -1, stmt, NULL);
	else
		rc = sqlite3_prepare(db, query, -1, stmt, NULL);
	// Relleno del cursor de datos del QUERY.
	sqlite3_bind_int(*stmt, 1, fileid);
	sqlite3_bind_int(*stmt, 2, particion);
	sqlite3_bind_int(*stmt, 3, elomin);
	sqlite3_bind_int(*stmt, 4, elomax);
	if(gana != 0)
		sqlite3_bind_int(*stmt, 5, gana);
}

// Funcion para obtener la siguiente partida resultado del QUERY anteriormente
// lanzado. Se pasa el descriptor de la base y el cursor del QUERY. devuelve
// la cabecera de partida y su lista de movimientos. Si no quedan mas, retorna '0'
// en caso contrario retorna '1'.
int nextPartida(sqlite3 *db,sqlite3_stmt *stmt,CPARTIDA_t *cabpar,MOVBIN_t *mov)
{
	int rc;
	uint8_t ganador;
	
	memset(cabpar,0,sizeof(CPARTIDA_t));	// rellena a cero cabpartida.
	rc = sqlite3_step(stmt);	// siguiente posicion del cursor del QUERY.
	if (rc == SQLITE_ROW) {
	  // rellena cabpartida y movimientos.
	  cabpar->elomed = sqlite3_column_int(stmt, 2);
	  cabpar->ind = sqlite3_column_int(stmt, 4);
	  ganador = sqlite3_column_int(stmt, 3);
	  const void *datos_resultado = sqlite3_column_blob(stmt, 5);
	  int tamano_resultado = sqlite3_column_bytes(stmt, 5);
	  cabpar->nmov = tamano_resultado/4;
	  memcpy(mov,datos_resultado,tamano_resultado);
	  // recodifica ganador.
	  switch(ganador)
      {
			case 1:
				cabpar->flags.ganablanca = 1;
				cabpar->flags.gananegra = 0;
				break;
			case 2:
				cabpar->flags.ganablanca = 0;
				cabpar->flags.gananegra = 1;
				break;
			case 3:
				cabpar->flags.ganablanca = 1;
				cabpar->flags.gananegra = 1;
				break;
			default:
				cabpar->flags.ganablanca = 0;
				cabpar->flags.gananegra = 0;
				break;
		}
		return 1;	// retorna OK.
    } else {
        return 0;	// no hay mas partidas en el QUERY.
    }
}

// libera el cursor usado en el QUERY.
void liberaQuery(sqlite3_stmt *stmt)
{
	sqlite3_finalize(stmt);
}

// Funcion para insertar un registro en la tabla de particiones de la base master.
// la base se supone ya abierta e indicada por su descriptor.
void insertaParticion(sqlite3 *db,int fileid,int particion,int base)
{
	int rc;
	sqlite3_stmt *stmt1;
	const char *query = "INSERT INTO particiones(fileid,particion,base) VALUES(?,?,?)";
	
	rc = sqlite3_prepare(db, query, -1, &stmt1, NULL);
	if (rc != SQLITE_OK) {
	  fprintf(stderr, "Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
	  sqlite3_close(db);
	  exit(2);
	}
	sqlite3_bind_int(stmt1, 1, fileid);
	sqlite3_bind_int(stmt1, 2, particion);
	sqlite3_bind_int(stmt1, 3, base);
	rc = sqlite3_step(stmt1);
	if (rc != SQLITE_DONE) {
        fprintf(stderr, "Error al insertar fila: %s\n", sqlite3_errmsg(db));
   }
   sqlite3_finalize(stmt1);
}

