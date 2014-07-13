/* 
 Weather Station
 By: J. Steven Welch
 July 10, 2014
 Much of this code is based on Nathan Seidle's code which is in turn based on Mike Grusin's USB Weather Board code.
 
 This code reads all the various sensors (wind speed, direction, rain gauge, humidty, pressure, light, batt_lvl)
 and sends it, via XBee, to a BeagleBone Black. The BeagleBone black then forwards that data to Weather Underground.
*/
#include <avr/wdt.h> //We need watch dog for this program
#include <Wire.h> //I2C needed for sensors
#include "MPL3115A2.h" //Pressure sensor
#include "HTU21D.h" //Humidity sensor
#include <XBee.h>

MPL3115A2 myPressure; //Create an instance of the pressure sensor
HTU21D myHumidity; //Create an instance of the humidity sensor
XBee xbee=XBee();

//Hardware pin definitions
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// digital I/O pins
const byte WSPEED = 3;
const byte RAIN = 2;
const byte STAT1 = 7;
const byte XBEE_SLEEP_PIN = 4;

// analog I/O pins
const byte WDIR = A0;
const byte LIGHT = A1;
const byte BATT = A2;
const byte REFERENCE_3V3 = A3;
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Global Variables
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//*****************************************************************
const float LOCAL_ALT_METERS=371.0; //SET FOR YOUR LOCAL ALTITUDE *
//*****************************************************************
long lastSecond; //The millis counter to see when a second rolls by
byte seconds; //When it hits 60, increase the current minute
byte seconds_2m; //Keeps track of the "wind speed/dir avg" over last 2 minutes array of data
byte minutes; //Keeps track of where we are in various arrays of data
byte minutes_10m; //Keeps track of where we are in wind gust/dir over last 10 minutes array of data

long lastWindCheck = 0;
volatile long lastWindIRQ = 0;
volatile byte windClicks = 0;
volatile long lastRainIRQ = 0;

uint8_t payload[32]; //buffer for data to send through XBee
uint8_t respData=0; //byte to hold data sent from BeagleBone to trigger events on the Arduino
//******************************************************************************************************************************
Tx16Request tx=Tx16Request(0x1000,payload,sizeof(payload)); //XBee packet to send - SET THE FIRST VALUE TO YOUR OWN XBEE VALUE *
//******************************************************************************************************************************
Rx16Response rx16 = Rx16Response();
uint8_t reportTime=20; //how often to report the weather - can be updated from BBB
uint8_t counter=0; //counter to track seconds for the above reportTime

//We need to keep track of the following variables:
//Wind speed/dir each update (no storage)
//Wind gust/dir over the day (no storage)
//Wind speed/dir, avg over 2 minutes (store 1 per second)
//Wind gust/dir over last 10 minutes (store 1 per minute)
//Rain over the past hour (store 1 per minute)
//Total rain over date (store one per day)

byte windspdavg[120]; //120 bytes to keep track of 2 minute average
int winddiravg[120]; //120 ints to keep track of 2 minute average
float windgust_10m[10]; //10 floats to keep track of 10 minute max
int windgustdirection_10m[10]; //10 ints to keep track of 10 minute max
volatile float rainHour[60]; //60 floating numbers to keep track of 60 minutes of rain

//These are all the weather values that wunderground expects:
int winddir; // [0-360 instantaneous wind direction]
float windspeedmph; // [mph instantaneous wind speed]
float windgustmph; // [mph current wind gust, using software specific time period]
int windgustdir; // [0-360 using software specific time period]
float windspdmph_avg2m; // [mph 2 minute average wind speed mph]
int winddir_avg2m; // [0-360 2 minute average wind direction]
float windgustmph_10m; // [mph past 10 minutes wind gust mph ]
int windgustdir_10m; // [0-360 past 10 minutes wind gust direction]
float humidity; // [%]
float tempf; // [temperature F]
float rainin; // [rain inches over the past hour)] -- the accumulated rainfall in the past 60 min
volatile float dailyrainin; // [rain inches so far today in local time]
float baromin;//a calculated number
float pressure;//a raw reading
float dewptf;//a calculated number

