import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Scanner;
import Interfaces.CPARTIDA_t;
import Interfaces.FLAGS_t;
import Interfaces.MOVBIN_t;
import Interfaces.MOV_t;
import Interfaces.PIEZAS_t;
import Interfaces.TABLERO_t;

public class GenBaseMain
{
    // este programa covierte la información de una base de partidas de ajedrez descritas en formato PNG 
    // a un formato binario mas tratable por medios informaticos.
    // Cada partida en la base PGN esta descrita por una serie de metadatos con mucha informacion 
    // sobre el lugar, fecha, contendientes, clasificación de los contendientes (puntos ELO) ganador
    // terminacion etc y una linea que describe cada uno de los movimientos efectuados junto con anotaciones
    // sobre cronometros y otras cosas.
    // se trata de recoger solo la informacion que nos interesa de los metadatos como el elo medio de los
    // contendientes, ganador de la partida, terminacion de esta e indice de la partida en el ficheo original PGN.
    // El tratamiento mas critico se produce en el analisis y codificacion de los movimientos de la partida.
    // La notacion PGN codifica el movimiento con el minimo numero de caracteres posible especificando en 
    // muchos casos solo el tipo de pieza que se mueve y su destino, el origen del movimiento solo puede ser determinado
    // si hay varias piezas con posibilidades, aplicando las reglas del ajedrez, analizando segun la forma de
    // moverse cada pieza cuales tienen la posibilidad de moverse a la posicion destino, descartando las que
    // tienen piezas interpuestas en su camino (escepto caballos) y las que al moverse dejan en jaque a su rey.
    // 
    // La idea es determinar en esta fase la posicion origen del movimiento, codificando en binario la pieza
    // que se mueve, su posicion origen y su posición destino. En la explotacion de esta base se trata de recrear
    // la partida movimiento a movimiento en un tablero virtual para comprobar la existencia de algun patron.
    // La recreacion del movimiento con esta anotacion es inmediata, basta con vaciar la casilla origen y colocar
    // en la casilla destino la pieza movida.
    //
    // Ademas al codificar las cosas en binario compactamos la informacion y evitamos utilizar continuamente
    // funciones para extraer la posicion numerica a partir de un string ASCII. Todo ello acelera considerablemente
    // la recreacion informatica de una partida.
    //
    // En la notacion PGN el tablero se ve con las piezas blancas en la parte inferior y las negras en la superior.
    // las filas se denotan con un numero (1:8) comenzando en la fila inferior y las columnas con una letra (a:h)
    // comenzando en la columna de la izquierda.
    // Las piezas en PGN se nombran por sus iniciales en ingles: K=rey, Q=reina, R=torre, N=caballo, B=alfil, P=peon.
    // aunque los peones no se suelen indicar. Posibles anotaciones serian:
    //
    // Ra4 => una torre se mueve a la posicion 'a4'.
    // Raa4 => la torre que esta en la columna 'a' se mueve a la posicion 'a4'
    // R4a4 => la torre que esta en la fila 4 se mueve a posicion 'a4'.
    // Rc4a4 => La torre que esta en 'c4' se mueve a la posicion 'a4.
    // a4 => un peon se mueve a la posicion 'a4'.
    // O-O => enroque corto.
    // O-O-O => enroque largo.
    // Rxa4 => una torre come al peon situado en 'a4'.
    // a8=R => el peon se mueve a 'a8' y promociona a torre.
    //
    // En la base binaria las posiciones son absolutas (0:63) coincidiendo el '0' con 'a8' y 63 con 'h1'.
    // de la posicion absoluta se puede deducir la fila = posicion/8. Y la columna como posicion%8. en nuestro
    // sistema la dila cero es la 8 del PGN y la columna cero es la 'a' del PGN.
    //
    // En las diversas funciones de este proceso tambien se utilizan las posiciones absolutas indicadas.

    // codificacion de las piezas
    static final int NADA = 0;
    static final int PEON = 1;
    static final int CABALLO = 2;
    static final int ALFIL = 3;
    static final int TORRE = 4;
    static final int REINA = 5;
    static final int REY = 6;
    static final int NEGRA = 0x8;
    static final int INVAL = 0xf;

    // La codificacion de una pieza ocupa tres bits y en el cuarto se codifica el color (NEGRA o no).
    // por lo tanto una pieza con su color se codifica en 4 bits.

    // el tablero se define por 64 bytes cuyo valor es la pieza que contiene.
    // esta es el contenido del tablero virtual al comienzo de una partida.
    static final int[] tabini = {0x0c,0x0a,0x0b,0x0d,0x0e,0x0b,0x0a,0x0c,
                                0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,
                                0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                                0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                                0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                                0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                                0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
                                0x04,0x02,0x03,0x05,0x06,0x03,0x02,0x04};

    //================== Estructuras de la salida binaria ===================
    static final int MAXMOV = 1000;    // maximo numero movimientos en una partida.
    static final int MAGIC	= 0x55AA;
     
    // Registro de partida. Consiste en una cabecera de partida seguida por un array de movimientos
    // El numero de movimientos en el array esta indicado en cabpartida.nmov.
    static CPARTIDA_t	cabpartida;
    static MOVBIN_t movimientos[] = new MOVBIN_t[MAXMOV];

    //=========================================================================		

    static int partidas = 0;		// Indice de partida en curso.
    static int npgn;				        // numero de movimiento en partida.
    static String gpgn = "";	// texto movimiento 
    static String gpgna = "";   


static void printPieza(int pieza)
{
	switch(pieza & 0x7)
	{
		case REY:
			if((pieza & NEGRA)>0)
				System.out.printf("K");
			else
				System.out.printf("k");
			break;
		case REINA:
			if((pieza & NEGRA)>0)
				System.out.printf("Q");
			else
				System.out.printf("q");
			break;
		case TORRE:
			if((pieza & NEGRA)>0)
				System.out.printf("R");
			else
				System.out.printf("r");
			break;
		case ALFIL:
			if((pieza & NEGRA)>0)
				System.out.printf("B");
			else
				System.out.printf("b");
			break;
		case CABALLO:
			if((pieza & NEGRA)>0)
				System.out.printf("N");
			else
				System.out.printf("n");
			break;
		case PEON:
			if((pieza & NEGRA)>0)
				System.out.printf("P");
			else
				System.out.printf("p");
			break;
		default:
			System.out.printf(".");
			break;
	}
}

static void showtab(int[] tab)
{
	int i;
	System.out.printf("\n   PARTIDA:%d MOV:%d pgn:%s\n",partidas,npgn,gpgn);
	System.out.printf("\n    a b c d e f g h\n");
	
	System.out.printf("\n 8  ");
	for(i=0;i<8;i++)
	{
		printPieza(tab[i]);
		System.out.printf(" ");
	}
	System.out.printf(" 7\n");
	
	System.out.printf(" 7  ");
	for(i=8;i<16;i++)
	{
		printPieza(tab[i]);
		System.out.printf(" ");
	}
	System.out.printf(" 15\n");
	
	System.out.printf(" 6  ");
	for(i=16;i<24;i++)
	{
		printPieza(tab[i]);
		System.out.printf(" ");
	}
	System.out.printf(" 23\n");
	
	System.out.printf(" 5  ");
	for(i=24;i<32;i++)
	{
		printPieza(tab[i]);
		System.out.printf(" ");
	}
	System.out.printf(" 31\n");
	
	System.out.printf(" 4  ");
	for(i=32;i<40;i++)
	{
		printPieza(tab[i]);
		System.out.printf(" ");
	}
	System.out.printf(" 39\n");
	
	System.out.printf(" 3  ");
	for(i=40;i<48;i++)
	{
		printPieza(tab[i]);
		System.out.printf(" ");
	}
	System.out.printf(" 47\n");
	
	System.out.printf(" 2  ");
	for(i=48;i<56;i++)
	{
		printPieza(tab[i]);
		System.out.printf(" ");
	}
	System.out.printf(" 55\n");
	
	System.out.printf(" 1  ");
	for(i=56;i<64;i++)
	{
		printPieza(tab[i]);
		System.out.printf(" ");
	}
	System.out.printf(" 63\n\n");
}
    // inicia las estructuras de partida binaria.
    private static void iniPart()
    {
        cabpartida = new CPARTIDA_t();
        cabpartida.setMagic(MAGIC);
    }

    // funcion para anhadir un movimiento a la lista de movimientos recodificados
    // para el registro de la base binaria.
    static void anadeMov(MOVBIN_t mov)
    {
        movimientos [cabpartida.getNmov()] = mov; // anhade movimiento
        cabpartida.setNmov(cabpartida.getNmov()+1);	// actualiza numero movimientos.
    }

    // funcion para volcar a la base de datos binaria, la partida en curso ya recodificada.
    static void vuelcaPart(BufferedOutputStream bufferedOutputStream)
    {       
        try {
            // Escribir en el archivo la cabecera
            byte[] bytesCabecera = cabpartida.getBytesCPARTIDA_t();
            bufferedOutputStream.write(bytesCabecera);

            // Escribir los movimientos
            for(int i = 0; i < cabpartida.getNmov();i++)
            {
                byte[] bytesMovimiento = movimientos[i].getByteMOVBIN_t();
                bufferedOutputStream.write(bytesMovimiento);
            }
       
        } catch (IOException e) {
            System.err.println("Número de partidas: " + partidas);
            System.err.printf("\n Fallo en escritura: %s \n\n",e.getMessage());
            System.exit(2);
        }
    }

