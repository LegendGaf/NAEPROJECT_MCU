#include <Arduino.h>
#include <Wire.h>
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define SERVICE_UUID        "fa860f46-42cb-4509-a234-57e7aa12d1e2"
#define CHARACTERISTIC_UUID_THERMAL "0ec3e1a7-9e07-4c7c-b1d9-8bab39cb2c43"
#define CHARACTERISTIC_UUID_ANALOG "0ec3e1a7-9e07-4c7c-b1d9-8bab39cb2c42" //TODO generate UUID
#define DEVICE_NAME         "LADYBUG_V1"
#define ROW_LENGTH          96
#define MAX_PIXELS          768
#define EMISSIVITY          0.95
#define analog_PIN   36    //TODO watch datasheet !!  :)
//TODO


const byte MLX90640_address = 0x33;

#define TA_SHIFT 8 //Default shift for MLX90640 in open air

// Camera frames
uint16_t mlx90640Frame[834];
float mlx90640To[768];
paramsMLX90640 mlx90640;

BLECharacteristic *pCharacteristic_thermal;
BLECharacteristic *pCharacteristic_analog;

// BLE page
char str[5120];
char *strPointer = str;

int currentPos = 0;
String strToAppend = "";

BLEServer *pServer;


//analogue in value
uint16_t analog_data = 0;


//Returns trstopue if the MLX90640 is detected on the I2C bus
boolean isConnected() {
    Wire.beginTransmission((uint8_t) MLX90640_address);
    if (Wire.endTransmission() != 0) {
        return (false);    //Sensor did not ACK
    }
    return (true);
}


void setup() {
    Wire.begin();
    Wire.setClock(400000); //Increase I2C clock speed to 400kHz

    Serial.begin(115200); //Fast debug as possible

    Serial.println("Thermal Camera starting...");

    if (isConnected() == false) {
        Serial.println("MLX90640 not detected at default I2C address. Please check wiring. Freezing.");
        while (1);
    }

    //Get device parameters - We only have to do this once
    int status;
    uint16_t eeMLX90640[832];
    status = MLX90640_DumpEE(MLX90640_address, eeMLX90640);
    if (status != 0) {
        Serial.println("Failed to load system parameters");
    }

    status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
    if (status != 0) {
        Serial.println("Parameter extraction failed");
    }

    MLX90640_SetRefreshRate(MLX90640_address, 0x04); //Set rate to 8Hz

    // Setup Bluetooth
    BLEDevice::init(DEVICE_NAME);
    pServer = BLEDevice::createServer();

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pCharacteristic_thermal = pService->createCharacteristic(
            CHARACTERISTIC_UUID_THERMAL,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_NOTIFY |
            BLECharacteristic::PROPERTY_INDICATE |
            BLECharacteristic::PROPERTY_BROADCAST
    );


    pCharacteristic_analog = pService->createCharacteristic(
            CHARACTERISTIC_UUID_ANALOG,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_NOTIFY |
            BLECharacteristic::PROPERTY_INDICATE |
            BLECharacteristic::PROPERTY_BROADCAST
    );

    // Create a BLE Descriptor for thermal senso.
    pCharacteristic_thermal->addDescriptor(new BLE2902());

    pCharacteristic_thermal->setValue("");

    // Create a BLE Descriptor for analog .
    pCharacteristic_analog->addDescriptor(new BLE2902());

    pCharacteristic_analog->setValue("");

    pService->start();

    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->start();



    //setup Anlog IN with correct range
    Serial.println("Analog In setup");
    //TODO



    Serial.println("All systems running!");
}

void loop() {
    for (byte x = 0; x < 2; x++) {
        int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);

        float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640);
        float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);

        float tr = Ta - TA_SHIFT;

        MLX90640_CalculateTo(mlx90640Frame, &mlx90640, EMISSIVITY, tr, mlx90640To);
    }

    int base = 0;
    String row = "";
    int newNum = 0;
    while (base < MAX_PIXELS) {
        strToAppend = "";
        currentPos = 0;
        row = String(base / ROW_LENGTH);
        strToAppend = row + ":";
        sprintf(strPointer + currentPos, strToAppend.c_str());
        currentPos += strToAppend.length();

        for (int y = 0; y < ROW_LENGTH; y++) {
            newNum = int(mlx90640To[base + y] * 10);
            strToAppend = String(newNum) + ",";
            sprintf(strPointer + currentPos, strToAppend.c_str());
            currentPos += strToAppend.length();
        }

        pCharacteristic_thermal->setValue(strPointer);
        pCharacteristic_thermal->notify();
        delay(20);

        base += ROW_LENGTH;
    }

    // read ANALOG
    analog_data = analogRead(analog_PIN);

    //TODO
    //here we send data of analog
    if(analog_data>2000) {
        Serial.print("analog value is :\n__METAL__DETECTED__");
        Serial.println(analog_data);
        Serial.print((analog_data*100)/4095);
        Serial.println("%");

    } else{
        Serial.print("analog value is :");
        Serial.println(analog_data);
    }

    pCharacteristic_analog->setValue(analog_data);
    pCharacteristic_analog->notify();

}

