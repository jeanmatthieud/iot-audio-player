/*
 Wtv020sd16p.h - Library to control a WTV020-SD-16P module to play voices from an Arduino board.
 Created by Diego J. Arevalo, August 6th, 2012.
 Released into the public domain.
 */

#ifndef Wtv020sd16p_h
#define Wtv020sd16p_h

typedef void (*busy_cb)(void);

class Wtv020sd16p
{
public:
  Wtv020sd16p(int resetPin, int clockPin, int dataPin, int busyPin, busy_cb busyCallback);
  void reset();
  void playVoice(int voiceNumber);
  void asyncPlayVoice(int voiceNumber);
  void stopVoice();
  void pauseVoice();
  void mute();
  void unmute();
  bool isBusy();
private:
  void sendCommand(unsigned int command);
  int _resetPin;
  int _clockPin;
  int _dataPin;
  int _busyPin;
  int _busyPinState;
};

#endif
