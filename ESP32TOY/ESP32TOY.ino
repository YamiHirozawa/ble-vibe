#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <SPI.h>
#include <LeloRemote.h> //Get the library here : https://github.com/scanlime/arduino-lelo-remote (dont forget to modify LeloRemote.h)
#include <driver/dac.h>
#include <math.h>

LeloRemote remote;

BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
BLECharacteristic* pRxCharacteristic = NULL;
String bleAddress = "30c6f743754e"; // CONFIGURATION: < Use the real device BLE address here.
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;

int vibration;

const int lookupTable[21] = {0,158,162,166,168,170,172,174,176,178,180,182,184,186,188,190,192,194,196,198,200};

// Function to lookup the new value based on input
int lookupValue(int inputValue) {
    // Ensure the input value is within bounds
    if (inputValue < 0) {
        inputValue = 0;
    } else if (inputValue > 20) {
        inputValue = 20;
    }
    
    // Return the value from the lookup table
    return lookupTable[inputValue];
}

#define SERVICE_UUID           "53300001-0023-4bd4-bbd5-a6920e4c5653"
#define CHARACTERISTIC_RX_UUID "53300002-0023-4bd4-bbd5-a6920e4c5653"
#define CHARACTERISTIC_TX_UUID "53300003-0023-4bd4-bbd5-a6920e4c5653"

// Define DAC pins
#define DAC_CH1 25
#define DAC_CH2 26

void UpdateRF(void) {
  Serial.print("vibration");
  Serial.println(vibration);

  int power = map(vibration, 0 , 20 , 0, remote.MAX_POWER);
  power = constrain(power, 0, remote.MAX_POWER);
  Serial.print("power");
  Serial.println(power);
  remote.txMotorPower(power);
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      BLEDevice::startAdvertising();
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
}; 

class MySerialCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      static uint8_t messageBuf[64];
      assert(pCharacteristic == pRxCharacteristic);
      std::string rxValue = pRxCharacteristic->getValue();
      
      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++)
          Serial.print(rxValue[i]);

        
        Serial.println();
        Serial.println("*********");
      }

      
      if (rxValue == "DeviceType;") {
        Serial.println("$Responding to Device Enquiry");
        memmove(messageBuf, "Z:ED:c857339ba2a6;", 18);
        // CONFIGURATION:               ^ Use a BLE address of the Lovense device you're cloning.
        pTxCharacteristic->setValue(messageBuf, 18);
        pTxCharacteristic->notify();
      } else if (rxValue == "Battery;") {
        memmove(messageBuf, "69;", 3);
        pTxCharacteristic->setValue(messageBuf, 3);
        pTxCharacteristic->notify();
      } else if (rxValue == "PowerOff;") {
        memmove(messageBuf, "OK;", 3);
        pTxCharacteristic->setValue(messageBuf, 3);
        pTxCharacteristic->notify();
      } else if (rxValue == "RotateChange;") {
        memmove(messageBuf, "OK;", 3);
        pTxCharacteristic->setValue(messageBuf, 3);
        pTxCharacteristic->notify();
      } else if (rxValue.rfind("Status:", 0) == 0) {
        memmove(messageBuf, "2;", 2);
        pTxCharacteristic->setValue(messageBuf, 3);
        pTxCharacteristic->notify();
      } else if (rxValue.rfind("Vibrate:", 0) == 0) {
        vibration = std::atoi(rxValue.substr(8).c_str());
        dacWrite(DAC_CH1, lookupValue(vibration));
        memmove(messageBuf, "OK;", 3);
        pTxCharacteristic->setValue(messageBuf, 3);
        pTxCharacteristic->notify();
        UpdateRF();
      } else if (rxValue.rfind("Vibrate1:", 0) == 0) {
        vibration = std::atoi(rxValue.substr(9).c_str());
        dacWrite(DAC_CH1, lookupValue(vibration));
        memmove(messageBuf, "OK;", 3);
        pTxCharacteristic->setValue(messageBuf, 3);
        pTxCharacteristic->notify();
        UpdateRF();
      } else if (rxValue.rfind("Vibrate2:", 0) == 0) {
        vibration = std::atoi(rxValue.substr(9).c_str());
        dacWrite(DAC_CH1, lookupValue(vibration));
        memmove(messageBuf, "OK;", 3);
        pTxCharacteristic->setValue(messageBuf, 3);
        pTxCharacteristic->notify();
        UpdateRF();
      } else {
        Serial.println("$Unknown request");        
        memmove(messageBuf, "ERR;", 4);
        pTxCharacteristic->setValue(messageBuf, 4);
        pTxCharacteristic->notify();
      }
    }
};

void setup() {
  Serial.begin(115200);

  SPI.begin();
  remote.reset();
  delay(100);
  remote.txMotorPower(0);
  delay(100);
  remote.txMotorPower(128);
  delay(100);
  remote.txMotorPower(0);
  delay(100);
  
  // Create the BLE Device
  BLEDevice::init("LVS-"); // CONFIGURATION: The name doesn't actually matter, The app identifies it by the reported id.

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristics
  pTxCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_TX_UUID,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pTxCharacteristic->addDescriptor(new BLE2902());

  pRxCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_RX_UUID,
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_WRITE_NR
                    );
  pRxCharacteristic->setCallbacks(new MySerialCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(100); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
}
