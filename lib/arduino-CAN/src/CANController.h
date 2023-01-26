// Copyright (c) Sandeep Mistry. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef CAN_CONTROLLER_H
#define CAN_CONTROLLER_H

#include <Arduino.h>

class CANControllerClass/* : public Stream */{

public:

  
  #pragma pack(push, 1)
  struct packet_t
  {
    uint32_t address;
    bool extended;
    bool rtr;
    uint8_t dlc;
    uint8_t length;
    uint8_t data[8];

    // Оператор копирования нужен?
  };
  #pragma pack(pop)

  //typedef std::function<void(int length)> on_receive_t;
  typedef std::function<void(packet_t &packet)> on_receive_t;


  int begin(long baudRate);
  void end();

  int beginPacket(int id, int dlc = -1, bool rtr = false);
  int beginExtendedPacket(long id, int dlc = -1, bool rtr = false);
  int endPacket();

  int parsePacket();
  long packetId();
  bool packetExtended();
  bool packetRtr();
  int packetDlc();

  // from Print
  size_t write(uint8_t byte);
  size_t write(const uint8_t *buffer, size_t size);

  // from Stream
  int available();
  int read();
  int peek();
  //void flush();

  void onReceive(on_receive_t);
  //void onReceive(void(*callback)(int));

  //int filter(int id) { return filter(id, 0x7ff); }
  //int filter(int id, int mask);
  //int filterExtended(long id) { return filterExtended(id, 0x1fffffff); }
  //int filterExtended(long id, long mask);

  //int observe();
  //int loopback();
  //int sleep();
  //int wakeup();

  bool SendPacket(packet_t packet);

  protected:
  CANControllerClass();
  ~CANControllerClass();

protected:
  on_receive_t _onReceive;
  //void (*_onReceive)(int);

  bool _packetBegun;
  long _txId;
  bool _txExtended;
  bool _txRtr;
  int _txDlc;
  int _txLength;
  uint8_t _txData[8];

  long _rxId;
  bool _rxExtended;
  bool _rxRtr;
  int _rxDlc;
  int _rxLength;
  int _rxIndex;
  uint8_t _rxData[8];
};

#endif
