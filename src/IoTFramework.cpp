
#include "IoTFramework.h"
#include <mdns.h>               // ESP IDF mdns library
#include <ESPmDNS.h>            // ESP mDNS service library

//initialize static class variables
IoTFramework* IoTFramework::_instance = nullptr;
boolean IoTFramework::_isSetup = false;

IoTFramework::IoTFramework() {
}    // emtpy implementation

IoTFramework::~IoTFramework() { 
      // todo: client up the mess
}

IoTFramework* IoTFramework::getInstance() {
    if (_instance == nullptr) {
        _instance = new IoTFramework();
    }
    return _instance;
}

void IoTFramework::addEventListener(ControllerEventListenerInterface* anEventListener) {
    Serial.println("Event listener added to IoTFramework");
    _eventListeners.push_back(anEventListener);
}

void IoTFramework::setWifiConfig(String aSsid, String aPassword, String aHostname, boolean useMDNS) {
    _ssid = aSsid;
    _password = aPassword;
    _hostname = aHostname;
    _useMDNS = useMDNS;
}

void IoTFramework::setup() {
    _mqttOutTopicError = _mqttTopLevelTopic + "/error";
    pinMode( _ledPin, OUTPUT); //init onboard led pin as output
    setupParallelTasks(); // setup parallell tasks: wifi, mqtt, OTA updates
}

void IoTFramework::setMqttConfig(String aMqttUsername, String aMqttPassword, String aMqttHostname, String aMqttTopLevelTopic) {
    _mqttUsername = aMqttUsername;
    _mqttPassword = aMqttPassword;
    _mqttBrokerHostname = aMqttHostname;
    _mqttTopLevelTopic = aMqttTopLevelTopic;
}

void IoTFramework::setTopicsToSubscribeTo( std::list<String> aTopicList) {
    _topicsToSubscribeTo = aTopicList;
    // Serial.println("Received topic list has length: " + String(aTopicList.size()));
    // Serial.println("IoTFramework topic list has length: " + String(_topicsToSubscribeTo.size()));
}

String IoTFramework::getMacAddress() {
    String mac = WiFi.macAddress();
    Serial.println("MAC=" + mac);
    return mac;
}

void IoTFramework::processEvent(Event event)
{
    // Serial.println("Inside processEvent of IoTFramework");
    for (ControllerEventListenerInterface* eventListener : _eventListeners)
    {
        // Serial.println("Inside for-loop of processEvent of IoTFramework");
        eventListener->receiveIoTFrameworkEvent( event);
    }
}

void IoTFramework::handleReceivedEvents() {
    int eventsToProcess = 5; // max number of message to process by mqtt client in this run
    while (_receivedEvents.empty()==false && eventsToProcess >= 0) {
        // Serial.println( "An event can be handled");
        Event event = _receivedEvents.front(); // get the first event in the queue
        String topic = event.getTopic();
        // Serial.println( "An event from front of list poked: " + topic);
        String payload = event.getPayload();
        Serial.println("About to publish: [" + topic + "]=" + payload);
        _mqttClient.publish( topic.c_str(), payload.c_str());
        // Serial.println( "Deleted the event");
        _receivedEvents.pop_front(); // remove this element after publishing
        // Serial.println( "Popped first element of list");
        eventsToProcess--;
    }
}

void IoTFramework::receiveIoTFrameworkEvent(Event event)
{
    // iot framework receives publish status events from controller
    // Serial.println( "An event received in IoTFramework::receiveEvent with topic: " + event.getTopic()); 
    _receivedEvents.push_front( event);
}

void IoTFramework::setupWifi()
{
    Serial.println("Setup wifi");
    Serial.print("Connecting to: ");
    Serial.println(_ssid);
    WiFi.setHostname(_hostname.c_str());
    Serial.print("Hostname set to ");
    Serial.println(_hostname);
    WiFi.begin(_ssid.c_str(), _password.c_str());
    Serial.println("Wifi begin called");
    int wifiConnectCounter = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(200);
        Serial.print(".");
        wifiConnectCounter++;
        if (wifiConnectCounter > _wifiConnectRetries)
        {
            ESP.restart();
        }
    }
    Serial.println();
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

