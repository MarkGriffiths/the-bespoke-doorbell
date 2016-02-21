/*

The Bespoke Pixel's Doorbell

 */

#include     <SPI.h>
#include     <Ethernet.h>
#include     <EthernetUdp.h>
#include     <Adafruit_NeoPixel.h>
#ifdef __AVR__
	#include <avr/power.h>
#endif

#define button_pin  2
#define led_act_pin 9
#define led_ntf_pin 3
#define neo_rng_pin 5
#define neo_count   16

Adafruit_NeoPixel strip = Adafruit_NeoPixel(neo_count, neo_rng_pin, NEO_GRB + NEO_KHZ800);

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = {
	0xBE, 0x59, 0x0E, 0xD0, 0x0B, 0xE1
};

unsigned int localPort = 8888;       // local port to listen for UDP packets
unsigned int notifyPort = 8989;

char timeServer[] = "time.thebespokepixel.net"; // You'll want your own time server!
char broadcast[] = "255.255.255.255";

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
const int NOT_PACKET_SIZE = 16;

byte packetBuffer[NTP_PACKET_SIZE];
byte notifyBuffer[NOT_PACKET_SIZE];

const char signature[]    = "GC📡  ";
const char farSignature[] = "MT🚀  ";

// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

int currentHours = 0;
int currentMinutes = 0;
int currentSeconds = 60;

bool needTime = false;
bool showTime = false;

int display = 0;

unsigned long currentMillis = 1000;
float brightness[] = {0.0, 0.0, 0.0, 0.0};

volatile bool old_button = HIGH;

volatile unsigned long timer;

// send an NTP request to the time server at the given address
void sendNTPpacket(char* address) {
	// set all bytes in the buffer to 0
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	// Initialize values needed to form NTP request
	// (see URL above for details on the packets)
	packetBuffer[0] = 0b11100011;   // LI, Version, Mode
	packetBuffer[1] = 0;     // Stratum, or type of clock
	packetBuffer[2] = 6;     // Polling Interval
	packetBuffer[3] = 0xEC;  // Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12]  = 49;
	packetBuffer[13]  = 0x4E;
	packetBuffer[14]  = 49;
	packetBuffer[15]  = 52;

	// all NTP fields have been given values, now
	// you can send a packet requesting a timestamp:
	Udp.beginPacket(address, 123); //NTP requests are to port 123
	Udp.write(packetBuffer, NTP_PACKET_SIZE);
	Udp.endPacket();
}

void sendNotifyPacket(char* address) {
	// set all bytes in the buffer to 0
	memset(notifyBuffer, 0, NOT_PACKET_SIZE);

	for (int i = 0; i < 8; i++){
		notifyBuffer[i] = signature[i];
	}

	notifyBuffer[8] = 0x30 + currentHours;
	notifyBuffer[9] = 0x21 + currentMinutes;
	notifyBuffer[10] = 0x21 + currentSeconds;
	notifyBuffer[11] = 0x2B;

	Udp.beginPacket(address, notifyPort);
	Udp.write(notifyBuffer, NOT_PACKET_SIZE);
	Udp.endPacket();
}

void sendBell() {
	bool button = digitalRead(button_pin);
	if (button == LOW and old_button == HIGH) {
		delay(20);
		button = digitalRead(button_pin);
		if (button == LOW) {
			Serial.println("Bell press");
			timer = millis();
		}
	}

	if (button == HIGH and old_button == LOW) {
		delay(20);
		button = digitalRead(button_pin);
		if (button == HIGH) {
			Serial.println("Bell release");
			if (millis() - timer < 1000) {
				if (display == 0) {
					sendNotifyPacket(broadcast);
					display = 1;
				} else {
					display = 0;
				}
			} else {
				if (display == 0) {
					display = 2;
					for (int i=0; i < neo_count; i++) {
						strip.setPixelColor(i, strip.Color(0,0,0));
					}
					strip.show();
				}
			}
		}
	}

	old_button = button;
}

void setup() {
	// Open serial communications and wait for port to open:
	Serial.begin(9600);
	while (!Serial) {
		; // wait for serial port to connect. Needed for native USB port only
	}
	strip.begin();
	strip.show();

	// start Ethernet and UDP
	if (Ethernet.begin(mac) == 0) {
		Serial.println("Failed to configure Ethernet using DHCP");
		// no point in carrying on, so do nothing forevermore:
		for (;;)
			;
	}

	attachInterrupt(digitalPinToInterrupt(button_pin), sendBell, CHANGE);
	Udp.begin(localPort);
	needTime = true;
}

