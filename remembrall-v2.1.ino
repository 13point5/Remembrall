#include <ESP8266WiFi.h>
#include <time.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "*****";
const char* password = "****";

// ThingSpeak constants
const char* thingspeak_host = "api.thingspeak.com";
String thingspeak_url = "/apps/thinghttp/send_request?api_key=QXF2AOP7XGCZS0GV";

// PushingBox scenario DeviceId code and API
String deviceId = "vF7445223F89E75D";
const char* logServer = "api.pushingbox.com";

// button
const int buttonPin = 12;
volatile bool button_state = false;

int bufferTime = 10;
int rled = 14;
int value=0;

String total_tasks="";

unsigned long previousMillis = 0;

const int interval = 30000;

// Use WiFiClient class to create TCP connections
WiFiClient Client;

void setup() {
  pinMode(rled, OUTPUT);
  pinMode(buttonPin, INPUT);

  attachInterrupt(buttonPin, buttonPressed, RISING);
  
  Serial.begin(115200); 
  Serial.println(); 
  initWifi();

  total_tasks = checkTasks();
}

void loop() {

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the time you should have toggled the LED
    previousMillis += interval;
    total_tasks = checkTasks();
  }
  
  if(total_tasks.length()>0 && button_state){
    //Client.stop();
    pushBullet(total_tasks);
  }
  
}

void buttonPressed() {
  Serial.println("Button Pressed");
  int button = digitalRead(buttonPin);
  if(button == HIGH)
  {
    button_state = true;
  }
  return;
}


String checkTasks(){
  Serial.print("Connecting to ");
  Serial.println(thingspeak_host);
  
  
  const int httpPort = 80;
  if (!Client.connect(thingspeak_host, httpPort)) {
    Serial.println("Connection failed");
    return "";
  }
  
  // We now create a URI for the request
  Serial.print("Requesting URL: ");
  Serial.println(thingspeak_host + thingspeak_url);
  Serial.println(String("TRY: ") + value + ".");
  
  // This will send the request to the server
  Client.print(String("GET ") + thingspeak_url + "&headers=false" + " HTTP/1.1\r\n" + "Host: " + thingspeak_host + "\r\n" + "Connection: close\r\n\r\n");
  delay(500);
  
  // Read all the lines of the reply from server and print them to Serial
  while(Client.available()){
    String line = Client.readStringUntil('\r');

    Serial.println(".");

    // JSON Parsing
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(line);

    if(root.size()>0){

    int num_items = root["items"].size();

    String due_tasks = "";
    
    if(num_items>0){
      
      int numNuls = 0;
      //Loop through all items
      for(int i=0; i<num_items; i++){
        
        String task_time = root["items"][i]["date_string"];
        
        if(task_time != "null"){
          String curr_task = root["items"][i]["content"];
          String curr_due_date = root["items"][i]["due_date_utc"];
          String currtime = getTime();
          checkForgot(curr_due_date, currtime);
          numNuls+=1;
          due_tasks += "\n" + curr_task;
          //pushBullet(curr_task);
          Serial.println("Task not completed yet!");
        }
        
      }

      if(numNuls==0){
        analogWrite(rled, 1023);
        Serial.println("Due date not set for task(s)");
        return "";
        
      } else if(numNuls!=0 && due_tasks.length()>0){
        Serial.println("Sent tasks!");
        return due_tasks;
      }
    }
    else{
      analogWrite(rled, 1023);
      Serial.println("No tasks due");
      return "";
    }
    
 }
  }
  value = value + 1;
  //delay(30000);
}

void pushBullet(String message){
  WiFiClient client;

  Serial.println("- connecting to pushing server: " + String(logServer));
  if (client.connect(logServer, 80)) {
    Serial.println("- succesfully connected");
    
    String postStr = "devid=";
    postStr += String(deviceId);
    postStr += "&message_param=";
    postStr += String(message);
    postStr += "\r\n\r\n";
    
    Serial.println("- sending data...");
    
    client.print("POST /pushingbox HTTP/1.1\n");
    client.print("Host: api.pushingbox.com\n");
    client.print("Connection: close\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
  }
  client.stop();
  Serial.println("- stopping the client");
  button_state = false;
}


void checkForgot(String duedate, String currtime){
    duedate.remove(24);
    duedate.remove(0,4);
    currtime.remove(0,4);

    if(duedate.substring(0,11) == currtime.substring(0,11)){
        if(blaSeconds(currtime)-blaSeconds(duedate) >= bufferTime*60){
           analogWrite(rled, 0);
        }
    }
    
}

int blaSeconds(String xtime){

    int hour = xtime.substring(12,14).toInt();
    int mins = xtime.substring(15,17).toInt();
    int sec = xtime.substring(18,20).toInt();

    return hour*3600 + mins*60 + sec;
}
  
String getTime() {
  WiFiClient client;
  while (!!!client.connect("google.com", 80)) {
    Serial.println("connection failed, retrying...");
    analogWrite(rled, 0);
    delay(500);
    analogWrite(rled, 1023);
  }

  client.print("HEAD / HTTP/1.1\r\n\r\n");
 
  while(!!!client.available()) {
     yield();
  }

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

void initWifi() {
   Serial.print("Connecting to ");
   Serial.print(ssid);

   WiFi.begin(ssid, password);
   
   while (WiFi.status() != WL_CONNECTED) {
      analogWrite(rled, 0);
      delay(500);
      analogWrite(rled, 1023);
      Serial.print(".");
   }
  Serial.print("\nWiFi connected, IP address: ");
  Serial.println(WiFi.localIP());
}