// call back function for MQTT communication, needs to be defined before MQTT constructor
void IoTFramework::setupMqttClient()
{
    _mqttClient.setClient( _wifiClient);
    Serial.println("Setup MQTT - mDNS set to " + _useMDNS ? "true" : "false");
    if (_useMDNS) {
        MDNS.begin(_hostname.c_str());
        String service = "mqtt"; // the mqtt service we are looking for
        String proto = "tcp"; // it works over the tcp protocol
        Serial.printf("Browsing for service %s.%s.local. ... \n", service.c_str(), proto.c_str());
        int n = MDNS.queryService(service, proto);
        if (n == 0) {
            Serial.println("no MQTT services found");
            // use hardcoded IP address instead
        } else {
            Serial.printf("Service(s) found = %d\n", n);
            // list the MQTT services found
            for ( int i = 0; i < n; i++) {
                Serial.printf("Service [%d]: %s=>(%s:%s)\n", i, MDNS.hostname(i).c_str(), MDNS.IP(i).toString().c_str(), String(MDNS.port(i)).c_str());
            }
            Serial.println("Found at least one mqtt broker service in local network");
            Serial.println("Taking first one found: " + MDNS.IP(0).toString());
            // take the first MQTT service found
            String localMqttHostname = MDNS.hostname(0) + ".local";
            localMqttHostname.toLowerCase();
            IPAddress localMqttBrokerAddress = MDNS.queryHost( MDNS.hostname(0));
            _mqttClient.setServer( localMqttBrokerAddress, _mqttPort);
        }
    } else {
        Serial.println("MQTT server set to hostname: " + _mqttBrokerHostname);
        IPAddress ipAddress;
        if (ipAddress.fromString( _mqttBrokerHostname)) {
            Serial.println("Parsed MQTT broker IP Address: " + ipAddress.toString());
            _mqttClient.setServer( ipAddress, _mqttPort);
        } else {
            _mqttClient.setServer( _mqttBrokerHostname.c_str(), _mqttPort);
        }
    }
    _mqttClient.setBufferSize( 10000); // increase with size of information model message size
    _mqttClient.setCallback( MqttDataRecievedCallback); // static callback method of IoTFramework class
    // connect to MQTT
    Serial.println("Connect to MQTT server with client id: " + _mqttClientID);
    _mqttClientID = _mqttClientID + this->getMacAddress();
    boolean isConnected = _mqttClient.connect( _mqttClientID.c_str(), _mqttUsername.c_str(), _mqttPassword.c_str());
    if (isConnected)
    {
        Serial.println();
        Serial.println("Connected to MQTT server");
        _mqttClient.publish( _mqttOutTopicError.c_str(), _mqttNoErrorMsg.c_str()); // clear any previous error messages
        Serial.println();
        if (!_topicsToSubscribeTo.empty()) {
            Serial.println("Subscribing to topics: ");
            for (String topic: _topicsToSubscribeTo) {
                Serial.println("-" + topic);
                _mqttClient.subscribe( topic.c_str());
            }
        } else {
            Serial.println("No topics to subscribe to");
        }
    }
    else
    {
        Serial.println();
        Serial.println("Not connected to MQTT server");
    }
}

void IoTFramework::parallellTaskLoopCode() {
    for (;;)
    {
        ArduinoOTA.handle();    // handle OTA events
        _mqttClient.loop();     // handle MQTT interaction
        handleReceivedEvents(); // handle incomming events of the iot framework
        // check if wifi and mqtt are still connected
        if (_wifiClient.connected() == false) {
            Serial.println("Wifi not connected! Setting it up");
            // setupWifi();
        }
        if (_mqttClient.connected()== false) {
            Serial.println("Mqtt not connected! Setting it up");
            // setupMqttClient();
        }
        int mqttStatus = _mqttClient.state();
        if (mqttStatus != 0) {
            Serial.println("Status mqtt is: " + String(mqttStatus));
        }
        flipLed(); // light the onboard led to show activity of this task (~2Hz -> based on 500 mS delay) [TODO: update this calculation]
        delay(500);
    }
}

