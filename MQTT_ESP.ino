#include <ArduinoJson.h>
#define _TASK_TIMEOUT
#include <TaskScheduler.h>
#include <WiFi.h>
#include "FS.h"
#include "SPIFFS.h"
#include <EEPROM.h>
#include <WebServer.h>
#include <PubSubClient.h>

//**************DEEP SLEEP CONFIG******************//
#define uS_TO_S_FACTOR 1000000  
#define TIME_TO_SLEEP  5


#define MQTT_CLIENT_NAME "ClientVBShightime123" 
#define VARIABLE_LABEL_TEMPC "tempC" // Assing the variable label
#define VARIABLE_LABEL_BAT "bat"
#define VARIABLE_LABEL_HUMID "humid" // Assing the variable label
#define DEVICE_LABEL "espThings" // Assig the device label


//************** MQTT TOPIC and PAYLOAD******************//
char mqttBroker[] = "things.ubidots.com";
char payload[100];
char topic1[100];
char topic2[100];
char topic3[100];

//************** Auxillary functions******************//
Scheduler ts;
WebServer server(80);
DynamicJsonBuffer jsonBuffer;
WiFiClient ubidots;
PubSubClient client(ubidots);

//**********Task Timer**************//
unsigned long taskSensorTimer = 0;
unsigned long taskWiFiTimer = 0;

//**********softAPconfig Timer*************//
unsigned long APTimer = 0;
unsigned long APInterval = 120000;

//**********staticAPconfig Timer*************//
unsigned long STimer = 0;
unsigned long SInterval = 120000;

//**********dhcpAPconfig Timer*************//
unsigned long DTimer = 0;
unsigned long DInterval = 120000;

//**********Config Timer*************//
unsigned long ConfigTimer = 0;
unsigned long ConfigInterval = 20000;

//*********SSID and Pass for AP**************//
 static char ssidWiFi[30];//Stores the router name
 static char passWiFi[30];//Stores the password
 char ssidAP[30];//Stores the router name
 char passAP[30];//Stores the password
 const char *ssidAPConfig = "adminesp32";
 const char *passAPConfig = "adminesp32";

//**********check for connection*************//
bool isConnected = true;
bool isAPConnected = false;


//*********ZigBee Frame**************/
uint8_t data[29];
int k = 10;
int i;

//*********Store Sensor Values**************/
static float humidity; 
static int16_t cTempint; 
static float cTemp ;
static float fTemp ;
static float battery ;
static float voltage ;
static int8_t nodeId;

char str_fTemp[10];
char str_cTemp[10];
char str_humid[10];
char str_bat[10];


//*********Static IP Config**************//
IPAddress ap_local_IP(192,168,1,77);
IPAddress ap_gateway(192,168,1,254);
IPAddress ap_subnet(255,255,255,0);
IPAddress ap_dhcp(192,168,4,1);


//*********Static IP WebConfig**************//
IPAddress ap_localWeb_IP;
IPAddress ap_Webgateway;
IPAddress ap_Websubnet;
IPAddress ap_dhcpWeb_IP;


//*********hold IP octet**************//
uint8_t ip0;
uint8_t ip1;
uint8_t ip2;
uint8_t ip3;


//*********IP Char Array**************//
char ipv4Arr[20];
char gatewayArr[20];           
char subnetArr[20];
char ipv4dhcpArr[20];


//*********Contains SPIFFS Info*************//
String debugLogData;

//*********UbiDots Credentials *************//
String tokenId;

//*********prototype for task callback***********//
void taskSensorCallback();
void taskSensorDisable();
void taskWiFiCallback();
void taskWiFiDisable();


//***********Tasks***********//
Task tSensor(4 * TASK_SECOND, TASK_FOREVER, &taskSensorCallback, &ts, false, NULL, &taskSensorDisable);
Task tWiFi(10* TASK_SECOND, TASK_FOREVER, &taskWiFiCallback, &ts, false, NULL, &taskWiFiDisable);


void callback(char* topic, byte* payload, unsigned int length) {
  char p[length + 1];
  memcpy(p, payload, length);
  p[length] = NULL;
  String message(p);
  Serial.write(payload, length);
  Serial.println(topic);
}