    // funcion para mostrar una traza.
    static private void debug(String mensa,int nivel,MOV_t  mov,TABLERO_t  tab)
    {       
        if(nivel < 2)
            return;
        
        System.out.printf("\n%s\n",mensa);
        System.out.printf("Part=>%d, MOV=>%d, %s-%s\n",partidas,npgn,gpgna,gpgn);
        System.out.printf("MORG=>%d, MDEST=>%d, PIEZA=>%d\n",mov.getOrg(),mov.getDest(),mov.getPieza());
    }

    // inicia partida.
    // Inicia las estructuras de posicion de las piezas y el tablero virtual.
    static private TABLERO_t iniciaJuego()
    {
        TABLERO_t tab = new TABLERO_t();
        PIEZAS_t blancas = new PIEZAS_t();
        PIEZAS_t negras = new PIEZAS_t();

        blancas.setRey(60);  
        blancas.setReina(0,59);
        blancas.setNreina(1);      
        blancas.setTorre(0,56);
        blancas.setTorre(1,63);
        blancas.setNtorre(2);    
        blancas.setAlfil(0,58);
        blancas.setAlfil(1,61);
        blancas.setNalfil(2);
        blancas.setCaballo(0,57);   
        blancas.setCaballo(1,62);       
        blancas.setNcaballo(2);        
        blancas.setPeon(0,48);
        blancas.setPeon(1,49);
        blancas.setPeon(2,50);
        blancas.setPeon(3,51);
        blancas.setPeon(4,52);
        blancas.setPeon(5,53);
        blancas.setPeon(6,54);
        blancas.setPeon(7,55);
        blancas.setNpeon(8);
        tab.setBlancas(blancas);
        
        negras.setRey(4);
        negras.setReina(0,3);
        negras.setNreina(1);
        negras.setTorre(0,0);
        negras.setTorre(1,7);
        negras.setNtorre(2);
        negras.setAlfil(0,2);  
        negras.setAlfil(1,5);   
        negras.setNalfil(2);
        negras.setCaballo(0,1);
        negras.setCaballo(1,6);
        negras.setNcaballo(2);
        negras.setPeon(0,8); 
        negras.setPeon(1,9);
        negras.setPeon(2,10);
        negras.setPeon(3,11);
        negras.setPeon(4,12);
        negras.setPeon(5,13);
        negras.setPeon(6,14);
        negras.setPeon(7,15); 
        negras.setNpeon(8);
        tab.setNegras(negras);

        int[] tabiniCopia = new int[tabini.length];
        System.arraycopy(tabini, 0, tabiniCopia, 0, tabini.length);
        
        tab.setTab(tabiniCopia);

        return tab;
    }

    // Elimina una pieza de las estructuras correspondientes a piezas activas.
    static private void eliminaPieza(int pieza,int posicion,TABLERO_t tab)
    {
        int i;
        PIEZAS_t piezas;
        
        // determina color.
        if((pieza & NEGRA)!=0)
            piezas = tab.getNegras();
        else
            piezas = tab.getBlancas();
        
        // Modifica la lista correspondiente a la pieza eliminada.	
        switch(pieza & 0x7)
        {
            case REINA:
                // localiza la pieza a eliminar en la lista.
                for(i=0;i<piezas.getNreina();i++)
                {
                    if(piezas.getReina(i) == posicion)
                        break;
                }
                // la elimina desplazando el resto de piezas de la lista una posicion a la izqu.
                for(;i<(piezas.getNreina() - 1);i++)
                        piezas.setReina(i,piezas.getReina(i+1));               
                piezas.setNreina(piezas.getNreina()-1);	// Ajusta el numero de piezas activas.
                break;
            case TORRE:
                for(i=0;i<piezas.getNtorre();i++)
                {
                    if(piezas.getTorre(i) == posicion)
                        break;
                }
                for(;i<(piezas.getNtorre() - 1);i++)
                    piezas.setTorre(i,piezas.getTorre(i+1)); 
                piezas.setNtorre(piezas.getNtorre()-1);
                break;
            case CABALLO:
                for(i=0;i<piezas.getNcaballo();i++)
                {
                    if(piezas.getCaballo(i) == posicion)
                        break;
                }
                for(;i<(piezas.getNcaballo() - 1);i++)
                    piezas.setCaballo(i,piezas.getCaballo(i+1)); 
                piezas.setNcaballo(piezas.getNcaballo()-1);
                break;
            case ALFIL:
                for(i=0;i<piezas.getNalfil();i++)
                {
                    if(piezas.getAlfil(i) == posicion)
                        break;
                }
                for(;i<(piezas.getNalfil() - 1);i++)
                    piezas.setAlfil(i,piezas.getAlfil(i+1)); 
                piezas.setNalfil(piezas.getNalfil()-1);
                break;
            case PEON:
                for(i=0;i<piezas.getNpeon();i++)
                {
                    if(piezas.getPeon(i) == posicion)
                        break;
                }
                for(;i<(piezas.getNpeon() - 1);i++)
                    piezas.setPeon(i,piezas.getPeon(i+1)); 
                piezas.setNpeon(piezas.getNpeon()-1);
                break;
            default:
                break;
        }	
    }

    // Funcion para actualizar el tablero virtual con el nuevo movimiento.
    static void actTablero(MOV_t mov,TABLERO_t tab)
    {
        tab.setContenidoTab(mov.getOrg(), NADA);
        if(tab.getContenidoTab(mov.getDest()) != NADA)	// sustituye pieza.
        {
            eliminaPieza(tab.getContenidoTab(mov.getDest()) ,mov.getDest(),tab);
        }
        else
        {
            // movimiento de comida de peon sin pieza en destino => come al paso.
            if(((mov.getPieza() & 0x7) == PEON) && (Math.abs(mov.getDest() - mov.getOrg()) != 8) && (Math.abs(mov.getDest() - mov.getOrg()) != 16))
            {
                debug("Come al paso",1,mov,tab);
                
                if((mov.getPieza() & NEGRA) != 0)
                {
                    if(tab.getContenidoTab(mov.getDest() -8) == PEON)
                    {
                        eliminaPieza(tab.getContenidoTab(mov.getDest() -8),mov.getDest() -8,tab);
                        tab.setContenidoTab(mov.getDest() -8, NADA);
                    }
                    else
                        debug("NO AL PASO",2,mov,tab);
                }
                else
                {
                    if(tab.getContenidoTab(mov.getDest() +8) == (PEON | NEGRA))
                    {
                        eliminaPieza(tab.getTab()[mov.getDest() + 8],mov.getDest() +8,tab);
                        tab.setContenidoTab(mov.getDest() +8, NADA);
                    //	debug("fcome",10,mov,tab);
                    }
                    else
                        debug("NO AL PASO1",2,mov,tab);
                }
            }
        }
        tab.setContenidoTab(mov.getDest(), mov.getPieza()); // pone pieza en la casilla destino.
    }

    // Actualiza las estructuras y tablero virtual por un movimiento de rey.
    static private void mueveRey(MOV_t mov,TABLERO_t tab)
    {
        PIEZAS_t piezas;
        
        // determina la estructura a actualizar en funcion del color de la pieza.
        if((mov.getPieza() & NEGRA) != 0)
            piezas = tab.getNegras();
        else
            piezas = tab.getBlancas();	
        
        piezas.setRey(mov.getDest());	// modifica la posicion del rey.
        actTablero(mov,tab);			// actualiza tablero virtual.
    }

    // Actualiza las estructuras y tablero virtual por un movimiento de reina.
    static void mueveReina(MOV_t mov,TABLERO_t tab)
    {
        int i;
        PIEZAS_t piezas;
        
        // determina la estructura a actualizar en funcion del color de la pieza.
        if((mov.getPieza() & NEGRA) != 0)
            piezas = tab.getNegras();
        else
            piezas = tab.getBlancas();	

        // localiza la pieza que se mueve
        for(i=0;i<piezas.getNreina();i++)
        {
            if(piezas.getReina(i) == mov.getOrg())
            {
                piezas.setReina(i,mov.getDest());	// actualiza posicion de la pieza
                break;
            }
        }
        if(i == piezas.getNreina())
            debug("No encontrado reina ORG",3,mov,tab);
        actTablero(mov,tab);	// actualiza tablero virtual.
    }

    // Actualiza las estructuras y tablero virtual por un movimiento de torre.
    static void mueveTorre(MOV_t mov,TABLERO_t tab)
    {
        int i;
        PIEZAS_t piezas;
        
        if((mov.getPieza() & NEGRA) != 0)
            piezas = tab.getNegras();
        else
            piezas = tab.getBlancas();		

        for(i=0;i<piezas.getNtorre();i++)
        {
            if(piezas.getTorre(i) == mov.getOrg())
            {
                piezas.setTorre(i, mov.getDest());
                break;
            }
        }
        if(i == piezas.getNtorre())
            debug("No encontrado torre ORG",3,mov,tab);
        actTablero(mov,tab);
    }