//These are not wunderground values, they are just for us
float batt_lvl = 4.0;
float light_lvl = 2.2;

// volatiles are subject to modification by IRQs
extern volatile unsigned long timer0_millis;
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Interrupt routines (these are called by the hardware interrupts, not by the main code)
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void rainIRQ()
// Count rain gauge bucket tips as they occur
// Activated by the magnet and reed switch in the rain gauge, attached to input D2
{
  if (millis() - lastRainIRQ > 10)
  {
    lastRainIRQ=millis();
    dailyrainin +=0.011;
    rainHour[minutes]+=0.011;
  }
}

void wspeedIRQ()
// Activated by the magnet in the anemometer (2 ticks per rotation), attached to input D3
{
  if (millis() - lastWindIRQ > 10) // Ignore switch-bounce glitches less than 10ms (142MPH max reading) after the reed switch closes
  {
    lastWindIRQ = millis(); //Grab the current time
    windClicks++; //There is 1.492MPH for each click per second.
  }
}

void setup()
{
  wdt_reset(); //Pet the dog
  wdt_disable(); //We don't want the watchdog during init

  Serial.begin(9600);
  
  xbee.setSerial(Serial);
  pinMode(XBEE_SLEEP_PIN,OUTPUT);
  digitalWrite(XBEE_SLEEP_PIN,HIGH); //xbee is put to sleep

  pinMode(WSPEED, INPUT_PULLUP); // input from wind meters windspeed sensor
  pinMode(RAIN, INPUT_PULLUP); // input from wind meters rain gauge sensor

  pinMode(WDIR, INPUT);
  pinMode(LIGHT, INPUT);
  pinMode(BATT, INPUT);
  pinMode(REFERENCE_3V3, INPUT);
  
  pinMode(STAT1, OUTPUT);

  midnightReset(); //Reset rain totals

  //Configure the pressure sensor
  myPressure.begin(); // Get sensor online
  myPressure.setModeBarometer(); // Measure pressure in Pascals from 20 to 110 kPa
  myPressure.setOversampleRate(128); // Set Oversample to the recommended 128
  myPressure.enableEventFlags(); // Enable all three pressure and temp event flags 
  myPressure.setModeActive(); // Go to active mode and start measuring!

  //Configure the humidity sensor
  myHumidity.begin();

  seconds = 0;
  lastSecond = millis();

  // attach external interrupt pins to IRQ functions
  attachInterrupt(0, rainIRQ, FALLING);
  attachInterrupt(1, wspeedIRQ, FALLING);

  // turn on interrupts
  interrupts();

  wdt_enable(WDTO_1S); //Unleash the beast
}

void loop()
{
  wdt_reset(); //Pet the dog

  //Keep track of which minute it is
  if(millis() - lastSecond >= 1000)
  {
    lastSecond += 1000;

    //Take a speed and direction reading every second for 2 minute average
    if(++seconds_2m > 119) seconds_2m = 0;

    //Calc the wind speed and direction every second for 120 second to get 2 minute average
    float currentSpeed = get_wind_speed();
    windspeedmph=currentSpeed;
    int currentDirection = get_wind_direction();
    windspdavg[seconds_2m] = (int)currentSpeed;
    winddiravg[seconds_2m] = currentDirection;

    //Check to see if this is a gust for the minute
    if(currentSpeed > windgust_10m[minutes_10m])
    {
      windgust_10m[minutes_10m] = currentSpeed;
      windgustdirection_10m[minutes_10m] = currentDirection;
    }

    //Check to see if this is a gust for the day
    //Resets at midnight each night
    if(currentSpeed > windgustmph)
    {
      windgustmph = currentSpeed;
      windgustdir = currentDirection;
    }

    //Blink stat LED briefly to show we are alive
    digitalWrite(STAT1, HIGH);
    delay(25);
    digitalWrite(STAT1, LOW);
    
    //Is it time to report the weather?
    if (++counter>=reportTime)
    {
      counter=0;
      reportWeather();
    }
    //If we roll over 60 seconds then update the arrays for rain and windgust
    if(++seconds > 59)
    {
      seconds = 0;
      if(++minutes > 59) minutes = 0;
      if(++minutes_10m > 9) minutes_10m = 0;

      rainHour[minutes] = 0; //Zero out this minute's rainfall amount
      windgust_10m[minutes_10m] = 0; //Zero out this minute's gust
    }
  }
//******************************************
  //See if we got something from the BeagleBoneBlack
  if(respData!=0)
  {
    if(respData == 0xFE) //Indicates that it is midnight local time
    {
      midnightReset(); //Reset a bunch of variables like rain and total rain
    }
    else if(respData == 0xFF) //Force a hardware reset
    {
      delay(2500); //This will cause the system to reset because we don't pet the dog
    }else{ //We got a reportTime number
      reportTime=respData; //Can report data every 3 to 253 seconds
      //Note that Weather Underground will not take data faster than every 2.5 seconds. 
    }
    respData=0;
  }
  delay(100); //Update every 100ms. No need to go any faster.
}

