#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SFE_BMP180.h>
#include <Wire.h>

//Network setup
const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";
const char* mqtt_broker = "MQTT_BROKER_IP_ADDRESS";

//Objects setup
WiFiClient espClient;
PubSubClient client(espClient);
SFE_BMP180 pressure_obj;

//Pin setup
const int moisture_sensor = A0;
const int water_pump = D3;

//Variables
long current_time;
long last_measure = 0;
long pump_timer = 0;
bool pump_switch = false;
int moist_value; 
double temp_value, pressure_value;

//--------------------------------------------------------------------------------------------------------------------------------

void wifi_setup(){
  delay(1000);
  
  //start connecting  
  Serial.println("Connecting to: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED){
      delay(500);
      Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected - ESP IP address: ");
  Serial.println(WiFi.localIP());
}

void mqtt_reconnect(){
  while(!client.connected()){
    Serial.print("Attempting MQTT connection...");

    if(client.connect("ESP8266-Beocin1")){
      Serial.println("Connection successful");
    } else{
      Serial.print("Connection failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);  
    }
  }  
}

//--------------------------------------------------------------------------------------------------------------------------------

int readMoisturePercentage(){
  int value = analogRead(moisture_sensor);
  //Serial.print("Raw value: ");
  //Serial.println(value);
  value = map(value, 1024,180,0,100);
  return value;  
}

//--------------------------------------------------------------------------------------------------------------------------------

void initBMP180(){
  if(pressure_obj.begin()){
    Serial.println("BMP180 initialisation successful");  
  }else{
    Serial.println("BMP180 initialisation fail"); 
    initBMP180();
  } 
}

double readTemperatureValue(){
  char status_; //lower memory usage
  double temp = 0;

  status_ = pressure_obj.startTemperature();
  if(status_ != 0){
    delay(status_);
    status_ = pressure_obj.getTemperature(temp);
    if(status_ == 0) Serial.println("Error while reading temperature value (step_2)"); 
  }else{
    Serial.println("Error while reading temperature value (step_1)");
  }

  return temp;
}

double readPressureValue(double temperature){
  char status_; //lower memory usage
  double pressure = 0;

  status_ = pressure_obj.startPressure(3);
  if(status_ != 0){
    delay(status_);
    status_ = pressure_obj.getPressure(pressure, temperature);
    if(status_ == 0) Serial.println("Error while reading pressure value (step_2)"); 
  }else{
    Serial.println("Error while reading pressure value (step_1)");
  }

  return pressure;
}

//--------------------------------------------------------------------------------------------------------------------------------

void setup() {
  pinMode(moisture_sensor, INPUT);
  pinMode(water_pump, OUTPUT);
  
  Serial.begin(115200);
  wifi_setup();
  client.setServer(mqtt_broker, 1883);

  initBMP180();

}

void loop() {
  if(!client.connected()) mqtt_reconnect();
  if(!client.loop()) client.connect("ESP8266-Beocin1");

  current_time = millis();

  if(current_time - last_measure > 10000){
    last_measure = current_time; 

    moist_value = readMoisturePercentage();
    Serial.print("Moisture sensor reading: ");
    Serial.print(moist_value);
    Serial.println("%");

    if(moist_value < 15 && !pump_switch){
      digitalWrite(water_pump, HIGH);
      pump_timer = millis();
      pump_switch = true;
    }

    temp_value = readTemperatureValue();
    Serial.print("Temperature sensor reading: ");
    Serial.print(temp_value);
    Serial.println(" degree C");

    pressure_value = readPressureValue(temp_value);
    Serial.print("Pressure sensor reading: ");
    Serial.print(pressure_value);
    Serial.println(" mb");

    //MQTT publish
    static char pub_moist_value[4];
    static char pub_temp_value[6];
    static char pub_pressure_value[8];
    
    dtostrf(moist_value, 4, 0, pub_moist_value);
    dtostrf(temp_value, 3, 2, pub_temp_value);
    dtostrf(pressure_value, 3, 2, pub_pressure_value);
    
    client.publish("/node_beocin1/moisture", pub_moist_value);
    client.publish("/node_beocin1/temperature", pub_temp_value);
    client.publish("/node_beocin1/pressure", pub_pressure_value);
  }

  if(current_time - pump_timer > 3000 && pump_switch){
    digitalWrite(water_pump, LOW);
    pump_switch = false;
  }

}