void reconnectMQTT() {
    // Loop until we're reconnected
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    // Attemp to connect
    if (client.connect(MQTT_CLIENT_NAME,tokenId.c_str(), "")) {
      Serial.println("Connected");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      // Wait 2 seconds before retrying
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  while(!Serial);
  WiFi.persistent(false);
  //WiFi.disconnect(true);
  WiFi.disconnect(true);
  SPIFFS.begin();
  delay(100);
  WiFi.mode(WIFI_AP);   
  Serial.println(WiFi.softAP(ssidAPConfig,passAPConfig) ? "soft-AP setup": "Failed to connect");
  delay(100);
  Serial.println(WiFi.softAPConfig(ap_local_IP, ap_gateway, ap_subnet)? "Configuring Soft AP" : "Error in Configuration");
  delay(100);
  delay(8000);
  
  if(WiFi.softAPgetStationNum()>0){
        handleClientAP();
       }

  EEPROM.begin(512);
  delay(100);
  File file = SPIFFS.open("/ip_set.txt", "r");     

  Serial.println("- read from file:");
     if(!file){
        Serial.println("- failed to open file for reading");
        return;
    }
    while(file.available()){
        debugLogData += char(file.read());
    } 
    file.close();
    if(debugLogData.length()>5){
       JsonObject& readRoot =jsonBuffer.parseObject(debugLogData);
          Serial.println("=====================================");
          Serial.println(debugLogData);
          Serial.println("=====================================");
          if(readRoot.containsKey("statickey")){
             String ipStaticValue= readRoot["staticIP"];
             String gatewayValue = readRoot["gateway"];
             String subnetValue =  readRoot["subnet"];
             String ssidStatic = readRoot["ssidStatic"];
             String passStatic = readRoot["passkeyStatic"];
             Serial.println("handle Started at"+'\t' + ipStaticValue);
             staticAPConfig(ipStaticValue,gatewayValue,subnetValue,ssidStatic,passStatic);}
           else if(readRoot.containsKey("dhcpDefault")){
                   String ipdhcpValue= readRoot["dhcpIP"];
                   String ssidDhcp = readRoot["ssidDhcp"];  
                   String passDhcp = readRoot["passkeyDhcp"];
                   Serial.println("handle Started at"+'\t' + ipdhcpValue);
                   dhcpAPConfig(ssidDhcp,passDhcp);}
           else if(readRoot.containsKey("dhcpManual")){
                   String ipdhcpValue= readRoot["staticIP"];
                   String ssidDhcp = readRoot["ssidDhcp"];  
                   String passDhcp = readRoot["passkeyDhcp"];
                   Serial.println("handle Started at"+'\t' + ipdhcpValue);
                   dhcpAPConfig(ssidDhcp,passDhcp);}
           else{
               handleClientAP();
               }
     }else{ handleClientAP();
   } 
  tokenId = read_string(100,100);
  Serial.println(tokenId);
  reconnectWiFi();
  tSensor.setTimeout(5 * TASK_SECOND);
  tWiFi.setTimeout(5 * TASK_SECOND);
  tSensor.enable();
}

void loop() {
  ts.execute();
}
  
//----------I2CCallback-----------//
void taskSensorCallback(){
  Serial.println("taskI2CStarted");
  Serial.print("timeout for this task: \t");
  Serial.println(tSensor.getTimeout());
  delay(10);
    if (Serial1.available())
  {
    data[0] = Serial1.read();
    delay(k);
    //chck for start byte
    if(data[0]==0x7E)
    {
    while (!Serial1.available());
    for ( i = 1; i< 29; i++)
      {
      data[i] = Serial1.read();
      delay(1);
      }
    if(data[15]==0x7F)  /////// to check if the recive data is correct
      {
  if(data[22]==1)  //////// make sure the sensor type is correct
         {  
            humidity = ((((data[24]) * 256) + data[25]) /100.0);
            cTempint = (((uint16_t)(data[26])<<8)| data[27]);
            cTemp = (float)cTempint /100.0;
            fTemp = cTemp * 1.8 + 32;
            fTemp /= 10.0;
            nodeId = data[16];
        Serial.print("Sensor Number  ");
        Serial.println(nodeId);
        Serial.print("Sensor Type  ");
        Serial.println(data[22]);
        Serial.print("Firmware Version  ");
        Serial.println(data[17]);
        Serial.println();
        if (voltage < 1)
          {
            Serial.println("Time to Replace The Battery");
          }
        }
      }
    }
  }
 }



  //----------I2CDisable-----------//
void taskSensorDisable(){
  unsigned long taskTime = millis() - taskSensorTimer;
  Serial.println(taskTime/1000);
  taskSensorTimer = millis();
  if(tSensor.timedOut()){
        Serial.println("taskSensor disabled");
        Serial.println("call taskWiFi");
        reconnectMQTT();
        tSensor.setCallback(&taskWiFiCallback);
        tWiFi.enable();
        tSensor.disable();
    }
  }
  
//----------WiFiCallback-----------//
void taskWiFiCallback(){
  Serial.println("taskWiFiCallbackStarted");
  Serial.print("timeout for this task: \t");
  Serial.println(tWiFi.getTimeout());
      if(!client.connected()){
          Serial.println("Client not connected");
          reconnectMQTT();
        } 
 
  dtostrf(fTemp, 4, 2, str_fTemp);
  dtostrf(cTemp, 4, 2, str_cTemp);
  dtostrf(humidity, 4, 2, str_humid);  
  dtostrf(voltage, 3, 2, str_bat);
  
  sprintf(topic1, "%s","");
  sprintf(topic1, "%s%s", "/v1.6/devices/", DEVICE_LABEL);
  sprintf(payload, "%s", ""); // Cleans the payload
  sprintf(payload, "{\"%s\":", VARIABLE_LABEL_TEMPC); // Adds the value
  sprintf(payload, "%s{\"value\":%s}", payload, str_cTemp); // Adds the value
  sprintf(payload, "%s}", payload); // Closes the dictionary brackets
  Serial.println(payload);
  Serial.println(client.publish(topic1,payload) ? "published" : "notpublished");
  
  sprintf(topic2, "%s","");
  sprintf(topic2, "%s%s", "/v1.6/devices/", DEVICE_LABEL);
  sprintf(payload, "%s", ""); // Cleans the payload
  sprintf(payload, "{\"%s\":", VARIABLE_LABEL_HUMID); // Adds the value
  sprintf(payload, "%s{\"value\":%s}", payload, str_humid); // Adds the value
  sprintf(payload, "%s}", payload); // Closes the dictionary brackets
  Serial.println(payload);
  Serial.println(client.publish(topic2,payload) ? "published" : "notpublished");
  
  sprintf(topic3, "%s","");
  sprintf(topic3, "%s%s", "/v1.6/devices/", DEVICE_LABEL);
  sprintf(payload, "%s", ""); // Cleans the payload
  sprintf(payload, "{\"%s\":", VARIABLE_LABEL_BAT); // Adds the value
  sprintf(payload, "%s{\"value\":%s}", payload, str_bat); // Adds the value
  sprintf(payload, "%s}", payload); // Closes the dictionary brackets
  Serial.println(payload);
  Serial.println(client.publish(topic3,payload) ? "published" : "not published");
  
  /* 4 is mininum width, 2 is precision; float value is copied onto str_sensor*/
  Serial.println("Publishing data to Ubidots Cloud");
  
  //int dataLength = payload.length()+1;
  //client.beginPublish(topic,dataLength,false); 
  //Serial.println(mqttCli.write(dataBuffer,dataLength) ? "published" : "published failed");
  //client.endPublish();
  client.loop();   
  } 
   
//----------WiFiDisable-----------//
void taskWiFiDisable(){
   unsigned long taskTime = millis() - taskWiFiTimer;
  Serial.println(taskTime/1000);
  taskWiFiTimer = millis();
  if(tWiFi.timedOut()){
    Serial.println("//taskWiFi disabled");
        Serial.println("call taskI2C");
        //enable I2C task again and call taskI2CCallback
        tWiFi.setCallback(&taskSensorCallback);
        tSensor.enable();
        //disables WiFi task
        tWiFi.disable();
    }
  } 

//****************************HANDLE ROOT***************************//
void handleRoot() {
   //Redisplay the form
   if(server.args()>0){
       for(int i=0; i<=server.args();i++){
          Serial.println(String(server.argName(i))+'\t' + String(server.arg(i)));
        }
     if(server.hasArg("ipv4static") && server.hasArg("gateway") &&  server.hasArg("subnet")){
      staticSet();
      }else if(server.arg("ipv4")!= ""){
          dhcpSetManual();
        }else{
           dhcpSetDefault();
          }    
    }else{
      File file = SPIFFS.open("/SelUbi_Settings.html", "r");
         server.streamFile(file,"text/html");
         file.close();
      }
}  

//****************************HANDLE DHCP***************************//
void handleDHCP(){
  File  file = SPIFFS.open("/ubi_dhcp.html", "r");
  server.streamFile(file,"text/html");
  file.close();}

//****************************HANDLE STATIC***************************//
void handleStatic(){
  File  file = SPIFFS.open("/ubi_static.html", "r");
  server.streamFile(file,"text/html");
  file.close();}

//*************Helper Meathod for Writing IP CONFIG**************//

//*************Helper 1 STATIC**************//

void staticSet(){
           JsonObject& root =jsonBuffer.createObject();
           String response="<p>The static ip is ";
           response += server.arg("ipv4static");
           response +="<br>";
           response +="The gateway ip is ";
           response +=server.arg("gateway");
           response +="<br>";
           response +="The subnet Mask is ";
           response +=server.arg("subnet");
           response +="</P><BR>";
           response +="<H2><a href=\"/\">go home</a></H2><br>";
           response += "<script> alert(\"Settings Saved\"); </script>";
           server.send(200, "text/html", response);
           String ipv4static = String(server.arg("ipv4static"));
           String gateway = String(server.arg("gateway"));
           String subnet = String(server.arg("subnet"));
           String ssid = String(server.arg("ssidStatic"));
           String passkey = String(server.arg("passkeyStatic"));
           root["statickey"]="staticSet";
           root["staticIP"] = ipv4static;
           root["gateway"] = gateway;
           root["subnet"] = subnet;
           root["ssidStatic"] = ssid;
           root["passkeyStatic"] = passkey;
           root.printTo(Serial);
           File fileToWrite = SPIFFS.open("/ip_set.txt", FILE_WRITE);
           if(!fileToWrite){
              Serial.println("Error opening SPIFFS");
              return;
            }
           if(root.printTo(fileToWrite)){
                Serial.println("--File Written");
            }else{
                Serial.println("--Error Writing File");
              } 
            fileToWrite.close();   
             isConnected = false;
    }

//*************Helper 2 DHCP MANUAL**************//

void dhcpSetManual(){
           JsonObject& root =jsonBuffer.createObject();
           String response="<p>The dhcp IPv4 address is ";
           response += server.arg("ipv4");
           response +="</P><BR>";
           response +="<H2><a href=\"/\">go home</a></H2><br>";
           response += "<script> alert(\"Settings Saved\"); </script>";
           server.send(200, "text/html", response);
           String ssid = String(server.arg("ssidDhcp"));
           String pass = String(server.arg("passkeyDhcp"));
          
           root["dhcpManual"]="dhcpManual";
           root["dhcpIP"] = "192.168.4.1";
           root["ssidDhcp"] = ssid;
           root["passkeyDhcp"] = pass;
           String JSONStatic;
           root.printTo(Serial);
           File fileToWrite = SPIFFS.open("/ip_set.txt", FILE_WRITE);
           if(!fileToWrite){
              Serial.println("Error opening SPIFFS");
            }
            
           if(root.printTo(fileToWrite)){
                Serial.println("--File Written");
            }else{
                Serial.println("--Error Writing File");
              }
               fileToWrite.close();           
              
             
           isConnected = false;        
  }

//*************Helper 3 DHCP DEFAULT**************//
  
void dhcpSetDefault(){
           JsonObject& root =jsonBuffer.createObject();
           String response="<p>The dhcp IPv4 address is ";
           response += server.arg("configure");
           response +="</P><BR>";
           response +="<H2><a href=\"/\">go home</a></H2><br>";
           response += "<script> alert(\"Settings Saved\"); </script>";
           server.send(200, "text/html", response);
           String ssid = String(server.arg("ssidDhcp"));
           String pass = String(server.arg("passkeyDhcp"));
           root["dhcpDefault"]="dhcpDefault";
           root["dhcpIP"] = "192.168.4.1";
           root["ssidDhcp"] = ssid;
           root["passkeyDhcp"] = pass;
           String JSONStatic;
           char JSON[120];
           root.printTo(Serial);
           File fileToWrite = SPIFFS.open("/ip_set.txt", FILE_WRITE);
           if(!fileToWrite){
              Serial.println("Error opening SPIFFS");
            }
           if(root.printTo(fileToWrite)){
                Serial.println("--File Written");
            }else{
                Serial.println("--Error Writing File");
              }           
           fileToWrite.close();  
           isConnected = false;          
      }

//****************HANDLE NOT FOUND*********************//
void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  message +="<H2><a href=\"/\">go home</a></H2><br>";
  server.send(404, "text/plain", message);
}

