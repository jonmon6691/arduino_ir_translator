// IR_Relay.ino - Relays IR signals from universal remote to audio reciever

#define PIN_IR_RECV 2
#define PIN_IR_SEND 3

#define DT_US_STARTPULSE_MIN 8500
#define DT_US_STARTPULSE_MAX 9500
#define DT_US_DATAWORD 68000
#define DT_US_POSTSTART_REST_THRESHOLD 3450
#define DT_US_DATA_THRESHOLD 1175

#define BUF_LEN 128

#define AR_PREFIX 0x482C
#define AR_UP 0x482C40BF

#define AR_UNK_00 0x482C00FF
#define AR_EQ     0x482C10EF
#define AR_SELECT 0x482C20DF
#define AR_UNK_30 0x482C30CF
#define AR_UP     0x482C40BF
#define AR_UNK_50 0x482C50AF
#define AR_DOWN   0x482C609F
#define AR_UNK_70 0x482C708f
#define AR_POWER  0x482C807F
#define AR_MUTE   0x482C906F
#define AR_RIGHT  0x482CA05F
#define AR_UNK_B0 0x482CB04F
#define AR_LEFT   0x482CC03F
#define AR_UNK_D0 0x482CD02F
#define AR_INPUT  0x482CE01F
#define AR_UNK_F0 0x482CF00F

#define TR_VOL_UP 0x5EA158A7
#define TR_VOL_DOWN 0x5EA1D827
#define TR_MUTE 0x5EA138C7
#define TR_CHAN_UP 0xFE808778
#define TR_CHAN_DOWN 0xFE80A758

volatile unsigned long ts_ms_led_off; // Half-second timer for the indicator light

volatile unsigned long ts_us_rise; // microsecond timestamp of the rising edge
volatile unsigned long ts_us_fall; // microsecond timestamp of the falling edge
volatile unsigned long ts_us_endword; // 68000 microsecond timer for the length of a dataword

volatile unsigned long dt_us_mark; // Length of on pulse
volatile unsigned long dt_us_space; // Length of off pulse (only valud suring dataword timer

volatile bool new_start; // Flag that a new start pulse has been detected

volatile enum {
  IRR_IDLE,
  IRR_POSTSTART,
  IRR_DATA
} IR_recv_state;

volatile unsigned long packet;

void IR_ISA() {
  // Only make one call to micros(), save the result for this routine
  unsigned long ts_us_ISA = micros();

  // Logic is inverted, LOW means IR is on
  switch (digitalRead(PIN_IR_RECV)) {
  case LOW: // IR Light on
    ts_us_rise = ts_us_ISA;
    if (ts_us_ISA < ts_us_endword) {
      dt_us_space = ts_us_ISA - ts_us_fall;
      switch (IR_recv_state) {
      case IRR_POSTSTART:
        if (dt_us_space < DT_US_POSTSTART_REST_THRESHOLD) {
          ts_us_endword = ts_us_ISA;
          IR_recv_state = IRR_IDLE;
        } else {
          IR_recv_state = IRR_DATA;
        }
        break;
      case IRR_DATA:
        if (dt_us_space < DT_US_DATA_THRESHOLD) {
          packet = packet << 1;
        } else {
          packet = (packet << 1) + 1;
        }
        break;
      }
    } else {
      IR_recv_state = IRR_IDLE;
    }
    break;
    
  case HIGH: // IR light off
    dt_us_mark = ts_us_ISA - ts_us_rise;
    ts_us_fall = ts_us_ISA;
    if (dt_us_mark > DT_US_STARTPULSE_MIN && dt_us_mark < DT_US_STARTPULSE_MAX) {
      packet = 0;
      ts_us_endword = ts_us_rise + DT_US_DATAWORD;
      ts_us_fall = ts_us_ISA;
      new_start = true;
      IR_recv_state = IRR_POSTSTART;
    }
    break;
  }
}

void sendNEC(unsigned long code) {
  pinMode(PIN_IR_SEND, OUTPUT);
  delayMicroseconds(9000);
  pinMode(PIN_IR_SEND, INPUT);
  delayMicroseconds(4500);
  for (unsigned long mask = 1UL << 31; mask; mask >>= 1) {
    pinMode(PIN_IR_SEND, OUTPUT);
    delayMicroseconds(600);
    pinMode(PIN_IR_SEND, INPUT);
    if (code & mask) {
      delayMicroseconds(1650);
    } else {
      delayMicroseconds(550);
    }
  }
  pinMode(PIN_IR_SEND, OUTPUT);
  delayMicroseconds(600);
  pinMode(PIN_IR_SEND, INPUT);
}

void sendNEC_repeat(void) {
  pinMode(PIN_IR_SEND, OUTPUT);
  delayMicroseconds(9000);
  pinMode(PIN_IR_SEND, INPUT);
  delayMicroseconds(2200);
  pinMode(PIN_IR_SEND, OUTPUT);
  delayMicroseconds(600);
  pinMode(PIN_IR_SEND, INPUT);
}

void setup() {
  Serial.begin(9600);
  new_start = false;
  IR_recv_state = IRR_IDLE;
  digitalWrite(PIN_IR_SEND, LOW); // Act as open collector
  pinMode(PIN_IR_SEND, INPUT); // xternal pullup
  pinMode(PIN_IR_RECV, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  
  attachInterrupt(digitalPinToInterrupt(PIN_IR_RECV), IR_ISA, CHANGE);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (micros() > ts_us_endword && new_start) {
    detachInterrupt(digitalPinToInterrupt(PIN_IR_RECV));
    new_start = false;
    Serial.println(packet, HEX);
    switch (packet) {
    case TR_VOL_UP:
      sendNEC(AR_UP);
      ts_ms_led_off = millis() + 500;
      digitalWrite(LED_BUILTIN, HIGH);
      break;
    case TR_VOL_DOWN:
      sendNEC(AR_DOWN);
      ts_ms_led_off = millis() + 500;
      digitalWrite(LED_BUILTIN, HIGH);
      break;
    case TR_MUTE:
      sendNEC(AR_POWER);
      ts_ms_led_off = millis() + 500;
      digitalWrite(LED_BUILTIN, HIGH);
      break;
    case TR_CHAN_DOWN:
      sendNEC(AR_INPUT);
      ts_ms_led_off = millis() + 500;
      digitalWrite(LED_BUILTIN, HIGH);
      break;
    case TR_CHAN_UP:
      sendNEC(AR_UNK_F0);
      ts_ms_led_off = millis() + 500;
      digitalWrite(LED_BUILTIN, HIGH);
      break;
    case 0:
      sendNEC_repeat();
      break;
    default:
      if (packet >> 16 == AR_PREFIX) {
        sendNEC(packet);
        ts_ms_led_off = millis() + 500;
        digitalWrite(LED_BUILTIN, HIGH);
        break;
      }
    }
    attachInterrupt(digitalPinToInterrupt(PIN_IR_RECV), IR_ISA, CHANGE);
  }
  
  if (millis() > ts_ms_led_off) {
    digitalWrite(LED_BUILTIN, LOW);
  }
}
