// modifier : ran D
// program based on 'ArduCAM_Mini_OV2640_FIFO_UART_Burst.ino' example file.
// last modification : 13/09/15
// BT relevant URLs :
//            https://www.youtube.com/watch?v=-z_0aU8VHzk
// based on : http://www.instructables.com/id/Modify-The-HC-05-Bluetooth-Module-Defaults-Using-A/?ALLSTEPS
//            http://www.techbitar.com/modify-the-hc-05-bluetooth-module-defaults-using-at-commands.html
//            http://robotics.stackexchange.com/questions/2056/bluetooth-module-hc-05-giving-error-0
 /* includes */
#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include <SD.h>
#include "memorysaver.h"

#include <SoftwareSerial.h>

#include <Servo.h>  // added on 18/10/15

 /* constants + special globals */

static const int MAIN_SERIAL_MODE = 1;//0 ; // 0-USB 1-BT

static const int BT_PROG_MODE     = 0 ;  // 0-false(BT mode) 1-true(serial mode)
static const int BTpinRX     = 9   ;   //9,10     ,0
static const int BTpinTX     = 10  ;    //10, 11  ,1-only one direction works ok
static const int BTpinATmode = 4   ;   // 
/**/

/* define operating connected arduCam-mini cameras */
#define NUM_CAMS_ON 1 //  0,1,or 2
#define CAM_1_IS_ON ;  // i have 2 at most
//#define CAM_2_IS_ON ;  // i have 2 at most

// set pin 10 as the slave select for the digital pot:
///ArduCam myCamArr[NUM_CAMS_ON];
const int SPI_CS[] = {8};   // {8,9}

#ifdef CAM_1_IS_ON
  const int SPI_CS1 = 2;//2,10;
  ArduCAM         myCAM1(OV2640,SPI_CS1);
#endif
#ifdef CAM_2_IS_ON
  const int SPI_CS2 = 9;//9;
  ArduCAM         myCAM2(OV2640,SPI_CS2);
#endif

SoftwareSerial  BTSerial(BTpinRX, BTpinTX); 

Servo myservo1;  // create servo object to control a servo 
Servo myservo2; 
int   Servo1Pos = 0;    // variable to store the servo position 
int   Servo2Pos = 0;    // variable to store the servo position 

void setup()
{  
  uint8_t vid,pid;
  uint8_t temp;
#if defined (__AVR__)
  Wire.begin(); 
#endif
#if defined(__arm__)
  Wire1.begin(); 
#endif
  Serial.begin(115200);//115200, 57600, 256000
//  while (!Serial) {
//     ; // wait for serial port to connect. Needed for Leonardo only
//  }
  Serial.println("ArduCAM Start!"); 
  Serial.println("attaching servo2");
  myservo2.attach(5);  // attaches the servo on pin 10 to the servo object 
  
    Serial.println("aobut to round servo2");
         
    for(Servo2Pos = -90; Servo2Pos<=0; Servo2Pos+=1)     // goes from 180 degrees to 0 degrees 
    {                                
      //mySerial_write_num(Servo2Pos);          
      Serial.print(Servo2Pos);
      myservo2.write(Servo2Pos);              // tell servo to go to position in variable 'pos' 
      delay(15);                       // waits 15ms for the servo to reach the position 
    } 


/////////////
  // set the SPI_CS as an output:
#ifdef CAM_1_IS_ON
  pinMode(SPI_CS1, OUTPUT);
#endif
#ifdef CAM_2_IS_ON
  pinMode(SPI_CS2, OUTPUT);
#endif

  Serial.println("starting SPI"); 
  // initialize SPI:
  SPI.begin(); 
  //SPI.setClockDivider(SPI_CLOCK_DIV2);
  //Check if the ArduCAM SPI bus is OK. By CAM1 , CAM2
#ifdef CAM_1_IS_ON
  myCAM1.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = myCAM1.read_reg(ARDUCHIP_TEST1);
  if(temp != 0x55)
  {
    Serial.println("SPI 1 interface Error!");
    Serial.print(temp, HEX);
    //while(1);//
  }
#endif
#ifdef CAM_2_IS_ON
 myCAM2.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = myCAM2.read_reg(ARDUCHIP_TEST1);
  if(temp != 0x55)
  {
    Serial.println("SPI 2 interface Error!");
    Serial.print(temp, HEX);
    //while(1);//
  }
#endif

  //myCAM.set_mode(MCU2LCD_MODE); // .. make 2 BT mode..

  //Check if the camera module type is OV2640. by CAM1 only
  // set as general myCam function
  myCAM1.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
  myCAM1.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
  if((vid != 0x26) || (pid != 0x42))
    Serial.println("Can't find OV2640 module!");
  else
    Serial.println("OV2640 1 detected");
    /*
  myCAM2.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
  myCAM2.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
  if((vid != 0x26) || (pid != 0x42))
    Serial.println("Can't find OV2640 module!");
  else
    Serial.println("OV2640 2 detected");
    */
  //Change to JPEG capture mode and initialize the OV2640 module
  // in CAM1 . is it affecting all CAMs on bus ??	
  // what about cam 2??
  myCAM1.set_format(JPEG);
  //myCAM.set_format(BMP);
  myCAM1.InitCAM();
  myCAM1.OV2640_set_JPEG_size(OV2640_320x240);
  myCAM1.clear_fifo_flag();
  //myCAM.write_reg(0x81, 0x00);
  myCAM1.write_reg(ARDUCHIP_FRAMES,0x00); //Bit[2:0]Number of frames to be captured
  
  // BT section : 
  if (BT_PROG_MODE)
  {    
    Serial.println("BT AT mode"); 
    pinMode(BTpinATmode, OUTPUT);  // this pin will pull the HC-05 pin 34 (key pin) HIGH to switch module to AT mode
    digitalWrite(BTpinATmode, HIGH); 
    BTSerial.begin(38400);  // HC-05 default speed in AT command mode
    Serial.println("Enter AT commands:");
  }
  else
  {
  Serial.println("serial printing : BT serial mode"); 
    BTSerial.begin(115200); //115200 ,230400// HC-05 default speed in AT command mode
//    while (!BTSerial) {
//     ; // wait for serial port to connect. Needed for Leonardo only
//  }
    BTSerial.println("bt command printing : BT serial mode");  //write
  }
/*  */
  mySerial_println_str("the chosen serial channel is this one");
 // mySerial_println_str("attaching servo2");
  Serial.println("attaching servo2");
  myservo2.attach(5);  // attaches the servo on pin 10 to the servo object 
  
    Serial.println("aobut to round servo2");
         
    for(Servo2Pos = -90; Servo2Pos<=0; Servo2Pos+=1)     // goes from 180 degrees to 0 degrees 
    {                                
      //mySerial_write_num(Servo2Pos);          
      Serial.print(Servo2Pos);
      myservo2.write(Servo2Pos);              // tell servo to go to position in variable 'pos' 
      delay(15);                       // waits 15ms for the servo to reach the position 
    } 
    
}