//When the BBB tells us it's midnight, reset some things
//millis() gets reset to zero here as well so there are no rollover issues
void midnightReset()
{
  //reset millis
  uint8_t oldSREG=SREG;
  cli();
  timer0_millis=(long)0;
  SREG=oldSREG;
  
  dailyrainin = 0; //Reset daily amount of rain

  windgustmph = 0; //Zero out this minute's gust
  windgustdir = 0; //Zero out the gust direction
  
  minutes = 0; //Reset minute tracker
  seconds = 0;
  lastSecond = millis(); //Reset variable used to track minutes
  lastWindCheck=millis(); //Reset variable to track instantaneous wind speed
  lastRainIRQ=millis();
  lastWindIRQ=millis();
}

//Calculates each of the variables that wunderground is expecting
void calcWeather()
{
  //Calc winddir
  winddir = get_wind_direction();

  //windspeedmph already calcuated in main loop
  //windgustmph and windgustdir are calculated throughout the day

  //Calc windspdmph_avg2m
  float temp = 0;
  for(int i = 0 ; i < 120 ; i++)
    temp += windspdavg[i];
  temp /= 120.0;
  windspdmph_avg2m = temp;

  //Calc winddir_avg2m
  temp = 0; //Can't use winddir_avg2m because it's an int
  for(int i = 0 ; i < 120 ; i++)
    temp += winddiravg[i];
  temp /= 120;
  winddir_avg2m = temp;

  //Calc windgustmph_10m
  //Calc windgustdir_10m
  //Find the largest windgust in the last 10 minutes
  windgustmph_10m = 0;
  windgustdir_10m = 0;
  //Step through the 10 minutes  
  for(int i = 0; i < 10 ; i++)
  {
    if(windgust_10m[i] > windgustmph_10m)
    {
      windgustmph_10m = windgust_10m[i];
      windgustdir_10m = windgustdirection_10m[i];
    }
  }

  //Calc humidity
  humidity = myHumidity.readHumidity();

  //Calc tempf from pressure sensor
  tempf = myPressure.readTempF();

  //Total rainfall for the day is calculated within the interrupt
  //Calculate amount of rainfall for the last 60 minutes
  rainin = 0;  
  for(int i = 0 ; i < 60 ; i++)
    rainin += rainHour[i];

  //Calc pressure
  baromin = myPressure.readPressureinHg(LOCAL_ALT_METERS);

  //Calc dewptf
  float tempC=(tempf-32)*5/9.0;
  float L=log(humidity/100.0);
  float M=17.27*tempC;
  float N=237.3+tempC;
  float B=(L+(M/N))/17.27;
  dewptf=((237.3*B)/(1.0-B))*9/5.0 +32;
  
  //Calc light level
  light_lvl = get_light_level();

  //Calc battery level
  batt_lvl = get_battery_level();
}

