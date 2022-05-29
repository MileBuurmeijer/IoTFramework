#ifndef IOT_FRAMEWORK_H
#define IOT_FRAMEWORK_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h> // the espressif version (not equal to the one from Arduino)
#include <list>
#include "event.h"
#include "controllerEventListener.h"

class IoTFramework : public ControllerEventListenerInterface {

private:
    static IoTFramework* _instance; // for use in singleton pattern
    // wifi variables
    String _ssid;
    String _password;
    String _hostname;
    boolean _useMDNS;
    const int _wifiConnectRetries = 40;
    // mqtt variables
    const int _mqttPort = 1883;
    String _mqttBrokerHostname;
    String _mqttClientID = "ESP32_IoTFramework_MqttClient_";
    String _mqttUsername;
    String _mqttPassword;
    String _mqttTopLevelTopic;
    String _mqttOutTopicError;
    String _mqttTopicToSubscribeTo;
    String _mqttWillTopic;
    String _mqttWillMsg = "iotdevicebase is gone";
    String _mqttHelloWrld = "Hello world, the iotdevicebase is alive";
    String _mqttNoErrorMsg = "No error yet ;-)";
    int _ledPin = 2;
    static boolean _isSetup;

    TaskHandle_t _parallelTask1;
    IPAddress _mqttBrokerIpAddress; // ip address of MQTT broker
    WiFiClient _wifiClient;        // wifi client for mqtt client
    PubSubClient _mqttClient;      // MQTT client
    boolean _ledState = true; // variable that hold the state of the onboard led
    std::list<ControllerEventListenerInterface*> _eventListeners;
    std::list<Event> _receivedEvents;
    std::list<String> _topicsToSubscribeTo;
    IoTFramework();

public:
    ~IoTFramework();
    IoTFramework( const IoTFramework&) = delete; // don't implement
    void operator=(const IoTFramework &) = delete; // don't implement
    static IoTFramework* getInstance();
    void addEventListener(ControllerEventListenerInterface* anEventListener);
    void setWifiConfig(String aSsid, String aPassword, String aHostname, boolean useMDNS);
    void setup();
    void setMqttConfig(String aMqttUsername, String aMqttPassword, String aMqttHostname, String aMqttTopLevelTopic);
    void setTopicsToSubscribeTo( std::list<String> topicList);
    String getMacAddress();
    void processEvent(Event event);
    void receiveIoTFrameworkEvent(Event event) override;
    void setupWifi();
    void setupMqttClient();
    void handleReceivedEvents();
    static void ParallelTaskOnCore0(void *pvParameters); // =>!! made this a static method, because instance methods can not be used when function pointers are used
    void parallellTaskLoopCode();
    boolean isSetup();
    boolean isWifiConnected();
    boolean isMqttConnected();
    void setupParallelTasks();
    static void MqttDataRecievedCallback(char *topic, byte *messagePayload, unsigned int messageLength); // =>!! made this a static method, because instance methods can not be used when function pointers are used
    void flipLed();
    void setupArduinoOTA();
}; // end of class definition

#endif