    // Actualiza las estructuras y tablero virtual por un movimiento de alfil.
    static void mueveAlfil(MOV_t mov,TABLERO_t tab)
    {
        int i;
        PIEZAS_t piezas;
        
        if((mov.getPieza() & NEGRA) != 0)
            piezas = tab.getNegras();
        else
            piezas = tab.getBlancas();	

        for(i=0;i<piezas.getNalfil();i++)
        {
            if(piezas.getAlfil(i) == mov.getOrg())
            {
                piezas.setAlfil(i,mov.getDest());
                break;
            }
        }
        if(i == piezas.getNalfil())
            debug("No encontrado alfil ORG",3,mov,tab);
        actTablero(mov,tab);
    }

    // Actualiza las estructuras y tablero virtual por un movimiento de caballo
    static void mueveCaballo(MOV_t mov,TABLERO_t tab)
    {
        int i;
        PIEZAS_t piezas;
        
        if((mov.getPieza() & NEGRA) != 0)
            piezas = tab.getNegras();
        else
            piezas = tab.getBlancas();	

        for(i=0;i<piezas.getNcaballo();i++)
        {
            if(piezas.getCaballo(i) == mov.getOrg())
            {
                piezas.setCaballo(i, mov.getDest());
                break;
            }
        }
        if(i == piezas.getNcaballo())
            debug("No encontrado caballo ORG",3,mov,tab);
        actTablero(mov,tab);
    }

    // Actualiza las estructuras y tablero virtual por un movimiento de peon
    static void muevePeon(MOV_t mov,TABLERO_t tab)
    {
        int i;
        PIEZAS_t piezas;
        
        if((mov.getPieza() & NEGRA) != 0)
            piezas = tab.getNegras();
        else
            piezas = tab.getBlancas();	

        for(i=0;i<piezas.getNpeon();i++)
        {
            if(piezas.getPeon(i) == mov.getOrg())
            {
                piezas.setPeon(i, mov.getDest());
                break;
            }
        }
        if(i == piezas.getNpeon())
        {
            debug("No encontrado peon org",3,mov,tab);
        }
        actTablero(mov,tab);
    }

    // mueve una pieza en el tablero.
    static void mueve(MOV_t mov,TABLERO_t tab)
    {
        MOVBIN_t movimiento = new MOVBIN_t();	// Estructura binaria de movimiento.
        
        // actualiza movimiento binario.
        // movimiento.setPiezas((mov.getPieza() << 4) | (mov.getPieza() & 0xf));
        movimiento.setPiezaorg(mov.getPieza());
        movimiento.setPiezades(mov.getPieza());

        movimiento.setOrigen(mov.getOrg());
        movimiento.setDestino(mov.getDest());
        anadeMov(movimiento);	// anhade movimiento binario a la lista de movimientos de salida.
        
        // mueve pieza en estructuras de piezas activas y tablero virtual.
        switch(tab.getTab()[mov.getOrg()] & 0x7)
        {
            case REY:
                mueveRey(mov,tab);
                break;
            case REINA:
                mueveReina(mov,tab);
                break;
            case TORRE:
                mueveTorre(mov,tab);
                break;
            case ALFIL:
                mueveAlfil(mov,tab);
                break;
            case CABALLO:
                mueveCaballo(mov,tab);
                break;
            default:
                muevePeon(mov,tab);
                break;
        }
    }

    // transforma posicion geometrica a indice absoluto en tablero (0:63).
    static int transform(int x,int y)
    {
        return((8+'0'-y)*8+(x-'a'));
    }

    // determina si el movimiento de la pieza indicada puede hacer entrar a su rey en jaque
    // lo que indica que esta pieza no puede ser el origen del movimiento.
    // se utiliza para resolver la ambiguedad cuando dos piezas pueden ser el origen del
    // movimiento pero una de ellas lo tiene impedido por que pondria a su rey en jaque.
    //
    // rey => pos rey,pieza=> pos pieza, color=> color pieza, indest=> destino de la pieza.
    // Retorna cero si no y uno si el movimiento provoca jaque.
    static int posiblejaque(int rey,int pieza,int color,int indest,TABLERO_t tab)
    {
        int diffx,diffy;
        int fila,colum,filpieza,columpieza;
        int i,j;
        int tipo;
        int incx;
        int incy;
        // posicion destino pieza que se mueve.
        int destx = indest%8;
        int desty = indest/8;
        
        int sobrepasa;
        
        // determina fila y columna en que se encuentra el rey
        colum = rey % 8;
        fila = rey/8;
        
        // determina fila y columna en la que se encuentra la pieza.
        columpieza = pieza % 8;
        filpieza = pieza/8;
        
        // si la pieza que se mueve y el rey estan en la misma fila y el destino
        // 	de la pieza es tambien la misma fila, no hay cambio de amenaza.
        // 	si el destino es otra fila y hay una pieza interpuesta tampoco supone amenaza.
        //		si hay una pieza detras de la que se mueve (en la misma fila) y es de distinto
        //		color que el rey y se trata de una torre o una reyna hay amenaza.
        if(fila == filpieza) // pieza y rey en la misma fila.
        {
            if(fila == desty) // si la pieza se mueve en la misma fila no genera nueva amenaza.
                return 0;
            if(colum > columpieza)	// El rey se encuentra a la derecha de la pieza
                incx = -1;
            else
                incx = 1;
            sobrepasa = 0;
            
            // examinamos las posiciones desde el rey en direccion a la pieza
            // dentro de los limites del tablero.
            for(i=colum+incx;((i>= 0) && (i<8));i+=incx)
            {
                if(i==columpieza)	// alcanzamos la posicion de la pieza.
                {
                    sobrepasa = 1;
                    continue;
                }
                tipo = tab.getContenidoTab(fila*8+i);	// pieza que se encuentra en la casilla examinada.
                if(tipo != NADA)
                {
                    if(sobrepasa == 0)	// pieza interpuesta, no hay nueva amenaza.
                        return 0;
                    // si hemos sobrepasado la posicion de la pieza y la pieza encontrada
                    // es una reyna o una torre en la misma fila tenemos una amenaza.
                    if(color != (tipo & NEGRA))
                    {
                        tipo &= 0x7;
                        if((tipo == REINA) | (tipo == TORRE))
                            return 1;	// pieza amenaza.
                        return 0;		// pieza interpuesta.
                    }
                    return 0;	// pieza interpuesta.
                }
            }
            return 0;	// no se encuentra amenaza.
        }
        
        // si la pieza que se mueve y el rey estan en la misma columna y el destino
        // 	de la pieza es tambien la misma columna, no hay cambio de amenaza.
        // 	si el destino es otra columna y hay una pieza interpuesta tampoco supone amenaza.
        //		si hay una pieza detras de la que se mueve (en la misma columna) y es de distinto
        //		color que el rey y se trata de una torre o una reyna hay amenaza.
        if(colum == columpieza) // pieza y rey en la misma columna
        {
            if(colum == destx)
                return 0;
            if(fila > filpieza)
                incy = -1;
            else
                incy = 1;
            sobrepasa = 0;
            for(i=fila+incy;((i>= 0) && (i<8));i+= incy)
            {
                if(i==filpieza)
                {
                    sobrepasa = 1;
                    continue;
                }
                tipo = tab.getContenidoTab(i*8+colum);
                if(tipo != NADA)
                {
                    if(sobrepasa == 0)	// pieza interpuesta.
                        return 0;
                    if(color != (tipo & NEGRA))
                    {
                        tipo &= 0x7;
                        if((tipo == REINA) | (tipo == TORRE))
                            return 1;	// pieza amenaza.
                        return 0;		// pieza interpuesta.
                    }
                    return 0;	// pieza interpuesta.
                }
            }
            return 0;	// no se encuentra amenaza.
        }

        diffx = Math.abs(colum - columpieza);
        diffy = Math.abs(fila - filpieza);
        
        // si la pieza que se mueve y el rey estan en la misma diagonal y el destino
        // 	de la pieza es tambien la misma diagonal, no hay cambio de amenaza.
        // 	si el destino es fuera de la diagonal y hay una pieza interpuesta tampoco supone amenaza.
        //		si hay una pieza detras de la que se mueve (en la misma diagonal) y es de distinto
        //		color que el rey y se trata de un alfil o una reyna hay amenaza.
        if(diffx == diffy)	// pieza en diagonal al rey
        {
            // determina incx e incy para moverse por la diagonal, segun la diagonal que se trate.
            incx = colum - columpieza;
            incy = fila - filpieza;
            
            // el destino esta tambien en una diagonal.
            if(Math.abs(colum -destx) == Math.abs(fila -desty))
            {
                if(incx > 0)
                {
                    if((colum -destx) > 0)
                    {
                        if(incy > 0)
                        {
                            if((fila - desty) > 0)
                                return 0;	// pieza se mueve en la misma diagonal, no hay cambio de amenaza
                        }
                        else
                        {
                            if((fila - desty) < 0)
                                return 0;	// pieza se mueve en la misma diagonal, no hay cambio de amenaza
                        }
                    }
                }
                else
                {
                    if((colum -destx) < 0)
                    {
                        if(incy > 0)
                        {
                            if((fila - desty) > 0)
                                return 0;	// pieza se mueve en la misma diagonal, no hay cambio de amenaza
                        }
                        else
                        {
                            if((fila - desty) < 0)
                                return 0;	// pieza se mueve en la misma diagonal, no hay cambio de amenaza
                        }
                    }
                }
            }
            
            //  calculo de los incrementos para seguir sentido de las diagonales.
            if(fila > filpieza)
            {
                incy = -1;
                if(colum > columpieza)
                    incx = -1;
                else
                    incx = 1;
            }
            else
            {
                incy = 1;
                if(colum > columpieza)
                    incx = -1;
                else
                    incx = 1;
            }
            sobrepasa = 0;
            // recorremos la diagonal desde el rey en el sentido de la pieza.
            for(i=fila+incy,j=colum+incx;((i>=0) && (i<8) && (j>=0) && (j<8));i+=incy,j+=incx)
            {
                if((i==filpieza) && (j == columpieza))
                {
                    sobrepasa = 1;	// se alcanza la pieza.
                    continue;
                }
                tipo = tab.getTab()[i*8+j]; // pieza en la casilla considerada
                if(tipo != NADA)			// hay una pieza
                {
                    if(sobrepasa == 0)	// pieza interpuesta.
                        return 0;			// no hay amenaza por que hay una pieza interpuesta.
                    if(color != (tipo & NEGRA))
                    {
                        tipo &= 0x7;
                        if((tipo == REINA) || (tipo == ALFIL))
                            return 1;	// pieza amenaza.
                        return 0;		// pieza interpuesta.
                    }
                    return 0;	// pieza interpuesta.
                }
            }
            return 0; //No se encuentra amenaza.
        }
        return 0;	// la pieza que se mueve no esta ni en fila ni en columna ni en diagonal al rey => no supone amenaza.
    }