//Returns the voltage of the light sensor based on the 3.3V rail
//This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
float get_light_level()
{
  float operatingVoltage = averageAnalogRead(REFERENCE_3V3);

  float lightSensor = averageAnalogRead(LIGHT);
  
  operatingVoltage = 3.3 / operatingVoltage; //The reference voltage is 3.3V
  
  lightSensor *= operatingVoltage;
  
  return(lightSensor);
}

//Returns the voltage of the raw pin based on the 3.3V rail
//This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
//Battery level is connected to the RAW pin on Arduino and is fed through two 5% resistors:
//3.9K on the high side (R1), and 1K on the low side (R2)
float get_battery_level()
{
  float operatingVoltage = averageAnalogRead(REFERENCE_3V3);

  float rawVoltage = averageAnalogRead(BATT);
  
  operatingVoltage = 3.30 / operatingVoltage; //The reference voltage is 3.3V
  
  rawVoltage *= operatingVoltage; //Convert the 0 to 1023 int to actual voltage on BATT pin
  
  rawVoltage *= 4.90; //(3.9k+1k)/1k - multiply BATT voltage by the voltage divider to get actual system voltage
  
  return(rawVoltage);
}

//Returns the instataneous wind speed
float get_wind_speed()
{
  float deltaTime = millis() - lastWindCheck; //750ms

  deltaTime /= 1000.0; //Covert to seconds

  float windSpeed = (float)windClicks / deltaTime; //3 / 0.750s = 4

  windClicks = 0; //Reset and start watching for new wind
  lastWindCheck = millis();

  windSpeed *= 1.492; //4 * 1.492 = 5.968MPH

  return(windSpeed);
}

int get_wind_direction() 
// read the wind direction sensor, return heading in degrees
{
  unsigned int adc;

  adc = averageAnalogRead(WDIR); // get the current reading from the sensor

  // The following table is ADC readings for the wind direction sensor output, sorted from low to high.
  // Each threshold is the midpoint between adjacent headings. The output is degrees for that ADC reading.
  // Note that these are not in compass degree order! See Weather Meters datasheet for more information.

  if (adc < 380) return (113);
  if (adc < 393) return (68);
  if (adc < 414) return (90);
  if (adc < 456) return (158);
  if (adc < 508) return (135);
  if (adc < 551) return (203);
  if (adc < 615) return (180);
  if (adc < 680) return (23);
  if (adc < 746) return (45);
  if (adc < 801) return (248);
  if (adc < 833) return (225);
  if (adc < 878) return (338);
  if (adc < 913) return (0);
  if (adc < 940) return (293);
  if (adc < 967) return (315);
  if (adc < 990) return (270);
  return (-1); // error, disconnected?
}


