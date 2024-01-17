#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>

// Defining commands to be executed when buttons on the webpage are clicked
#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4
#define UP_LEFT 5
#define UP_RIGHT 6
#define DOWN_LEFT 7
#define DOWN_RIGHT 8
#define PARK_BUTTON 9
#define SERVO_TEST_BUTTON 10
#define STOP 0



/*
* Ultrasonic Sensors:
- Echo1/Trigger1: The ultrasonic sensor mounted on the servo motor.
- Echo2/Trigger2: The ultrasonic sensor fixed on the back of the car.
*/

// Defining the pin numbers for ultrasonic sensor #1
const int ECHO1 = 13;
const int TRIGGER1 =14 ;

// Defining the pin numbers for ultrasonic sensor #2
const int ECHO2 = 32;
const int TRIGGER2 = 19;

// The pin used to feed angle measures to the servo motor
const int SERVOPIN = 15;


// variables used to calculate the distances using the ultrasonic sensor
unsigned long onetime;
int dist1;
int dist2;

// distance threshold for the wall the car is moving next to
const int distance_threshold = 21;


// Left Motor Pins
const int IN1 = 5; // Motor 1 (Left) control
const int IN2 = 25; // Motor 1 (Left) control
const int enA = 27; // Motor 1 speed control

// Right Motor Pins
const int IN3 = 21; // Motor 2 (Right) control
const int IN4 = 26; // Motor 2 (Right) control
const int enB = 23; // Motor 2 speed control

// defining an instance of the servo motor
Servo ultrasonicServo;




// defining the parameters of our wifi hotspot to be used for connection and localization
const char* ssid     = "nasnas";
const char* password = "12345678";

// Creating a server socket on port 80 for the webpage
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// html code for the webpage
const char* htmlHomePage PROGMEM = R"HTMLHOMEPAGE(
<!DOCTYPE html>
<html>
  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <style>
    .arrows {
      font-size:70px;
      color:red;
    }
    .circularArrows {
      font-size:80px;
      color:blue;
    }
    td {
      background-color:black;
      border-radius:25%;
      box-shadow: 5px 5px #888888;
    }
    td:active {
      transform: translate(5px,5px);
      box-shadow: none; 
    }

    .noselect {
      -webkit-touch-callout: none; /* iOS Safari */
        -webkit-user-select: none; /* Safari */
         -khtml-user-select: none; /* Konqueror HTML */
           -moz-user-select: none; /* Firefox */
            -ms-user-select: none; /* Internet Explorer/Edge */
                user-select: none; /* Non-prefixed version, currently
                                      supported by Chrome and Opera */
    }
    </style>
  </head>
  <body class="noselect" align="center" style="background-color:white">
     
    <h1 style="color: teal;text-align:center;">Hash Include Electronics</h1>
    <h2 style="color: teal;text-align:center;">Wi-Fi &#128663; Control</h2>
    
    <table id="mainTable" style="width:400px;margin:auto;table-layout:fixed" CELLSPACING=10>
      <tr>
        <td ontouchstart='onTouchStartAndEnd("5")' ontouchend='onTouchStartAndEnd("0")'><span class="arrows" >&#11017;</span></td>
        <td ontouchstart='onTouchStartAndEnd("1")' ontouchend='onTouchStartAndEnd("0")'><span class="arrows" >&#8679;</span></td>
        <td ontouchstart='onTouchStartAndEnd("6")' ontouchend='onTouchStartAndEnd("0")'><span class="arrows" >&#11016;</span></td>
      </tr>
      
      <tr>
        <td ontouchstart='onTouchStartAndEnd("3")' ontouchend='onTouchStartAndEnd("0")'><span class="arrows" >&#8678;</span></td>
        <td></td>    
        <td ontouchstart='onTouchStartAndEnd("4")' ontouchend='onTouchStartAndEnd("0")'><span class="arrows" >&#8680;</span></td>
      </tr>
      
      <tr>
        <td ontouchstart='onTouchStartAndEnd("7")' ontouchend='onTouchStartAndEnd("0")'><span class="arrows" >&#11019;</span></td>
        <td ontouchstart='onTouchStartAndEnd("2")' ontouchend='onTouchStartAndEnd("0")'><span class="arrows" >&#8681;</span></td>
        <td ontouchstart='onTouchStartAndEnd("8")' ontouchend='onTouchStartAndEnd("0")'><span class="arrows" >&#11018;</span></td>
      </tr>
    
      <tr>
        <td ontouchstart='onTouchStartAndEnd("9")' ontouchend='onTouchStartAndEnd("0")'><span class="circularArrows" >&#8634;</span></td>
        <td style="background-color:white;box-shadow:none"></td>
        <td ontouchstart='onTouchStartAndEnd("10")' ontouchend='onTouchStartAndEnd("0")'><span class="circularArrows" >&#8635;</span></td>
      </tr>
    </table>

    <script>
      var webSocketUrl = "ws:\/\/" + window.location.hostname + "/ws";
      var websocket;
      
      function initWebSocket() 
      {
        websocket = new WebSocket(webSocketUrl);
        websocket.onopen    = function(event){};
        websocket.onclose   = function(event){setTimeout(initWebSocket, 2000);};
        websocket.onmessage = function(event){};
      }

      function onTouchStartAndEnd(value) 
      {
        websocket.send(value);
      }
          
      window.onload = initWebSocket;
      document.getElementById("mainTable").addEventListener("touchend", function(event){
        event.preventDefault()
      });      
    </script>
    
  </body>
