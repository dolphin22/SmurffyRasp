#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <RF24/RF24.h>
#include <curl/curl.h>

#define MAX_SIZE 19 

using namespace std;

// GPIO 22 CE and CEO0 CSN with SPI Speed @ 8Mhz
RF24 radio(RPI_V2_GPIO_P1_15, RPI_V2_GPIO_P1_24, BCM2835_SPI_SPEED_8MHZ);

// Radio pipe addresses for the 2 nodes to communicate
const uint8_t pipes[][6] = {"1Node", "2Node"};

char message[MAX_SIZE+1];

struct MemoryStruct {
  char *memory;
  size_t size;
};
struct MemoryStruct chunk; 
 
static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
  mem->memory = (char*)realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    /* out of memory! */ 
    fprintf(stderr, "not enough memory (realloc returned NULL)\n");
    return 0;
  }
 
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}

CURL *curl;
char deviceID[8];
char temperature[8];
char humidity[8];
char airspeed[8];

int send(void) {
  CURLcode res;

  char data[1024];

  /* get a curl handle */
  curl = curl_easy_init();
  if(curl) {
    /* First set the URL that is about to receive our POST. */
    curl_easy_setopt(curl, CURLOPT_URL, "http://smurffyjs.ismurffy.com/api/sensors");

    /* Next comes the header */    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    /* Now specify the POST data */
    sprintf(data, "{\"deviceID\":\"%s\","
                  "\"temperature\":\"%s\","
                  "\"humidity\":\"%s\","
                  "\"airspeed\":\"%s\"}", deviceID, temperature, humidity, airspeed);    
    
    // fprintf(stderr, "%s\n", data); 

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);

    /* send all data to this function  */ 
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
 
    /* we pass our 'chunk' struct to the callback function */ 
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);  

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    
    if(res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
    } else {
      fprintf(stderr, "Result: %s\n", chunk.memory);
      chunk.memory[0] = '\0';
    }

    
    /* always cleanup */
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
  }

  return 0;
}


int main(int argc, char** argv) {

  chunk.memory =(char*) malloc(1);  /* will be grown as needed by the realloc above */ 
  chunk.size = 0;    /* no data at this point */ 

  /* In windows, this will init the winsock stuff */
  curl_global_init(CURL_GLOBAL_ALL);
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
			
      deviceID[0] = message[0];
      deviceID[1] = message[1];
      deviceID[2] = message[2];
      deviceID[3] = '\0';

      temperature[0] = message[4];
      temperature[1] = message[5];
      temperature[2] = message[6];
      temperature[3] = message[7];
      temperature[4] = message[8];
      temperature[5] = '\0';

      humidity[0] = message[10];
      humidity[1] = message[11];
      humidity[2] = message[12];
      humidity[3] = message[13];
      humidity[4] = message[14];
      humidity[6] = '\0';

      airspeed[0] = message[16];
      airspeed[1] = message[17];
      airspeed[2] = message[18];
      airspeed[3] ='\0';

      send();
			delay(925);
		}

	}

  free(chunk.memory);
  curl_global_cleanup();
	
  return 0;
}

