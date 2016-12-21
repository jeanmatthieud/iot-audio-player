# Platform
Expressif
-  Arduino/ESP8266@2.1.0

# Libraries
- PubSubClient@2.6
- WiFiManager@0.12
- Json@5.6.2

# API
## Messages
Endpoint: `/v1.0/messages/[chip-id]`
Example of received message: `{"sensor": "audio-output", "busy": true}`

## Actions
Endpoint: `/v1.0/actions/[chip-id]`

### Play
Example: `{ "name": "play", "parameters": { "track": 0 } }`, `{ "name": "play", "parameters": { "track": 1 } }`

### Pause
Send: `{ "name": "pause" }`

### Mute
Send: `{ "name": "mute" }`

### Unmute
Send: `{ "name": "unmute"}`

## Volume
Send: `{ "name": "volume", "parameters": { "value": 0 }}` to `{ "name": "volume", "parameters": { "value": 7 }}`

## Pulse
Send: `{ "name": "pulse", "parameters": { "duration": 2000 }}`

Duration is in ms :

* 

## Errors
Endpoint: `/v1.0/errors/[chip-id]`