</html> 

)HTMLHOMEPAGE";

// the parking function is called using the designated button on the webpage
void park() {
  /*
  First Algorithm
  1- The mounted ultrasonic sensor rotates to face the wall
  2- The car moves next to the wall until both sensors are within the parking slot
     * This hapens when they both record a distance larger than the distance between the car and the wall
  3- The car turns left about its axis to become perpendicular to the parking spot
  4- The mounted ultrasonic rotates to face the back of the car to measure distance behind it as it moves backwards
  5- The car moves backward into the spot
  6- The car turns right about its axis to become parallel to the spot
  */

  // Step 1
  ultrasonicServo.write(180);
  delay(100);

  // Step 2
  dist1 = ultr1_getDistance();
  delay(100);
  dist2 = ultr2_getDistance();
  delay(100);
  moveForward(70,70);
  while(dist1 < distance_threshold || dist2 < distance_threshold)
  {
  dist1 = ultr1_getDistance();
  delay(10);

  dist2 = ultr2_getDistance();
  delay(10);

  }
  stopMotors();
  delay(1000);

  // Step 3
  turnLeft(80,80);
  delay(500);
  stopMotors();
  delay(10);

  // Step 4
  ultrasonicServo.write(90);
  delay(1000);
  
  // Step 5
  moveBackward(70 , 70 );
  delay(300);
  stopMotors();

  // Step 6
  turnRight(80,80);
  delay(500);
  stopMotors();
}

// The stopping function writes the same value (LOW) to both IN pins of each motor to stop them
void stopMotors()
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// A function that gets the distance from the first ultrasonic sensor (mounted on the servo motor)
int ultr1_getDistance(void){
  // Making sure the trigger is set to LOW before starting operation
  digitalWrite(TRIGGER1,LOW);
  delayMicroseconds(2);


  // Setting the trigger to high for 10 microseconds then setting it to low to trigger the sensor
  digitalWrite(TRIGGER1,HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER1,LOW);

  // Calculating the time to get the distanc
  onetime = pulseIn(ECHO1,HIGH);

  // Since light travels at 343 m/sec in air, we calculate speed by multiplying speed * time
  // dividing the time by 2 to get the time of half a lap (since the full lap is where the signal goes then comes back)
  dist1 = onetime * 0.0343/2;

  return dist1;
}

// A function that gets the distance from the second ultrasonic sensor (fixed)
int ultr2_getDistance(void){
  // Making sure the trigger is set to LOW before starting operation
  digitalWrite(TRIGGER2,LOW);
  delayMicroseconds(2);

  // Setting the trigger to high for 10 microseconds then setting it to low to trigger the sensor
  digitalWrite(TRIGGER2,HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER2,LOW);

  // Calculating the time to get the distanc
  onetime = pulseIn(ECHO2,HIGH);

  // Since light travels at 343 m/sec in air, we calculate speed by multiplying speed * time
  // dividing the time by 2 to get the time of half a lap (since the full lap is where the signal goes then comes back)
  dist2 = onetime * 0.0343/2;

  return dist2;
}

// Reversing the direction of a motor by reversing the values written to both IN pins
// Note: We wanted the single wheel to be at the back of the car for stability, so HIGH, LOW moves our wheels backwards
void moveBackward(int speed1, int speed2) {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(enA, speed1);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(enB, speed2);
}

// Moving both motors in the same direction and speed to make the car move forward
void moveForward(int speed1, int speed2) {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  analogWrite(enA, speed1);;

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(enB, speed2);
}

// Making the car turn left around its axis by moving the motors in opposite directions
void turnLeft(int speed1, int speed2) {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(enA, speed1);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(enB, speed2);
}

// Reversing the directions of both wheels to make the car spin in the opposite direction
void turnRight(int speed1, int speed2) {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  analogWrite(enA, speed1);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(enB, speed2);
}

