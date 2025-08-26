
#include <DS3231.h>
#include <Wire.h>



//####################################### time stuff ###############################
//borrowed from Eric Ayars 3231 library

DS3231 myRTC;

byte year;
byte month;
byte date;
byte dOW;
byte hour;
byte minute;
byte second;

void getDateStuff(byte& year, byte& month, byte& date, byte& dOW,
                  byte& hour, byte& minute, byte& second) {
    // Call this if you notice something coming in on
    // the serial port. The stuff coming in should be in
    // the order YYMMDDwHHMMSS, with an 'x' at the end.
    boolean gotString = false;
    char inChar;
    byte temp1, temp2;
    char inString[20];
    
    byte j=0;
    while (!gotString) {
        if (Serial.available()) {
            inChar = Serial.read();
            inString[j] = inChar;
            j += 1;
            if (inChar == 'x') {
                gotString = true;
            }
        }
    }
    Serial.println(inString);
    // Read year first
    temp1 = (byte)inString[0] -48;
    temp2 = (byte)inString[1] -48;
    year = temp1*10 + temp2;
    // now month
    temp1 = (byte)inString[2] -48;
    temp2 = (byte)inString[3] -48;
    month = temp1*10 + temp2;
    // now date
    temp1 = (byte)inString[4] -48;
    temp2 = (byte)inString[5] -48;
    date = temp1*10 + temp2;
    // now Day of Week
    dOW = (byte)inString[6] - 48;
    // now hour
    temp1 = (byte)inString[7] -48;
    temp2 = (byte)inString[8] -48;
    hour = temp1*10 + temp2;
    // now minute
    temp1 = (byte)inString[9] -48;
    temp2 = (byte)inString[10] -48;
    minute = temp1*10 + temp2;
    // now second
    temp1 = (byte)inString[11] -48;
    temp2 = (byte)inString[12] -48;
    second = temp1*10 + temp2;
}
//###########################################################################################


//####################################  Pins ################################################
const uint8_t PISO_DATA = 3; // 74HC165 Q7 -> D3
const uint8_t SIPO_DATA = 4; // 74HC595 DS -> D4
const uint8_t LATCH_PIN = 5; // 74HC165 SH/LD + 74HC595 RCLK -> D5
const uint8_t CLOCK_PIN = 6; // CLK shared -> D6
//###########################################################################################


//################################### module count ##########################################
// Upper bound for possible amount of connected modules (for safety) expand if nececarry
const uint8_t MAX_165 = 16;           
const uint8_t MAX_595 = MAX_165 * 2;
// the read bytes, and the bytes to be printed out
uint8_t inputBytes[MAX_165];   // holds one byte per 165
uint8_t outputBytes[MAX_595];  // holds one byte per 595
//###########################################################################################




// shortening for common on/off pulses
inline void pulseClock() { // pulse the clock once
  digitalWrite(CLOCK_PIN, HIGH);
  digitalWrite(CLOCK_PIN, LOW);
}




//######################################  data in/ 165 stuff  #########################################################

inline void load165() {   // load the paralell inputs for 165
  digitalWrite(LATCH_PIN, LOW);   // SH/LD low = load parallel
  delayMicroseconds(1);
  digitalWrite(LATCH_PIN, HIGH);  // back to shift mode
  delayMicroseconds(1);
}

// Reads one full 165 and tells you if it's the last one (only the last in the chain has b7 set to 1)
uint8_t read165Byte(bool &isLast) {
  uint8_t b = 0;
  int lastBit = 0;

  for (uint8_t i = 0; i < 8; i++) {
    int bitVal = digitalRead(PISO_DATA);
    b |= (bitVal << i);   
    pulseClock();
  }

  isLast = (b & 1) == 1;
  return b;
}


// read all of the 165's untill the one with a high b7 (sentinel)
void fillInput() {

  load165();

  // Read chip-by-chip until sentinel
  uint8_t num165 = 0;  // the number of 165's seen/modules connected
  for (; num165 < MAX_165; num165++) {
    bool lastChip = false;
    inputBytes[num165] = read165Byte(lastChip);
    if (lastChip) {
      num165++;  // count this one too
      break;
    }
  }
  //if (num165 == 0) return; // nothing detected


  // Debug print
  Serial.print("Read "); Serial.print(num165); Serial.println(" x 165's:");
  for (uint8_t i = 0; i < num165; i++) {
    Serial.print("Chip ");
    Serial.print(i);
    Serial.print(" = ");
    for (int bit = 7; bit >= 0; bit--) {
      Serial.print((inputBytes[i] >> bit) & 1);
    }
    Serial.println();
  }
}
//############################################################################################


