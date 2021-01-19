/**
 * Example how to connect an ESP32 to Sensirion's SmartGadget
 * with SHT31 temperature and humidity sensor
 * https://developer.sensirion.com/platforms/sht31-smart-gadget-development-kit/
 * https://github.com/Sensirion/SmartGadget-Firmware/
 * 
 * original author unknown
 * updated by chegewara
 * source:
 * https://github.com/nkolban/ESP32_BLE_Arduino/blob/master/examples/BLE_client/BLE_client.ino
 * 
 * changed for SmartGadget by DLE Jan. 2021
 * 
 * Extended for TTGO T1 ESP32 Gadget and MQTT by Maurin Widmer, 2021
 */

#include <Arduino.h>

#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <Time.h>

#include "BLEDevice.h"

// load Wifi credentials
#include "credentials.h"

/**********************************************************************************************/

#define mqtt_server "lumicsraspi4" //CHANGE MQTT Broker address
#define mqtt_port 1883 // define MQTT port
#define mqtt_user "your_username" //not used, make a change in the mqtt initializer
#define mqtt_password "your_password" //not used, make a change in the mqtt initializer
String ssid = WIFI_SSID;      //CHANGE (WiFi Name)
String pwd = WIFI_PASSWD;      //CHANGE (WiFi password)
String sensor_location = "freezer"; // define sensor location

//Define the last 4 digits of the Smart Gadget (shown when ble is turned on)
#define SmartGadgetAdr "8c:84"

const int publish_every_s = 15; //only read publish sensor every 15s

//#define DEBUG 

#define sw_version "v1.0" // Define Software Version
/***********************************************************************************************/

//values
float t, rh, bat;

//display
// This Gadget has a 1.14inch TFT display with a width of 135 and a height of 240
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
TFT_eSPI tft = TFT_eSPI(135,240);  // Invoke library, pins defined in User_Setup.h

// Display related
#define SENSIRION_GREEN 0x6E66

#define GFXFF 1
#define FSS9 &FreeSans9pt7b
#define FSS12 &FreeSans12pt7b
#define FSS18 &FreeSans18pt7b
#define FSS24 &FreeSans24pt7b

//wifi stuff
String mac;
String MAC_ADDRESS;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
String sensor_topic = "sensors/" + sensor_location + "/sht31";
String device_type = "HumiGadget";
String device_name = "esp32";

int wifi_setup_timer = 0;
int publish_counter = 0;

// Functions

// System and Wifi
void setup_wifi() {
  char ssid1[30];
  char pass1[30];
  ssid.toCharArray(ssid1, 30);
  pwd.toCharArray(pass1, 30);
  Serial.println();
  Serial.print("Connecting to: " + String(ssid1));
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid1, pass1);
  while (WiFi.status() != WL_CONNECTED) {
    delay(10);
    Serial.print(".");
    wifi_setup_timer ++;
    if (wifi_setup_timer > 999) {     //Restart the ESP if no connection can be established
      ESP.restart();                 
    }
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {                                                  //Reconnect to the MQTT server
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    char id[mac.length() + 1];
    mac.toCharArray(id, mac.length() + 1);
    if (mqttClient.connect(id)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(100);
    }
  }
}

// display functions
void display_values(){
  tft.setFreeFont(FSS9);
  tft.fillScreen(TFT_BLACK);
  tft.drawString("T", 10, 120, GFXFF);
  tft.drawString("RH", 210, 120, GFXFF);
  tft.drawFastVLine(120, 0, 120, TFT_WHITE);

  tft.setFreeFont(FSS24);
  tft.setTextColor(SENSIRION_GREEN);
  tft.drawString(String(int(round(t))), 20, 40, GFXFF);
  tft.drawString(String(int(round(rh))), 150, 40, GFXFF);

  Serial.println("new values to display send");

}