void loop()
{
  uint8_t temp,temp_last;
  uint8_t start_capture = 0;  
  static int Ts, Tc, Td, Tt, Tu, T0;
  
  T0   = millis();
  temp = mySerial_read(); //Serial.read();   // get requests for settings or camera functions
  Tu   = millis();
  
  // BT section
//  if (BT_PROG_MODE)
/*  {
   // keeping connection between BT serial to Artuino serial   
    // Keep reading from HC-05 and send to Arduino Serial Monitor
    if (BTSerial.available())
    //{
     /// Serial.write(" bt available : ");
      Serial.write(BTSerial.read());
    //}
    // Keep reading from Arduino Serial Monitor and send to HC-05
    if (Serial.available())
      BTSerial.write(Serial.read());
  }
 */
  // end of BT section
  
  
  switch(temp)
  {
  case 0:
    myCAM1.OV2640_set_JPEG_size(OV2640_160x120);
   mySerial_println_str("res change to Min");
    break;
  case 1:
    myCAM1.OV2640_set_JPEG_size(OV2640_176x144);
    break;
  case 2:
    myCAM1.OV2640_set_JPEG_size(OV2640_320x240);
    ///BTSerial.write("res change");
    break;
  case 3:
    myCAM1.OV2640_set_JPEG_size(OV2640_352x288);
    break;
  case 4:
    myCAM1.OV2640_set_JPEG_size(OV2640_640x480);
    break;
  case 5:
    myCAM1.OV2640_set_JPEG_size(OV2640_800x600);
    break;
  case 6:
    myCAM1.OV2640_set_JPEG_size(OV2640_1024x768);
    break;
  case 7:
    myCAM1.OV2640_set_JPEG_size(OV2640_1280x1024);
    break;
  case 8:
    myCAM1.OV2640_set_JPEG_size(OV2640_1600x1200);
    mySerial_println_str("res change to Max");
       
         // change motor 2
        // mySerial_println_str("aobut to round servo2");
         Serial.println("aobut to round servo2");
         
    for(Servo2Pos = 180; Servo2Pos>=0; Servo2Pos-=1)     // goes from 180 degrees to 0 degrees 
    {                                
      //mySerial_write_num(Servo2Pos);          
      Serial.print(Servo2Pos);
      myservo2.write(Servo2Pos);              // tell servo to go to position in variable 'pos' 
      delay(15);                       // waits 15ms for the servo to reach the position 
    } 
    
    break;
    
  case 0x0a:
    //temp_last = myCAM.read_reg(0x03);
    //myCAM.write_reg(0x83, temp_last|0x40);
    myCAM1.set_bit(ARDUCHIP_TIM,LOW_POWER_MODE);
    break;
  case 0x0b:
    myCAM1.clear_bit(ARDUCHIP_TIM,LOW_POWER_MODE);
    break;
  case 0x0c:
    myCAM1.set_bit(ARDUCHIP_FIFO,FIFO_RDPTR_RST_MASK);
    read_fifo_again(myCAM1);
    break;
  case 0x0d:  //reset
    myCAM1.set_bit(ARDUCHIP_GPIO,GPIO_RESET_MASK);
    myCAM1.clear_bit(ARDUCHIP_GPIO,GPIO_RESET_MASK);
    break;
  case 0x0e:  //pwdn low
    ///myCAM.set_bit(ARDUCHIP_GPIO,GPIO_POWER_MASK);
    break;   
  case 0x0f:  //pwdn
    ///myCAM.clear_bit(ARDUCHIP_GPIO,GPIO_POWER_MASK);
    break;   
  case 0x10:
    start_capture = 1;     
    //Serial.print("Start Capture from 1"); 
    mySerial_println_str("Start Capture from 1"); 
/*
#ifdef CAM_2_IS_ON
    Serial.println(" & 2"); 
#else    
    Serial.println(" "); 
#endif
*/
    Ts = millis();
    myCAM1.flush_fifo();
    
#ifdef CAM_2_IS_ON
    myCAM2.flush_fifo();
#endif
        ///BTSerial.write("captured 1+2");
    //Serial.println("phase 2"); 
    break;
  default:
    break;
  }

  if(start_capture)
  {
    //Serial.println("phase 3"); 
    //Clear the capture done flag 
    myCAM1.clear_fifo_flag();	 
    //Start capture
    myCAM1.start_capture();
    //Serial.println("phase 4"); 

#ifdef CAM_2_IS_ON
    //Clear the capture done flag 
    myCAM2.clear_fifo_flag();	 
    //Start capture
    myCAM2.start_capture();
#endif    
    start_capture = 0;	
    Tc = millis(); 
    //Serial.println("phase 5"); 
  }
  if(myCAM1.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK))
  {
    //Serial.println("phase 6"); 
    Td = millis();    
//    Serial.print("Capture 1");    
    mySerial_println_str("Capture 1 Done!");   
   /* 
#ifdef CAM_2_IS_ON
    Serial.println("+2 Done!");
#else
    Serial.println(" Done!");
#endif
*/
//    read_fifo_burst();
    read_fifo_burst(myCAM1);  
#ifdef CAM_2_IS_ON
    read_fifo_burst(myCAM2);
#endif
    Tt = millis();
    //Clear the capture done flag 
    myCAM1.clear_fifo_flag();  
#ifdef CAM_2_IS_ON
    myCAM2.clear_fifo_flag(); 
#endif       
    start_capture = 0;
    /*
    Serial.println("");
    Serial.println("-----------------------");
    Serial.println("Time used:");
    Serial.println(Tu-T0, DEC);
    Serial.println(Tc-Ts, DEC);
    Serial.println(Td-Tc, DEC);
    Serial.println(Tt-Td, DEC);
*/

    mySerial_println_str("");
    mySerial_println_str("-----------------------");
    mySerial_println_str("Time used:");
    //mySerial_println(Tu-T0, DEC);
   // mySerial_println(Tc-Ts, DEC);
 //   mySerial_println(Td-Tc, DEC);
//    mySerial_println(Tt-Td, DEC);

  }
}

