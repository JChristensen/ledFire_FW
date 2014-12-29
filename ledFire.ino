//LED PWM Fire Effect
//by Jack Christensen Dec 2014
//
//Hardware design and circuit description at http://goo.gl/5VZZYZ
//This firmware available at http://goo.gl/VueLrD
//
//Set fuses: E:0xFF, H:0xD6, L:0x62 (same as factory settings, except 1.8V BOD)
//avrdude -p t84 -U lfuse:w:0x62:m -U hfuse:w:0xd6:m -U efuse:w:0xff:m -v
//
//"LED PWM Fire Effect" by Jack Christensen is licensed under CC BY-SA 4.0,
// http://creativecommons.org/licenses/by-sa/4.0/

#include <movingAvg.h>    //http://github.com/JChristensen/movingAvg
#include <avr/sleep.h>

class fireLED
{
    public:
        fireLED(uint8_t pin, uint8_t minDutyCycle, uint8_t maxDutyCycle, uint32_t minDelay, uint32_t maxDelay);
        void begin(void);
        void run(void);
    
    private:
        uint8_t _pin;
        uint8_t _minDutyCycle, _maxDutyCycle;
        uint32_t _minDelay, _maxDelay;
        uint32_t _lastChange;
        uint32_t _duration;
};

//constructor
fireLED::fireLED(uint8_t pin, uint8_t minDutyCycle, uint8_t maxDutyCycle, uint32_t minDelay, uint32_t maxDelay)
{
    _pin = pin;
    _minDutyCycle = minDutyCycle;
    _maxDutyCycle = maxDutyCycle;
    _minDelay = minDelay;
    _maxDelay = maxDelay;
}

//initialization
void fireLED::begin(void)
{
    pinMode(_pin, OUTPUT);
    _lastChange = millis();
    _duration = 0;
}

//run the LED
void fireLED::run(void)
{
    if ( millis() - _lastChange >= _duration ) {
        _lastChange += _duration;
        _duration = _minDelay + random(_maxDelay - _minDelay);
        analogWrite( _pin, _minDutyCycle + random(_maxDutyCycle - _minDutyCycle) );
    }
}

const uint8_t MIN_DUTY_CYCLE = 20;
const uint8_t MAX_DUTY_CYCLE = 255;
const uint32_t MIN_DELAY = 50;
const uint32_t MAX_DELAY = 500;

const uint8_t ledPin1 = 2;
const uint8_t ledPin2 = 3;
const uint8_t ledPin3 = 4;
const uint8_t ledPin4 = 5;
const uint8_t boostEnable = 7;
const uint8_t vSelectPin = 10;        //ground for 3.3V Vcc, tie to Vcc for 5V Vcc
const uint8_t unusedPins[] = { 0, 1, 6, 8, 9 };

int minVcc;
movingAvg Vcc;

fireLED led1(ledPin1, MIN_DUTY_CYCLE, MAX_DUTY_CYCLE, MIN_DELAY, MAX_DELAY);
fireLED led2(ledPin2, MIN_DUTY_CYCLE, MAX_DUTY_CYCLE, MIN_DELAY, MAX_DELAY);
fireLED led3(ledPin3, MIN_DUTY_CYCLE, MAX_DUTY_CYCLE, MIN_DELAY, MAX_DELAY);
fireLED led4(ledPin4, MIN_DUTY_CYCLE, MAX_DUTY_CYCLE, MIN_DELAY, MAX_DELAY);

void setup(void)
{
    pinMode(boostEnable, OUTPUT);
    digitalWrite(boostEnable, HIGH);
    minVcc = digitalRead(vSelectPin) ? 4500 : 3000;

    for (uint8_t i = 0; i < sizeof(unusedPins) / sizeof(unusedPins[0]); i++) {
        pinMode(i, INPUT_PULLUP);
    }
    led1.begin();
    led2.begin();
    led3.begin();
    led4.begin();
}

void loop(void)
{
    const uint32_t interval = 1000;
    static uint32_t lastRead;
    
    //Vcc measurement, if battery is low and regulator not able to maintain regulated voltage,
    //turn off LEDs, shut down the regulator, and go into power-down sleep mode forever or until a reset whichever comes first.
    if (millis() - lastRead >= interval) {
        lastRead += interval;
        int v = readVcc();
        int avgVcc = Vcc.reading(v);
        if (avgVcc < minVcc) {
            digitalWrite(ledPin1, LOW);
            digitalWrite(ledPin2, LOW);
            digitalWrite(ledPin3, LOW);
            digitalWrite(ledPin4, LOW);
            digitalWrite(boostEnable, LOW);
            gotoSleep();
        }
    }
    led1.run();
    led2.run();
    led3.run();
    led4.run();
}

//read 1.1V reference against Vcc
//from http://code.google.com/p/tinkerit/wiki/SecretVoltmeter
int readVcc(void)
{
    ADMUX = _BV(MUX5) | _BV(MUX0);
    delay(5);                                 //Vref settling time
    ADCSRA |= _BV(ADSC);                      //start conversion
    loop_until_bit_is_clear(ADCSRA, ADSC);    //wait for it to complete
    return 1126400L / ADC;                    //calculate AVcc in mV (1.1 * 1000 * 1024)
}

void gotoSleep(void)
{
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
//    MCUCR &= ~(_BV(ISC01) | _BV(ISC00));      //INT0 on low level
//    GIMSK |= _BV(INT0);                       //enable INT0
    byte adcsra = ADCSRA;                     //save ADCSRA
    ADCSRA &= ~_BV(ADEN);                     //disable ADC
    cli();                                    //stop interrupts to ensure the BOD timed sequence executes as required
    byte mcucr1 = MCUCR | _BV(BODS) | _BV(BODSE);  //turn off the brown-out detector
    byte mcucr2 = mcucr1 & ~_BV(BODSE);
    MCUCR = mcucr1;
    MCUCR = mcucr2;
//    sei();                                    //keep interrupts disabled, sleep until external reset or power-on reset
    sleep_cpu();                              //go to sleep
    sleep_disable();                          //wake up here
    ADCSRA = adcsra;                          //restore ADCSRA
}