//Reports the weather string to the BBB
void reportWeather()
{
  calcWeather(); //Go calc all the various sensors
  //load all data into the payload buffer for transfer to the BBB
  payload[0]=winddir>>8 & 0xFF;
  payload[1]=winddir & 0xFF;
  payload[6]=windgustdir>>8 & 0xFF;
  payload[7]=windgustdir & 0xFF;
  payload[10]=winddir_avg2m>>8 & 0xFF;
  payload[11]=winddir_avg2m & 0xFF;
  payload[14]=windgustdir_10m>>8 & 0xFF;
  payload[15]=windgustdir_10m & 0xFF;
  //convert floats to unsigned ints as they will not be bigger than 16 bits (even when multiplied by 100)
  unsigned int twindspeedmph=(unsigned int)(windspeedmph*10);
  payload[2]=twindspeedmph>>8 & 0xFF;
  payload[3]=twindspeedmph & 0xFF;
  unsigned int twindgustmph=(unsigned int)(windgustmph*10);
  payload[4]=twindgustmph>>8 & 0xFF;
  payload[5]=twindgustmph & 0xFF;
  unsigned int twindspdmph_avg2m=(unsigned int)(windspdmph_avg2m*10);
  payload[8]=twindspdmph_avg2m>>8 & 0xFF;
  payload[9]=twindspdmph_avg2m & 0xFF;
  unsigned int twindgustmph_10m=(unsigned int)(windgustmph_10m*10);
  payload[12]=twindgustmph_10m>>8 & 0xFF;
  payload[13]=twindgustmph_10m & 0xFF;
  unsigned int thumidity=(unsigned int)(humidity*10);
  payload[16]=thumidity>>8 & 0xFF;
  payload[17]=thumidity & 0xFF;
  unsigned int ttempf=(unsigned int)(tempf*10);
  payload[18]=ttempf>>8 & 0xFF;
  payload[19]=ttempf & 0xFF;
  unsigned int trainin=(unsigned int)(rainin*100);
  payload[20]=trainin>>8 & 0xFF;
  payload[21]=trainin & 0xFF;
  unsigned int tdailyrainin=(unsigned int)(dailyrainin*100);
  payload[22]=tdailyrainin>>8 & 0xFF;
  payload[23]=tdailyrainin & 0xFF;
  unsigned int tpressure=(unsigned int)(baromin*100);
  payload[24]=tpressure>>8 & 0xFF;
  payload[25]=tpressure & 0xFF;
  unsigned int tdewptf=(unsigned int)(dewptf*10);
  payload[26]=tdewptf>>8 & 0xFF;
  payload[27]=tdewptf & 0xFF;
  unsigned int tbatt_lvl=(unsigned int)(batt_lvl*100);
  payload[28]=tbatt_lvl>>8 & 0xFF;
  payload[29]=tbatt_lvl & 0xFF;
  unsigned int tlight_lvl=(unsigned int)(light_lvl*100);
  payload[30]=tlight_lvl>>8 & 0xFF;
  payload[31]=tlight_lvl & 0xFF;
  digitalWrite(XBEE_SLEEP_PIN,LOW); //wake up the xbee
  delay(50); //give it time to wake up
  xbee.send(tx); //send the data
  wdt_reset(); //Pet the dog
  long newTime=millis();
  while (millis()-newTime <=500) //we give the BBB 500 mS to respond
  {
    xbee.readPacket();
    if (xbee.getResponse().isAvailable())
    {
      if (xbee.getResponse().getApiId()==RX_16_RESPONSE)
      {
        xbee.getResponse().getRx16Response(rx16);
        respData=rx16.getData(0);
      }
    }
  }
  wdt_reset(); //Pet the dog again
  digitalWrite(XBEE_SLEEP_PIN,HIGH); //put it back to sleep
/*
  Serial.println();
  Serial.print("$,winddir=");
  Serial.print(winddir);
  Serial.print(",windspeedmph=");
  Serial.print(windspeedmph, 1);
  Serial.print(",windgustmph=");
  Serial.print(windgustmph, 1);
  Serial.print(",windgustdir=");
  Serial.print(windgustdir);
  Serial.print(",windspdmph_avg2m=");
  Serial.print(windspdmph_avg2m, 1);
  Serial.print(",winddir_avg2m=");
  Serial.print(winddir_avg2m);
  Serial.print(",windgustmph_10m=");
  Serial.print(windgustmph_10m, 1);
  Serial.print(",windgustdir_10m=");
  Serial.print(windgustdir_10m);
  Serial.print(",humidity=");
  Serial.print(humidity, 1);
  Serial.print(",tempf=");
  Serial.print(tempf, 1);
  Serial.print(",rainin=");
  Serial.print(rainin, 2);
  Serial.print(",dailyrainin=");
  Serial.print(dailyrainin, 2);
  Serial.print(",pressure=");
  Serial.print(baromin, 2);
  Serial.print(",dewpoint=");
  Serial.print(dewptf,1);
  Serial.print(",batt_lvl=");
  Serial.print(batt_lvl, 2);
  Serial.print(",light_lvl=");
  Serial.print(light_lvl, 2);
  Serial.print(",");
  Serial.println("#");
  Serial.println();
  */
}

//Takes an average of readings on a given pin
//Returns the average
int averageAnalogRead(int pinToRead)
{
  byte numberOfReadings = 8;
  unsigned int runningValue = 0; 

  for(int x = 0 ; x < numberOfReadings ; x++)
    runningValue += analogRead(pinToRead);
  runningValue /= numberOfReadings;

  return(runningValue);  
}