    // Funciones para determinar el origen de una pieza que se mueve.
    // si se trata de un rey el origen es la posicion actual anotada en la estructura.
    static int reymovorg(int color,TABLERO_t tab)
    {
        if(color == NEGRA)
            return(tab.getNegras().getRey());
        else
            return(tab.getBlancas().getRey());
    }

    // Funcion para determinar la posicion origen del movimiento de una reina.
    // tener en cuenta que puede haber hasta nueve reinas y pudiera darse la circunstancia
    // de que hasta ocho puedan moverse a la posicion destino indicada. Hay que determinar
    // cual es la unica que puede moverse a dicha posicion. Primero tiene que estar en la fila
    // o columna indicadas si se proporcionan estas, luego tiene que estar en la misma fila, columna 
    // o diagonal que el destino, no puede haber piezas interpuestas en su movimiento
    // y por ultimo tiene que ser un movimiento legal, es decir no puede 
    // provocar que su rey entre en jaque.
    // orgx tiene valor distinto a -1 si en el movimiento se indica la columna origen.
    // orgy tiene valor distinto a -1 si en el movimiento se indica la fila origen.
    // la funcion retorna la posicion origen absoluta del movimiento.
    static int reinamovorg(int indest,int orgx,int orgy,int color,TABLERO_t tab)
    {
        PIEZAS_t piezas;
        int i;
        int destx = indest%8;	        // columna destino
        int desty = indest/8;	        // fila destino.
        int[] posible = new int[10];	// lista de posibles candidatas.
        int nposible = 0;		        // numero de posibles candidatas.
        int[] posjaque= new int[10];	// 
        int nposjaque = 0;
        
        // segun el color selecciona las estructuras de posicion a utilizar.
        if(color == NEGRA)
            piezas = tab.getNegras();
        else
            piezas = tab.getBlancas();
            
        if((orgx >= 0) && (orgx < 8)) // se proporciona columna.
        {
            if((orgy >= 0) && (orgy < 8)) // se proporciona fila.
                return(orgy*8 + orgx); // la orden de movimiento incluye el origen del mismo.
            else  // buscamos reina que esta en la misma columna.
            {
                for(i=0;i<piezas.getNreina();i++)
                {
                    if(piezas.getReina(i)%8 == orgx)
                    {
                        int diffx = Math.abs(destx - orgx);
                        int diffy = Math.abs(desty - piezas.getReina(i)/8);
                        
                        if((diffx == 0) || (diffy == 0) || (diffx == diffy)) // reina en misma columna, anotamos como posible
                        {
                            posible[nposible] = piezas.getReina(i);
                            nposible++;
                        }
                    }
                }
            }
        }
        else if((orgy >= 0) && (orgy < 8)) // se proporciona fila.
        {
            for(i=0;i<piezas.getNreina();i++)
            {
                if(piezas.getReina(i)/8 == orgy)
                {
                    int diffx = Math.abs(destx - piezas.getReina(i)%8);
                    int diffy = Math.abs(desty - orgy);
                    
                    if((diffx == 0) || (diffy == 0) || (diffx == diffy)) // reina en la misma fila, anotamos como posible
                    {
                        posible[nposible] = piezas.getReina(i);
                        nposible++;
                    }
                }
            }
        }
        else  // no se proporciona ni fila ni columna.
        {
            
            // tiene que estar en la misma columna o fila o diagonal del destino.
            // y no tiene que haber piezas interpuestas entre origen y destino.
            for(i=0;i<piezas.getNreina();i++)
            {
                int tmpx = piezas.getReina(i)%8;	// columna de la reina que consideramos.
                int tmpy = piezas.getReina(i)/8;	// fila de la reina que consideramos.
                int diffx = Math.abs(destx - tmpx);	// diferencia de columna entre la reina considerada y el destino.
                int diffy = Math.abs(desty - tmpy);	// diferencia de fila entre la reina considerada y el destino.
                
                // posible reina pues esta en la misma fila o columna o en una diagonal
                // despues hay que verificar que no hay piezas interpuestas.
                if((diffx == 0) || (diffy == 0) || (diffx == diffy)) // anotamos como posible.
                {
                    posible[nposible] = piezas.getReina(i);
                    nposible++;
                }
            }
        }
        // eleccion entre las posibles.
        if(nposible == 0)	// pieza no encontrada.
            return -1;
        if(nposible == 1)	// solo uno posible.
            return(posible[0]);
        else  // varios posibles, hay que verificar piezas interpuestas.
        {
            for(i=0;i<nposible;i++)
            {
                int tmpx = posible[i]%8; // columna pieza ensayada
                int tmpy = posible[i]/8; // fila pieza ensayada
                int incx;
                int incy;
                int x,y;
                
                // determinamos el sentido de los incrementos para movernos hacia el destino.
                if(tmpx == destx)
                {
                    incx = 0;
                }
                else if(tmpx > destx)
                    incx = -1;
                else
                    incx = 1;
                if(tmpy == desty)
                    incy = 0;
                else if(tmpy > desty)
                    incy = -1;
                else
                    incy = 1;

                for(x=tmpx+incx,y=tmpy+incy;(x != destx) || (y != desty);x+=incx,y+=incy)
                {
                    if(tab.getContenidoTab(y*8+x) != NADA)
                        break;
                }
                if((x == destx) && (y==desty))	// no ha encontrado piezas inter. la anotamos para verificar que no provoca jaque de su rey al moverse
                {
                    posjaque[nposjaque] = posible[i];
                    nposjaque++;
                }
            }
            if(nposjaque == 0)
                return -1;		// no se encuentra ninguna con posibilidades.
            if(nposjaque == 1)
                return(posjaque[0]);	// encontrada solo una como posible.
            else   	// desambiguacion por jaque. simple, la primera que cumpla
            {
                for(i=0;i<nposjaque;i++)
                    {
                        if(posiblejaque(piezas.getRey(),posjaque[i],color,indest,tab) == 0) // pieza no incurre en jaque de su rey al moverse.
                        return(posjaque[i]);
                }
                return(posjaque[0]);	// si ninguna devolvemos la primera.
            }
        }
    }

