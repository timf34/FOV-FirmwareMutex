/********************************************************************************/
// AWS Server Setup//
/********************************************************************************/
#include "secrets.h"
#include <WiFiClientSecure.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mutex>

#define AWS_IOT_PUBLISH_TOPIC "esp32/globalTest"
#define AWS_IOT_SUBSCRIBE_TOPIC "dalymount_IRL/pub"

WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// Initializing various vars
float prevX = 0;
float prevY = 0;

std::mutex myMutex;  // create a mutex object
int sharedValue = 0; // create a shared integer variable

int period = 200; // Period which board checks for new messages from server
unsigned long time_now = 0;

int xSpd = 12500;
int ySpd = 12500;

int xReceived = 0;
int yReceived = 0;

void wifiManagerSetup()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin("tim", "shopkeeper2");
    Serial.println("Connecting to Wi-Fi (ensure that it's 2.4Ghz!!!");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
}

void connectAWS()
{
    wifiManagerSetup();

    // Configure WiFiClientSecure to use the AWS IoT device credentials
    net.setCACert(AWS_CERT_CA);
    net.setCertificate(AWS_CERT_CRT);
    net.setPrivateKey(AWS_CERT_PRIVATE);

    // Connect to the MQTT broker on the AWS endpoint we defined earlier
    client.setServer(AWS_IOT_ENDPOINT, 8883);

    // Create a message handler
    client.setCallback(messageHandler);

    Serial.print("Connecting to AWS IOT");

    while (!client.connect(THINGNAME))
    {
        Serial.print(".");
        delay(100);
    }

    if (!client.connected())
    {
        Serial.println("AWS IoT Timeout!");
        return;
    }

    // Subscribe to a topic
    client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
    Serial.println("AWS IoT Connected!");
}

void messageHandler(char *topic, byte *payload, unsigned int length)
{

    Serial.print("incoming: ");
    Serial.println(topic);

    myMutex.lock();

    // Read in the variable.
    StaticJsonDocument<200> doc;
    deserializeJson(doc, payload);
    float timestamp = doc["T"];
    xReceived = doc["X"];
    yReceived = doc["Y"];

    Serial.print("X Coord: ");
    Serial.println(xReceived);
    Serial.print("Y Coord: ");
    Serial.println(yReceived);

    speedCalc(prevX, prevY, xReceived, yReceived);

    Serial.print("SpdC X Coord: ");
    Serial.println(xReceived);
    Serial.print("SpdCY Coord: ");
    Serial.println(yReceived);

    myMutex.unlock();

    // Now the TaskCore1 task can operate 

    prevX = xReceived;
    prevY = yReceived;
}

// The pinned task function for Core 1
void TaskCore1(void *pvParameters)
{
    int *xReceived = (int *)pvParameters;

    for (;;)
    {
        // Acquire the mutex lock
        myMutex.lock();

        // Deep copy the shared resource to a new variable 
        int xReceivedCopy = *xReceived;

        // Print the shared value
        Serial.println("TaskCore1: sharedValue = " + String(*xReceived));

        // Release the mutex lock
        myMutex.unlock();

        // Implement the stepper motor code here 

        // Delay for some time
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void setup()
{
    Serial.begin(9600);
    while (!Serial)
        ; // wait for Serial to be ready

    connectAWS();

    // Create a pinned task on Core 1
    xTaskCreatePinnedToCore(
        TaskCore1,            // task function
        "TaskCore1",          // task name
        10000,                // stack size
        (void *)&xReceived, // task parameters (sharedValue as a void pointer)  // Note: we will just pass the x coord for now. 
        1,                    // task priority
        NULL,                 // task handle
        1                     // core number (Core 1)
    );

    Serial.println("Mutex Example");
}

// void loop()
// {
//   if (millis() >= time_now + period)
//   {
//     time_now += period;
//     client.loop();
//     Serial.println("loop");
//     //  Serial.println(time_now);
//   }
// }

void loop()
{
    if (millis() >= time_now + period)
    {
        time_now += period;

        client.loop();

        Serial.println("loop");

    }

    // // Acquire the mutex lock
    // myMutex.lock();

    // // Access and modify the shared resource
    // sharedValue++;

    // // Print the shared value
    // Serial.println("Core 0: sharedValue = " + String(sharedValue));

    // // Release the mutex lock
    // myMutex.unlock();
}

    void speedCalc(float x1, float y1, float x2, float y2)
    {                   // calculate required stepper speed based on distance the ball has the travel within an alotted time
        float t = 0.15; // timeframe to complete movement

        float dx = abs(x2 - x1); // distance to next coordinate
        float dy = abs(y2 - y1);
        float sx = dx / t; // in mm/s //speed needed to get to next point within allowed timeframe
        float sy = dy / t;
        xSpd = (sx * (26000 / 103)); // convert speed  mm/s to stepper speed
        ySpd = (sy * (18500 / 65));
    }