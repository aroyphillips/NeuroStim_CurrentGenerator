#include <SPI.h>


#define AOut A22
#define rstPin 25
#define distPin A21
 
#define MAXAOUT 3.3


float VAL2DAC = 4095/3.3; // Volt = val*3.3/4095 --> 

// Incoming Bit Stream should look like this: '<p0,2.00,150,010!t0,2,150,10,20,10!^1,3,100,80,30!g0,2,120,100,10!r0,3,60,10!>'


// registers to store incoming data
const byte numChars = 32;
char pulse[14];
char train[numChars];
char gaus[numChars];
char tri[numChars];


// booleans to guide reception of serial data
boolean newData = false;

boolean isPulse = false;
boolean isTrain = false;
boolean isGaus = false;
boolean isTri = false;

// store timing to avoid using delays
unsigned long beforeRxMillis;
unsigned long afterRxMillis;

//Incoming Paramters

// Pulse params
boolean isPulseActive;
float pulseAmp;
int pulseDuration;
int pulseDelay;
float pulseEnd;

// Pulse Train params

boolean isTrainActive;
float trainAmp;
float trainDuration;
float trainDelay;
float trainWidth;
float trainEnd;
float ptStart; // sliding location of pulse starts
float trainT; // period of pulse = 1/freq
boolean hasSpiked;

// Triangle Pulse params

boolean isTriActive;
float triAmp;
float triDuration;
float triPeak;
float triDelay;
float triEnd;
float triUpSlope;
float triDownSlope;

// Gaussian params
boolean isGausActive;
float gausAmp;
float gausCenter;
float gausWidth;
float gausDelay;


// Output Voltage variables
volatile float value;

boolean hasPulsedThisLap = false; //Controls pulse, so only pulses max once per lap
boolean hasPulsed = false; // indicates whether pulse needs to be brought to resting output


// Distance
float dist;
volatile float distCorrection;


void setup() {
    pinMode(AOut, OUTPUT);
    pinMode(rstPin, INPUT);
    pinMode(distPin, INPUT);
    analogWriteResolution(12);
    analogReadResolution(12);
    attachInterrupt(digitalPinToInterrupt(rstPin), reset_distance, RISING);
    Serial.begin(9600); //Teensy ignores parameter and runs at 12MB/sec
    //Serial.println("<Teensy is ready>");
    analogWrite(AOut, 0);
}

void loop() {
    recvWithStartEndMarkers();
    outputVolts();
}



// Receives string from serial and stores values

void recvWithStartEndMarkers() {
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char startMarker = '<';
    char endMarker = '>';
    char phraseEnd = '!';
    char rc;
    
    
    while (Serial.available() > 0 && newData == false) {
        rc = Serial.read();
        //delay(100);
       
        if (rc == startMarker) {
          recvInProgress = true;
          beforeRxMillis = millis();
        }
        
        else if (recvInProgress == true) {
            
            // Read parameters for different output shapes
            if (rc != endMarker) {
                switch(rc) {
                  case 'p':
                    // Receiving parameters for Pulse 
                    clearBools();
                    ndx = 0;
                    isPulse = true;
                    break;
                  case 't':
                    // Receiving parameters for Pulse Train
                    clearBools();
                    ndx = 0;
                    isTrain = true;
                    break;    
                  case '^':
                    // Receiving parameters for triangle pulse
                    clearBools();
                    ndx = 0;
                    isTri = true;
                    break;  

                  default:
                   // Receiving parameters based on flag
                   if (isPulse){
                    if (rc == phraseEnd){
                    }
                    else if (rc!='p'){
                      pulse[ndx] =rc;
                      ndx++;
                    }
                   }
                   else if (isTrain){
                    if (rc == phraseEnd){
                    }
                    else if (rc!='t'){
                      train[ndx] = rc;
                      ndx++;
                    }
                   }
                   else if (isGaus){
                    if (rc == phraseEnd){
                    }
                    else if (rc!='g'){
                      gaus[ndx] = rc;
                      ndx++;
                    }
                   }
                   else if (isTri){
                    if (rc == phraseEnd){
                    }
                    else if (rc!='^'){
                      tri[ndx] = rc;
                      ndx++;
                    }
                   }
                   else{
                    noFlags();                                      
                   }
                   break;
                }
                if (ndx >= numChars) {
                    ndx = numChars - 1;
                }
            }
            else {
                recvInProgress = false;
                ndx = 0;
                newData = true;
            }
        }
    }
    
}