void display_error(){
  tft.setTextSize(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  // display mac address
  tft.drawString("Error", 30,20, 4);
  tft.drawString("I/O Sensor", 10,50, 4);
  tft.drawString(mac, 180, 120, 2);


  Serial.println("Sensor is not responding");
}

void publish_data() {
// Check whether connection to the broker is true
  if (!mqttClient.connected() && WiFi.status() == WL_CONNECTED) {
    Serial.println("Client got disconnected from Broker.");
    reconnect();
  }
  mqttClient.publish((sensor_topic + "/T").c_str(), ("T,site="+ sensor_location +" value=" + String(t)).c_str(), false);
  mqttClient.publish((sensor_topic + "/RH").c_str(), ("RH,site="+ sensor_location +" value=" + String(rh)).c_str(), false);
  mqttClient.publish((sensor_topic + "/BAT").c_str(), ("BAT,site="+ sensor_location +" value=" + String(bat)).c_str(), false);
  Serial.println("Data published");
}

//BLE
// The remote service we wish to connect to.
static BLEUUID serviceUUID_t("00002234-B38D-4985-720E-0F993A68EE41");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID_t("00002235-B38D-4985-720E-0F993A68EE41");

// The remote service we wish to connect to.
static BLEUUID serviceUUID_rh("00001234-B38D-4985-720E-0F993A68EE41");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID_rh("00001235-B38D-4985-720E-0F993A68EE41");

// The remote service we wish to connect to.
static BLEUUID serviceUUID_bat("0000180F-0000-1000-8000-00805f9b34fb");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID_bat("00002a19-0000-1000-8000-00805f9b34fb");


static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic_t;
static BLERemoteCharacteristic* pRemoteCharacteristic_rh;
static BLERemoteCharacteristic* pRemoteCharacteristic_bat;
static BLEAdvertisedDevice* myDevice;



union u_tag {
   byte b[4];
   float fval;
} u;

static void notifyCallback_t(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    #ifdef DEBUG
      Serial.print("Notify callback for characteristic ");
      Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
      Serial.print(" of data length ");
      Serial.println(length);
    #endif
    
    u.b[0] = pData[0];
    u.b[1] = pData[1];
    u.b[2] = pData[2];
    u.b[3] = pData[3];
    t = u.fval;
    #ifdef DEBUG
      Serial.print(u.fval);
      Serial.println();
    #endif
}

static void notifyCallback_rh(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    #ifdef DEBUG
      Serial.print("Notify callback for characteristic ");
      Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
      Serial.print(" of data length ");
      Serial.println(length);
    #endif
    
    u.b[0] = pData[0];
    u.b[1] = pData[1];
    u.b[2] = pData[2];
    u.b[3] = pData[3];
    rh = u.fval;
    #ifdef DEBUG
      Serial.print(u.fval);
      Serial.println();
    #endif   
}


static void notifyCallback_bat(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    #ifdef DEBUG
      Serial.print("Notify callback for characteristic ");
      Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
      Serial.print(" of data length ");
      Serial.println(length);
    #endif
    bat = pData[0];
    Serial.print(pData[0]);
    Serial.print("\t");    
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    #ifdef DEBUG
      Serial.println("onDisconnect");
    #endif
  }
};

