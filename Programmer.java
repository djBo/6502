import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintStream;

import gnu.io.CommPort;
import gnu.io.CommPortIdentifier;
import gnu.io.SerialPort;
import gnu.io.SerialPortEvent;
import gnu.io.SerialPortEventListener;


public class Programmer {
	public static final Object _lock = new Object();

	public static interface ConsoleHandler {
		public void onReady(String s, SerialWriter sw);
	}

	public static class ConsoleReader implements Runnable {
		private ConsoleHandler _handler;
		SerialWriter _sw;
		private byte[] buffer = new byte[1024];

		public ConsoleReader(ConsoleHandler handler, SerialWriter sw) {
			_handler = handler;
			_sw = sw;
		}

		@Override
		public void run() {
			int c = 0;

            try {
            	while (true) {
            		int len = 0;
            		while ( ( c = System.in.read()) > -1 ) {
            			if ( c == '\r' ) {
            				continue;
            			}
            			if ( c == '\n' ) {
            				break;
            			}
            			buffer[len++] = (byte) c;
            		}
            		_handler.onReady(new String(buffer, 0, len), _sw);
            	}
            } catch ( IOException e ) {
                e.printStackTrace();
                System.exit(-1);
            }
		}
	}

    public static class SerialReader implements SerialPortEventListener {
        private InputStream in;
        private byte[] buffer = new byte[1024];

        public SerialReader ( InputStream in ) {
            this.in = in;
        }

        public void serialEvent(SerialPortEvent arg0) {
            int data;
            try {
                int len = 0;
                while ( ( data = in.read()) > -1 ) {
                    buffer[len++] = (byte) data;
                    if ( data == '\n' ) {
                        break;
                    }
                }
                System.out.print(new String(buffer, 0, len));
            } catch ( IOException e ) {
                e.printStackTrace();
                System.exit(-1);
            }
        }
    }

    public static class SerialWriter implements Runnable {

        private OutputStream out;
        private PrintStream _ps;
        private byte[] _b;
        private String _s = null;

        public SerialWriter ( OutputStream out ) {
            this.out = out;
            _ps = new PrintStream(out);
        }

        public void println(String s) {
        	_s = s;
        	synchronized (_lock) {
        		_lock.notify();
        	}
        }

        public void write(byte[] b) {
        	_b = b;
        	synchronized (_lock) {
        		_lock.notify();
        	}
        }

        public void run () {
        	while (true) {
        		try {
        			synchronized (_lock) {
        				_lock.wait();
        			}
        			if (_s == null) {
        				try {
							this.out.write(_b);
						} catch (IOException e) {
						}
        			} else {
        				this._ps.print(_s + "\n");
        				_s = null;
					}
				} catch (InterruptedException e1) {
				}
        	}
        }
    }

	public static void LOAD(File file, SerialWriter sw) {
		byte[] buf = new byte[8192];
		InputStream in = null;
		try {
			try {
				in = new FileInputStream(file);
				if (in.read(buf) == -1) {
					throw new IOException("EOF reached while trying to read the whole file");
				}

				sw.println("LOAD");

				try {
					Thread.sleep(1000);
				} catch (InterruptedException e) {}

				sw.write(buf);

			} finally {
				try {
					if (in != null)
						in.close();
				} catch (IOException e) {
				}
			}
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	public static void main(String[] args) {
		if (args.length == 1) {
			if (args[0].equalsIgnoreCase("LIST")) {
				java.util.Enumeration<CommPortIdentifier> portEnum = CommPortIdentifier.getPortIdentifiers();
				if (portEnum.hasMoreElements()) {
					while (portEnum.hasMoreElements()) {
						CommPortIdentifier portIdentifier = portEnum.nextElement();
						if (portIdentifier.getPortType() == CommPortIdentifier.PORT_SERIAL)
							System.out.println(portIdentifier.getName());
					}
				} else {
					System.out.println("Error: No serial COM ports found");
				}
			} else {
				try {
					CommPortIdentifier portIdentifier = CommPortIdentifier.getPortIdentifier(args[0].toUpperCase());
					if (portIdentifier.getPortType() == CommPortIdentifier.PORT_SERIAL) {
						if (portIdentifier.isCurrentlyOwned()) {
							System.out.println("Error: " + args[0] + " is currently in use");
						} else {
							CommPort commPort = portIdentifier.open("Programmer", 2500);
							if ( commPort instanceof SerialPort ) {
				                SerialPort serialPort = (SerialPort) commPort;
				                serialPort.setSerialPortParams(57600,SerialPort.DATABITS_8,SerialPort.STOPBITS_1,SerialPort.PARITY_NONE);

				        		ConsoleHandler ch = new ConsoleHandler() {
				        			@Override
				        			public void onReady(String s, SerialWriter sw) {
			        					if (s.equalsIgnoreCase("EXIT")) {
			        						System.exit(0);
			        					} else if (s.startsWith("LOAD ")) {
			        						String filename = s.substring(5);
			        						File file = new File(filename);
			        						if (file.exists()) {
			        							if (file.length() == 8192) {
			        								LOAD(file, sw);
			        							} else {
			        								System.out.println("Error: File must be exactly 8Kb");
			        							}
			        						} else {
			        							System.out.println("Error: File doesn't exist");
			        						}
			        					} else {
			        						sw.println(s);
			        					}
				        			}
				        		};

				        		SerialReader sr = new SerialReader(serialPort.getInputStream());
				        		SerialWriter sw = new SerialWriter(serialPort.getOutputStream());

				        		ConsoleReader cr = new ConsoleReader(ch, sw);
				        		(new Thread(cr)).start();
				                (new Thread(sw)).start();
				                serialPort.addEventListener(sr);
				                serialPort.notifyOnDataAvailable(true);

				            } else {
				                System.out.println("Error: Only serial ports are handled by this example");
				            }
						}

					} else {
						System.out.println("Error: " + args[0] + " is not a serial COM port");
					}
				} catch (Exception e) {
					e.printStackTrace();
				}
			}
		} else {
			System.out.println("Usage: java -jar Programmer.jar <COMx>|<LIST>");
		}
	}
}
