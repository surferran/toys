/*
Name:		doSymaFly.ino
Last updated : 21/11/15
(Created:	11/19/2015 9:34:07 PM)
Author:	Ran_the_User
*/

/* TODO:
	28/01/16
	check all control commands are available by keyboard/serial chars.
	  possible that only one of the directions is infrastructured.
*/
#include <util/atomic.h>
#include <EEPROM.h>
#include "iface_nrf24l01.h"

//#define USING_LEONARDO   // options are : USING_UNO , USING_LEONARDO
#define USING_UNO
//#define DESIRED_PROTOCOL ..

#define ledPin    4  //13 // LED  - D13

#define MOSI_pin  MOSI  
#define SCK_pin   SCK   
#define MISO_pin  MISO  

#define CE_pin    9   //
#define CS_pin    10  // CSN pin
#ifdef USING_LEONARDO
// pins definitions for Arduino Leonardo :
#define MOSI_on     PORTB |= _BV(2)    // PB2
#define MOSI_off    PORTB &= ~_BV(2)   // 
#define SCK_on      PORTB |= _BV(1)    // PB1
#define SCK_off     PORTB &= ~_BV(1)   // 

#define CE_on       PORTB |= _BV(5)    // PB5
#define CE_off      PORTB &= ~_BV(5)   // 
#define CS_on       PORTB |= _BV(6)    // PB6
#define CS_off      PORTB &= ~_BV(6)   // 

#define  MISO_on    (PINB & _BV(3))    // PB3
#endif
#ifdef USING_UNO
// pins definitions for Arduino Uno :
#define MOSI_on     PORTB |= _BV(3)  // PB3
#define MOSI_off    PORTB &= ~_BV(3) // PB3
#define SCK_on      PORTB |= _BV(5)  // PB5
#define SCK_off     PORTB &= ~_BV(5) // PB5

#define CE_on       PORTB |= _BV(1)   // PB1
#define CE_off      PORTB &= ~_BV(1)  // PB1
#define CS_on       PORTB |= _BV(2)   // PB2
#define CS_off      PORTB &= ~_BV(2)  // PB2

#define  MISO_on    (PINB & _BV(4)) // PB4
#endif

#define RF_POWER 3 // 0-3, it was found that using maximum power can cause some issues, so let's use 2... 

// PPM stream settings. recieve by serial commands.
//#define CHANNELS 12 // number of channels in ppm stream, 12 ideally
enum chan_order{
    THROTTLE,
    AILERON,
    ELEVATOR,
    RUDDER,
    AUX1,  // (CH5)  led light, or 3 pos. rate on CX-10, H7, or inverted flight on H101
    AUX2,  // (CH6)  flip control
    AUX3,  // (CH7)  still camera
    AUX4,  // (CH8)  video camera
    AUX5,  // (CH9)  headless
    AUX6,  // (CH10) calibrate Y (V2x2), pitch trim (H7), RTH (Bayang, H20), 360deg flip mode (H8-3D, H22)
    AUX7,  // (CH11) calibrate X (V2x2), roll trim (H7)
    AUX8,  // (CH12) Reset / Rebind
};

#define PPM_MIN           100 //1000
///#define PPM_SAFE_THROTTLE 110 // 1050 
#define PPM_MID           500
#define PPM_MAX           250 //2000
///#define PPM_MIN_COMMAND   130// 1300
#define PPM_MAX_COMMAND   700

// optional supported protocols. this version is SYMAX , (JJRC H8 mini, Floureon H101) limited.
enum {
    PROTO_BAYANG,       // EAchine H8 mini, H10, BayangToys X6, X7, X9, JJRC JJ850, Floureon H101
    PROTO_SYMAX5C1,     // Syma X5C-1 (not older X5C), X11, X11C, X12    
    PROTO_END
};

// EEPROM locations
enum{
    ee_PROTOCOL_ID = 0,
    ee_TXID0,
    ee_TXID1,
    ee_TXID2,
    ee_TXID3
};

uint8_t               transmitterID[4];
uint8_t               current_protocol;
static volatile bool  ppm_ok	= false;
uint8_t               packet[32];
static bool           reset		= true;
volatile uint16_t     Servo_data[12];
static uint16_t       ppm[12] = {PPM_MIN,PPM_MIN,PPM_MIN,PPM_MIN,PPM_MID,PPM_MID,
                                 PPM_MID,PPM_MID,PPM_MID,PPM_MID,PPM_MID,PPM_MID,};

uint8_t  status_ans = 0;

byte cmd = 0;     // a place to put our serial data
//
#define GET_FLAG(ch, mask) (ppm[ch] > PPM_MAX_COMMAND ? mask : 0)
//

#include "nRF24L01.h"
#include "softSPI.h"
#include "SymaX.h"
#include "Bayang.h"
//#include ""

