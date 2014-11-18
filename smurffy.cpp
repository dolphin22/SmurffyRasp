#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <RF24/RF24.h>

#define MAX_SIZE 19 

using namespace std;

// GPIO 22 CE and CEO0 CSN with SPI Speed @ 8Mhz
RF24 radio(RPI_V2_GPIO_P1_15, RPI_V2_GPIO_P1_24, BCM2835_SPI_SPEED_8MHZ);

// Radio pipe addresses for the 2 nodes to communicate
const uint8_t pipes[][6] = {"1Node", "2Node"};

char message[MAX_SIZE+1];

int main(int argc, char** argv) {

	printf("Smurffy Hub Initializing...\n\r");
	radio.begin();
	radio.setAutoAck(1);		// Enable autoACK
	radio.enableAckPayload();	// Allow ack payloads
	radio.setRetries(0, 15);	// Wait 4000us between 2 tries, give up after 3 attempts
	radio.setPayloadSize(MAX_SIZE+1);
	radio.openWritingPipe(pipes[1]);
	radio.openReadingPipe(1, pipes[0]);
	
	radio.startListening();
	// Display rf configuration for debugging
	radio.printDetails();

	while(1) {
		uint8_t pipeNo;
		if(radio.available(&pipeNo)) {
			radio.flush_tx();
			while(radio.available()) {
				radio.read(message, MAX_SIZE+1);
			}
			printf("Got message size=%i value=%s.\n\r", MAX_SIZE+1,  message);

			printf("Sent response.\n\r");
			
			delay(925);
		}
	}

	return 0;
}
