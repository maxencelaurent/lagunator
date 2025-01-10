#ifndef POWER_METER_H
#define POWER_METER_H

#include <Arduino.h>

/**
 * Let's read power consumption
 *
 * mosty inspired by <EmonLib.h> (which doesn't work on Nano Every boards...)
 *
 *
 * Sensor generates sine wave centered on 2.5 V
 *  - `U_rms` depends on main current `I_main`
 *  - 1 volt RMS represents `i_perVolt` amps (ie 20A with YHDC SCT013-020)
 *  - U_
 *
 *   |
 *   |
 *   |   _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ U_max                 ( ~ 3.9 @ Imain = 20A )
 *   |         ,-'''-.
 *   |      ,-'........-..................U_rms = U_max / √2     ( 3.5 V @ Imain = 20A )
 *   |    ,'             `.
 *   |  ,'                 `.
 *   | /                     \
 *   |/                       \
 *  -|-------------------------\--------------------------       ( relative zero = 2.5V)
 *   |                          \                       /
 *   |                           \                     /
 *   |                            `.                 ,'
 *   |                              `.             ,'
 *   |                                `-.       ,-' ----- -U_rms            = 1.5V
 *   |                        _ _ _ _ _ _`-,,,-'_ _ _ _ _ _ -U_max          = 1.08 V
 *   |
 *   |
 *
 *
 *
 * ACK. : Sensor generates a "pure" sine wave -1 +1 V.
 *      *****************************************
 *      *  NEGATIVE TENSION WILL DAMAGES BOARDS *
 *      *****************************************
 *
 * This simple circuit shifts the wave within acceptable bounds (ie 0 +5V)
 *
 *                                       o------------- 5V
 *                                       |
 *                                      | | R1 = 10k
 *                                      | |
 *          ↓                            |
 *          ↓                       +    |
 *          ↓ ⫖------------o------||-----o------------- Analog Pin
 *          ↓ ⫖     |            10µF    |
 *   i_main ↓ ⫖    | |                  | | R2 = R1
 *          ↓ ⫖    |_|                  |_|
 *          ↓ ⫖     |                    |
 *          ↓ ⫖------------o-------------o------------- GND
 *          ↓
 *          ↓
 *
 *       [YHDC SCT013-020]     [ THE CIRCUIT ]
 *
 *
 *
 * This one adds an ON-ON-OFF switch with a pull-down resistor
 *
 *                                       o------------- 5V
 *                                       |
 *                                      | | R1 = 10k
 *                                      | |
 *          ↓                            |
 *          ↓                       +    |                  5V --\
 *          ↓ ⫖------------o------||-----o----------------------- \----0------- Analog Pin
 *          ↓ ⫖     |            10µF    |            floating --       |
 *   i_main ↓ ⫖    | |                  | | R2 = 2*R1                  | | Pull-down resistor = R2
 *          ↓ ⫖    |_|                  |_|                            |_|
 *          ↓ ⫖     |                    |                              |
 *          ↓ ⫖------------o-------------o------------------------------o------- GND
 *          ↓
 *          ↓
 *
 *       [YHDC SCT013-020]     [ THE CIRCUIT ]
 *
 */

#define U_MAX 5000
#define NB_SAMPLES 1480u


class PowerMeter {

private:
    /** `current` probe input analog pin */
    pin_size_t aPin;
    double i_perVolt;
    double refZero;

    int sample;
    int minSample;
    int maxSample;

    double filtered;
    double sum;

    double U_rms;
    double amps_threshold;

    double amps;

    bool _isRunning;

    void updateU(unsigned int nbSamples);

public:
    /** constructor */
    PowerMeter(pin_size_t analogPin, double i_perVolt, double amps_threshold);
    void init();
    void update();
    void update(unsigned int nbSamples);
    bool isRunning();
    void forceIsRunning(bool newIsRunning);
    void logHeaders();
    void log(String name);
};

#endif

