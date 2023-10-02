// modulo : sellistapart.c
// autor  : Antonio Pardo Redondo
//
// Utilidad para generar la lista de particiones a procesar seleccionando
// por fileid en la tabla de particiones de la base master.
//
// utiliza la variable de entorno $PATHAJEDREZ para localizar la base.
// recibe como parametros fileidmin y fileidmaxla salida se genera por 'stdout'.
// si fileidmax es cero selecciona desde fileidmin hasta el maximo registrado en la base,
// ambos a cero indica toda la base sin restricciones.
//
#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>

	
// sentencia SQL para crear la tabla de particiones en modo 'master'.
char seltodo[] = "SELECT * FROM \"particiones\")";


// Funcion de callback para recoger el resultado de la ejecucion de la sentencia SQL.	
static int callback(void *NotUsed, int argc, char **argv, char **azColName){

 printf("%s,%s,%s\n",argv[0],argv[1],argv[2]);
 return 0;
}

int main(int argc, char **argv){
 sqlite3 *db;
 char *zErrMsg = 0;
 int rc;
 char sql[200];
 int fileidmin;
 int fileidmax;

 if( argc!=4 ){
	fprintf(stderr, "Usage: %s <pathdatabase> <fileidmin> <fileidmax>\n", argv[0]);
	return(1);
 }
 fileidmin = atoi(argv[2]);
 fileidmax = atoi(argv[3]);
 
 // apertura de la base
 rc = sqlite3_open(argv[1], &db);
 if( rc ){
	fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
	sqlite3_close(db);
	return(1);
 }
 if((fileidmin == 0) && (fileidmax == 0))
 {
	 sprintf(sql,"SELECT * FROM \"particiones\";");
 }
 else if(fileidmax == 0)
 {
	 sprintf(sql,"SELECT * FROM \"particiones\" WHERE \"fileid\" >= %d;",fileidmin);
 }
 else
	sprintf(sql,"SELECT * FROM \"particiones\" WHERE \"fileid\" >= %d AND \"fileid\" <= %d;",fileidmin,fileidmax);
 
 rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);

 sqlite3_close(db);
 return 0;
}
