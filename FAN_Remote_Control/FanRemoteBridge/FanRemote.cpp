#include "FanRemote.h"

// ─── static tables ────────────────────────────────────────────
const bool FanRemote::NEEDS_F[14] = {
    false,true,false,true,false,true,true,false,true,true,true,false,true,false
};

// Room A 
const FanRemote::RemoteButton FanRemote::roomACodes[14] = {
    {483869160},{483869128},{483869097},{483869065},{483869034},{483869002},
    {483869073},{483868971},{483868790},{483869010},{483868939},{483868727},
    {483868821},{483868853}
};

// Room B
const FanRemote::RemoteButton FanRemote::roomBCodes[14] = {
    {534602216},{534602184},{534602153},{534602121},{534602090},{534602058},
    {534602129},{534602027},{534601846},{534602066},{534601995},{534601783},
    {534601877},{534601909}
};

// Room C 
const FanRemote::RemoteButton FanRemote::roomCCodes[14] = {
    {1524200},{1524168},{1524137},{1524105},{1524074},{1524042},
    {1524113},{1524011},{1523830},{1524050},{1523979},{1523767},
    {1523861},{1523893}
};

// Room D 
const FanRemote::RemoteButton FanRemote::roomDCodes[14] = {
    {526115304},{526115272},{526115241},{526115209},{526115178},{526115146},
    {526115217},{526115115},{526114934},{526115154},{526115083},{526114871},
    {526114965},{526114997}
};


// ──────────────────────────────────────────────────────────────

// ctor
FanRemote::FanRemote(uint8_t txPin,
                     uint16_t pulseLen,
                     uint8_t protocol,
                     uint8_t repeats)
{
    mySwitch.enableTransmit(txPin);
    mySwitch.setProtocol(protocol, pulseLen);
    mySwitch.setRepeatTransmit(repeats);
}

// public API
void FanRemote::send(Room room, uint8_t button)
{
    if (button == 0 || button > 14) {
        Serial.println(F("⛔ Invalid button")); return;
    }
    switch (room) {
        case ROOM_A: transmit(roomACodes, button, "Room A"); break; 
        case ROOM_B: transmit(roomBCodes, button, "Room B"); break; 
        case ROOM_C: transmit(roomCCodes, button, "Room C"); break;
        case ROOM_D: transmit(roomDCodes, button, "Room D"); break; 
        default:     Serial.println(F("⛔ Unknown room"));
    }
}

// helper
void FanRemote::transmit(const RemoteButton* codes,
                         uint8_t button,
                         const char* roomName)
{
    Serial.printf("→ %s, button %u\n", roomName, button);
    mySwitch.send(codes[button-1].value, BIT_LENGTH, NEEDS_F[button-1]);
    Serial.println(F("✓ Sent"));
}
