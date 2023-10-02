package Interfaces;

// lista de piezas activas de un determinado color.
// su contenido es la posicion absoluta en el tablero (0:63) de cada pieza activa.	
public class PIEZAS_t
{
    private int	rey;				        // posicion rey.
	private int[] reinas = new int[9];       // posiciones reinas, varias debido a promociones.
	private int	nreina;                     // numero de reinas activas.
	private int[] torres = new int[10];	    // posiciones torres, puede ser mas de dos por promociones.
	private int	ntorre;                     // numero de torres activas.
	private int[] alfiles = new int[10];      // posiciones alfiles, puede ser mas de dos por promociones.
	private int	nalfil;                     // numero alfiles activos.
	private int[] caballos = new int[10];	// posiciones caballos, puede ser mas de dos por promociones.
	private int	ncaballo;		            // numero caballos activos.
    private int[] peones = new int[10];       // posiciones peones.
	private int	npeon;			            // numero peones activos.    

    public int getRey() {
        return rey;
    }
    public void setRey(int rey) {
        this.rey = rey;
    }
    public int[] getReinas() {
        return reinas;
    }
    public void setReinas(int[] reinas) {
        this.reinas = reinas;
    }
    public int getReina(int posicion) {
        return reinas[posicion];
    }
    public void setReina(int posicion, int valor) {
        this.reinas[posicion] = valor;
    }
    public int getNreina() {
        return nreina;
    }
    public void setNreina(int nreina) {
        this.nreina = nreina;
    }
    public int[] getTorres() {
        return torres;
    }
    public void setTorres(int[] torres) {
        this.torres = torres;
    }
    public int getTorre(int posicion) {
        return torres[posicion];
    }
    public void setTorre(int posicion, int valor) {
        this.torres[posicion] = valor;
    }
    public int getNtorre() {
        return ntorre;
    }
    public void setNtorre(int ntorre) {
        this.ntorre = ntorre;
    }
    public int[] getAlfiles() {
        return alfiles;
    }
    public void setAlfiles(int[] alfiles) {
        this.alfiles = alfiles;
    }
    public int getAlfil(int posicion) {
        return alfiles[posicion];
    }
    public void setAlfil(int posicion, int valor) {
        this.alfiles[posicion] = valor;
    }
    public int getNalfil() {
        return nalfil;
    }
    public void setNalfil(int nalfil) {
        this.nalfil = nalfil;
    }
    public int[] getCaballos() {
        return caballos;
    }
    public void setCaballos(int[] caballos) {
        this.caballos = caballos;
    }
    public int getCaballo(int posicion) {
        return caballos[posicion];
    }
    public void setCaballo(int posicion, int valor) {
        this.caballos[posicion] = valor;
    }
    public int getNcaballo() {
        return ncaballo;
    }
    public void setNcaballo(int ncaballo) {
        this.ncaballo = ncaballo;
    }
    public int[] getPeones() {
        return peones;
    }
    public void setPeones(int[] peones) {
        this.peones = peones;
    }
    public int getPeon(int posicion) {
        return peones[posicion];
    }
    public void setPeon(int posicion, int valor) {
        this.peones[posicion] = valor;
    }
    public int getNpeon() {
        return npeon;
    }
    public void setNpeon(int npeon) {
        this.npeon = npeon;
    }
}