//***************Parse bytes from string******************//

void parseBytes(const char* str, char sep, byte* bytes, int maxBytes, int base) {
    for (int i = 0; i < maxBytes; i++) {
        bytes[i] = strtoul(str, NULL, base);  // Convert byte
        str = strchr(str, sep);               // Find next separator
        if (str == NULL || *str == '\0') {
            break;                            // No more separators, exit
        }
        str++;                                // Point to next character after separator
    }
}

//****************HANDLE CLIENT 192.168.1.77*********************//

void handleClientAP(){
  if( !isAPConnected ){
       WiFi.mode(WIFI_AP);
       Serial.println(WiFi.softAP(ssidAPConfig,passAPConfig) ? "soft-AP setup": "Failed to connect");
       delay(100);
       Serial.println(WiFi.softAPConfig(ap_local_IP, ap_gateway, ap_subnet)? "Configuring Soft AP" : "Error in Configuration");       
    } 
  Serial.println(WiFi.softAPIP());
  server.begin();
  server.on("/", handleRoot); 
  server.on("/dhcp", handleDHCP);
  server.on("/static", handleStatic);
  server.onNotFound(handleNotFound);  
   
  APTimer = millis();
    
  while(isConnected && millis()-APTimer<= APInterval) {
        server.handleClient();  }       
   esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
   esp_deep_sleep_start();    
  }
