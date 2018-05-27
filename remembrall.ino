#include <ESP8266WiFi.h>
#include <time.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "************";                    // TODO: Enter your WiFi ssid
const char* password = "***";                         // TODO: Enter your WiFi password

// ThingHTTP
const char* thinghttp_host = "api.thinghttp.com";
String thinghttp_API_KEY = "***************";         // TODO: Enter your ThingHTTP app API key
String thinghttp_url = "/apps/thinghttp/send_request?api_key=" + thinghttp_API_KEY;

// PushingBox
String deviceId = "***************";                  // TODO: Enter your PushingBox DeviceID
const char* logServer = "api.pushingbox.com";

// Button
const int buttonPin = 12; // Pin D6 on NodeMCU
volatile bool button_state = false;

// To avoid Soft Reset
unsigned long previousMillis = 0; // To avoid Soft Reset
unsigned long currentMillis;
const int interval = 30 * 1000; // The Remembrall checks for due tasks every 30s

// Miscellaneous Declarations
int bufferTime = 10; // If any task is due after bufferTime(minutes) then the Remembrall will be triggered
int rled = 13; // LED pin: D7 on NodeMCU used for You_Know_What
const int httpPort = 80; // Port 80 is used for HTTP requests
String total_tasks=""; // Stores the tasks that is sent via PushBullet


void setup() {
  Serial.begin(115200);
  pinMode(rled, OUTPUT);
  pinMode(buttonPin, INPUT);

  attachInterrupt(buttonPin, buttonPressed, RISING); // Interrupts are used so that you can send notification at any moment
  initWifi(); // Connect to WiFi
  total_tasks = checkTasks(); // Check tasks for the first time
}

void loop() {

  currentMillis = millis(); // Get milliseconds after powering on

  if (currentMillis - previousMillis >= interval) { // If interval(ms) have passed then check for tasks
    previousMillis = millis(); // Assign current millis
    total_tasks = checkTasks(); // Check for due tasks
  }

  if(total_tasks.length()>0 && button_state){ // If there are due tasks and the button was pushed..
    pushBullet(total_tasks); // Send notification
  }

}


// ISR: Interrupt Service Routine
void buttonPressed() {
  Serial.println("Button Pressed");
  int button = digitalRead(buttonPin); // Read the current state of the button
  if(button == HIGH){
    button_state = true; // If pressed set the state as true
  }
  return; // ISR must return nothing!!!
}

// Check for Due Tasks and return if any
String checkTasks(){

  WiFiClient Client; // Use WiFiClient class to create TCP connections

  Serial.print("Connecting to ");
  Serial.println(thinghttp_host);

  if (!Client.connect(thinghttp_host, httpPort)) {  // Let you know if connection was unsuccesfull
    Serial.println("Connection failed");
    return "";
  }

  // Create a URI for the request
  int value=0; // Used for debugging API Calls
  Serial.print("Requesting URL: ");
  Serial.println(thinghttp_host + thinghttp_url);
  Serial.println(String("Trial #") + value);

  // Send the request to the server
  Client.print(String("GET ") + thinghttp_url + "&headers=false" + " HTTP/1.1\r\n" + "Host: " + thinghttp_host + "\r\n" + "Connection: close\r\n\r\n");
  delay(500);

  // Read the entire response from server and show it on the Serial Monitor
  while(Client.available()){
    String line = Client.readStringUntil('\r');
    Serial.print(".");

    // JSON Parsing
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(line);

    if(root.size()>0){ // If there was a response
      int num_items = root["items"].size(); // Get total number of tasks
      String due_tasks = ""; // Every time you get a response is received assume that there are no due tasks

      if(num_items>0){  // If there are tasks

        int numNuls = 0; // Check for tasks with no due date

        for(int i=0; i<num_items; i++){  //Loop through all items
          if(task_time != "null"){ // If due date was set
            String curr_task = root["items"][i]["content"]; // Task Content
            String curr_due_date = root["items"][i]["due_date_utc"]; // Due time in UTC
            String currtime = getTime(); // Current UTC time
            checkForgot(curr_due_date, currtime); // Check if bufferTime minutes have passed after you forgot
            numNuls+=1; // Increment since due date was set
            due_tasks += "\n" + curr_task; // Construct the Message of the notification to be sent
            Serial.println("Task not completed yet!");
          }
        }

        Client.stop(); // Stop the Client to avoid Soft Reset

        // Check if there were tasks with due dates and return
        if(numNuls==0){
          digitalWrite(rled, LOW);; // Turn off the LED because you haven't technically forgotten it
          Serial.println("Due date not set for task(s)");
          return "";
        } else if(numNuls!=0 && due_tasks.length()>0){
          Serial.println("Sent tasks!");
          return due_tasks;
        }

      } else{
          digitalWrite(rled, LOW);; // Turn off the LED because there are no due tasks
          Serial.println("No tasks due");
          return "";
        }
    }
  }
  value = value + 1; // Used to know how many trials have passed to get a response
}

