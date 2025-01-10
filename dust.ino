#include <RCSwitch.h>
#include "powermeter.h"


// #define __DEBUG_POWER__

/* `current` probe fo the dust extractor  */
#define DUST_EXT_A_INPUT_PIN A0
#define DUST_EXT_threshold 1
#define DUST_EXT_STATUS_OUTPUT_PIN 9

#define RF_RECEIVE_PIN 3
#define RF_SEND_PIN 2

#define MANUAL_SW_PIN 4
#define MANUAL_SW_STATUS_OUTPUT_PIN 18

#define NB_MACHINES 3

#define RADIAL_SAW_GATE_OUTPUT_PIN 19
#define RADIAL_SAW_STATUS_INPUT_PIN 5
#define RADIAL_SAW_A_INPUT_PIN A1

#define RADIAL_SAW_RF_LED_PIN 13
#define RADIAL_SAW_RF_ON_CODE 6599000ul
#define RADIAL_SAW_RF_OFF_CODE 6598996ul


#define BAND_SAW_GATE_OUTPUT_PIN 20
#define BAND_SAW_STATUS_INPUT_PIN 6
#define BAND_SAW_A_INPUT_PIN A2

#define PT_GATE_OUTPUT_PIN 21
#define PT_STATUS_INPUT_PIN 7
#define PT_A_INPUT_PIN A3

#define NONE 0

#define _20A 20

class PowerMeter;

class Machine {
public:
    String name;
    /* digital output pin to control the gate */
    pin_size_t gateOutputPin;
    /* digital input pin the gate send the status back */
    pin_size_t statusInputPin;
    /* graceful delay after the machine stopped */
    unsigned long graceful_delay;
    /* */
    unsigned long close_at_millis;
    bool isRunning;
    bool isGateOpen;
    bool delayOverflow;
    bool gracefulStop;
    bool rfStatus;
    pin_size_t rfStatusPin;
    unsigned long rfOnCode;
    unsigned long rfOffCode;
    PowerMeter powermeter;

    Machine(
        String name,
        pin_size_t gateOutputPin,
        pin_size_t statusInputPin,
        pin_size_t powermeterPin,
        double threshold,
        unsigned long graceful_delay,
        unsigned long rfOnCode,
        unsigned long rfOffCode,
        pin_size_t rfPin
    )
        : name(name),
          gateOutputPin(gateOutputPin),
          statusInputPin(statusInputPin),
          graceful_delay(graceful_delay),
          rfStatusPin (rfPin),
          rfOnCode(rfOnCode),
          rfOffCode(rfOffCode),
          powermeter(PowerMeter(powermeterPin, _20A, threshold)
                    ) {
        isRunning = false;
        rfStatus = false;
        isGateOpen = false;
        delayOverflow = false;
    }

};

PowerMeter dustPower = PowerMeter(DUST_EXT_A_INPUT_PIN, _20A, DUST_EXT_threshold);


Machine machines[NB_MACHINES] = {
    // Scie à onglet, long graceful delay
    Machine( "Scie à onglet",
             RADIAL_SAW_GATE_OUTPUT_PIN,
             RADIAL_SAW_STATUS_INPUT_PIN,
             RADIAL_SAW_A_INPUT_PIN,
             0.5, 10000,
             RADIAL_SAW_RF_ON_CODE,
             RADIAL_SAW_RF_OFF_CODE,
             RADIAL_SAW_RF_LED_PIN
           ),
    // Band saw
    Machine( "Scie à ruban ",
             BAND_SAW_GATE_OUTPUT_PIN,
             BAND_SAW_STATUS_INPUT_PIN,
             BAND_SAW_A_INPUT_PIN,
             0.5, 3000,
             NONE, NONE, NONE),
    // PT
    Machine("Rabo-Degau   ",
            PT_GATE_OUTPUT_PIN,
            PT_STATUS_INPUT_PIN,
            PT_A_INPUT_PIN,
            0.5, 3000,
            NONE, NONE, NONE )
};

// RF transmitter
RCSwitch myReceiveSwitch = RCSwitch();
RCSwitch mySendSwitch = RCSwitch();

int i;

byte nbRunning = 0;
byte nbFullyOpen = 0;

bool isStopping = false;;
unsigned long stopAs = 0;
unsigned long now;

uint8_t manualSwitch = LOW;


void setPIN(pin_size_t pin, bool status) {
    digitalWrite(pin, status ? HIGH : LOW);
}

