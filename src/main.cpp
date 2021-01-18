#include <Arduino.h>
#include <Log.h>
#include <MqttSerial.h>
#include <limero.h>

#define PIN_LED PF_3

class LedBlinker : public Actor {
  uint32_t _pin;
  bool _on;

public:
  Sink<bool> blinkSlow;
  TimerSource blinkTimer;

  LedBlinker(Thread &thr, uint32_t pin, uint32_t delay)
      : Actor(thr), blinkSlow(1), blinkTimer(thr, delay, true) {
    _pin = pin;
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, 1);

    blinkSlow.sync([&](bool flag) {
      if (flag)
        blinkTimer.interval(500);
      else
        blinkTimer.interval(100);
    });

    blinkTimer >> [&](const TimerMsg &) {
      digitalWrite(_pin, _on);
      _on = !_on;
    };
  }
};

Thread mainThread("main");           // single thread arduino
MqttSerial mqtt(mainThread, Serial); // serial mqtt output powered by mainThread
void serialEvent() { mqtt.onRxd(&mqtt); }; // capture serial data incoming
TimerSource tick(mainThread, 1000, true);  // create a timer event every 1 sec
ValueFlow<int> x;                        // source of integers
Sink<int> y(3);                           // sink for integers , buffer for 3 of them 
LambdaSource<uint64_t> systemTime([]() { return millis(); });
LedBlinker led(mainThread, PIN_LED, 100);

void setup() {
  Serial.begin(115200);
  Serial.println(" Limero MQTTSerial started.");
  Sys::hostname("lm4f120");
  mqtt.init();
  // capture any change to x also in mqtt topic
   x == mqtt.topic<int>("system/x");
  mqtt.fromTopic<int>("system/y") >> y;
// handle async the updates to y
  y.async(mainThread,[](const int &yNew) { INFO(" received %d ", yNew); });

  systemTime >> mqtt.toTopic<uint64_t>("system/upTime");
  // at tick of timer , do increment, report millis()
  tick >> [&](const TimerMsg &tm) {
    x = x() + 1;
    systemTime.request();
  };
  // blink slower when connected
  mqtt.connected >> led.blinkSlow;
}

void loop() { mainThread.loop(); } // let the beast go.

extern "C" void _exit() {}
