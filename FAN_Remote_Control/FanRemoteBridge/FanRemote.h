#pragma once
#include <Arduino.h>
#include "RCSwitchStretch.h"

class FanRemote {
public:
    // Room A-D   (old names in comments)
    enum Room : uint8_t {
        ROOM_A,   
        ROOM_B,   
        ROOM_C,   
        ROOM_D    
    };

    FanRemote(uint8_t txPin,
              uint16_t pulseLen  = 350,
              uint8_t  protocol  = 1,
              uint8_t  repeats   = 20);

    void send(Room room, uint8_t button);   // button 1-14

private:
    struct RemoteButton { uint32_t value; };

    void transmit(const RemoteButton* codes,
                  uint8_t button,
                  const char* roomName);

    static const bool          NEEDS_F[14];
    static const RemoteButton  roomACodes[14]; 
    static const RemoteButton  roomBCodes[14]; 
    static const RemoteButton  roomCCodes[14]; 
    static const RemoteButton  roomDCodes[14]; 

    RCSwitch mySwitch;
    const uint8_t BIT_LENGTH = 29;
};
