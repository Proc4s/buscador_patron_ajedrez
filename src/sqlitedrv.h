// modulo : funaux.h
// autor  : Antonio Pardo Redondo
//
// Modulo que implementa las funciones para interactuar con la base de datos SQLITE.
// En concreto implementa las funciones para conectarse a la base, lanzar un QUERY,
// obtener el resultado del mismo, comenzar una transaccion, finalizar esta y efectuar
// una inserccion en una tabla de la base.
//
#ifndef SQLITEDRV_H
#define SQLITEDRV_H

#include "ajedrez.h"
#include <sqlite3.h>

// funcion para conectar con la base de datos indicada por su path.
// pone la base en modo asincrono para ganar velocidad.
extern void conectaSqlite(sqlite3 **db,char *basename);

// Funcion para desconectar de la base de datos indicada por su descriptor.
extern void desconectaSqlite(sqlite3 *db);

// Funcion para indicar el comienzo de una transaccion de escritura en la base.
// se indican el descriptor de la base y el descriptor del cursor a usar en las insercciones.
extern void beginTransW(sqlite3 *db,sqlite3_stmt **stmt);

// Funcion para finalizar una transaccion de escritura.
// se indican el descriptor de la base y el descriptor del cursor usado en las insercciones.
extern void endTransW(sqlite3 *db,sqlite3_stmt *stmt);

// Funcion para volcar una partida indicada por su cabecera y lista de movimientos al cursor de inserccion
// actual en la base de datos. Se indican el descriptor de la base, el cursor de inserccion, el identificador
// del fichero de partidas original y el numero de particion en proceso.
extern void vuelcaPart(sqlite3 *db,sqlite3_stmt *stmt,CPARTIDA_t *cabpar,
								MOVBIN_t *mov,int fileid,int particion);

// Funcion para lanzar un QUERY de consulta a la base, se indica fileid, particion, elomin, elomax y ganador
// como criterios de busqueda. Se indica ademas el descriptor de la base y el cursor a usar para el resultado
// del QUERY.						
extern void lanzaQueryR(sqlite3 *db,sqlite3_stmt **stmt,int fileid,
								int particion,int elomin,int elomax,int gana);

// Funcion para obtener la siguiente partida resultado del QUERY anteriormente
// lanzado. Se pasa el descriptor de la base y el cursor del QUERY. devuelve
// la cabecera de partida y su lista de movimientos. Si no quedan mas, retorna '0'
// en caso contrario retorna '1'.						
extern int nextPartida(sqlite3 *db,sqlite3_stmt *stmt,CPARTIDA_t *cabpar,
								MOVBIN_t *mov);

// libera el cursor usado en el QUERY.								
extern void liberaQuery(sqlite3_stmt *stmt);

// Funcion para insertar un registro en la tabla de particiones de la base master.
// la base se supone ya abierta e indicada por su descriptor.
extern void insertaParticion(sqlite3 *db,int fileid,int particion,int base);

#endif //SQLITEDRV_H
