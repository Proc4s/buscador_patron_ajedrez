package Interfaces;

// estructura que detalla un movimiento en binario.
public class MOVBIN_t
{
    private int	piezaorg;	// pieza origen.
	private int	piezades;	// pieza destino.
	private int	origen;	    // posicion origen absoluta de la pieza (0:63).
	private int	destino;    // posicion destino absoluta de la pieza (0:63). 
    
    public int getPiezaorg() {
        return piezaorg;
    }
    public void setPiezaorg(int piezaorg) {
        this.piezaorg = piezaorg;
    }
    public int getPiezades() {
        return piezades;
    }
    public void setPiezades(int piezades) {
        this.piezades = piezades;
    }
    public int getOrigen() {
        return origen;
    }
    public void setOrigen(int origen) {
        this.origen = origen;
    }
    public int getDestino() {
        return destino;
    }
    public void setDestino(int destino) {
        this.destino = destino;
    }
    public byte[] getByteMOVBIN_t()
    {
		byte[] aux = new byte[4];

		aux[0] = (byte) (piezaorg & 0xFF);
        aux[1] = (byte) (piezades & 0xFF);
        aux[2] = (byte) (origen & 0xFF);
        aux[3] = (byte) (destino & 0xFF);

		return aux;
	}
}