// Copyright (c) 2018 Cirque Corp. Restrictions apply. See: www.cirque.com/sw-license


#include <Arduino.h> 
#include "I2C.h"
#include "Project_Config.h"

// ---------------- tuning ----------------
#define I2C_DELAY        5     // half clock period
#define I2C_NUM_CHANNELS    CONFIG_NUM_SENSORS  // one per trackpad
#define I2C_RX_BUFFER_SIZE  64    // must be >= largest report you read in one go
#define I2C_STRETCH_TIMEOUT 50000 // max microseconds to wait for slave clock stretching (Gen6 stretches while preparing a HID report)

// ---------------- pin map ----------------
// These come from Project_Config.h 
static const uint8_t sdaPins[] = {CONFIG_HOST_SDA0_PIN, CONFIG_HOST_SDA1_PIN, CONFIG_HOST_SDA2_PIN, CONFIG_HOST_SDA3_PIN};
static const uint8_t sclPins[] = {CONFIG_HOST_SCL0_PIN, CONFIG_HOST_SCL1_PIN, CONFIG_HOST_SCL2_PIN, CONFIG_HOST_SCL3_PIN};

// ---------------- per-channel state ----------------
static uint8_t  rxBuffer[I2C_RX_BUFFER_SIZE];
static uint8_t  rxIndex  = 0;
static uint8_t  rxLength = 0;


// report status
static i2c_status_t txStatus = I2C_SUCCESS;

static inline bool validChannel(uint8_t ch) { return ch < I2C_NUM_CHANNELS; }

// ---------------- line control (open drain) ----------------

static inline void sdaHigh(uint8_t ch) { pinMode(sdaPins[ch], INPUT); }
static inline void sdaLow (uint8_t ch) { pinMode(sdaPins[ch], OUTPUT); digitalWrite(sdaPins[ch], LOW); }
static inline void sclLow (uint8_t ch) { pinMode(sclPins[ch], OUTPUT); digitalWrite(sclPins[ch], LOW); }
static inline bool sdaRead(uint8_t ch) { pinMode(sdaPins[ch], INPUT); return digitalRead(sdaPins[ch]); }

//
static void sclHigh(uint8_t ch) {
  pinMode(sclPins[ch], INPUT);
  uint32_t waited = 0;
  while (digitalRead(sclPins[ch]) == LOW && waited < I2C_STRETCH_TIMEOUT) {
    delayMicroseconds(1);
    waited++;
  }
}

// ---------------- bus primitives ----------------
static void i2c_start(uint8_t ch) {

  sdaHigh(ch); delayMicroseconds(I2C_DELAY);   // let SDA rise BEFORE clocking
  sclHigh(ch); delayMicroseconds(I2C_DELAY);
  sdaLow(ch);  delayMicroseconds(I2C_DELAY);
  sclLow(ch);  delayMicroseconds(I2C_DELAY);
}

static void i2c_stop(uint8_t ch) {
  sdaLow(ch);  delayMicroseconds(I2C_DELAY);
  sclHigh(ch); delayMicroseconds(I2C_DELAY);
  sdaHigh(ch); delayMicroseconds(I2C_DELAY);
  delayMicroseconds(I2C_DELAY);
}

// Send 8 bits MSB first, then read the slave's ACK bit.
// Returns true if the slave ACKed (pulled SDA low).
static bool i2c_writeByte(uint8_t ch, uint8_t byte) {
  for (int8_t i = 7; i >= 0; i--) {
    if ((byte >> i) & 1) sdaHigh(ch); else sdaLow(ch);
    delayMicroseconds(I2C_DELAY);
    sclHigh(ch); delayMicroseconds(I2C_DELAY);   // slave samples SDA on this rising edge
    sclLow(ch);  delayMicroseconds(I2C_DELAY);
  }
  sdaHigh(ch);  delayMicroseconds(I2C_DELAY);    // release SDA so the slave can answer
  sclHigh(ch);  delayMicroseconds(I2C_DELAY);
  bool ack = !sdaRead(ch);                          // ACK = slave pulled SDA LOW
  sclLow(ch);   delayMicroseconds(I2C_DELAY);
  return ack;
}