//***************************STATIC Helper method**************************//

void  staticAPConfig(String IPStatic, String gateway, String subnet, String ssid, String pass){
           IPStatic.toCharArray(ipv4Arr,sizeof(IPStatic)+2);
           gateway.toCharArray(gatewayArr,sizeof(gateway)+2);
           subnet.toCharArray(subnetArr,sizeof(subnet)+2);
           ssid.toCharArray(ssidAP,sizeof(ssid)+2);
           pass.toCharArray(passAP, sizeof(pass)+2);
           Serial.print(ssidAP);
           Serial.print(passAP);
           byte ip[4];
           parseBytes(ipv4Arr,'.', ip, 4, 10);
           ip0 = (uint8_t)ip[0];
           ip1 = (uint8_t)ip[1];
           ip2 = (uint8_t)ip[2];
           ip3 = (uint8_t)ip[3];
           IPAddress ap_local(ip0,ip1,ip2,ip3);
           ap_localWeb_IP = ap_local;
           parseBytes(gatewayArr,'.', ip, 4, 10);
           ip0 = (uint8_t)ip[0];
           ip1 = (uint8_t)ip[1];
           ip2 = (uint8_t)ip[2];
           ip3 = (uint8_t)ip[3];
           IPAddress ap_gate(ip0,ip1,ip2,ip3);
           ap_Webgateway = ap_gate;
           parseBytes(subnetArr,'.', ip, 4, 10);
           ip0 = (uint8_t)ip[0];
           ip1 = (uint8_t)ip[1];
           ip2 = (uint8_t)ip[2];
           ip3 = (uint8_t)ip[3];
           IPAddress ap_net(ip0,ip1,ip2,ip3);  
           ap_Websubnet= ap_net;
          
           WiFi.disconnect(true);
           WiFi.mode(WIFI_AP);   
           Serial.println(WiFi.softAP(ssidAP,passAP) ? "Setting up SoftAP" : "error setting up");
           delay(100);       
           Serial.println(WiFi.softAPConfig(ap_localWeb_IP, ap_gate, ap_net) ? "Configuring softAP" : "kya yaar not connected");    
           Serial.println(WiFi.softAPIP());
           server.begin();
           server.on("/", handleStaticForm); 
           server.onNotFound(handleNotFound);
   
            STimer = millis();
            while(millis()-STimer<= SInterval) {
               server.handleClient();  }       
           
}