void buildNewData() {
    if (newData == true) {
        parsePulseData(pulse);
        parseTrainData(train);
        parseTriData(tri);
        parseGausData(gaus);
        newData = false;  
      }
       
}


void outputVolts(){
    //float currDist;
    
    float currDist;
    currDist = analogRead(distPin)/VAL2DAC*100;
    dist = currDist-distCorrection;
    
    //dist = analogRead(distPin)/VAL2DAC*100;

    // check each possible shape output and modify output volt accordingly

    // Pulse
    if (isPulseActive && (dist>pulseDelay) && (dist<pulseEnd) && !hasPulsed && !hasPulsedThisLap){

      value = value + pulseAmp;
      hasPulsed = true;
      hasPulsedThisLap = true;
      //Serial.print("pulsing: ");
      //Serial.println(dist);
    }
    else if (isPulseActive && (dist>pulseEnd) && hasPulsed){
      //Serial.print("pulse down: ");
      //Serial.println(dist);
      value = value - pulseAmp;
      hasPulsed = false;
    }


    // Pulse Train

    if (isTrainActive && (dist>trainDelay) && (dist<trainEnd)){
      if (!hasSpiked && ((dist-ptStart)<trainWidth) && ((dist-ptStart)>0)){
        value = value + trainAmp;

        /*
        Serial.print("Spiking up:");
        Serial.print(dist);
        Serial.print(" start:");
        Serial.print(ptStart);
        Serial.print(" width: ");
        Serial.print(trainWidth);
        Serial.print(" Output: ");
        Serial.println(value);
        */
        hasSpiked = true;
        
      }
      else if(hasSpiked && (dist-ptStart)<trainT && ((dist-ptStart)>trainWidth)){
        value = value - trainAmp;
        hasSpiked = false;
        ptStart = ptStart + trainT;

        /*
        Serial.print("Spiking Down:");
        Serial.print(dist);
        Serial.print(" start:");
        Serial.print(ptStart);
        Serial.print(" period: ");
        Serial.print(trainT);
        Serial.print(" Output: ");
        Serial.println(value);
        */
      }
    }


    
    // Triangle Pulse (right now needs to be only activated on its own

    if (isTriActive && (dist>triDelay) && (dist<triPeak)){
      //Serial.print("going up:");
      //Serial.println(value);
        value = triUpSlope*(dist-triDelay);
    }
    else if(isTriActive && (dist>triPeak) && (dist<triEnd)){
      //Serial.println("going Down");
        value = triDownSlope*(dist-triEnd);
    }
    else if (isTriActive && (dist<triDelay || dist>triEnd)){
      value = 0;
    }
    analogWrite(AOut, value*VAL2DAC);
    //Serial.println(dist);
}  


void clearBools() {
  isPulse = false;
  isTrain = false;
  isGaus = false;
  isTri = false;
}

void noFlags() {
  Serial.println("No shape flags read");
}


// Parsing functions to read serial data

void parsePulseData(char pulse_str[]) {

    // split the data into its parts
    
  char * strtokIndx; // this is used by strtok() as an index

  strtokIndx = strtok(pulse_str, ","); // this continues where the previous call left off
  isPulseActive =(*strtokIndx=='1');     // converts char* to a boolean


  strtokIndx = strtok(NULL, ",");
  pulseAmp = atof(strtokIndx);     // convert char* to a float

  strtokIndx = strtok(NULL, ",");
  pulseDuration = atof(strtokIndx);

  strtokIndx = strtok(NULL, ",");
  pulseDelay = atof(strtokIndx);     // convert this part to a float

  pulseEnd = pulseDelay + pulseDuration;


  /*
  Serial.print("Is Pulse?");
  Serial.println(isPulseActive);
  Serial.print("Pulse Amp");
  Serial.println(pulseAmp);
  Serial.print("Pulse Duration:");
  Serial.println(pulseDuration);
  Serial.print("Pulse Delay");
  Serial.println(pulseDelay);
  */
}