// Send Notification via PushingBox API
void pushBullet(String message){

  WiFiClient pushClient;
  Serial.println("- connecting to pushing server: " + String(logServer));

  if (pushClient.connect(logServer, httpPort)) {
    Serial.println("- succesfully connected");

    // Constructing the body of POST request
    String postStr = "devid=";
    postStr += String(deviceId);
    postStr += "&message_param=";
    postStr += String(message);
    postStr += "\r\n\r\n";

    Serial.println("- sending data...");

    // Send request
    pushClient.print("POST /pushingbox HTTP/1.1\n");
    pushClient.print("Host: api.pushingbox.com\n");
    pushClient.print("Connection: close\n");
    pushClient.print("Content-Type: application/x-www-form-urlencoded\n");
    pushClient.print("Content-Length: ");
    pushClient.print(postStr.length());
    pushClient.print("\n\n");
    pushClient.print(postStr);
  }

  pushClient.stop(); // Stop the client because there's a lot of load on the module
  Serial.println("- stopping the client");
  button_state = false;
}

/***********Helper Functions**********/

// Check if you forgot the task
void checkForgot(String duedate, String currtime){

  // String manipulation of the DateTime strings
  duedate.remove(24);
  duedate.remove(0,4);
  currtime.remove(0,4);

  if( (duedate.substring(0,11) == currtime.substring(0,11)) && (blaSeconds(currtime)-blaSeconds(duedate) >= bufferTime*60) ){   // If the days are the same and bufferTime minutes have passed
    digitalWrite(rled, HIGH);; // Trigger the LED
  }
}

// Convert the time into seconds instead of using libraries
int blaSeconds(String xtime){
  // Pretty self-explanatory
  int hour = xtime.substring(12,14).toInt();
  int mins = xtime.substring(15,17).toInt();
  int sec = xtime.substring(18,20).toInt();

  return hour*3600 + mins*60 + sec;
}

// Get current time from HTTP1.1 header
String getTime() {
  WiFiClient client;

  while (!!!client.connect("google.com", httpPort)) { // Wait till connection is established and blink the LED
    Serial.println("connection failed, retrying...");
    digitalWrite(rled, HIGH);;
    delay(100);
    digitalWrite(rled, LOW);;
  }

  client.print("HEAD / HTTP/1.1\r\n\r\n"); // Send request

  while(!!!client.available()) {
    yield();
  }

  // Read the UTC DateTime
  while(client.available()){
    if (client.read() == '\n') {
      if (client.read() == 'D') {
        if (client.read() == 'a') {
          if (client.read() == 't') {
            if (client.read() == 'e') {
              if (client.read() == ':') {
                client.read();
                String theDate = client.readStringUntil('\r');
                client.stop();
                theDate.replace("," , "");
                theDate.remove(24);
                return theDate;
              }
            }
          }
        }
      }
    }
  }
}

// Connect to WiFi
void initWifi() {
  Serial.print("Connecting to ");
  Serial.print(ssid);

  WiFi.begin(ssid, password); // Begin WiFI connection

  while (WiFi.status() != WL_CONNECTED) { // Blink the LED while connecting to WiFi
    digitalWrite(rled, HIGH);;
    delay(100);
    digitalWrite(rled, LOW);;
    Serial.print(".");
  }

  Serial.print("\nWiFi connected, IP address: ");
  Serial.println(WiFi.localIP()); // Print the IP of NodeMCU, just for fun
}
