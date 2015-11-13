//includes
#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>
#include <Servo.h>

// size of buffer used to capture HTTP requests
#define REQ_BUF_SIZE 100

//used to decrease the size of the program 
//   by commenting out the Serial 
#define debugMode false

byte mac[] = { 0x90, 0xA2, 0xDA, 0x0F, 0x93, 0x02 };
// IP address in the range of the IPv4 address of the computer
IPAddress ip(192, 168, 10, 108);
// create a server at port 80
EthernetServer server(80);
// stores the incoming HTTP request
String req;

// the web page file on the SD card
File webFile;
// incoming HTTP request stored as null terminated string
char HTTP_req[REQ_BUF_SIZE] = {0};
// index into HTTP_req buffer
char req_index = 0;

//Pin configurations for servo, speaker and switch
int servoPin = 6;
int spkrPin = 3;
int swPin = 5;

Servo myservo;
int pos;
//Positions of the servo when unlocked and locked
int posU = 104; int posL = 4; //TowerPro MG946R
//int posU = 4; int posL = 94; //Futaba S9303
boolean currLockState = 0;  //1 if the lock is currently in locked state
boolean destLockState = 0;  //desired state

boolean lastButton = LOW;
boolean currentButton = LOW;

//Frequencies of different tones
int c4 = 261, d4 = 293, e4 = 329, f4 = 349, g4 = 392;

void setup()
{
  // disable Ethernet chip
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  #if debugMode
  Serial.begin(9600);
  Serial.println("Debugging");
  Serial.println("Serial Connection established");
  Serial.println("Initializing SD card...");
  #endif
  // initialize SD card
  if (!SD.begin(4)) {
      #if debugMode
      Serial.println("ERROR - SD card initialization failed!");
      #endif
      return;    // init failed
  }
  #if debugMode
  Serial.println("SUCCESS - SD card initialized.");
  #endif
  // check for index.htm file
  if (!SD.exists("index.htm")) {
      #if debugMode||a
      Serial.println("ERROR - Can't find index.htm file!");
      #endif
      return;
  }
  #if debugMode
  Serial.println("SUCCESS - Found index.htm file.");
  #endif
  if (!SD.exists("lock.jpg")) {
      #if debugMode
      Serial.println("ERROR - Can't find lock.jpg file!");
      #endif
      return;
  }
  #if debugMode
  Serial.println("SUCCESS - Found lock.jpg file.");
  #endif
  if (!SD.exists("unlock.jpg")) {
      #if debugMode
      Serial.println("ERROR - Can't find unlock.jpg file!");
      #endif
      return;
  }
  #if debugMode
  Serial.println("SUCCESS - Found unlock.jpg file.");
  #endif
  
  Ethernet.begin(mac, ip);  // initialize Ethernet device
  server.begin();           // initialize server and begin listening for clients
  pinMode(swPin, INPUT);
  myservo.attach(servoPin);  // attaches the servo on pin 9 to the servo object
  myservo.write(posU);
  delay(1000);
}

