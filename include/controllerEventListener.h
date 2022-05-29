#ifndef CONTROLLER_EVENT_LISTENER_INTERFACE_H
#define CONTROLLER_EVENT_LISTENER_INTERFACE_H

#include "event.h"

class ControllerEventListenerInterface {
    public:
        virtual void receiveIoTFrameworkEvent( Event event) {};
};

#endif