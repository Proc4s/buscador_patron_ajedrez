package Interfaces;

// Estructura de cabecera de partida en binario	
public class CPARTIDA_t 
{
	// short reser;	// reservado para futuro uso.
    private int magic;	// magig number fijo a 0x55AA
	private FLAGS_t	flags = new FLAGS_t();	// flags de la partida.
	private int	nmov;		// numero de movimientos de la partida en lista de movimientos.
	private int elomed;	    // elo media de los jugadores,
	private int ind;		// indice de la partida en el fichero PGN original.
    
    public int getMagic() {
        return magic;
    }
    public void setMagic(int magic) {
        this.magic = magic;
    }
    public FLAGS_t getFlags() {
        return flags;
    }
    public void setFlags(FLAGS_t flags) {
        this.flags = flags;
    }
    public int getNmov() {
        return nmov;
    }
    public void setNmov(int nmov) {
        this.nmov = nmov;
    }
    public int getElomed() {
        return elomed;
    }
    public void setElomed(int elomed) {
        this.elomed = elomed;
    }
    public int getInd() {
        return ind;
    }
    public void setInd(int ind) {
        this.ind = ind;
    }

    public byte[] getBytesCPARTIDA_t()
    {
        byte[] magictobytes = Conversorbytes.int16tobyte(magic);
        byte flagbyte = flags.getByteFLAGS_t();
        byte[] nmovtobytes = Conversorbytes.int16tobyte(nmov);
        byte[] elomedtobytes = Conversorbytes.int16tobyte(elomed);
        byte[] indtobytes = Conversorbytes.int32tobyte(ind);

        byte[] arraybytes = new byte[magictobytes.length + 1 + 1 + nmovtobytes.length + elomedtobytes.length + indtobytes.length];

        System.arraycopy(magictobytes, 0, arraybytes, 0, magictobytes.length);
        arraybytes[magictobytes.length + 1]=flagbyte;
        System.arraycopy(nmovtobytes, 0, arraybytes, magictobytes.length  + 1 + 1, nmovtobytes.length);
        System.arraycopy(elomedtobytes, 0, arraybytes, magictobytes.length  + 1 + 1 + nmovtobytes.length, elomedtobytes.length);
        System.arraycopy(indtobytes, 0, arraybytes, magictobytes.length  + 1 + 1 + nmovtobytes.length + elomedtobytes.length, indtobytes.length);

        return arraybytes;
    }
}