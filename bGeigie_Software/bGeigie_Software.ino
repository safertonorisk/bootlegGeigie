#include <SoftwareSerial.h>
#include <TinyGPS.h>
#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SimpleTimer.h>
#include <ArduinoQueue.h>

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

static void smartdelay(unsigned long ms);
float print_float(float val, float invalid, int len, int prec);
int print_int(unsigned long val, unsigned long invalid, int len);
String print_date(TinyGPS &gps);
static void print_str(const char *str, int len);

long counts=0;
long inMinute = 0;
long inFiveSecs = 0;
long allCounts = 0;
String sdStat="Warm^",utcDate="",date="",valid="";
boolean isFirst=true;

int satNum;
float flat,flon,falt,hdop;
unsigned long age;
//--------------------- MAIN FUNCTIONS --------------------//
  #if (SSD1306_LCDHEIGHT != 32) // 
  #error("Height incorrect, please fix Adafruit_SSD1306.h!");
  #endif
  
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); 
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
  if(minuteBuffer.isFull()){
    inMinute -= minuteBuffer.dequeue();    

    smartdelay(1000);

    gps.f_get_position(&flat, &flon, &age);
    falt = gps.f_altitude();
    satNum = gps.satellites(), 
    hdop = gps.hdop()/100;
    date = print_date(gps);
    utcDate = date;
    
    valid = (satNum>=6&&satNum!=TinyGPS::GPS_INVALID_SATELLITES) ? "A":"V";
    char latHem;
    if(flat<0) {
      flat*=-1;
      latHem = 'S';
    }
    else{latHem = 'N';}
      
    char lonHem;
    if(flon<0) {
      flon*=-1;
      lonHem = 'W';
    }
    else{lonHem = 'E';}
        
    if(age<=5000ul){
      ISOfy();
      String lineOut = "$BNRDD,1326,"+ date + "," + inMinute + "," + inFiveSecs + "," + allCounts + ",A," + String(flat,4) +"," + latHem +"," +String(flon,4)+","+lonHem +","+String(falt,1)+","+valid+","+String(satNum)+","+String(hdop,2)+"*";
      lineOut+=getCheckSum(lineOut);
      writeSD(lineOut);
      //Serial.println(lineOut);
    }
    else{
      isFirst=true;
    }
  }
      freeRam ();
      //Serial.println(ESP.getFreeHeap());
      displayVals();
  
}
void ISOfy(){
  utcDate = utcDate.substring(5,7)+utcDate.substring(8,10);
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
    String filename="1326"+utcDate+".log";
    Serial.println(filename);
    if(logFile = SD.open(filename, FILE_WRITE)){
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
  
  for(int i=0;i<7-String(inMinute).length()-String(satNum).length();i++){
    display.print(" ");
  }
  display.print(satNum);
  display.println("^");
  display.display();
}

//--------------

static void smartdelay(unsigned long ms)
{
  unsigned long start = millis();
  do 
  {
    while (GPSMod.available())
      gps.encode(GPSMod.read());
  } while (millis() - start < ms);
}

String print_date(TinyGPS &gps)
{
  int year;
  byte month, day, hour, minute, second, hundredths;
  unsigned long age;
  gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);
  if (age == TinyGPS::GPS_INVALID_AGE)
    Serial.print("");
  else
  {
    char sz[32];
    sprintf(sz, "%02d-%02d-%02dT%02d:%02d:%02dZ",
        year, month, day, hour, minute, second);
    return sz;
  }
  print_int(age, TinyGPS::GPS_INVALID_AGE, 5);
  smartdelay(0);
}
int print_int(unsigned long val, unsigned long invalid, int len)
{
  char sz[32];
  if (val == invalid)
    strcpy(sz, "*******");
  else
    sprintf(sz, "%ld", val);
  sz[len] = 0;
  for (int i=strlen(sz); i<len; ++i)
    sz[i] = ' ';
  if (len > 0) 
    sz[len-1] = ' ';
  Serial.print(sz);
  smartdelay(0);
}