bool connectToServer() {
    #ifdef DEBUG
      Serial.print("Forming a connection to ");
      Serial.println(myDevice->getAddress().toString().c_str());
    #endif
    
    BLEClient*  pClient  = BLEDevice::createClient();
    #ifdef DEBUG
      Serial.println(" - Created client");
    #endif
    
    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    #ifdef DEBUG
      Serial.println(" - Connected to server");
    #endif
    
    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService_t = pClient->getService(serviceUUID_t);
    if (pRemoteService_t == nullptr) {
      #ifdef DEBUG
        Serial.print("Failed to find our service UUID: ");
        Serial.println(serviceUUID_t.toString().c_str());
      #endif
      
      pClient->disconnect();
      return false;
    }
    #ifdef DEBUG
      Serial.println(" - Found our t service");
    #endif
    
     // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService_rh = pClient->getService(serviceUUID_rh);
    if (pRemoteService_rh == nullptr) {
      #ifdef DEBUG
        Serial.print("Failed to find our service UUID: ");
        Serial.println(serviceUUID_rh.toString().c_str());
      #endif
      
      pClient->disconnect();
      return false;
    }
    #ifdef DEBUG
      Serial.println(" - Found our rh service");
    #endif
    
     // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService_bat = pClient->getService(serviceUUID_bat);
    if (pRemoteService_bat == nullptr) {
      #ifdef DEBUG
        Serial.print("Failed to find our service UUID: ");
        Serial.println(serviceUUID_bat.toString().c_str());
      #endif
      
      pClient->disconnect();
      return false;
    }
    #ifdef DEBUG
      Serial.println(" - Found our bat service");
    #endif
    
    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic_t = pRemoteService_t->getCharacteristic(charUUID_t);
    if (pRemoteCharacteristic_t == nullptr) {
      #ifdef DEBUG
        Serial.print("Failed to find our characteristic UUID: ");
        Serial.println(charUUID_t.toString().c_str());
      #endif
      pClient->disconnect();
      return false;
    }
    #ifdef DEBUG
      Serial.println(" - Found our T characteristic");
    #endif
    
    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic_rh = pRemoteService_rh->getCharacteristic(charUUID_rh);
    if (pRemoteCharacteristic_rh == nullptr) {
      #ifdef DEBUG
        Serial.print("Failed to find our characteristic UUID: ");
        Serial.println(charUUID_rh.toString().c_str());
      #endif
      pClient->disconnect();
      return false;
    }
    #ifdef DEBUG
      Serial.println(" - Found our T characteristic");
    #endif   

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic_bat = pRemoteService_bat->getCharacteristic(charUUID_bat);
    if (pRemoteCharacteristic_bat == nullptr) {
      #ifdef DEBUG
        Serial.print("Failed to find our characteristic UUID: ");
        Serial.println(charUUID_bat.toString().c_str());
      #endif
      pClient->disconnect();
      return false;
    }
    #ifdef DEBUG
      Serial.println(" - Found our T characteristic");
    #endif 
    
    #ifdef DEBUG
      // Read the value of the characteristic.
      if(pRemoteCharacteristic_t->canRead()) {
        std::string value = pRemoteCharacteristic_t->readValue();
        Serial.print("The characteristic value was: ");
        
        u.b[0] = value[0];
        u.b[1] = value[1];
        u.b[2] = value[2];
        u.b[3] = value[3];        
        Serial.print(u.fval);
      }
      if(pRemoteCharacteristic_bat->canRead()) {
        std::string value = pRemoteCharacteristic_bat->readValue();
        Serial.print("The characteristic value was: ");
        Serial.println((int16_t)value[0]);
      }
    #endif

    if(pRemoteCharacteristic_t->canNotify())
      pRemoteCharacteristic_t->registerForNotify(notifyCallback_t);
    if(pRemoteCharacteristic_rh->canNotify())
      pRemoteCharacteristic_rh->registerForNotify(notifyCallback_rh);
    if(pRemoteCharacteristic_bat->canNotify())
      pRemoteCharacteristic_bat->registerForNotify(notifyCallback_bat);

    connected = true;
}

/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    #ifdef DEBUG
      Serial.print("BLE Advertised Device found: ");
      Serial.println(advertisedDevice.toString().c_str());
    #endif
    // hardcoding the last 4 digits of mac BLE Address of the Humi Gadget to make sure to only connect to right device
    if ((advertisedDevice.getAddress().toString().find(SmartGadgetAdr) != std::string::npos ) && (advertisedDevice.getName() == "Smart Humigadget")) { 
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;

    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks


void setup() {
  //Serial
  Serial.begin(115200);

  // Setup Display
  tft.init();
  tft.setRotation(1); 
  tft.setTextSize(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Humi Reader", 30,20, 4);
  tft.drawString("Connecting to Wifi", 10,50, 4);
  tft.drawString(sw_version, 95,90, 4);
  delay(100);

  // Wifi
  setup_wifi();                                      //connect to wifi
  mqttClient.setServer(mqtt_server, mqtt_port);               //connect wo MQTT server
  //get MAC address
  MAC_ADDRESS = WiFi.macAddress();                    
  Serial.println(MAC_ADDRESS);
   //shorten MAC address to the last four digits (without ":")
  mac =  MAC_ADDRESS.substring(9, 11) + MAC_ADDRESS.substring(12, 14) + MAC_ADDRESS.substring(15, 17);  

  //BLE
  #ifdef DEBUG
    Serial.println("Starting Arduino BLE Client application...");
  #endif
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 10 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1500);
  pBLEScan->setWindow(550);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(10, false);
}


void loop() {

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer()) {
      #ifdef DEBUG
        Serial.println("We are now connected to the BLE Server.");
      #endif
    } else {
      #ifdef DEBUG
        Serial.println("We have failed to connect to the server; there is nothin more we will do.");
      #endif
    }
    doConnect = false;
  }
  
  if (connected) {
    Serial.print("Temp: ");
    Serial.println(t);
    Serial.print("RH: ");
    Serial.println(rh);

    display_values();
    
    if(publish_counter >= publish_every_s){
      publish_data();
      publish_counter = 0;
    }
    publish_counter++;

  }else if(doScan){
    BLEDevice::getScan()->start(0);  // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
    display_error();
  }
   mqttClient.loop();
  delay(1000);
}