// Extra function used to test the servo motor (as there was a spare button in the webpage)
void TestServo()
{
  for(int i=0;i<25;++i)
  {
    ultrasonicServo.write(0);
    delay(1000);
    ultrasonicServo.write(90);
    delay(1000);
    ultrasonicServo.write(180);
    delay(1000);
  }
}

// A function that handles the pressing on different buttons on the webpage by mapping them to functions
void processCarMovement(String inputValue)
{
  Serial.printf("Got value as %s %d\n", inputValue.c_str(), inputValue.toInt());  
  switch(inputValue.toInt())
  {
    case UP:
      moveForward(80,80);                
      break;
  
    case DOWN:
      moveBackward(80,80);
      break;
  
    case LEFT:
      turnLeft(80,80);
      break;
  
    case RIGHT:
      turnRight(80,80);
      break;
      
    case UP_LEFT:
      moveForward(60, 90);
      break;
  
    case UP_RIGHT:
      moveForward(90, 60);
      break;
  
    case DOWN_LEFT:
      moveBackward(60, 90);
      break;
  
    case DOWN_RIGHT:
      moveBackward(90, 60);
      break;
  
    case PARK_BUTTON:
      park();
      break;
  
    case SERVO_TEST_BUTTON:
      TestServo();
      break;
  
    case STOP:
      digitalWrite(IN1,LOW);
      digitalWrite(IN2,LOW);
      digitalWrite(IN3,LOW);
      digitalWrite(IN4,LOW);
      break;
  
    default:
      stopMotors();
      break;
  }
}

// Event handler function called when the home webpage is requested
void handleRoot(AsyncWebServerRequest *request) 
{
  request->send_P(200, "text/html", htmlHomePage);
}

// Event handler function called when the home webpage is not found
void handleNotFound(AsyncWebServerRequest *request) 
{
    request->send(404, "text/plain", "File Not Found");
}

// A function that maps te socket event to functions
void onWebSocketEvent(AsyncWebSocket *server, 
                      AsyncWebSocketClient *client, 
                      AwsEventType type,
                      void *arg, 
                      uint8_t *data, 
                      size_t len) 
{                      
  switch (type) 
  {
    // When a client is connected
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      //client->text(getRelayPinsStatusJson(ALL_RELAY_PINS_INDEX));
      break;
    // when a client disconnects, the car stops
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      processCarMovement("0");
      break;
    // the event that the client presses a button (sends data to the server): the process car movement function is called
    case WS_EVT_DATA:
      AwsFrameInfo *info;
      info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) 
      {
        std::string myData = "";
        myData.assign((char *)data, len);
        processCarMovement(myData.c_str());       
      }
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
    default:
      break;  
  }
}

// our ssid: "nasnas"
const char* ssidArray[] = {"besho", "carr_id", "loc_id"};
int arraySize = sizeof(ssidArray) / sizeof(ssidArray[0]);


double localization()
{
  double distance=0;
  // Scanning for nearby ssids and connection strengths
  int numNetworks = WiFi.scanNetworks();
  Serial.println("--------------------------------------------------");
  for (int i = 0; i < numNetworks; i++)
  {
    for (int j = 0; j < arraySize; j++) {
      // Getting only the strength of the desired networks.
      if(String(WiFi.SSID(i)) == String(ssidArray[j]))
      {
        // Getting the distance from the signal strength
        double s = (-59 - WiFi.RSSI(i)) / (10.0 * 3);
        distance = pow(10.0, s);


        Serial.println(String(ssidArray[j]) + " with RSSI = " + String(WiFi.RSSI(i)));
        Serial.println("has distance of = " + String(distance) + "\n");
      }
    }
  }
  return distance ;
}

void setup(void) 
{
  // setting the baudrate
  Serial.begin(9600);
  delay(1000);

  // initiating the hotspot
  WiFi.softAP(ssid, password);
  
  // Getting the ip of the hotspot to set up the webpage
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // setting up the server socket for the webpage
  server.on("/", HTTP_GET, handleRoot);
  server.onNotFound(handleNotFound);
  
  // adding the event handler to it
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  
  // starting the server
  server.begin();
  
  
  // initializing the motor pins as output pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(enA, OUTPUT);

  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(enB, OUTPUT);

  //ultrasonics pin setup
  pinMode(ECHO1,INPUT);
  pinMode(TRIGGER1,OUTPUT);
  pinMode(ECHO2,INPUT);
  pinMode(TRIGGER2,OUTPUT);

  // servo setup
  ultrasonicServo.attach(SERVOPIN);
  
}

void loop() 
{ 
  ws.cleanupClients(); 
}
