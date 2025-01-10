#include "powermeter.h"

#define  ADC_BITS   10
// counts = 2^10
#define ADC_COUNTS (1 << ADC_BITS)

#define SQRT_2 1.414213562

#define ABS_97_mV 20
#define ABS_4897_mV 1002


// #define PRINT_WAVE 1


/** constructor */
PowerMeter::PowerMeter(pin_size_t analogPin, double i_perVolt, double amps_threshold) : aPin(analogPin), i_perVolt(i_perVolt), amps_threshold(amps_threshold) {
    refZero = ADC_COUNTS >> 1;

    sample = 0;
    minSample = 1023;
    maxSample = 0;

    sum = 0;
    filtered = 0;
    _isRunning = false;
}


void PowerMeter::init() {
    pinMode(this->aPin, INPUT);
}

/**
 * Update U rms
 */
void PowerMeter::updateU(unsigned int nbSamples) {

    /*
     * Detect edge case (to avoid time-consuming sampling)
     */
    sample = analogRead(aPin);

    // below minimum wave sine => toggled off
    if (sample < ABS_97_mV) {
        this->U_rms = 0;
        return;
    }

    // above maximum wave sine => toggled on
    if (sample > ABS_4897_mV) {
        this->U_rms = U_MAX;
        return;
    }

    int sMin = 1023;
    int sMax = 0;

#ifdef PRINT_WAVE
    int samples[nbSamples];
#endif


    // theory reads Urms = Umax / sqrt(2)
    // too much noise to use as-is
    // Let's sample the wave and compute root-mean-square (RMS)
    sum = 0;
    for (unsigned i = 0; i < nbSamples; i++) {
        sample = analogRead(aPin);

#ifdef PRINT_WAVE
        samples[i] = sample;
#endif

        sMin = min(sMin, sample);
        sMax = max(sMax, sample);

        // auto adjust zero reference
        refZero = (refZero + (sample-refZero)/1024);
        filtered = sample - refZero;
        sum += filtered * filtered;
    }


    // recheck bounds (has been switched while sampling the wave?)

    // below minimum wave sine => toggled off
    if (sMin < ABS_97_mV) { // < 100 mV
        this->U_rms = 0;
        return;
    }

    // above maximum wave sine => toggled on
    if (sMax > ABS_4897_mV) { // > ~4.9V
        this->U_rms = U_MAX;
        return;
    }

#ifdef PRINT_WAVE
    for (unsigned i = 0; i < nbSamples; i++) {
        Serial.print(samples[i]);
        Serial.print(", ");
    }
    Serial.println();
#endif

    // should add a calibration factor as in EnomLib.h
    // but not required here
    this->U_rms = map( sqrt(sum / nbSamples), 0, 1023, 0, 5000);

    // store min and max for history
    minSample = min(sMin, minSample);
    maxSample = max(sMax, maxSample);
}

void PowerMeter::update(unsigned int nbSamples) {
    // compute U rms
    this->updateU(nbSamples);

    // compute Imain
    this->amps = this->U_rms / 1000 * this->i_perVolt;
    this->_isRunning = this->amps > this->amps_threshold;
}

void PowerMeter::update() {
    this->update(NB_SAMPLES);
}


void PowerMeter::forceIsRunning(bool n) {
    this->_isRunning = n;
}

bool PowerMeter::isRunning() {
    return this->_isRunning;
}

void PowerMeter::logHeaders() {
    Serial.println("Name         \tU rms\tAmps\tThres.\tRunning\tZero\tmin\tmax ever");
}

void PowerMeter::log(String name) {
    Serial.print(name);
    Serial.print('\t');

    Serial.print(this->U_rms, 2);
    Serial.print('\t');

    Serial.print(this->amps, 2);
    Serial.print('\t');

    Serial.print(this->amps_threshold, 2);
    Serial.print('\t');

    Serial.print(this->_isRunning ? "yes" : "--");
    Serial.print('\t');

    Serial.print(this->refZero);
    Serial.print('\t');

    Serial.print(this->minSample);
    Serial.print('\t');
    Serial.print(this->maxSample);

    Serial.println("");
}