void loop()
{
  EthernetClient client = server.available();
  // check if there is any client available
  if (client) {
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      // check if there's any client data available to read
      if (client.available()) {
        char c = client.read();  // read 1 byte (character)
        if (req_index < (REQ_BUF_SIZE - 1)) {
          HTTP_req[req_index] = c;          // save HTTP request character
          req_index++;
        }
        if (c == '\n' && currentLineIsBlank) {
          client.println("HTTP/1.1 200 OK");
          // remainder of header follows below, depending on if
          // web page or XML page is requested
          // Ajax request - send XML file
          if (StrContains(HTTP_req, "ajax_inputs")) {
              // send rest of HTTP header
              client.println("Content-Type: text/xml");
              client.println("Connection: keep-alive");
              client.println();
              ProcessLockRequest();
              XML_response(client);
          } else {  // web page request
            if (StrContains(HTTP_req, "GET / ")) {
              client.println("Content-Type: text/html");
              client.println("Connection: keep-alive");
              client.println();
              webFile = SD.open("index.htm");
              //Serial.println("Tried to open index.html");
            } else if (StrContains(HTTP_req, "GET /lock.jpg")) {
              webFile = SD.open("lock.jpg");
              if (webFile) {
                  client.println();
              }
            } else if (StrContains(HTTP_req, "GET /unlock.jpg")) {
              webFile = SD.open("unlock.jpg");
              if (webFile) {
                  client.println();
              }
            }
            if (webFile) {
                while(webFile.available()) {
                    //Serial.println("Available");
                    client.write(webFile.read()); // send web page to client
                }
                webFile.close();
            }
          }
          #if debugMode||a
          // display received HTTP request on serial port
          Serial.print(HTTP_req);
          #endif
          // reset buffer index and all buffer elements to 0
          req_index = 0;
          StrClear(HTTP_req, REQ_BUF_SIZE);
          break;
        }
        // every line of text received from the client ends with \r\n
        if (c == '\n') {
          // last character on line of received text
          // starting new line with next character read
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // a text character was received from client
          currentLineIsBlank = false;
        }
      }
    }
    delay(1);      // give the web browser time to receive the data
    client.stop(); // close the connection
  }
  processSwitch();
}

void ProcessLockRequest(void)
{
  if (StrContains(HTTP_req, "Lock=1") && !currLockState) {
    destLockState = true;
    rotateServo();
  } else if (StrContains(HTTP_req, "Lock=0") && currLockState) {
    destLockState = false;
    rotateServo();
  }
}

void rotateServo()
{
  if (!(destLockState^currLockState)) {
    return;
  }
  if (destLockState) {
    #if debugMode||a
    Serial.println("      Locking");
    #endif
    playTones(c4, d4, g4, 200);
    for (pos=posU; pos>=posL; pos-=5) {
      myservo.write(pos);
      delay(15);
    }
    #if debugMode
    Serial.println("      Locked");
    #endif
  } else {
    #if debugMode
    Serial.println("      Unlocking");
    #endif
    playTones(g4, d4, c4, 200);
    for (pos=posL; pos<=posU; pos+=5) {
      myservo.write(pos);
      delay(15);
    }
    #if debugMode
    Serial.println("      Unlocked");
    #endif
  }
  currLockState = destLockState;
  delay(1000);
}

void playTones(int t1, int t2, int t3, int tlen)
{
  tone(spkrPin, t1, tlen); delay(tlen/2);
  tone(spkrPin, t2, tlen); delay(tlen/2);
  tone(spkrPin, t3, tlen); delay(tlen/2);
}

void processSwitch()
{
  currentButton = debounce(lastButton);
  if (lastButton == LOW && currentButton == HIGH) {
    destLockState = !currLockState;
    rotateServo();
  }
  lastButton = currentButton;
}


boolean debounce (boolean last)
{
  boolean current = digitalRead(swPin);
  if (last != current) {
    delay(5);
    current = digitalRead(swPin);
  }
  return current;
}

void XML_response(EthernetClient cl)
{
    cl.print("<?xml version = \"1.0\" ?>");
    cl.print("<inputs>");
    cl.print("<Lock>");
    if (currLockState) {
        cl.print("locked");
    }
    else {
        cl.print("unlocked");
    }
    cl.println("</Lock>");
    cl.print("</inputs>");
}

// sets every element of str to 0 (clears array)
void StrClear(char *str, char length)
{
    for (int i = 0; i < length; i++) {
        str[i] = 0;
    }
}

// searches for the string sfind in the string str
// returns 1 if string found
// returns 0 if string not found
char StrContains(char *str, char *sfind)
{
    char found = 0;
    char index = 0;
    char len;
    
    len = strlen(str);    
    if (strlen(sfind) > len) {
        return 0;
    }
    while (index < len) {
        if (str[index] == sfind[found]) {
            found++;
            if (strlen(sfind) == found) {
                return 1;
            }
        } else {
            found = 0;
        }
        index++;
    }
    return 0;
}