    // Funcion para determinar la posicion origen del movimiento de una torre.
    // tener en cuenta que puede haber hasta diez torres y pudiera darse la circunstancia
    // de que hasta cuatro puedan moverse a la posicion destino indicada. Hay que determinar
    // cual es la unica que puede moverse a dicha posicion. Primero tiene que estar en la fila
    // o columna indicadas si se proporcionan estas, luego tiene que estar en la misma fila o columna 
    // que el destino, no puede haber piezas interpuestas en su movimiento y por ultimo tiene que 
    // ser un movimiento legal, es decir no puede provocar que su rey entre en jaque.
    // orgx tiene valor distinto a -1 si en el movimiento se indica la columna origen.
    // orgy tiene valor distinto a -1 si en el movimiento se indica la fila origen.
    // la funcion retorna la posicion origen absoluta del movimiento.
    static int torremovorg(int indest,int orgx,int orgy,int color,TABLERO_t tab)
    {
        PIEZAS_t piezas;
        int i;
        int destx = indest%8;	// columna destino
        int desty = indest/8;	// fila destino
        int[] posible = new int[10];			// lista de posibles piezas origen.
        int nposible = 0;			// numero de posibles piezas origen.
        int[] posjaque = new int[10];			// lista posibles piezas a comprobar jaque propio
        int nposjaque = 0;		// numero de piezas a comprobar jaque propio.
        
        if(color == NEGRA)
            piezas = tab.getNegras();
        else
            piezas = tab.getBlancas();
            
        if((orgx >= 0) && (orgx < 8)) // se proporciona columna.
        {
            if((orgy >= 0) && (orgy < 8)) // se proporciona fila.
                return(orgy*8 + orgx);
            else  // buscamos torre que esta en la misma columna.
            {
                for(i=0;i<piezas.getNtorre();i++)
                {
                    if(piezas.getTorre(i)%8 == orgx) // ademas debe estar en la misma fila o columna que el destino.
                    {
                        if((destx == orgx) || (desty == piezas.getTorre(i)/8))
                        {
                            posible[nposible] = piezas.getTorre(i);	// anotamos posible candidata.
                            nposible++;
                        }
                    }
                }
            }
        }
        else if((orgy >= 0) && (orgy < 8)) // se proporciona fila.
        {
            for(i=0;i<piezas.getNtorre();i++)
            {
                if(piezas.getTorre(i)/8 == orgy)
                {
                    if((desty == orgy) || (destx == piezas.getTorre(i)%8)) // misma fila, anotamos posible candidata.
                    {
                        posible[nposible] = piezas.getTorre(i);
                        nposible++;
                    }
                }
            }
        }
        else  // no se proporciona ni fila ni columna.
        {
            
            // tiene que estar en la misma columna o fila del destino.
            for(i=0;i<piezas.getNtorre();i++)
            {
                int tmpx = piezas.getTorre(i)%8;   // columna de la candidata
                int tmpy = piezas.getTorre(i)/8;	// fila de la candidata.
                
                if((tmpx == destx) || (tmpy == desty)) // posible torre hay que verificar que no hay piezas interpuestas.
                {
                    posible[nposible] = piezas.getTorre(i);	// anotamos posible candidata.
                    nposible++;
                }
            }
        }
        // eleccion entre las posibles.
        if(nposible == 0)
            return -1;	// no se ha encontrado ninguna posible candidata.
        if(nposible == 1)	// solo uno posible.
            return(posible[0]);
        else  // varios posibles, hay que verificar piezas interpuestas.
        {
            for(i=0;i<nposible;i++)
            {
                int tmpx = posible[i]%8;	// columna de la candidata
                int tmpy = posible[i]/8;	// fila de la candidata.
                int incx;
                int incy;
                int x,y;
                
                // determinamos los incrementos a aplicar para movernos desde el destino a la pieza a ensayar.
                if(tmpx == destx)
                {
                    incx = 0;
                    if(tmpy > desty)
                        incy = -1;
                    else
                        incy = 1;
                }
                else
                {
                    incy = 0;
                    if(tmpx > destx)
                        incx = -1;
                    else
                        incx = 1;
                }
                // nos movemos desde el destino a la pieza ensayada buscando posibles piezas interpuestas.
                for(x=tmpx+incx,y=tmpy+incy;(x != destx) || (y != desty);x+=incx,y+=incy)
                {
                    if(tab.getContenidoTab(y*8+x) != NADA)
                        break;
                }
                if((x == destx) && (y==desty))	// no ha encontrado piezas inter.
                {
                    posjaque[nposjaque] = posible[i];	// anotamos como posible para comprobar mov ilegal por jaque.
                    nposjaque++;
                }
            }
            if(nposjaque == 0)	// no se ha encontrado ninguna pieza posible.
                return -1;
            if(nposjaque == 1)	// se ha encontrado solo una posible.
                return(posjaque[0]);
            else   	// desambiguacion por jaque. simple, la primera que cumpla
            {
                for(i=0;i<nposjaque;i++)
                {
                    if(posiblejaque(piezas.getRey(),posjaque[i],color,indest,tab) == 0) // no provoca jaque propio al moverse
                        return(posjaque[i]);
                }
                return(posjaque[0]);	// si ninguna devolvemos la primera.
            }
        }
    }

    // Funcion para determinar la posicion origen del movimiento de alfil.
    // tener en cuenta que puede haber hasta diez alfiles y pudiera darse la circunstancia
    // de que hasta cuatro puedan moverse a la posicion destino indicada. Hay que determinar
    // cual es el unico que puede moverse a dicha posicion. Primero tiene que estar en la fila
    // o columna indicadas si se proporcionan estas, luego tiene que estar en una 
    // diagonal del destino, no puede haber piezas interpuestas en su movimiento
    // y por ultimo tiene que ser un movimiento legal, es decir no puede 
    // provocar que su rey entre en jaque.
    // orgx tiene valor distinto a -1 si en el movimiento se indica la columna origen.
    // orgy tiene valor distinto a -1 si en el movimiento se indica la fila origen.
    // la funcion retorna la posicion origen absoluta del movimiento.
    static int alfilmovorg(int indest,int orgx,int orgy,int color,TABLERO_t tab)
    {
        PIEZAS_t piezas;
        int i;
        int destx = indest%8;
        int desty = indest/8;
        int[] posible = new int[10];
        int nposible = 0;
        int[] posjaque = new int[10];
        int nposjaque = 0;
        
        if(color == NEGRA)
            piezas = tab.getNegras();
        else
            piezas = tab.getBlancas();
            
        if((orgx >= 0) && (orgx < 8)) // se proporciona columna.
        {
            if((orgy >= 0) && (orgy < 8)) // se proporciona fila.
                return(orgy*8 + orgx);
            else  // buscamos alfil que esta en la misma columna.
            {
                for(i=0;i<piezas.getNalfil();i++)
                {
                    if(piezas.getAlfil(i)%8 == orgx)
                    {
                        int diffx = Math.abs(destx - orgx);
                        int diffy = Math.abs(desty - piezas.getAlfil(i)/8);
                        
                        if(diffx == diffy)
                        {
                            posible[nposible] = piezas.getAlfil(i);
                            nposible++;
                        }
                    }
                }
            }
        }
        else if((orgy >= 0) && (orgy < 8)) // se proporciona fila.
        {
            for(i=0;i<piezas.getNalfil();i++)
            {
                if(piezas.getAlfil(i)/8 == orgy)
                {
                    int diffx = Math.abs(destx - piezas.getAlfil(i)%8);
                    int diffy = Math.abs(desty - orgy);
                        
                    if(diffx == diffy)
                    {
                        posible[nposible] = piezas.getAlfil(i);
                        nposible++;
                    }
                }
            }
        }
        else  // no se proporciona ni fila ni columna.
        {		
            // tiene que estar en una diagonal del destino.
            for(i=0;i<piezas.getNalfil();i++)
            {
                int tmpx = piezas.getAlfil(i)%8;
                int tmpy = piezas.getAlfil(i)/8;
                int diffx = Math.abs(tmpx - destx);
                int diffy = Math.abs(tmpy - desty);
                
                if(diffx == diffy) // posible alfil hay que verificar que no hay piezas interpuestas.
                {
                    posible[nposible] = piezas.getAlfil(i);
                    nposible++;
                }
            }
        }
        // eleccion entre las posibles.
        if(nposible == 0)
            return -1;
        if(nposible == 1)	// solo uno posible.
            return(posible[0]);
        else  // varios posibles, hay que verificar piezas interpuestas y ambiguedad jaque
        {
            for(i=0;i<nposible;i++)
            {
                int tmpx = posible[i]%8;
                int tmpy = posible[i]/8;
                int incx;
                int incy;
                int x,y;
                
                if(tmpx > destx)
                    incx = -1;
                else
                    incx = 1;
                if(tmpy > desty)
                    incy = -1;
                else
                    incy = 1;
                for(x=tmpx+incx,y=tmpy+incy;x != destx;x+=incx,y+=incy)
                {
                    if(tab.getContenidoTab(y*8+x) != NADA)
                        break;
                }
                if((x == destx) && (y == desty))	// no ha encontrado piezas inter en la diagonal.
                {
                    posjaque[nposjaque] = posible[i];
                    nposjaque++;
                }
            }
            if(nposjaque == 0)
                return -1;
            if(nposjaque == 1)
                return(posjaque[0]);
            else   	// desambiguacion por jaque. simple, la primera que cumpla
            {
                for(i=0;i<nposjaque;i++)
                {
                    if(posiblejaque(piezas.getRey(),posjaque[i],color,indest,tab) == 0)
                        return(posjaque[i]);
                }
                return(posjaque[0]);	// si ninguna devolvemos la primera.
            }
        }
    }