/*

Laguna ON
Decimal: 994881 (24Bit)
Binary: 000011110010111001000001
Tri-State: not applicable
PulseLength: 288 microseconds
Protocol: 1
Raw data: 8940,300,876,288,884,284,892,284,888,848,344,824,356,812,364,804,368,220,936,252,916,836,348,236,924,836,344,820,360,808,368,220,936,248,916,840,348,232,928,256,912,272,900,272,892,280,896,848,340,


Laguna OFF
Decimal: 994882 (24Bit)
Binary: 000011110010111001000010
Tri-State: not applicable
PulseLength: 289 microseconds
Protocol: 1
Raw data: 8968,292,884,288,888,280,892,276,896,848,340,816,356,812,368,804,368,216,940,248,916,840,344,240,924,836,348,816,360,808,364,224,936,248,916,840,348,232,924,260,908,268,904,272,896,848,340,240,920,
*/

// REAL LAGUNA CODES
#define START_LAGUNA_CODE 994881ul
#define STOP_LAGUNA_CODE 994882ul

// ML-FAN FAKE LAGUNA CODES
// #define START_LAGUNA_CODE 1512240ul
// #define STOP_LAGUNA_CODE 1853712ul

#define LAGUNA_CODE_LENGTH 24

void startDustExtraction() {
    if (!dustPower.isRunning()) {
        Serial.println("                                          Start dust extraction");
        mySendSwitch.send(START_LAGUNA_CODE, LAGUNA_CODE_LENGTH);
    }
}

void stopDustExtraction() {
    if (dustPower.isRunning()) {
        Serial.println("                                          Stop dust extraction");
        mySendSwitch.send(STOP_LAGUNA_CODE, LAGUNA_CODE_LENGTH);
    }
}




void doBlink(pin_size_t pin, int onDuration, int offDuration, int count) {
    for (int i = 0; i< count; i++) {
        digitalWrite(pin, HIGH);
        delay(onDuration);
        digitalWrite(pin, LOW);
        delay(offDuration);
    }
}


void initLed(pin_size_t pin) {
    pinMode(pin, OUTPUT);
    doBlink(pin, 300, 200, 3);
}

int hasError = 0b00000000;

void setup() {
    Serial.begin(9600);

    Serial.println("                                      RESTARTING");

    // Init RF Receiver
    myReceiveSwitch.enableReceive(digitalPinToInterrupt(RF_RECEIVE_PIN));

    // Init RF Sender
    mySendSwitch.setProtocol(1);
    mySendSwitch.enableTransmit(RF_SEND_PIN);
    // mySendSwitch.setPulseLength(320);

    // Dust extractor pins
    dustPower.init();
    pinMode(DUST_EXT_A_INPUT_PIN, INPUT);
    initLed(DUST_EXT_STATUS_OUTPUT_PIN);

    // Manual switch pins
    pinMode(MANUAL_SW_PIN, INPUT_PULLUP);
    initLed(MANUAL_SW_STATUS_OUTPUT_PIN);

    // Machine pins
    for (i = 0; i < NB_MACHINES; i++) {
        Machine *m = &machines[i];

        m->powermeter.init();

        pinMode(m->statusInputPin, INPUT);
        pinMode(m->gateOutputPin, OUTPUT);

        if (m->rfStatusPin) {
            initLed(m->rfStatusPin);
        }

        // Open the gate
        digitalWrite(m->gateOutputPin, HIGH);
        int counter = 0;
        bool ok = false;

        // wait gate has been reported open
        do {
            if (digitalRead(m->statusInputPin)) {
                // Gate reported open
                ok = true;
                delay(1000);
            } else {
                // not open yet, let's wait
                delay(250);
                counter++;
            }
        } while (counter < 15 && !ok);


        if (!ok) {
            // toggle
            hasError += (1 << i);
            // blink the green light
            pinMode(m->statusInputPin, OUTPUT);
            doBlink(m->statusInputPin, 200, 100, 30);
            pinMode(m->statusInputPin, INPUT);
        }

        digitalWrite(m->gateOutputPin, LOW);


        m->close_at_millis = 0;
        m->isRunning = false;
        m->isGateOpen = false;
        m->delayOverflow = false;
    }
}

void rfReceive() {
    if (myReceiveSwitch.available()) {
        unsigned long value = myReceiveSwitch.getReceivedValue();
        // unsigned int length = myReceiveSwitch.getReceivedBitlength();
        Serial.print("Receive RF: ");
        Serial.println(value);

        bool catched = false;
        if (value == START_LAGUNA_CODE || value == STOP_LAGUNA_CODE) {
            Serial.println(" => LAGUNA CODE");
            catched = true;
        }

        // Check machine codes
        for (i = 0; i < NB_MACHINES; i++) {
            Machine* m = &machines[i];

            if (m->rfOnCode && m->rfOnCode == value) {
                Serial.print("Start ");
                Serial.println(m->name);
                m->rfStatus = true;
                catched = true;
            }

            if (m->rfOffCode && m->rfOffCode == value) {
                Serial.print("Stop ");
                Serial.println(m->name);
                m->rfStatus = false;
                catched = true;
            }
        }

        if (!catched) {
            Serial.print("Alien Code ");
            output(myReceiveSwitch.getReceivedValue(), myReceiveSwitch.getReceivedBitlength(), myReceiveSwitch.getReceivedDelay(), myReceiveSwitch.getReceivedRawdata(), myReceiveSwitch.getReceivedProtocol());
        }

        myReceiveSwitch.resetAvailable();
    }
}

