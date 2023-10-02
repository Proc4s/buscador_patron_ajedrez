package Interfaces;

// Movimiento de una pieza.
public class MOV_t 
{
    private int pieza;	// codigo de pieza en destino 
	private int org;	// indice origen.(0-63)
	private int dest;	// indice destino.(0-63)
    
    public int getPieza() {
        return pieza;
    }
    public void setPieza(int pieza) {
        this.pieza = pieza;
    }
    public int getOrg() {
        return org;
    }
    public void setOrg(int org) {
        this.org = org;
    }
    public int getDest() {
        return dest;
    }
    public void setDest(int dest) {
        this.dest = dest;
    }
}