    // Funcion para determinar la posicion origen del movimiento del caballo.
    // tener en cuenta que puede haber hasta diez caballos y pudiera darse la circunstancia
    // de que hasta ocho puedan moverse a la posicion destino indicada. Hay que determinar
    // cual es el unico que puede moverse a dicha posicion. Primero tiene que estar en la fila
    // o columna indicadas si se proporcionan estas, luego tiene que estar  
    // a distancia de caballo y por ultimo tiene que ser un movimiento legal, es decir no puede 
    // provocar que su rey entre en jaque.
    // orgx tiene valor distinto a -1 si en el movimiento se indica la columna origen.
    // orgy tiene valor distinto a -1 si en el movimiento se indica la fila origen.
    // la funcion retorna la posicion origen absoluta del movimiento.
    static int caballomovorg(int indest,int orgx,int orgy,int color,TABLERO_t tab)
    {
        PIEZAS_t piezas;
        int i;
        int destx,desty;
        int[] posjaque = new int[10];
        int nposjaque = 0;
        
        destx = indest % 8;
        desty = indest/8;
        
        if(color == NEGRA)
            piezas = tab.getNegras();
        else
            piezas = tab.getBlancas();
            
        if((orgx >= 0) && (orgx < 8)) // se proporciona columna.
        {
            if((orgy >= 0) && (orgy < 8)) // se proporciona fila.
                return(orgy*8 + orgx);
            else  // buscamos caballo que esta en la misma columna y puede acceder a destino.
            {
                for(i=0;i<piezas.getNcaballo();i++)
                {
                    if(piezas.getCaballo(i)%8 == orgx)
                    {
                        int filcab = piezas.getCaballo(i)/8;
                        int diffx = Math.abs(orgx - destx);
                        int diffy = Math.abs(filcab - desty);
                        
                        if((diffx == 1) && (diffy == 2))
                            return(piezas.getCaballo(i));
                        if((diffx == 2) && (diffy == 1))
                            return(piezas.getCaballo(i));
                    }
                }
            }
        }
        else if((orgy >= 0) && (orgy < 8)) // se proporciona fila.
        {
            for(i=0;i<piezas.getNcaballo();i++)
            {
                if(piezas.getCaballo(i)/8 == orgy)
                {
                    int colcab = piezas.getCaballo(i)%8;
                    int diffx = Math.abs(colcab - destx);
                    int diffy = Math.abs(orgy - desty);
                    
                    if((diffx == 1) && (diffy == 2))
                        return(piezas.getCaballo(i));
                    if((diffx == 2) && (diffy == 1))
                        return(piezas.getCaballo(i));
                }
            }
        }
        else  // no se proporciona ni fila ni columna.
        {
            for(i=0;i<piezas.getNcaballo();i++)
            {
                int diffx = Math.abs((piezas.getCaballo(i)%8) - destx);
                int diffy = Math.abs((piezas.getCaballo(i)/8) - desty);
                
                // puede haber ambiguedad por jaque
                if((diffx == 1) && (diffy == 2))
                {
                    posjaque[nposjaque] = piezas.getCaballo(i);
                    nposjaque++;
                }
                else if((diffx == 2) && (diffy == 1))
                {
                    posjaque[nposjaque] = piezas.getCaballo(i);
                    nposjaque++;
                }
            }
            if(nposjaque == 0)
                return -1;
            if(nposjaque == 1)
                return(posjaque[0]);
            for(i=0;i<nposjaque;i++)
            {
                if(posiblejaque(piezas.getRey(),posjaque[i],color,indest,tab) == 0)
                    return(posjaque[i]);
            }
            return(posjaque[0]);	// si ninguna devolvemos la primera.
        }
        return -1;
    }

    // Funcion para determinar la posicion origen del movimiento un peon.
    // si hay una comida puede haber hasta dos peones que pueden moverse al mismo destino.
    // si no hay comida solo puede haber uno.
    // orgx tiene valor distinto a -1 si en el movimiento se indica la columna origen.
    // orgy tiene valor distinto a -1 si en el movimiento se indica la fila origen.
    // la funcion retorna la posicion origen absoluta del movimiento.
    static int peonmovorg(int indest,int orgx,int come,int color,TABLERO_t tab)
    {
        PIEZAS_t piezas;
        int[] posjaque = new int[10];
        int nposjaque = 0;
        int i;
        
        // peon come, se mueve uno en diagonal.
        if(come > 0)
        {
            if(color == NEGRA)
            {
                // la fila sera una menos que la fila destino.
                if((orgx >=0) && (orgx<8)) // se proporciona columna origen
                    return(((indest/8)-1)*8 + orgx);
                else 
                {
                    // no se proporciona columna origen, 2 posibilidades.
                    // excepto que el destino sea un extremo (a,h) entonces
                    // solo hay una posibilidad.
                    if((indest%8) == 0)
                        return(indest-7);
                    if((indest%8) == 7)
                        return(indest-9);
                    // puede haber ambiguedad....
                    if(tab.getContenidoTab(indest -9) == (PEON | NEGRA))
                    {
                        posjaque[nposjaque] = indest -9;
                        nposjaque++;
                    }
                    else if(tab.getContenidoTab(indest -7) == (PEON | NEGRA))
                    {
                        posjaque[nposjaque] = indest -7;
                        nposjaque++;
                    }
                }
            }
            else  // blancas.
            {
                // la fila sera una mas que la fila destino.
                if((orgx >=0) && (orgx<8)) // se proporciona columna origen
                    return(((indest/8)+1)*8 + orgx);
                else  // no se proporciona columna origen, 2 posibilidades.
                {
                    // no se proporciona columna origen, 2 posibilidades.
                    // excepto que el destino sea un extremo (a,h) entonces
                    // solo hay una posibilidad.
                    if((indest%8) == 0)
                        return(indest+9);
                    if((indest%8) == 7)
                        return(indest+7);
                    // puede haber ambiguedad.
                    if(tab.getContenidoTab(indest +9) == PEON)
                    {
                        posjaque[nposjaque] = indest +9;
                        nposjaque++;
                    }
                    else if(tab.getContenidoTab(indest +7) == PEON)
                    {
                        posjaque[nposjaque] = indest +7;
                        nposjaque++;
                    }
                }
            }
        }
        // peon no come, se mantiene en columna y estara 1 o dos posiciones
        // de distancia
        else 
        {
            if(color == NEGRA)
            {
                if(tab.getContenidoTab(indest -8) == (PEON | NEGRA))
                    return(indest -8);
                else
                    return(indest -16);
            }
            else
            {
                if(tab.getContenidoTab(indest +8) == PEON)
                    return(indest +8);
                else
                    return(indest +16);	
            }
        }
        if(nposjaque == 0) // ninguno cumple condiciones
            return -1;
        if(nposjaque == 1)	// solo uno cumple condiciones.
            return(posjaque[0]);
            
        if(color == NEGRA)
            piezas = tab.getNegras();
        else
            piezas = tab.getBlancas();
        
        // comprobacion de que el movimiento es legal	
        for(i=0;i<nposjaque;i++)
        {
            if(posiblejaque(piezas.getRey(),posjaque[i],color,indest,tab) == 0)
                return(posjaque[i]);
        }
        return(posjaque[0]);
    }

    // calcula tipo, origen y destino del movimiento y mueve pieza.
    // pgn => string de movimiento.
    // color => color del movimiento.
    // come => indicacion de que este movimiento come una pieza.
    // mv => movimiento decodificado resultado de la funcion.
    // tab => situacion actual de las piezas.
    static void muevePieza(String pgn, int color,int come,MOV_t mov,TABLERO_t tab)
    {
        int orgx;
        int orgy;
        int pieza;
        int len;
        
        len = pgn.length();
        if((len > 5) || (len < 2)) // movimiento invalido.
        {
            debug("MOV-INVAL",4,mov,tab);
            mov.setPieza(INVAL);
            return;
        }
        int i = 0;
        switch(pgn.charAt(i))  // El primer caracter indica el tipo de pieza, excepto en peon
        {
            case 'K':
                pieza = (REY | color);
                i++;
                break;
            case 'Q':
                pieza = (REINA | color);
                i++;
                break;
            case 'R':
                pieza = (TORRE | color);
                i++;
                break;
            case 'B':
                pieza = (ALFIL | color);
                i++;
                break;
            case 'N':
                pieza = (CABALLO | color);
                i++;
                break;
            default:
                pieza = (PEON | color);
                len++;
                break;
        }
        mov.setPieza(pieza);
        len--;
        switch(len)
        {
            case 2:	// solo se indica posicion destino
                mov.setDest(transform(pgn.charAt(i),pgn.charAt(i+1)));
                orgx = -1;
                orgy = -1;
                break;
            case 3:	// se indica fila o columna destino
                mov.setDest(transform(pgn.charAt(i+1),pgn.charAt(i+2)));
                if(pgn.charAt(i) >='a')
                {
                    orgx = pgn.charAt(i) -'a';
                    orgy = -1;
                }
                else
                {
                    orgx = -1;
                    orgy = 8+ '0'-pgn.charAt(i);
                }
                break;
            default: // se indica origen y destino
                mov.setDest(transform(pgn.charAt(i+2),pgn.charAt(i+3)));
                mov.setOrg(transform(pgn.charAt(i),pgn.charAt(i+1)));
                orgx = mov.getOrg()%8;
                orgy = mov.getOrg()/8;
                break;
        }

        // calculamos el origen en funcion de la pieza, destino, color e indicaciones de origen
        switch(pieza & 0x7)	// pieza sin color.
        {
            case REY :
                mov.setOrg(reymovorg(pieza & NEGRA,tab));
                break;
            case REINA:
                mov.setOrg(reinamovorg(mov.getDest(),orgx,orgy,pieza & 0x8,tab));
                break;
            case TORRE:
                mov.setOrg(torremovorg(mov.getDest(),orgx,orgy,pieza & 0x8,tab));
                break;
            case ALFIL:
                mov.setOrg(alfilmovorg(mov.getDest(),orgx,orgy,pieza & 0x8,tab));
                break;
            case CABALLO:
                mov.setOrg(caballomovorg(mov.getDest(),orgx,orgy,pieza & 0x8,tab));
                break;
            default:
                mov.setOrg(peonmovorg(mov.getDest(),orgx,come,pieza & 0x8,tab));
                break;
        }
        if(mov.getOrg() == -1)	// no se ha encontrado origen valido
        {
            debug("deterORG inval",5,mov,tab);
        }
        mueve(mov,tab);	// mueve la pieza.
    }