//***************************WiFi Credintial Form**************************//

void dhcpAPConfig(String ssid, String pass){
      ssid.toCharArray(ssidAP,sizeof(ssid)+2);
      pass.toCharArray(passAP, sizeof(pass)+2);
      WiFi.mode(WIFI_OFF);
      WiFi.softAPdisconnect(true);
      delay(1000);
      WiFi.mode(WIFI_AP);
      Serial.println(WiFi.softAP(ssidAP,passAP) ? "Setting up SoftAP" : "error setting up");
      delay(200);
      Serial.println(WiFi.softAPIP());
      
      server.begin();
      server.on("/", handleStaticForm); 
      server.onNotFound(handleNotFound);
      DTimer = millis();
      while(millis()-DTimer<= DInterval) {
       server.handleClient();  }
  }

//****************************HANDLE STATIC FORM***************************//

void handleStaticForm() {
if(server.hasArg("ssid") && server.hasArg("passkey") && server.hasArg("token")){
       handleSubmitForm();
    }else{
           File  file = SPIFFS.open("/UbiDots.html", "r");
           server.streamFile(file,"text/html");
           file.close();
      }
  }
  
//****************************WiFi Credintial Submit****************************//

void handleSubmitForm() {
      String response="<p>The ssid is ";
      response += server.arg("ssid");
      response +="<br>";
      response +="And the password is ";
      response +=server.arg("passkey");
      response +="<br>";
      response +="And the tokenId is ";
      response += server.arg("token");
      response +="</P><BR>";
      response +="<H2><a href=\"/\">go home</a></H2><br>";
      server.send(200, "text/html", response);
      ROMwrite(String(server.arg("ssid")),String(server.arg("passkey")),String(server.arg("token")));
  }