uint8_t read_fifo_burst(ArduCAM myCAM)
{
    uint32_t length = 0;
    uint8_t temp,temp_last;
    length = myCAM.read_fifo_length();
    if(length >= 393216)  //384 kb
    {
      //Serial.println("Not found the end.");
      mySerial_println_str("Not found the end.");
      return 0;
    }
    ///Serial.println(length);///
      mySerial_println_num(length);    
    //digitalWrite(10,LOW);
    myCAM.CS_LOW();
    myCAM.set_fifo_burst();
    //SPI.transfer(0x3C);
    //SPI.transfer(0x00);  //First byte is 0xC0 ,not 0xff??
    //Serial.write(0xff);
    length--;
    while( length-- )
    {
	temp_last = temp;
	temp = SPI.transfer(0x00);
	//Serial.write(temp);
        mySerial_write_num(temp);
	if( (temp == 0xD9) && (temp_last == 0xFF) )
		break;
	delayMicroseconds(10);//10 (org) or 12??
    }
    //digitalWrite(10,HIGH);
    myCAM.CS_HIGH();
}

void read_fifo_again(ArduCAM myCAM)
{
    read_fifo_burst(myCAM);
}
/////////////////////////////////////////////////////
void mySerial_write_str(char *str)      {  if (  MAIN_SERIAL_MODE == 0 )  Serial.write(str);     else    BTSerial.write(str);   }
void mySerial_println_str(char *str)    {  if (  MAIN_SERIAL_MODE == 0 )  Serial.println(str);   else    BTSerial.println(str); }
int mySerial_read()  { if (  MAIN_SERIAL_MODE == 0 )   	return Serial.read();   else        return BTSerial.read(); }
void mySerial_println_num(uint32_t num) {  if (  MAIN_SERIAL_MODE == 0 )  Serial.println(num);   else    BTSerial.println(num); }
void mySerial_write_num(uint8_t num)    {  if (  MAIN_SERIAL_MODE == 0 )  Serial.write(num);     else    BTSerial.write(num);   }
 
