#include <SoftwareSerial.h>
#include <TinyGPS.h>
#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SimpleTimer.h>
#include <ArduinoQueue.h>

#define LOGO16_GLCD_HEIGHT 16 
#define LOGO16_GLCD_WIDTH  16 
#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2
#define OLED_RESET -1
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define RxGPS 0
#define TxGPS 2

SoftwareSerial GPSMod(RxGPS, TxGPS);
TinyGPS gps;
File logFile;
SimpleTimer timerCount;
ArduinoQueue<unsigned long> minuteBuffer(13);

void gpsdump(TinyGPS &gps);
void printFloat(double f, int digits);

long counts=0;
long inMinute = 0;
long inFiveSecs = 0;
long allCounts = 0;
String satNum,utcTime,utcDate,lat,lon,alt,hdop,valid, sdStat="Warm^";
boolean isFirst=true;

//--------------------- MAIN FUNCTIONS --------------------//
  #if (SSD1306_LCDHEIGHT != 32) // 
  #error("Height incorrect, please fix Adafruit_SSD1306.h!");
  #endif
  
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600); 
  GPSMod.begin(9600);
  delay(3000);
  //pinMode(1, OUTPUT);
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();
  
  
  timerCount.setInterval(5000,count5sec);
}

void loop() {
  // put your main code here, to run repeatedly:
  timerCount.run();
  //digitalWrite(1, LOW);
  //display.invertDisplay(false);
  if(analogRead(A0)>=10){
    counts++; 
  }
  
}

//--------------------- SENSOR OPERATIONS --------------------//

void count5sec(){
  inFiveSecs = counts;
  inMinute += counts;
  allCounts += counts;
  minuteBuffer.enqueue(counts);
  counts = 0; 
  Serial.println(inMinute);
  if(minuteBuffer.isFull()){
    inMinute -= minuteBuffer.dequeue();    
    String readString = "";
    boolean gga = false,rmc = false;
    long prevTime=millis();
    while (GPSMod.available()&&!gga||!rmc&&millis()-prevTime>4000) {
      
        /*char c = GPSMod.read();  //gets one byte from serial buffer
        readString += c; //makes the string readString*/
        readString = GPSMod.readStringUntil('\r');Serial.println(readString);
        int readLen = readString.length();
        if(readString.indexOf("GPRMC")>0&&readString.indexOf("⸮")<0){
          rmc=true;
          //Serial.println(readString);
          utcDate = "";
          for(int i=0, arrind = 0;i<readLen;i++){
            if(readString[i]==',') {arrind++;}
            else{
              switch(arrind){
                case 9: 
                    utcDate+=readString[i]; break;
                case 10:
                    i=readLen;break;
              }
            }
          }
        }
        else if(readString.indexOf("GPGGA")>0&&readString.indexOf("⸮")<0){
          //Serial.println(readString);
          gga=true;satNum = ""; utcTime = "";  lat = ""; lon = ""; alt = ""; hdop = ""; valid = ""; 
          for(int i=0, arrind = 0;i<readLen;i++){
            if(readString[i]==',') {arrind++;}
            else{
              switch(arrind){
                case 1: utcTime+=readString[i];break;
                case 2: lat+=readString[i];break;
                case 3: lat+=(","+(String)readString[i]);break;
                case 4: lon+=readString[i];break;
                case 5: lon+=(','+(String)readString[i]);break;
                case 6: valid = (readString[i]=='1')? "A": "V";break;
                case 7: satNum += readString[i];break;
                case 8: hdop += readString[i];break;
                case 9: alt += readString[i];break;
                case 10: i = readLen;break;
              }
            }
          }
        }
    }
    if(utcTime!=""&&utcDate!=""&&lat!=""&&lon!=""&&alt!=""&&hdop!=""&&valid=="V"||valid!=""&&satNum!="00"||satNum!=""||satNum!="0"){
      ISOfy();
      String lineOut = "$BNRDD,1326,"+ utcDate +"T" + utcTime+ "Z," + inMinute + "," + inFiveSecs + "," + allCounts + ",A," + lat +"," +lon+","+alt+","+valid+","+satNum+","+hdop+"*";
      lineOut+=getCheckSum(lineOut);
      writeSD(lineOut);
      Serial.println(lineOut);
    }
  }
      freeRam ();
      //Serial.println(ESP.getFreeHeap());
      displayVals();
  
}
void ISOfy(){
  utcDate = "20"+utcDate.substring(4,6)+"-"+utcDate.substring(2,4)+"-"+utcDate.substring(0,2);
  utcTime = utcTime.substring(0,2)+":"+utcTime.substring(2,4)+":"+utcTime.substring(4,6);
}
String getCheckSum(String inp){
        String out="";
        int checksum = 0;
        for(int i = 1; inp[i]!='*'; i++) {
            checksum = checksum ^ inp[i];
        }
        out=String(checksum,HEX);
        out.toUpperCase();
        return out;
    }
void writeSD(String GeigieFormat){
  if(SD.begin(15)){
    if(logFile = SD.open("1326"+utcDate.substring(5,7)+utcDate.substring(8,10)+".log", FILE_WRITE)){
      if(isFirst){
        logFile.println("# NEW LOG");
        logFile.println("# format=1.3.6nano");
        logFile.println("# deadtime=on");
        isFirst=false;
      }      
      logFile.println(GeigieFormat);
      logFile.close();
      sdStat="Write";
    }
    else{
      sdStat="No SD";
    }
  }
  
  else{
      sdStat="No SD";
    }
}

int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
void displayVals(){
  float usv = (float)inMinute*0.003;
  int cpmOut = inMinute;
  display.clearDisplay();
  display.display();
  display.setRotation(2);
  display.setTextSize(2);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0,0);
  
  if(usv<=10)display.print(usv,3);
  else if(usv<=100)display.print(usv,2);
  else display.print(usv,1);
  display.println("uSv/h");
  
  display.setTextSize(1);
  display.print(inMinute);display.print("CPM");
  if(inMinute>100){display.invertDisplay(true);}
  else{display.invertDisplay(false);}
  for(int i=0;i<6-String(inMinute).length();i++){
    display.print(" ");
  }
  display.print(sdStat);
  
  int sats = satNum.toInt();
  for(int i=0;i<7-String(inMinute).length()-String(sats).length();i++){
    display.print(" ");
  }
  display.print(sats);
  display.println("^");
  display.display();
}