//----------Write to ROM-----------//
void ROMwrite(String s, String p, String t){
 s+=";";
 write_EEPROM(s,0);
 p+=";";
 write_EEPROM(p,50);
 t+=";";
 write_EEPROM(t,100);
 EEPROM.commit();   
}

 //***********Write to ROM**********//
void write_EEPROM(String x,int pos){
  for(int n=pos;n<x.length()+pos;n++){
  //write the ssid and password fetched from webpage to EEPROM
   EEPROM.write(n,x[n-pos]);
  }
}

  
  //****************************EEPROM Read****************************//
String read_string(int l, int p){
  String temp;
  for (int n = p; n < l+p; ++n)
    {
   // read the saved password from EEPROM
     if(char(EEPROM.read(n))!=';'){
     
       temp += String(char(EEPROM.read(n)));
     }else n=l+p;
    }
  return temp;
}

//****************************Connect to WiFi****************************//
void reconnectWiFi(){
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
        String string_Ssid="";
        String string_Password="";
        string_Ssid= read_string(30,0); 
        string_Password= read_string(30,50);        
        Serial.println("ssid: "+ string_Ssid);
        Serial.println("Password: "+string_Password);
        string_Password.toCharArray(passWiFi,30);
        string_Ssid.toCharArray(ssidWiFi,30);
               
  delay(400);
  WiFi.begin(ssidWiFi,passWiFi);
  while (WiFi.status() != WL_CONNECTED)
  {
      delay(500);
      Serial.print(".");
  }
  Serial.print("Connected to:\t");
  Serial.println(WiFi.localIP());
  client.setServer(mqttBroker, 1883);
  client.setCallback(callback);
}