void setup()
{
    pinMode(ledPin	, OUTPUT);
    digitalWrite(ledPin, LOW); //start LED off

    pinMode(MOSI_pin, OUTPUT);
    pinMode(SCK_pin	, OUTPUT);
    pinMode(CS_pin	, OUTPUT);
    pinMode(CE_pin	, OUTPUT);
    pinMode(MISO_pin, INPUT);
 
    TCCR1A = 0;  //reset timer1
    TCCR1B = 0;
    TCCR1B |= (1 << CS11);  //set timer1 to increment every 1 us @ 8MHz, 0.5 us @16MHz

    set_txid(false);
	selectProtocol();

	//Serial.begin(9600);
	Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB
  }
  Serial.print("initial protocol is for : ");
  Serial.print(current_protocol);
  Serial.print("_ remote test on . Virtual Transmiter ID : ");
  Serial.print(transmitterID[0]);   Serial.print(transmitterID[1]);
  Serial.print(transmitterID[2]);   Serial.print(transmitterID[3]);
  Serial.println();
  Serial.println("control by : ");
  Serial.println(" up   arrow - go up (inc throttel),\n down arrow - go down(dec throttel) ");
  Serial.println(" left arrow - yaw left(alleron-),\n right arrow - yaw right(aleron+) ");
  Serial.println(" 'w'        - pitch up(elevetor-),\n 'z' - pitch down(elevetor+) ");
  Serial.println(" 'a'        - roll left(rudder-),\n 's' - roll right(rudder+) ");
  
}

void loop()
{
    uint32_t timeout;
    // reset / rebind
    if(reset || ppm[AUX8] > PPM_MAX_COMMAND) {
        reset = false;
        selectProtocol();        
        NRF24L01_Reset();//
        NRF24L01_Initialize();
        init_protocol();
		Serial.println("done reset");
    }
	ppm[AUX3] = 0;
	ppm[AUX4] = 0;

     /// check serial inputs from user    
    if (Serial.available())
    {
      
		cmd=Serial.read();
		Serial.write(cmd); Serial.write(','); Serial.write('\n');
		switch (cmd)			// 'w' 'z' 'a,s,i,m,j,k' 'r' 'c' 'p'
		 {
		  case 'w':  if (ppm[THROTTLE]<=(PPM_MAX-10))	ppm[THROTTLE]+=10;	Serial.println(ppm[THROTTLE]);    break;
		  case 'z':  if (ppm[THROTTLE]>=(PPM_MIN+10))	ppm[THROTTLE]-=10;	Serial.println(ppm[THROTTLE]);    break;

		  case 'i':  if (ppm[ELEVATOR] <= (PPM_MAX - 10)) ppm[ELEVATOR] += 10;	Serial.println(ppm[ELEVATOR]);    break;
		  case 'm':  if (ppm[ELEVATOR] >= (PPM_MIN + 10)) ppm[ELEVATOR] -= 10;	Serial.println(ppm[ELEVATOR]);    break;

		  case 'a':  if (ppm[RUDDER] <= (PPM_MAX - 10)) ppm[RUDDER] += 10;	Serial.println(ppm[RUDDER]);    break;
		  case 's':  if (ppm[RUDDER] >= (PPM_MIN + 10)) ppm[RUDDER] -= 10;	Serial.println(ppm[RUDDER]);    break;

		  case 'j':  if (ppm[AILERON] <= (PPM_MAX - 10)) ppm[AILERON] += 10;	Serial.println(ppm[AILERON]);    break;
		  case 'k':  if (ppm[AILERON] >= (PPM_MIN + 10)) ppm[AILERON] -= 10;	Serial.println(ppm[AILERON]);    break;

		  case 'r':  
			Serial.println("//reseting//, MIN");     ppm[THROTTLE]=PPM_MIN;	 Serial.println(ppm[THROTTLE]);    reset=true;
			break;

		  case 'c':      Serial.println("camera still ");		ppm[AUX3] = PPM_MAX_COMMAND + 1;	 Serial.println(ppm[AUX3]);    break;
		  case 'v':      Serial.println("video ");				ppm[AUX4] = PPM_MAX_COMMAND + 1;	 Serial.println(ppm[AUX4]);    break;

		  case 'p':
				Serial.println("packet status:");
				for (int j =0 ; j < 32; j++)
				{
					Serial.print(packet[j]);
					Serial.print(" , ");
				}
				Serial.println(" .. ");
				break;
		 }
	  cmd=0;
  
    }
    //// process protocol    
   /// timeout = process_SymaX();
	switch (current_protocol) {
	case PROTO_BAYANG:
		timeout = process_Bayang();
		break;
	case PROTO_SYMAX5C1:
		timeout = process_SymaX();
		break;
	}
    
    // wait before dealing & sending next packet
    while(micros() < timeout)    {   };
}

void set_txid(bool renew)
{
    uint8_t i;
    for(i=0; i<4; i++)
        transmitterID[i] = EEPROM.read(ee_TXID0+i);
    if(renew || (transmitterID[0]==0xFF && transmitterID[1]==0x0FF)) {
        for(i=0; i<4; i++) {
            transmitterID[i] = random() & 0xFF;
            EEPROM.update(ee_TXID0+i, transmitterID[i]); 
        }            
    }
}

void selectProtocol() 
{
    // startup stick commands
    
    //if(ppm[RUDDER] < PPM_MIN_COMMAND)        // Rudder left
    //    set_txid(true);                      // Renew Transmitter ID
    
    // protocol selection
   current_protocol= PROTO_SYMAX5C1 ;
///current_protocol = PROTO_BAYANG; 
    // update eeprom 
    EEPROM.update(ee_PROTOCOL_ID, current_protocol);
    // wait for safe throttle
}

void init_protocol()
{   
        //    Symax_init();
          //  SymaX_bind();

	switch (current_protocol) {
	case PROTO_BAYANG:
		Bayang_init();
		Bayang_bind();
		break;
	case PROTO_SYMAX5C1:
		Symax_init();
		SymaX_bind();
		break;
	}
    Serial.println("done init & bind protocol");
}

