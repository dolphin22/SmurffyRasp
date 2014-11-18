#include <SPI.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include <Wire.h>
#include <LibHumidity.h>

#define MAX_SIZE 19

// Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 7 & 8 
RF24 radio(7,8);

// SHT25
LibHumidity sht25 = LibHumidity(0);

// Airspeed
int airspeedPin = 3;

byte pipes[][6] = {"1Node","2Node"};

// Sleep declarations
typedef enum { wdt_16ms = 0, wdt_32ms, wdt_64ms, wdt_128ms, wdt_250ms, wdt_500ms, wdt_1s, wdt_2s, wdt_4s, wdt_8s } wdt_prescalar_e;

void setup_watchdog(uint8_t prescalar);
void do_sleep(void);

const short sleep_cycles_per_transmission = 4;
volatile short sleep_cycles_remaining = sleep_cycles_per_transmission;

void setup() {


  Serial.begin(57600);
  printf_begin();
  printf("\n\rSmurrfy Sensor\n\r");
  
  // Prepare sleep parameters
  // Only the ping out role uses WDT.  Wake up every 4s to send a ping
  setup_watchdog(wdt_4s);

  // Setup and configure rf radio
  radio.begin();                          // Start up the radio
  radio.setAutoAck(1);                    // Ensure autoACK is enabled
  radio.enableAckPayload();
  radio.setRetries(4,2);                // Max delay between retries & number of retries
  radio.setPayloadSize(MAX_SIZE+1);
  radio.openWritingPipe(pipes[0]);
  radio.openReadingPipe(1,pipes[1]);
  
  radio.startListening();                 // Start listening
  radio.printDetails();                   // Dump the configuration of the rf unit for debugging
}

void loop(void){
    
    radio.powerUp();                                // Power up the radio after sleeping
    radio.stopListening();                          // First, stop listening so we can talk.
    
    char data[MAX_SIZE+1];
                   
    char deviceID[3] = "01";
    char tempF[5], humiF[5], airspeed[5];
    
    dtostrf(sht25.GetTemperatureC(),5,2,tempF);
    wdt_reset();
    dtostrf(sht25.GetHumidity(),5,2,humiF); 
    wdt_reset(); 
    
    sprintf(data, "S%sT%.5sH%.5sA%d", deviceID, tempF, humiF, analogRead(airspeedPin));
    data[MAX_SIZE+1] = 0;
    
    printf("Now sending %s.\n\r", data);

    radio.write(data, MAX_SIZE+1);
    
    /*
    if(!radio.available()){                             // If nothing in the buffer, we got an ack but it is blank
        printf("Got blank response.\n\r");     
    }else{      
        while(radio.available() ){                      // If an ack with payload was received
            radio.read( data, 16 );                  // Read it, and display the data
        }
        printf("Got response %s.\n\r",data);
    }
    */
    
    unsigned long started_waiting_at = millis();    // Wait here until we get a response, or timeout (250ms)
    bool timeout = false;
    while ( ! radio.available()  ){
        if (millis() - started_waiting_at > 250 ){  // Break out of the while loop if nothing available
          timeout = true;
          break;
        }
    }
    
    if ( timeout ) {                                // Describe the results
        printf("Failed, response timed out.\n\r");
    }else{
        while(radio.available()) {      
          radio.read( data, MAX_SIZE+1 );
        }
        
        printf("Got response %s.\n\r",data);
    }

    // Shut down the system
    delay(500);
    
                                    // Power down the radio.  
    radio.powerDown();              // NOTE: The radio MUST be powered back up again manually
                                    // Sleep the MCU.
    do_sleep();
}

void wakeUp(){
  sleep_disable();
}

// Sleep helpers

//Prescaler values
// 0=16ms, 1=32ms,2=64ms,3=125ms,4=250ms,5=500ms
// 6=1 sec,7=2 sec, 8=4 sec, 9= 8sec

void setup_watchdog(uint8_t prescalar){

  uint8_t wdtcsr = prescalar & 7;
  if ( prescalar & 8 )
    wdtcsr |= _BV(WDP3);
  MCUSR &= ~_BV(WDRF);                      // Clear the WD System Reset Flag
  WDTCSR = _BV(WDCE) | _BV(WDE);            // Write the WD Change enable bit to enable changing the prescaler and enable system reset
  WDTCSR = _BV(WDCE) | wdtcsr | _BV(WDIE);  // Write the prescalar bits (how long to sleep, enable the interrupt to wake the MCU
}

ISR(WDT_vect)
{
  //--sleep_cycles_remaining;
  Serial.println("WDT");
}

void do_sleep(void)
{
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // sleep mode is set here
  sleep_enable();
  attachInterrupt(0,wakeUp,LOW);
  WDTCSR |= _BV(WDIE);
  sleep_mode();                        // System sleeps here
                                       // The WDT_vect interrupt wakes the MCU from here
  sleep_disable();                     // System continues execution here when watchdog timed out  
  detachInterrupt(0);  
  WDTCSR &= ~_BV(WDIE);  
}
