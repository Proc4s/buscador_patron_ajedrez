// modulo : creabaseSqlite.c
// autor  : Antonio Pardo Redondo
//
// Utilidad para crear las bases y tablas de sqlite para la aplicacion de ajedrez.
//
// se indica el path de la base a crear y su tipo: 'master' o 'aux'. La base
// master lleva la tabla adicional 'particiones' que contiene el 'idfile',
// 'particion' y numero de 'base' de todas las particiones insertadas en todas las bases.
//
#include <stdio.h>
#include <sqlite3.h>
#include <string.h>

// sentencia SQL para crear la tabla de partidas.
char createPartidas[] = "CREATE TABLE \"partidas\" (\
	\"fileid\"	INTEGER,\
	\"particion\"	INTEGER,\
	\"elomed\"	INTEGER,\
	\"ganador\"	INTEGER,\
	\"partidaid\"	INTEGER,\
	\"movimientos\"	BLOB)";
	
// sentencia SQL para crear la tabla de particiones en modo 'master'.
char createParticiones[] = "CREATE TABLE \"particiones\" (\
	\"fileid\"	INTEGER,\
	\"particion\"	INTEGER,\
	\"base\"	INTEGER)";

// sentencia SQL para crear el indice por elomed en la tabla de partidas.
char createIndexElo[] = "CREATE INDEX \"eloid\" ON \"partidas\" (\"elomed\"	ASC)";

// sentencia SQL para crear el indice por 'fileid'-'particion' en la tabla 'partidas'.
char createIndexPart[] = "CREATE INDEX \"partid\" ON \"partidas\" (\
	\"fileid\"	ASC,\
	\"particion\"	ASC)";

// sentencia SQL para crear el indice por 'fileid' en la tabla 'particiones'.
char createIndexFech[] = "CREATE INDEX \"fechaid\" ON \"particiones\" (\"fileid\"	ASC)";	

// Funcion de callback para recoger el resultado de la ejecucion de la sentencia SQL.	
static int callback(void *NotUsed, int argc, char **argv, char **azColName){
 int i;
 for(i=0; i<argc; i++){
	printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
 }
 printf("\n");
 return 0;
}

int main(int argc, char **argv){
 sqlite3 *db;
 char *zErrMsg = 0;
 int rc;

 if( argc!=3 ){
	fprintf(stderr, "Usage: %s <pathdatabase> <master/aux>\n", argv[0]);
	return(1);
 }
 // debe espeificarse 'master' o 'aux'.
 if((strstr(argv[2],"master") == NULL) && (strstr(argv[2],"aux") == NULL))
 {
	 fprintf(stderr,"Tipo base invalido=>%s\n",argv[2]);
	 return(1);
 }
 // creacion de la base
 rc = sqlite3_open(argv[1], &db);
 if( rc ){
	fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
	sqlite3_close(db);
	return(1);
 }
 // creacion tabla 'partidas'.
 rc = sqlite3_exec(db, createPartidas, callback, 0, &zErrMsg);
 if( rc!=SQLITE_OK ){
	fprintf(stderr, "SQL error: %s\n", zErrMsg);
	sqlite3_free(zErrMsg);
 }
 // creacion indice ELO.
 rc = sqlite3_exec(db, createIndexElo, callback, 0, &zErrMsg);
 if( rc!=SQLITE_OK ){
	fprintf(stderr, "SQL error: %s\n", zErrMsg);
	sqlite3_free(zErrMsg);
 }
 // creacion indice fileid-particion
 rc = sqlite3_exec(db, createIndexPart, callback, 0, &zErrMsg);
 if( rc!=SQLITE_OK ){
	fprintf(stderr, "SQL error: %s\n", zErrMsg);
	sqlite3_free(zErrMsg);
 }
 // modo 'master'.
 if(strstr(argv[2],"master") != NULL)
 {
	 // creamos tabla 'particiones'
	 rc = sqlite3_exec(db, createParticiones, callback, 0, &zErrMsg);
	 if( rc!=SQLITE_OK ){
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	 }
	 // creamos indice por 'fileid'
	 rc = sqlite3_exec(db, createIndexFech, callback, 0, &zErrMsg);
	 if( rc!=SQLITE_OK ){
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	 }
 }
 sqlite3_close(db);
 return 0;
}