void parseTrainData(char train_str[]) {

    // split the data into its parts
    
  char * strtokIndx; // this is used by strtok() as an index

  strtokIndx = strtok(train_str, ","); // this continues where the previous call left off
  isTrainActive =(*strtokIndx=='1');     // converts char* to a boolean


  strtokIndx = strtok(NULL, ",");
  trainAmp = atof(strtokIndx);     // convert char* to a float

  strtokIndx = strtok(NULL, ",");
  trainDuration = atof(strtokIndx);

  strtokIndx = strtok(NULL, ",");
  trainDelay = atof(strtokIndx);     // convert this part to a float

  strtokIndx = strtok(NULL, ",");
  trainT = atof(strtokIndx);     // convert this part to a float

  strtokIndx = strtok(NULL, ",");
  trainWidth = atof(strtokIndx);     // convert this part to a float
  
  trainEnd = trainDelay + trainDuration;
  ptStart = trainDelay;
  hasSpiked = false;

  /*
  Serial.print("Is Train?");
  Serial.println(isTrainActive);
  Serial.print("Train Amp");
  Serial.println(trainAmp);
  Serial.print("Train Duration:");
  Serial.println(trainDuration);
  Serial.print("Train Delay");
  Serial.println(trainDelay);
  Serial.print("Train Frequency:");
  Serial.println(trainFreq);
  Serial.print("Train Width");
  Serial.println(trainWidth);
  */
}



void parseTriData(char tri_str[]) {

    // split the data into its parts
    
  char * strtokIndx; // this is used by strtok() as an index

  strtokIndx = strtok(tri_str, ","); // this continues where the previous call left off
  isTriActive =(*strtokIndx=='1');     // converts char* to a boolean


  strtokIndx = strtok(NULL, ",");
  triAmp = atof(strtokIndx);     // convert char* to a float

  strtokIndx = strtok(NULL, ",");
  triDuration = atof(strtokIndx);

  strtokIndx = strtok(NULL, ",");
  triPeak = atof(strtokIndx);     // convert this part to a float

  strtokIndx = strtok(NULL, ",");
  triDelay = atof(strtokIndx);     // convert this part to a float



  triEnd = triDuration + triDelay;
  triUpSlope = triAmp / (triPeak-triDelay);
  triDownSlope = triAmp /(triPeak-triEnd);

  /*
  Serial.print("Is Triangle?");
  Serial.println(isTriActive);
  Serial.print("Triangle Amp");
  Serial.println(triAmp);
  Serial.print("Triange Duration:");
  Serial.println(triDuration);
  Serial.print("Triangle Peak");
  Serial.println(triPeak); 
  Serial.print("Triangle Delay");
  Serial.println(triDelay); 
  Serial.print("Triangle UpSlope");
  Serial.println(triUpSlope); 
  Serial.print("Triangle DownSlope");
  Serial.println(triDownSlope);
  Serial.print("Triangle End");
  Serial.println(triEnd); 
  */
}

void parseGausData(char g_str[]) {

    // split the data into its parts
    
  char * strtokIndx; // this is used by strtok() as an index

  strtokIndx = strtok(g_str, ","); // this continues where the previous call left off
  isGausActive =(*strtokIndx=='1');     // converts char* to a boolean


  strtokIndx = strtok(NULL, ",");
  gausAmp = atof(strtokIndx);     // convert char* to a float

  strtokIndx = strtok(NULL, ",");
  gausCenter = atof(strtokIndx);

  strtokIndx = strtok(NULL, ",");
  gausWidth = atof(strtokIndx);     // convert this part to a float

  strtokIndx = strtok(NULL, ",");
  gausDelay = atof(strtokIndx);     // convert this part to a float
  
  /*
  Serial.print("Is Gaussian?");
  Serial.println(isGausActive);
  Serial.print("Guassian Amp");
  Serial.println(gausAmp);
  Serial.print("Gaussian Center:");
  Serial.println(gausCenter);
  Serial.print("Gaussian Width");
  Serial.println(gausWidth); 
  Serial.print("Gaussian Delay");
  Serial.println(gausDelay);
  */ 
}

// INTERUPT SERVICE ROUTINE

void reset_distance(){
  value = 0;
  float distRead = analogRead(distPin)/VAL2DAC*100;
  // Adjust for distance offset, ensure distance read is after reset
    if (distRead<20){
      distCorrection = distRead;
    } 
  analogWrite(AOut,0);
  buildNewData();
  ptStart = trainDelay;
  hasSpiked = false;
  hasPulsed = false;
  hasPulsedThisLap = false;
}




// to get variable type
void types(String a){Serial.println("it's a String");}
void types(int a)   {Serial.println("it's an int");}
void types(char* a) {Serial.println("it's a char*");}
void types(float a) {Serial.println("it's a float");} 
