package Interfaces;

// Codificacion del byte de flags de la partida
public class FLAGS_t
{
    private boolean	ganablanca = false;		// indicacion de que ganan las blancas.
	private boolean	gananegra = false;		// indicacion de que ganan las negras.
	private boolean	tnormal = false;		// la partida ha terminado normalmente.

	public boolean isGanablanca() {
		return ganablanca;
	}
	public void setGanablanca(boolean ganablanca) {
		this.ganablanca = ganablanca;
	}
	public boolean isGananegra() {
		return gananegra;
	}
	public void setGananegra(boolean gananegra) {
		this.gananegra = gananegra;
	}
	public boolean isTnormal() {
		return tnormal;
	}
	public void setTnormal(boolean tnormal) {
		this.tnormal = tnormal;
	}
	public byte getByteFLAGS_t()
    {
		byte aux = 0;

		if(this.ganablanca)
		{
			aux |= 0x1;
		}
		if(this.gananegra)
		{
			aux |= 0x2;
		}
		if(this.tnormal)
		{
			aux |= 0x4;
		}

		return aux;
	}
}