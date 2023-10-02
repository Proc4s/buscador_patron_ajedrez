package Interfaces;

public class Conversorbytes {
    public static byte[] int16tobyte(int elem)
    {
        byte[] arraybytes = new byte[2];
        arraybytes[1] = (byte) ((elem >> 8) & 0xFF);
        arraybytes[0] = (byte) (elem  & 0xFF);

        return arraybytes;
    }

    public static byte[] int32tobyte(int elem)
    {
        byte[] arraybytes = new byte[4];
        arraybytes[3] = (byte) ((elem >> 24) & 0xFF);
        arraybytes[2] = (byte) ((elem >> 16) & 0xFF);
        arraybytes[1] = (byte) ((elem >> 8) & 0xFF);
        arraybytes[0] = (byte) (elem  & 0xFF);

        return arraybytes;
    }
}