boolean IoTFramework::isSetup() {
    return _isSetup;
}

boolean IoTFramework::isWifiConnected() {
    return _wifiClient.connected();
}

boolean IoTFramework::isMqttConnected() {
    return _mqttClient.connected();
}

void IoTFramework::ParallelTaskOnCore0(void *pvParameters) // =>!! made this a static method, because instance methods can not be used when function pointers are used
{
    IoTFramework* iotFramework = IoTFramework::getInstance();
    iotFramework->setupWifi();
    iotFramework->setupMqttClient(); // setup the MQTT communication
    iotFramework->setupArduinoOTA(); // setup Arduino OTA
    delay(500); // wait for setup
    iotFramework->_isSetup = true; // flag that all setup is done and that the parallel task loop starts from here onwards
    Serial.println("Wifi connection state: " + String(iotFramework->isWifiConnected()?"connected":"NOT connected"));
    Serial.println("Mqtt connection state: " + String(iotFramework->isMqttConnected()?"connected":"NOT connected"));
    Serial.println("Starting up the parallell task loop on core 0 (core 1 runs main sketch)");
    iotFramework->parallellTaskLoopCode();
}

void IoTFramework::setupParallelTasks()
{
    //create a task that will be executed in the ParallelTask1Code() function, with priority 1 and executed on core 0
    xTaskCreatePinnedToCore(
        ParallelTaskOnCore0,/* Task function. */ // =>!! made this a static method, because instance methods can not be used when function pointers are used
        "Task1",            /* name of task. */
        5000,               /* stack size of task */
        NULL,               /* parameter of the task */
        1,                  /* priority of the task */
        &_parallelTask1,    /* Task handle to keep track of created task */
        0);                 /* pin task to core 0 */
}

void IoTFramework::MqttDataRecievedCallback(char *topic, byte *messagePayload, unsigned int messageLength)
{
    // Serial.print("Message length: ");
    // Serial.println( messageLength);
    // Serial.print("Message chars recieved: ");
    char messageBuffer[messageLength + 1];
    for (int i = 0; i < messageLength; i++)
    {
        messageBuffer[i] = (char)messagePayload[i];
        // Serial.print( (char) messagePayload[i]);
    }
    messageBuffer[messageLength] = 0;
    // Serial.println();
    String message = messageBuffer;
    String inTopic = String(topic);
    Serial.println("Processing topic[" + inTopic + "] & message=" + message);
    Event event(inTopic, message);
    IoTFramework* iotFramework = IoTFramework::getInstance();
    iotFramework->processEvent( event);
}

void IoTFramework::flipLed()
{
    // change state of built in blue LED
    if (_ledState)
    {
        digitalWrite(_ledPin, HIGH);
    }
    else
    {
        digitalWrite(_ledPin, LOW);
    }
    _ledState = !_ledState; // change led state for next loop
}

void IoTFramework::setupArduinoOTA()
{
    ArduinoOTA.setHostname(String(_hostname + "-ESP32").c_str());
    Serial.println( "Setting up Arduino OTA");
    ArduinoOTA
        .onStart([]()
                    {
                        String type;
                        if (ArduinoOTA.getCommand() == U_FLASH)
                            type = "sketch";
                        else // U_SPIFFS
                            type = "filesystem";

                        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
                        Serial.println("Start updating " + type);
                    })
        .onEnd([]()
                { Serial.println("\nEnd"); })
        .onProgress([](unsigned int progress, unsigned int total)
                    { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
        .onError([](ota_error_t error)
                    {
                        Serial.printf("Error[%u]: ", error);
                        if (error == OTA_AUTH_ERROR)
                            Serial.println("Auth Failed");
                        else if (error == OTA_BEGIN_ERROR)
                            Serial.println("Begin Failed");
                        else if (error == OTA_CONNECT_ERROR)
                            Serial.println("Connect Failed");
                        else if (error == OTA_RECEIVE_ERROR)
                            Serial.println("Receive Failed");
                        else if (error == OTA_END_ERROR)
                            Serial.println("End Failed");
                    });

    ArduinoOTA.begin();
    Serial.println( "Arduino OTA ready");
}