void loop() {
    rfReceive();

    if (hasError) {
        doBlink(MANUAL_SW_STATUS_OUTPUT_PIN, 200, 100, 3);
        delay(500);

        doBlink(MANUAL_SW_STATUS_OUTPUT_PIN, 200, 100, hasError);
    }

    now = millis();

    // Detect if extractor is running
    dustPower.update();

    // HACK: uncomment once CT
    setPIN(DUST_EXT_STATUS_OUTPUT_PIN, dustPower.isRunning());
    // END HACK

    manualSwitch = !digitalRead(MANUAL_SW_PIN);
    setPIN(MANUAL_SW_STATUS_OUTPUT_PIN, manualSwitch);

    // count fully open gates
    nbFullyOpen = 0;
    // count running machines
    nbRunning = 0;

    stopAs = 0;
    isStopping = false;

    // Step 1: open gate
    for (i = 0; i < NB_MACHINES; i++) {
        Machine* m = &machines[i];

        // int machineInput_mV = readMv(m->currentAInputPin);
        m->powermeter.update();
        bool newIsRunning = m->rfStatus || m->powermeter.isRunning();

        m->isGateOpen = digitalRead(m->statusInputPin);

        if (m->isGateOpen) {
            nbFullyOpen += 1;
        }

        if (newIsRunning) {
            nbRunning += 1;
        }

        if (m->rfStatusPin) {
            setPIN(m->rfStatusPin, m->rfStatus);
        }

        if (!m->isRunning && newIsRunning) {
            // just started
            m->isRunning = true;
            m->close_at_millis = 0;
            // Open gate
            setPIN(m->gateOutputPin, true);
            Serial.print("OPEN ");
            Serial.print(m->name);
            Serial.println("gate");

        } else if (m->isRunning && !newIsRunning) {
            // just stopped -> start graceful delay
            m->isRunning = false;
            m->gracefulStop = true;
            m->close_at_millis = now + m->graceful_delay;
            m->delayOverflow = m->graceful_delay < now;
            Serial.print("Stop ");
            Serial.print(m->name);
            Serial.println("gracefully");
        }

        if (m->gracefulStop) {
            isStopping = true;
            stopAs = max(stopAs, m->close_at_millis);
        }
    }

    // Step 2: start dust extractor
    if ((nbRunning && nbFullyOpen) || manualSwitch) {
        // make sure to start dust extractor once at least one gate is open
        startDustExtraction();
    }


#ifdef __DEBUG_POWER__
    dustPower.logHeaders();
    dustPower.log("Laguna       ");
    for (i = 0; i < NB_MACHINES; i++) {
        machines[i].powermeter.log(machines[i].name);
    }
    Serial.print("Manuel       \t");
    Serial.println(manualSwitch);
#endif

    // step 3: stop dust extractorA
#ifdef __DEBUG__
    Serial.print("DUST running ");
    Serial.println(dustPower.isRunning());

    Serial.print("Switch ");
    Serial.println(manualSwitch);

    Serial.print("StopAs ");
    Serial.println(stopAs);

    Serial.print("NbRunning ");
    Serial.println(nbRunning);

    Serial.print("now ");
    Serial.println(now);
#endif

    if (!manualSwitch  // the manual switch is off
            && (
                (!isStopping && nbRunning == 0) // not stopping but no machine running
                ||
                (isStopping && stopAs < now)) // or stopping and delay reached
       )
    {
        stopDustExtraction();
    }

    // Step 4. close gates
    for (i = 0; i < NB_MACHINES; i++) {

        // Do not close the gate if :
        //  - the dust extractor is still running
        //  - it's the last one open
        //  - the manual switch is not toggled

        if (dustPower.isRunning() && nbFullyOpen == 1 && !manualSwitch) {
            // not possible to close more gateS
            Serial.println("Wait for the dust extractor to stop to close gate");
            break;
        }

        Machine* m = &machines[i];
        if (m->gracefulStop && m->close_at_millis < now) {
            // end of graceful delay
            m->gracefulStop = false;
            m->close_at_millis = 0;

            Serial.print("Stop ");
            Serial.print(m->name);
            Serial.println("NOW");
            setPIN(m->gateOutputPin, false);
        }

    }

    delay(250);
}
