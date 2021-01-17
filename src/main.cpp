#include <Arduino.h>

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
 * Extended for TTGO T1 ESP32 GAdget and MQTT by Maurin Widmer, 2021
 */

#include "BLEDevice.h"

#include "credentials.h"

//#define DEBUG 

//Define the last 4 digits of the Smart Gadget (shown when ble is turned on)
#define SmartGadgetAdr "8c:84"

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

float t, rh, bat;

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
    
    
    //if (advertisedDevice.getName() == "Smart Humigadget") { // then it would connect to the first foudn gadget called like that
    //if (advertisedDevice.getAddress().toString() == "c0:39:41:a8:8c:84") { // hardcoding the BLE Address of the Humi Gadget to make sure t only connect to right device
    if ((advertisedDevice.getAddress().toString().find(SmartGadgetAdr) != std::string::npos ) && (advertisedDevice.getName() == "Smart Humigadget")) { // hardcoding the BLE Address of the HUmi Gadget to make sure t oonly connect to right device
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;

    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks


void setup() {
  Serial.begin(115200);
  #ifdef DEBUG
    Serial.println("Starting Arduino BLE Client application...");
  #endif
  Serial.println("realtive_humidity\ttemperature");
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
    // do nothing specific here, services are using the notification system
    Serial.print("Temp: ");
    Serial.println(t);
    Serial.print("RH: ");
    Serial.println(rh);
  }else if(doScan){
    BLEDevice::getScan()->start(0);  // this is just eample to start scan after disconnect, most likely there is better way to do it in arduino
  }
  
  delay(1000);
}