//######################################  data out / 595 stuff  ##############################
void shift16bit(uint16_t word) {  // i thought you could pull latch low, shift all the bits through, then pull latch high to reveal it, but appearantly not. maybe i implemented something wrong in circuit, but if you do the latch outside the loop everything breaks
  for (int b = 15; b >= 0; b--) {    
    digitalWrite(LATCH_PIN, LOW);
    digitalWrite(SIPO_DATA, (word >> b) & 1);
    //delay(10);  // add back for cool shift register look
    pulseClock();
    digitalWrite(LATCH_PIN, HIGH);
  }
}


const uint8_t intTo8bitA[10] = {  // order of bits to represent a digit on the first of the displays
  0b01110111, // 0
  0b00010100, // 1
  0b10110011, // 2
  0b10110110, // 3
  0b11010100, // 4
  0b11100110, // 5
  0b11100111, // 6
  0b00110100, // 7
  0b11110111, // 8
  0b11110110 // 9
};
const uint8_t intTo8bitB[10] = {  // order of bits on second display. Ideally, this would have been the same, but that would have made routing a lot messier (blame whoever decided the 7seg pinout)
  0b01110111, // 0
  0b01000001, // 1
  0b00111011, // 2
  0b01101011, // 3
  0b01001101, // 4
  0b01101110, // 5
  0b01111110, // 6
  0b01000011, // 7
  0b01111111, // 8
  0b01101111 // 9
};

void shiftDigit(int num){  // the function that will be called in the main bit of the code. Get an int, figure out the corresponding word for each 7seg, and shift it all out
  uint8_t wordA;  uint8_t wordB;
  if(num > 9){
    wordA = intTo8bitA[num / 10];  // tieren i første digit
    wordB = intTo8bitB[num % 10];  // eneren i andre digit
  }
  else{
    wordA = intTo8bitA[0];         // 0 i første digit når vi er under 10
    wordB = intTo8bitB[num % 10];  // eneren i andre digit
  }

  uint16_t combined = ((uint16_t)wordA << 8) | wordB;  // concatenate 'em

  shift16bit(combined);
}
//############################################################################################



//############################# define module class, ohhh yeah baby, classes ############################################
class Module {      
  public:         
    float timezone;   // +- hours (including decimal)
    bool isMinute;    // is this a minute module ?
};


void setup() {
  // For RTC
  Wire.begin();

  // declare pinmodes
  pinMode(PISO_DATA, INPUT);
  pinMode(SIPO_DATA, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);

  // set initial values
  digitalWrite(CLOCK_PIN, LOW);
  digitalWrite(LATCH_PIN, HIGH);

  // todo: maybe increase for more accurate time-setting.
  Serial.begin(9600);

  fillInput(); // read the input registers
}

void loop() {
  // If something is coming in on the serial line, it's
    // a time correction so set the clock accordingly.
    if (Serial.available()) {
        getDateStuff(year, month, date, dOW, hour, minute, second);
        
        myRTC.setClockMode(false);  // set to 24h
        //setClockMode(true); // set to 12h
        
        myRTC.setYear(year);
        myRTC.setMonth(month);
        myRTC.setDate(date);
        myRTC.setDoW(dOW);
        myRTC.setHour(hour);
        myRTC.setMinute(minute);
        myRTC.setSecond(second);
        
        // Test of alarm functions
        // set A1 to one minute past the time we just set the clock
        // on current day of week.
        myRTC.setA1Time(dOW, hour, minute+1, second, 0x0, true,
                        false, false);
        // set A2 to two minutes past, on current day of month.
        myRTC.setA2Time(date, hour, minute+2, 0x0, false, false,
                        false);
        // Turn on both alarms, with external interrupt
        myRTC.turnOnAlarm(1);
        myRTC.turnOnAlarm(2);
        
    }


  // todo: implement the RTC, and figure out how it handles timezones, (need to know that before we go to the module-timezone controll)
  // todo: create "module" class, 
    // self variables:
        // timezone, 
        // weather it is a minute type
    // functions:
        // return time in self.timezone / return minute in self.timezone (if minute)
  
  delay(1000);      

  for(int i = 0; i <= 99; i++){  
    shiftDigit(i);
    delay(500);
  }
  
  delay(3000);
}
