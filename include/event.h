#include <Arduino.h>

#ifndef EVENT_H
#define EVENT_H

class Event {
    private: 
        String _payload;
        String _topic;
    public:
        Event( String aTopic, String aPayload) {
            _topic = aTopic;
            _payload = aPayload;
        }

        String getTopic() {
            return _topic;
        }

        String getPayload() {
            return _payload;
        }
};

#endif