package Interfaces;

// Tablero. Contiene el tablero propiamente dicho
// y la lista de piezas blancas y negras activas
// para acelerar operaciones.
public class TABLERO_t
{
	private int[]	tab = new int[64];    // tablero. Contenido pieza que ocupa esa casilla.
	private PIEZAS_t blancas = new PIEZAS_t();             // lista piezas blancas.
	private PIEZAS_t negras = new PIEZAS_t();              // lista piezas negras.
	
	public int[] getTab() {
		return tab;
	}
	public void setTab(int[] tab) {
		this.tab = tab;
	}
	public int getContenidoTab(int posicion) {
		return tab[posicion];
	}
	public void setContenidoTab(int posicion, int valor) {
		this.tab[posicion] = valor;
	}
	public PIEZAS_t getBlancas() {
		return blancas;
	}
	public void setBlancas(PIEZAS_t blancas) {
		this.blancas = blancas;
	}
	public PIEZAS_t getNegras() {
		return negras;
	}
	public void setNegras(PIEZAS_t negras) {
		this.negras = negras;
	}
}