    // funcion que realiza un enrroque largo.
    static void enroquel(TABLERO_t tab,int color)
    {
        int i;
        MOVBIN_t movimiento = new MOVBIN_t();
        
        if(color == NEGRA)
        {
            // movimiento.setPiezas(((TORRE | NEGRA) << 4) | ((TORRE | NEGRA) & 0xf));
            movimiento.setPiezaorg(TORRE | NEGRA);
            movimiento.setPiezades(TORRE | NEGRA);
            movimiento.setOrigen(0);
            movimiento.setDestino(3);
            anadeMov(movimiento);
            // movimiento.setPiezas(((REY | NEGRA) << 4) | ((REY | NEGRA) & 0xf));
            movimiento.setPiezaorg(REY | NEGRA);
            movimiento.setPiezades(REY | NEGRA);
            movimiento.setOrigen(4);
            movimiento.setDestino(2);
            anadeMov(movimiento);
            
            tab.setContenidoTab(4, NADA);
            tab.setContenidoTab(2, REY | NEGRA);
            tab.setContenidoTab(0, NADA);
            tab.setContenidoTab(3, TORRE | NEGRA);
            PIEZAS_t negras = tab.getNegras();
            negras.setRey(2);          
            for(i=0;i<tab.getNegras().getNtorre();i++)
            {
                if(tab.getNegras().getTorre(i) == 0)
                {
                    negras.setTorre(i,3);
                    return;
                }
            }
            tab.setNegras(negras);
        }
        else
        {
            // movimiento.setPiezas((TORRE << 4) | (TORRE & 0xf));
            movimiento.setPiezaorg(TORRE);
            movimiento.setPiezades(TORRE);
            movimiento.setOrigen(56);
            movimiento.setDestino(59);
            anadeMov(movimiento);
            // movimiento.setPiezas((REY << 4) | (REY & 0xf));
            movimiento.setPiezaorg(REY);
            movimiento.setPiezades(REY);
            movimiento.setOrigen(60);
            movimiento.setDestino(58);
            anadeMov(movimiento);
            
            tab.setContenidoTab(60, NADA);
            tab.setContenidoTab(58, REY);
            tab.setContenidoTab(56, NADA);
            tab.setContenidoTab(59, TORRE);
            PIEZAS_t blancas = tab.getBlancas();
            blancas.setRey(58);            
            for(i=0;i<tab.getBlancas().getNtorre();i++)
            {
                if(tab.getBlancas().getTorre(i) == 56)
                {
                    blancas.setTorre(i,59);
                    return;
                }
            }
            tab.setBlancas(blancas);
        }
    }


    // funcion que realiza un enroque corto.
    static void enroquec(TABLERO_t tab,int color)
    {
        int i;
        MOVBIN_t movimiento = new MOVBIN_t();
        PIEZAS_t negras = tab.getNegras();
        PIEZAS_t blancas = tab.getBlancas();
        
        if(color == NEGRA)
        {   
            // movimiento.setPiezas(((TORRE | NEGRA) << 4) | ((TORRE | NEGRA) & 0xf));
            movimiento.setPiezaorg(TORRE | NEGRA);
            movimiento.setPiezades(TORRE | NEGRA);
            movimiento.setOrigen(7);
            movimiento.setDestino(5);
            anadeMov(movimiento);
            // movimiento.setPiezas(((REY | NEGRA) << 4) | ((REY | NEGRA) & 0xf));
            movimiento.setPiezaorg(REY | NEGRA);
            movimiento.setPiezades(REY | NEGRA);
            movimiento.setOrigen(4);
            movimiento.setDestino(6);
            anadeMov(movimiento);
            
            Short rey = 6;
            tab.setContenidoTab(4, NADA);
            tab.setContenidoTab(6, REY | NEGRA);
            tab.setContenidoTab(7, NADA);
            tab.setContenidoTab(5, TORRE | NEGRA);
            negras.setRey(rey);
            tab.setNegras(negras);
            for(i=0;i<negras.getNtorre();i++)
            {
                if(negras.getTorre(i) == 7)
                {
                    negras.setTorre(i,5);
                    return;
                }
            }
            tab.setNegras(negras);
        }
        else
        {
            // movimiento.setPiezas((TORRE << 4) | (TORRE & 0xf));
            movimiento.setPiezaorg(TORRE);
            movimiento.setPiezades(TORRE);
            movimiento.setOrigen(63);
            movimiento.setDestino(61);
            anadeMov(movimiento);
            // movimiento.setPiezas((REY << 4) | (REY & 0xf));
            movimiento.setPiezaorg(REY);
            movimiento.setPiezades(REY);
            movimiento.setOrigen(60);
            movimiento.setDestino(62);
            anadeMov(movimiento);
            
            tab.setContenidoTab(60, NADA);
            tab.setContenidoTab(62, REY);
            tab.setContenidoTab(63, NADA);
            tab.setContenidoTab(61, TORRE);
            blancas.setRey(62);
            for(i=0;i<tab.getBlancas().getNtorre();i++)
            {
                if(blancas.getTorre(i) == 63)
                {
                    blancas.setTorre(i, 61);
                    return;
                }
            }
            tab.setBlancas(blancas);
        }
    }


    // funcion de promocion de peon. El peon ya ha movido
    // solo sustituir el peon en mov->dest por pieza de promocion mov->pieza.
    static void promoPieza(MOV_t mov,TABLERO_t tab)
    {
        int i;
        PIEZAS_t piezas;
        MOVBIN_t movimiento = new MOVBIN_t();
        
        cabpartida.setNmov(cabpartida.getNmov()-1); // anulamos ultimo movimiento anotado.
        // movimiento.setPiezas(((PEON | (mov.getPieza() & NEGRA)) << 4) | mov.getPieza() & 0xf);
        movimiento.setPiezaorg(PEON | (mov.getPieza() & NEGRA));
        movimiento.setPiezades(mov.getPieza());
        movimiento.setOrigen(mov.getOrg());
        movimiento.setDestino(mov.getDest());
        anadeMov(movimiento);
        
        // sustituir pieza en el tablero.
        tab.setContenidoTab(mov.getDest(), mov.getPieza());
        // dar de baja peon involucrado.
        if((mov.getPieza() & NEGRA )!= 0)
            piezas = tab.getNegras();
        else
            piezas = tab.getBlancas();
        
        // Eliminacion del peon.	
        for(i=0;i<piezas.getNpeon();i++)	// localizamos peon involucrado
        {
            if(piezas.getPeon(i) == mov.getDest())
                break;
        }
        for(i++;i<piezas.getNpeon();i++)	// desplazamos el resto de peones de la lista
        {
            piezas.setPeon(i-1, piezas.getPeon(i));
        }
        piezas.setNpeon(piezas.getNpeon() -1);
        
        // damos de alta la pieza sustituida.	
        switch(mov.getPieza() & 0x7)
        {
            case REINA:
                piezas.setReina(piezas.getNreina(), mov.getDest());
                piezas.setNreina(piezas.getNreina()+1);
                break;
            case TORRE:
                piezas.setTorre(piezas.getNtorre(), mov.getDest());
                piezas.setNtorre(piezas.getNtorre()+1);
                break;
            case ALFIL:
                piezas.setAlfil(piezas.getNalfil(), mov.getDest());
                piezas.setNalfil(piezas.getNalfil()+1);
                break;
            case CABALLO:
                piezas.setCaballo(piezas.getNcaballo(), mov.getDest());
                piezas.setNcaballo(piezas.getNcaballo()+1);
                break;
            default:
                debug("Promociona pieza invalida",6,mov,tab);
        }
    }