// Read 8 bits MSB first, then send ACK (more bytes wanted) or NACK (last byte).
static uint8_t i2c_readByte(uint8_t ch, bool sendAck) {
  uint8_t byte = 0;
  sdaHigh(ch);                                      // release SDA; the slave drives it
  for (int8_t i = 7; i >= 0; i--) {
    sclHigh(ch); delayMicroseconds(I2C_DELAY);
    byte |= (sdaRead(ch) ? (1 << i) : 0);
    sclLow(ch);  delayMicroseconds(I2C_DELAY);
  }
  if (sendAck) sdaLow(ch); else sdaHigh(ch);
  delayMicroseconds(I2C_DELAY);
  sclHigh(ch); delayMicroseconds(I2C_DELAY);
  sclLow(ch);  delayMicroseconds(I2C_DELAY);
  sdaHigh(ch);
  return byte;
}

/************************************************************/
/********************  PUBLIC FUNCTIONS *********************/

//put both lines in the idle (released, high) state for this channel.
void I2C_init(uint8_t i2c_channel, uint32_t clockFrequency) {
  (void)clockFrequency;
  if (!validChannel(i2c_channel)) return;
  sdaHigh(i2c_channel);
  pinMode(sclPins[i2c_channel], INPUT);
}

//START + address byte in write mode. Records whether the device answered.
void I2C_beginTransmission(uint8_t i2c_channel, uint8_t address) {
  if (!validChannel(i2c_channel)) { txStatus = I2C_OTHER_ERROR; return; }
  txStatus = I2C_SUCCESS;
  i2c_start(i2c_channel);
  if (!i2c_writeByte(i2c_channel, (uint8_t)(address << 1))) {
    txStatus = I2C_ADDRESS_NACKED;   // nobody home at this address
  }
}

// Send one data byte. Returns 1 for "one byte written" to match Wire's contract.
uint8_t I2C_write(uint8_t i2c_channel, uint8_t data) {
  if (!validChannel(i2c_channel)) return 0;
  if (txStatus != I2C_SUCCESS) return 0;    // already failed; don't keep pushing
  if (!i2c_writeByte(i2c_channel, data)) {
    txStatus = I2C_DATA_NACKED;
    return 0;
  }
  return 1;
}


i2c_status_t I2C_endTransmission(uint8_t i2c_channel, bool stop) {
  if (!validChannel(i2c_channel)) return I2C_OTHER_ERROR;
  if (stop) i2c_stop(i2c_channel);
  return txStatus;
}

// START + address byte in read mode, then read 'count' bytes into rxBuffer.
// Returns the number of bytes actually read (0 if the device did not answer).
uint32_t I2C_request(uint8_t i2c_channel, uint8_t address, uint32_t count, bool stop) {
  rxIndex  = 0;
  rxLength = 0;
  if (!validChannel(i2c_channel)) return 0;

  i2c_start(i2c_channel);
  if (!i2c_writeByte(i2c_channel, (uint8_t)((address << 1) | 1))) {
    txStatus = I2C_ADDRESS_NACKED;
    i2c_stop(i2c_channel);
    return 0;                                // no device: return 0 bytes
  }

  rxLength = (count > I2C_RX_BUFFER_SIZE) ? I2C_RX_BUFFER_SIZE : (uint8_t)count;
  for (uint8_t i = 0; i < rxLength; i++) {
    //ACK every byte except the last, which gets a NACK to end the read.
    rxBuffer[i] = i2c_readByte(i2c_channel, i < (rxLength - 1));
  }

  if (stop) i2c_stop(i2c_channel);
  return rxLength;
}

uint32_t I2C_available(uint8_t i2c_channel) {
  (void)i2c_channel;
  return (uint32_t)(rxLength - rxIndex);
}

uint8_t I2C_read(uint8_t i2c_channel) {
  (void)i2c_channel;
  if (rxIndex < rxLength) return rxBuffer[rxIndex++];
  return 0;
}

uint8_t I2C_readBytes(uint8_t i2c_channel, uint8_t * buffer, size_t count) {
  uint8_t n = 0;
  while (n < count && I2C_available(i2c_channel)) {
    buffer[n++] = I2C_read(i2c_channel);
  }
  return n;
}

//Reports whether the fixed buffer is big enough. Increase I2C_RX_BUFFER_SIZE
//above if this returns false for the report size you need.
bool I2C_setBufferLength(uint32_t requiredBuffLength) {
  return requiredBuffLength <= I2C_RX_BUFFER_SIZE;
}