void loop() {
	unsigned long clock = millis() % 1000;
	if (clock < currentMillis) {
		int packet_size = Udp.parsePacket();

		if (packet_size != 0) {
			Serial.print("Got packet");
			Serial.println(packet_size);
		}

		if (packet_size == NOT_PACKET_SIZE) {
			Udp.read(notifyBuffer, NOT_PACKET_SIZE);
			Serial.print("Got notification");
			for (int i = 0; i < packet_size; i++){
				Serial.print(char(notifyBuffer[i]));
			}
			Serial.println();
			display = 0;
		}

		if (packet_size == NTP_PACKET_SIZE) {
			// We've received a packet, read the data from it
			Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

			unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
			unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
			// combine the four bytes (two words) into a long integer
			// this is NTP time (seconds since Jan 1 1900):
			unsigned long secsSince1900 = highWord << 16 | lowWord;

			// now convert NTP time into everyday time:
			// Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
			const unsigned long seventyYears = 2208988800UL;
			// subtract seventy years:
			unsigned long epoch = secsSince1900 - seventyYears;

			int twentyFourHour = (epoch % 86400)  / 3600;
			currentHours = (epoch  % 43200)  / 3600;
			currentMinutes = (epoch  % 3600) / 60;
			currentSeconds = epoch  % 60;

			Serial.print(twentyFourHour);
			Serial.print("  ");
			Serial.print(currentHours);
			Serial.print(" : ");
	        Serial.print(currentMinutes);
			Serial.print(" : ");
	        Serial.println(currentSeconds);

			if (twentyFourHour < 6 or twentyFourHour > 21) {
				brightness[0] = 0.1;
				brightness[1] = 0.0;
				brightness[2] = 0.0;
				brightness[3] = 0.05;
			} else if (twentyFourHour < 9) {
				brightness[0] = 0.4;
				brightness[1] = 0.4;
				brightness[2] = 0.0;
				brightness[3] = 0.4;
			} else if (twentyFourHour > 17) {
				brightness[0] = 0.4;
				brightness[1] = 0.1;
				brightness[2] = 0.0;
				brightness[3] = 0.4;
			} else {
				brightness[0] = 0.1;
				brightness[1] = 1.0;
				brightness[2] = 0.1;
				brightness[3] = 1.0;
			}
			showTime = true;
			analogWrite(led_act_pin, 0);
			analogWrite(led_ntf_pin, 10);
		}

		if (currentSeconds++ == 60) {
			currentSeconds = 0;
			if (currentMinutes % 10 == 0) needTime = true;
			if (currentMinutes++ == 60) {
				currentMinutes = 0;

				if (currentHours++ == 12) {
					currentHours=0;
				}
			}
		}
	}

	if (needTime) {
		analogWrite(led_act_pin, 32);
		analogWrite(led_ntf_pin, 0);
		sendNTPpacket(timeServer);
		needTime = false;
		showTime = false;
	} else {
		int hourPixel =   int(16.0 - (((currentHours   + (currentMinutes / 60.0)) / 12.0) * 16.0 - 0.5)) % 16;
		int minutePixel = int(16.0 - (((currentMinutes + (currentSeconds / 60.0)) / 60.0) * 16.0 - 0.5)) % 16;
		int secondPixel = int(16.0 - ((currentSeconds / 60.0)                             * 16.0 - 0.5)) % 16;
		int msPixel =     int(16.0 - ((clock / 1000.0)                                    * 16        )) % 16;

		switch (display) {
			case 1:
				for (int q=0; q < 8; q++) {
					for (int i=0; i < neo_count; i=i+8) {
						strip.setPixelColor(i+q, strip.Color(0,127,0));    //turn every third pixel on
					}
					strip.show();

					delay(50);

					for (int i=0; i < neo_count; i=i+8) {
						strip.setPixelColor(i+q, 0);        //turn every third pixel off
					}
				}
				break;
			case 2:
				for (int i=0; i < neo_count; i++) {
					strip.setPixelColor(i, strip.Color(255,255,255));
					strip.show();
					delay(50);
				}
				break;
			case 0:
				for(uint16_t i=0; i < neo_count; i++) {
					int red = 0;
					int green = 0;
					int blue = 0;

					if (i == secondPixel) {
						red += 32 * brightness[2];
						green += 32 * brightness[0];
						blue += 32 * brightness[1];
					}

					if (i == minutePixel) {
						red += 160 * brightness[0];
						green += 160 * brightness[1];
						blue += 160 * brightness[2];
					}

					if (i == hourPixel) {
						red += 48 * brightness[0];
						green += 48 * brightness[1];
						blue += 48 * brightness[2];
					}
					if (showTime) {
						if (i == msPixel) {
							blue  += 24 * brightness[3];
						}
					}
					if (red > 255) {
						red = 255;
					}
					if (green > 255) {
						green = 255;
					}
					if (blue > 255) {
						blue = 255;
					}
					strip.setPixelColor(i, strip.Color(red, green, blue));

				}
				strip.show();
				delay(5);
				break;
		}
	}

	Ethernet.maintain();
	currentMillis = clock;
}