    // analiza string de movimiento y procesa movimientos especiales
    // como enrroque y promocion.
    // filtra signos especiales y efectua movimiento de la pieza.
    static void determov(String pgn, int color,MOV_t mov,TABLERO_t tab)
    {
        String pgntmp ="";
        String pgntmp1 = "";
        int come = 0;
        int pieza = NADA;
        int i;
        
        // procesamiento de enrroque.
        if(pgn.indexOf('O') != -1)
        {
            if(pgn.equals("O-O-O")) // enrroque largo
                enroquel(tab,color);
            else
                enroquec(tab,color);		// enrroque corto
            return;
        }
        // anotacion pieza comida y supresion indicacion ('x')de string.
        if((i = pgn.indexOf('x') )!= -1)
        {
            come = 1;
            pgntmp = pgn.substring(0, i) + pgn.substring(i + 1, pgn.length());

        }
        else
        {
            come = 0;
            pgntmp=pgn;
        }
        // procesamiento promocion.
        if((i = pgntmp.indexOf('=')) != -1)
        {
            pgntmp1 = pgntmp.substring(0, i);
            switch(pgntmp.charAt(i+1))
            {
                case 'Q':
                pieza = REINA | color;
                break;
            case 'R':
                pieza = TORRE | color;
                break;
            case 'B':
                pieza = ALFIL | color;
                break;
            case 'N':
                pieza = CABALLO | color;
                break;
            default:
                debug("Determov pieza invalida",7,mov,tab);
            }
            muevePieza(pgntmp1,color,come,mov,tab); // mueve peon
            mov.setPieza(pieza);
            promoPieza(mov,tab);	// promociona peon
            return;
        }
        muevePieza(pgntmp,color,come,mov,tab);
    }

    // static char[] copiarmovimiento(String texto)
    // {
    //     char[] tmp = new char[50];
    //     texto.getChars(0, Math.min(texto.length(), tmp.length), tmp, 0);
    //     return tmp;
    // }

    public static void main(String[] args) {
        TABLERO_t tablero;
        MOV_t mov = new MOV_t();
        FLAGS_t flags; 

        String linea = "";
        int color;
        long  movimientos = 0;
        int punte,punte1,punte2;
        String tmp ="";
    //---------------
        int elo1 = 0;
        int elo2 = 0;
        // int fd;


        // la invocacion es con el nombre del comando y el fichero de salida de la base binaria.
        // la entrada de datos se supone que se efectua por STDIN que proviene de una PIPE.
        // ejemplo:
        //  zstdcat file_png.zst | ./genbase file_salida_bin
        if(args.length < 1)
        {
            System.err.printf("Usage: java Main <filebin> < fileorg %n");
            System.exit(1);
        }

        // creamos fichero de salida.
        File archivo = new File(args[0]);
        archivo.setReadable(true, false); // Permisos de lectura para el propietario y grupo
        archivo.setWritable(true, false); // Permisos de escritura para el propietario y grupo
        archivo.setExecutable(false, false); // Sin permisos de ejecución para el propietario y grupo
        
        partidas = 0;

        try {  
            // Scanner scanner = new Scanner(System.in);
            File archivoLectura = new File(args[1]);
            archivoLectura.setReadable(true, false); // Permisos de lectura para el propietario y grupo  
            archivoLectura.setWritable(false, false); // Permisos de escritura para el propietario y grupo 

            //FileOutputStream fileOutputStream = new FileOutputStream(archivo, true);
            FileOutputStream fileOutputStream = new FileOutputStream(archivo);
            BufferedOutputStream bufferedOutputStream = new BufferedOutputStream(fileOutputStream);

            Scanner scanner = new Scanner(archivoLectura);


            while (scanner.hasNextLine())  	// lectura linea a linea hasta que no haya mas.
            {
                linea = scanner.nextLine();

                if(linea.startsWith("[Event"))	// linea comienzo de partida.
                {
                    partidas++;

                    tablero = iniciaJuego();	// inicia las estructuras del tablero virtual
                    iniPart();					// inicia las estructuras binarias de salida.
                    flags = cabpartida.getFlags();
                    
                    while(scanner.hasNextLine())	// sigue leyendo lineas.
                    {
                        linea = scanner.nextLine();
                        if(!linea.equals(""))
                        {
                            if(linea.startsWith("[Event"))	// comienzo de nueva partida.
                            {
                                partidas++;
                            }
                            if(linea.charAt(0) != '1')	// no se trata de la linea de movimientos.
                            {
                                if(linea.startsWith("[WhiteElo"))	// linea de elo de blancas.
                                {
                                    punte = linea.indexOf('"');	// el dato esta entre comillas.
                                    elo1 = Integer.parseInt(linea.substring(punte + 1, linea.indexOf('"', punte + 1)));
                                }
                                else if(linea.startsWith("[BlackElo"))	// linea de elo de negras.
                                {
                                    punte = linea.indexOf('"');	// el dato esta entre comillas.
                                    elo2 = Integer.parseInt(linea.substring(punte + 1, linea.indexOf("\"", punte + 1)));
                                }
                                else if(linea.startsWith("[Result"))	// linea de resultado de la partida.
                                {                                
                                    if(linea.indexOf("1-0") != -1)	// ganan blancas.
                                    {
                                        flags.setGanablanca(true);
                                        flags.setGananegra(false);
                                    }
                                    else if(linea.indexOf("0-1") != -1)	// ganan negras.
                                    {
                                        flags.setGanablanca(false);
                                        flags.setGananegra(true);
                                    }
                                    else if(linea.indexOf("1/2") != -1)	// tablas, anotamos que ganan ambos.
                                    {
                                        flags.setGanablanca(true);
                                        flags.setGananegra(true);
                                    }
                                }
                                else if(linea.startsWith("[Termination"))	// Terminacion de la partida.
                                {
                                    if(linea.indexOf("Normal") != -1)	// teminacion normal
                                        flags.setTnormal(true);
                                }
                                cabpartida.setFlags(flags);
                                continue;
                            }
                            
                            // Linea de movimientos..
                            cabpartida.setInd(partidas); 
                            cabpartida.setElomed((elo1+elo2)/2); 
                            
                            // recorremos todos los movimientos, blancas empiezan por n.. y negras por n... luego sigue la especificacion
                            // del movimiento y entre llaves anotaciones varias.
                            punte = 0;
                            while(true)
                            {
                                punte1 = linea.indexOf('{',punte);	// buscamos comienzo anotaciones
                                if(punte1 == -1)	// no encuentra anotaciones.
                                    break;

                                tmp = linea.substring(punte, punte1); // copia movimiento sin anotaciones a tmp.	
                                punte1 = linea.indexOf('}',punte1+1); // apunta a la llave de cierre de anotaciones.
                                
                                punte1+=2;	// apunta a siguiente movimiento.
                                    
                                // numero de movimiento
                                punte2 = tmp.indexOf('.',0);	// buscamos el final del numero de movimiento
                                if(punte2 == -1)	            // no encuentra el movimiento.
                                    System.exit(1);
                                try{
                                    npgn = Integer.parseInt(tmp.substring(0, punte2));
                                }catch(NumberFormatException e)
                                {
                                    // System.out.printf("\n Partida:%d Movimiento:%d tmp:%s \n",partidas,npgn,tmp);
                                    System.out.println("\n Partida:"+partidas+" ;Movimiento:"+ npgn +" ;tmp:"+ tmp +" \n");
                                    System.out.println("Mensaje de la excepción: " + e.getMessage());
                                } 
                                
                                if(tmp.indexOf("...") != -1)	// movimiento de negras
                                    color = 1;
                                else
                                    color = 0;
                                    
                                punte = tmp.indexOf(' '); // apunta al blanco de separacion entre numero de movimiento y texto de este.
                                punte++;						// apunta al texto del movimiento.
                                punte2 = tmp.indexOf(' ',punte); // apunta al final del texto del movimiento.
                                // recorremos el texto del movimiento desde el final hacia el principio.
                                // hasta encontrar una informacion de posicion (sera la destino) o una indicacion de pieza (promocion)o enrroque
                                // eliminamos resto de la cola.
                                for(;punte2 != punte;punte2--)		
                                {
                                    if((tmp.charAt(punte2) >= '0') && (tmp.charAt(punte2) <= '9'))	// encontrada indicacion de fila.
                                        break;
                                    if((tmp.charAt(punte2) == 'O') || (tmp.charAt(punte2) == 'Q') || (tmp.charAt(punte2) == 'R') | (tmp.charAt(punte2) == 'N') || (tmp.charAt(punte2) == 'B')) // encontrada indicacion de pieza o enrroque
                                        break;
                                }
                                // copiamos descripcion filtrada (eliminada cola descriptiva).
                                gpgn = tmp.substring(punte, punte2+1);                        
                                movimientos++;
                            
                                determov(gpgn,color*8,mov,tablero);	// interpretamos movimiento y lo anotamos tanto en tablero como en salida binaria.

                                if((movimientos%1000)== 0)	// si llevamos analizados 1000 movimientos sacamos traza de progreso informativa.
                                {
                                    System.out.printf("ind=>%d, mov=>%d         \r",partidas,movimientos);
                                    System.out.flush();
                                }
                                punte = punte1;
                            }
                            vuelcaPart(bufferedOutputStream);
                            break;
                        }                        
                    }
                }
            }
            scanner.close();
            bufferedOutputStream.close();
            fileOutputStream.close();   // cerramos fichero de base binaria generada.
        } catch (FileNotFoundException e) {
            System.err.printf("\nNo se encuentra el fichero de entrada\n\n");
            System.exit(2);
        } catch (IOException e) {
            System.err.println("Número de partidas: " + partidas);
            System.err.printf("\n Fallo en escritura: %s \n\n",e.getMessage());
            System.exit(2);
        }
        System.out.printf("\n");
    }
}
