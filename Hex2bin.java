import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;


public class Hex2bin {
	public static void main(String[] args) {
		Boolean swap = false;
		Boolean inv = false;
		if (args.length > 0) {
			for (String name : args) {
				if ("-swap".equalsIgnoreCase(name)) {
					swap = true;
					continue;
				}
				if ("-inv".equalsIgnoreCase(name)) {
					inv = true;
					continue;
				}
				File input = new File(name);
				if (input.exists()) {
					try {
						File output = new File(name + ".bin");
						if (name.contains(".")) output = new File(name.substring(0, name.lastIndexOf('.')) + ".bin");
						System.out.print(input.getName() + " => " + output.getName() + " ... ");

						FileOutputStream out = new FileOutputStream(output);
						try {
							try (BufferedReader br = new BufferedReader(new FileReader(input))) {
								int sum = 0;
								for (String line; (line = br.readLine()) != null; ) {
									Integer max = line.length() / 2;
									String [] bytes = line.split("(?<=\\G.{2})");
									byte[] data = new byte[bytes.length];

									for (int i = 0; i < max; i++) {
										Byte b = (byte) (Integer.parseInt(bytes[i], 16) & 0xFF);
										if (inv)
											b = (byte) ((Integer.reverse(b) >> 24) & 0xFF);
										if (swap)
											b = (byte) ( (b & 0x0F) << 4 | (b & 0xF0) >> 4);
										data[i] = b;
										sum += b;
									}
									out.write(data);
								}
								sum %= 256;
								System.out.println(sum == 0 ? "Ok" : String.format("%02x", (256 - sum)).toUpperCase());
							}
						} finally {
							out.close();
						}
					} catch (Exception e) {
						e.printStackTrace();
					}
				} else {
					System.out.println(name + " is not a valid file");
				}
			}		
		}
	}
}
