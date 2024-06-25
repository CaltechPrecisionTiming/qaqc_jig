/* Firmware to control the BTL QA/QC board at Caltech.
 * 
 * Anthony LaTorre <alatorre@caltech.edu>
 * Lautaro Narvaez <lautaro@caltech.edu> */
#include <Wire.h>
#include "PCA9557.h"
#include "AD5593R.h"
#include <errno.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <limits.h>
#include <SPI.h>

/* The actual 3.3 V reference for the Teensy. */
#define TEENSY_3V 3.3124

/* The voltage dropped across the BAP65-02 diode. */
#define BAP65_02_VOLTAGE_DROP 0.71

#define ETHERNET

#define LEN(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

//#define PCA9557_DEBUG

#define MAX_MSG_LENGTH 1024

/* Maximum number of stepper steps to take when trying to find home.
 *
 * FIXME: Need to determine an actual number for this. */
// An inductive proximity sensor is installed to anchor the home position
// Thus, no limit is set here
#define MAX_STEPS 1000000

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA
};
IPAddress ip(192, 168, 1, 177);

unsigned int localPort = 8888;      // local port to listen on

// buffers for receiving and sending data
char packetBuffer[UDP_TX_PACKET_MAX_SIZE];  // buffer to hold incoming packet,

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

/* Generic delay after setting pins high/low. */
#define DELAY 10

char cmd[MAX_MSG_LENGTH];
char err[MAX_MSG_LENGTH];
char msg[MAX_MSG_LENGTH];
int k = 0;

bool debug = false;

/* Which pin address each of the HV relays are connected to, i.e. the HV relay
 * labelled KC1 on the board is controlled by changing the output of pin 6. */
#define KC1 6
#define KC2 7
#define KC3 4
#define KC4 5
#define KC5 2
#define KC6 3
#define ADC 1
/* The I/O number for the AD5593R. */
#define THERMISTOR1 0
#define THERMISTOR2 1
#define THERMISTOR3 2
#define TEC_SENSE 3
#define TEC_CTRL1 4
#define TEC_CTRL2 5
#define TEC_CTRL3 6

/* Teensy outputs. */
#define PIN_MR1       (36)
#define PIN_MR2       (32)
#define PIN_ATT       (37)
#define PIN_THRU      (38)
/* Pins to set the step and microstep size. */
#define PIN_STP_FAULT (0)
#define PIN_STP_SLEEP (1)
#define PIN_STP_RESET (2)
#define PIN_STP_M2    (3)
#define PIN_STP_M1    (4)
#define PIN_STP_M0    (5)
#define PIN_STP_STEP  (6)
#define PIN_STP_DIR   (7)
#define PIN_DAC_CLEAR (8)
#define PIN_BI_SD     (9)
#define PIN_CS        (10)
#define PIN_MOSI      (11)
/* Analog pins. */
/* HV voltage and current. */
#define PIN_BIAS_IREAD (14)
#define PIN_BIAS_VREAD (15)
#define PIN_EXTMON_UC (24)
#define PIN_STP_HOME   (17)
/* Stepper enable. Goes through two limit switches first. */
#define PIN_STP_EN_UC  (23)

/* Array of HV relay pins and names. */
int hv_relays[6] = {KC1,KC2,KC4,KC3,KC6,KC5};
const char *hv_relay_names[6] = {"KC1","KC2","KC4","KC3","KC6","KC5"};
int tec_relays[3] = {TEC_CTRL1, TEC_CTRL2, TEC_CTRL3};
const char *tec_relay_names[3] = {"TEC_CTRL1", "TEC_CTRL2", "TEC_CTRL3"};
int thermistors[4] = {THERMISTOR1, THERMISTOR2, THERMISTOR3, TEC_SENSE};
const char *thermistor_names[4] = {"THERMISTOR1", "THERMISTOR2", "THERMISTOR3", "TEC_SENSE"};

/* Base address for the PCA9557. This base address is modified by the three
 * least significant bits set by the DIP switches on each board. */
#define BASE_ADDR 0x18

/* Note: The DIP switches are backwards from what you'd think, i.e. if the
 * switches are in position 0,0,1, then you need to talk to address 0b100. I've
 * ordered the list here so they behave as expected, i.e. if the switches are
 * in position 0,0,1, then you should use bus[1]. */
PCA9557 bus[8] = {
    PCA9557(BASE_ADDR, &Wire),
    PCA9557(BASE_ADDR+4, &Wire), // 001
    PCA9557(BASE_ADDR+2, &Wire), // 010
    PCA9557(BASE_ADDR+6, &Wire), // 011
    PCA9557(BASE_ADDR+1, &Wire), // 100
    PCA9557(BASE_ADDR+5, &Wire), // 101
    PCA9557(BASE_ADDR+3, &Wire), // 110
    PCA9557(BASE_ADDR+7, &Wire)  // 111
};

/* Which 3 module boards are present. For the final setup this should be all
 * 1's. However when testing a small number of boards, we have no way to test
 * which boards are connected since the Wire library resets the whole Teensy if
 * it fails to communicate, so we need to manually keep track of which boards
 * are present. */
int active[8] = {
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1
};

/* Create the object to talk to the AD5593R. Note that we don't set a pin for
 * the base address. I'm not entirely sure what the author of that package had
 * in mind with that pin, but we manually manage setting the address pin each
 * time we want to talk to it by setting the ADC pin on the PCA9557.
 *
 * Note that the way the code is structured, you have to set that pin HIGH on
 * the ones you don't want to talk to and only set it low for the one you do
 * want to talk to.
 *
 * The pinout for the AD5593 is as follows:
 *
 *     4  - I/O0 - thermistor A1_1
 *     5  - I/O1 - thermistor A1_2
 *     6  - I/O2 - thermistor A1_3
 *     7  - I/O3 - TEC sense
 *     8  - I/O4 - TEC control 1
 *     9  - I/O5 - TEC control 2
 *     10 - I/O6 - TEC control 3
 *     11 - I/O7 - NC
 *
 * The VREF pin is connected to ground with a capacitor. */
AD5593R gpio(-1);
bool my_DACs[8] = {0,0,0,0,1,1,1,1};
bool my_ADCs[8] = {1,1,1,1,0,0,0,0};

int reset()
{
    int i, j, rv;

    rv = 0;

    /* Set the stepper motor to step in full steps.
     * See https://www.pololu.com/product/2133. */
    digitalWrite(PIN_STP_M0,LOW);
    digitalWrite(PIN_STP_M1,LOW);
    digitalWrite(PIN_STP_M2,LOW);
    digitalWrite(PIN_MR1,LOW);
    digitalWrite(PIN_MR2,LOW);
    digitalWrite(PIN_ATT,LOW);
    digitalWrite(PIN_THRU,LOW);

    /* Set all the relays to not attenuated. */
    set_attenuation(false);

    /* Loop over each of the 3 module boards. */
    for (i = 0; i < (int) LEN(bus); i++) {
        if (!active[i]) {
            sprintf(err, "Skipping board %i because it's not active\n", i);
            Serial.print(err);
            continue;
        }

        /* Loop over the six HV relays. */
        for (j = 0; j < (int) LEN(hv_relays); j++) {
            /* Set the pin to be an output.
             *
             * Note: I have the code here set up to be able to detect an error,
             * but it turns out for some reason that it never errors and if it
             * can't talk to the chip it just resets the whole Teensy. I tried
             * setting a timeout, but it seems the wire library for the teensy
             * doesn't have the timeout function added yet. I'm going to keep
             * this code here in case we figure out some way to update it. */
            if (!bus[i].pinMode(hv_relays[j],OUTPUT)) {
                sprintf(err, "Error setting pin mode for address 0x%1x %s. Aborting...\n", i, hv_relay_names[j]);
                Serial.print(err);
                rv = -1;
                break;
            }
            delay(DELAY);
            if (bus_write(i, hv_relays[j], LOW)) {
                sprintf(err, "Error setting relay for board %i LOW\n", i);
                Serial.print(err);
                rv = -1;
            }
            delay(DELAY);
        }

        /* Set the pin which sets the address for the GPIO chip to be an
         * output. */
        if (!bus[i].pinMode(ADC,OUTPUT)) {
            sprintf(err, "Error setting pin mode for address 0x%1x ADC. Aborting...\n", i);
            Serial.print(err);
            rv = -1;
        }

        /* Set the address pin HIGH so we don't talk to multiple chips at once. */
        if (bus_write(i, ADC, HIGH)) {
            sprintf(err, "Error setting address pin for board %i HIGH\n", i);
            Serial.print(err);
            rv = -1;
        }
        delay(DELAY);
    }

    /* Loop over each of the 3 module boards. */
    for (i = 0; i < (int) LEN(bus); i++) {
        if (!active[i]) {
            sprintf(err, "Skipping board %i because it's not active\n", i);
            Serial.print(err);
            rv = -1;
            continue;
        }

        /* Enable the ADC pin so we can talk to this chip. */
        if (bus_write(i, ADC, LOW)) {
            sprintf(err, "Error setting address pin for board %i LOW\n", i);
            Serial.print(err);
            rv = -1;
            continue;
        }

        delay(DELAY);
        gpio.configure_DACs(my_DACs);
        gpio.configure_ADCs(my_ADCs);
        gpio.enable_internal_Vref();
        gpio.set_DAC_max_2x_Vref();
        gpio.set_ADC_max_1x_Vref();
        if (gpio.write_DAC(TEC_CTRL1,0) < 0) {
            sprintf(err, "Error setting TEC relay 1 on bus %i\n", i);
            Serial.print(err);
            rv = -1;
        }
        if (gpio.write_DAC(TEC_CTRL2,0) < 0) {
            sprintf(err, "Error setting TEC relay 2 on bus %i\n", i);
            Serial.print(err);
            rv = -1;
        }
        if (gpio.write_DAC(TEC_CTRL3,0) < 0) {
            sprintf(err, "Error setting TEC relay 3 on bus %i\n", i);
            Serial.print(err);
            rv = -1;
        }
        delay(DELAY);
        /* Always need to set it back low at the end to avoid talking to
         * multiple chips. */
        if (bus_write(i, ADC, HIGH)) {
            sprintf(err, "Error setting address pin for board %i HIGH\n", i);
            Serial.print(err);
            rv = -1;
        }
    }

    /* Set the HV dac pin select off (high). */
    pinMode(PIN_CS,OUTPUT);
    digitalWrite(PIN_CS,HIGH);

    disable_hv();
    return rv;
}

/* Clock in the decade counter to set the attenuation.
 *
 * See https://www.ti.com/lit/ds/symlink/cd74hc4017.pdf?ts=1674132460909. */
int set_attenuation(bool ison)
{
    int i;

    /* Bring the master reset high to zero all the outputs. */
    digitalWrite(PIN_MR1,HIGH);
    digitalWrite(PIN_MR2,HIGH);

    delay(DELAY);

    /* Bring the master reset low to enable us to clock in the coils. */
    digitalWrite(PIN_MR1,LOW);
    digitalWrite(PIN_MR2,LOW);

    digitalWrite(ison ? PIN_ATT : PIN_THRU,LOW);

    delay(DELAY);

    for (i = 0; i < 20; i++) {
        /* Clock in either the attenuated or unattenuated relays. */
        digitalWrite(ison ? PIN_ATT : PIN_THRU,HIGH);
        /* Datasheet says it needs at least 10 ms, so we wait 20 ms. Previously
         * this was set to 10 ms, but 1 relay during testing wouldn't properly
         * close. */
        delay(20);
        digitalWrite(ison ? PIN_ATT : PIN_THRU,LOW);
        delay(DELAY);
    }

    /* Bring the master reset high to zero all the outputs. */
    digitalWrite(PIN_MR1,HIGH);
    digitalWrite(PIN_MR2,HIGH);

    return 0;
}

uint16_t DAC_WRITE_THROUGH = 0x4000;
uint16_t DAC_POWER_DOWN = 0x8000;
uint16_t DAC_NO_OP = 0x0000;
uint16_t DAC_POWER_DOWN_NORMAL = 0x0000;
uint16_t DAC_POWER_DOWN_HIGH = 0x400;
uint16_t DAC_POWER_DOWN_GND_100K = 0x800;
uint16_t DAC_POWER_DOWN_GND_1K = 0xc00;
float DAC_VREF = 2.048;

double HV_R1 = 1e6;
double HV_R2 = 14e3;

/* Set the DC DC boost converter output voltage to a given value using a
 * feedback loop. Returns 0 on success, -1 on error.
 *
 * This function was needed when we realized that the output voltage of the
 * DC/DC boost converter depends on the current being drawn (which means it
 * depends on the number of relays open).
 *
 * The strategy is simple: we first try to set the voltage using the correction
 * factors determined by Lautaro (see the comments to set_hv()). Then, we
 * correct this guess by the difference between the readback voltage and the
 * value we want. We try this 10 times and quit early if the error is ever less
 * than 10 mV. */
int set_hv_feedback(float value)
{
    int i;
    float guess;
    double readback;

    guess = value;
    for (i = 0; i < 2; i++) {
        if (set_hv(guess)) return -1;
        delay(1000);
        if (extmon_vread(&readback)) return -1;
        guess -= readback - value;

        if (guess > 50) {
            sprintf(err, "error: setpoint = %.2f V, guess = %.2f V, readback = %.2f V. Next guess would be more than 50 volts! Is there a problem with the readback?", value, guess, readback);
            return -1;
        }

        if (guess < 0) {
            sprintf(err, "error: setpoint = %.2f V, guess = %.2f V, readback = %.2f V. Next guess would be less than 0 volts! Is there a problem with the readback?", value, guess, readback);
            return -1;
        }

        if (fabs(readback-value) < 0.01) break;
    }

    return 0;
}

/* Set the DC DC boost converter output voltage to a given value.
 * Returns 0 on success, -1 on error.
 *
 * According to the docs here:
 * https://www.analog.com/media/en/technical-documentation/data-sheets/3482fa.pdf,
 * we should set the control voltage according to:
 *
 * R1 = R2(Vout2/Vref - 1)
 * R1/R2 = Vout2/Vref - 1
 * R1/R2 + 1 = Vout2/Vref
 * Vref = Vout2/(R1/R2 + 1)
 * Vapd = Vout2 - 5
 *
 * Lautaro measured the set/actual voltage and found the following values:
 *
 * Set   Actual
 * ----- ------
 * 30    32.123
 * 32.5  34.6
 * 35    37.066
 * 40    41.99
 * 41.5  43.475
 *
 * and fit these to the following equation:
 *
 * actual = 0.986*set + 2.55
 *
 * Therefore, we want to change the set value to:
 *
 * (value - 2.55)/0.986 */
int set_hv(float value)
{
    value = (value - 2.55)/0.986;
    /* Add the voltage drop from the 300 + 50 ohm resistors. */
    double r = 350 + 500e3/8.0;
    double v = value;
    double i = v/r;
    value += i*350;
    /* Add the voltage drop from the BAP65-02 diode. */
    value += BAP65_02_VOLTAGE_DROP;
    float vref = (value+5)/(HV_R1/HV_R2 + 1);
    return set_dac(vref);
}

/* Set the DC DC boost converter output voltage to a given value.
 * Returns 0 on success, -1 on error. */
int disable_hv(void)
{
    uint16_t code = DAC_POWER_DOWN | DAC_POWER_DOWN_GND_100K;
    /* Set the HV dac pin select on (low). */
    digitalWrite(PIN_CS,LOW);
    SPI.beginTransaction(SPISettings(14000000, MSBFIRST, SPI_MODE1));
    SPI.transfer16(code);
    delay(DELAY);
    SPI.endTransaction();
    digitalWrite(PIN_CS,HIGH);
    return 0;
}

/* Set the DAC for the HV bias control to a given voltage `value`. This output
 * voltage controls the HV bias level by setting the voltage at the DC DC boost
 * converter.
 *
 * Based on the docs at
 * https://www.analog.com/media/en/technical-documentation/data-sheets/max5214-max5216.pdf,
 * I tried to guess the correct SPI settings. It's definitely MSB first, but
 * the exact SPI mode is a little more tricky to tell. From Figure 2 it looks
 * like the clock polarity is low at idle and that the data is clocked in on
 * the falling edge, but I can't be sure. */
int set_dac(float value)
{
    uint16_t code = (value/DAC_VREF)*16384;
    enable_dac();
    delay(DELAY);
    /* Set the HV dac pin select on (low). */
    digitalWrite(PIN_CS,LOW);
    SPI.beginTransaction(SPISettings(14000000, MSBFIRST, SPI_MODE1));
    SPI.transfer16(DAC_WRITE_THROUGH | code);
    delay(DELAY);
    SPI.endTransaction();
    digitalWrite(PIN_CS,HIGH);
    return 0;
}

int enable_dac()
{
    digitalWrite(PIN_CS,LOW);
    SPI.beginTransaction(SPISettings(14000000, MSBFIRST, SPI_MODE1));
    SPI.transfer16(DAC_POWER_DOWN | DAC_POWER_DOWN_NORMAL);
    delay(DELAY);
    SPI.endTransaction();
    digitalWrite(PIN_CS,HIGH);
    return 0;
}

void setup()
{
    Serial.begin(9600);
    Wire.begin();

    SPI.begin();

#ifdef ETHERNET
    // start the Ethernet
    Ethernet.begin(mac,ip);

    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
        while (true) {
            delay(1); // do nothing, no point running without Ethernet hardware
        }
    }

    if (Ethernet.linkStatus() == LinkOFF) {
        Serial.println("Ethernet cable is not connected.");
    }

    // start UDP
    Udp.begin(localPort);
#endif

    /* Pins to set the step and microstep size. */
    pinMode(PIN_STP_M0,OUTPUT);
    pinMode(PIN_STP_M1,OUTPUT);
    pinMode(PIN_STP_M2,OUTPUT);
    /* Master reset. */
    pinMode(PIN_MR1,OUTPUT);
    pinMode(PIN_MR2,OUTPUT);
    /* Relay clock attenuation. */
    pinMode(PIN_ATT,OUTPUT);
    /* Relay clock through. */
    pinMode(PIN_THRU,OUTPUT);
    /* Stepper home limit switch. */
    pinMode(PIN_STP_HOME,INPUT);
    pinMode(PIN_STP_STEP,OUTPUT);
    pinMode(PIN_STP_DIR,OUTPUT);
    pinMode(PIN_STP_FAULT,INPUT);
    pinMode(PIN_STP_SLEEP,OUTPUT);
    pinMode(PIN_STP_RESET,OUTPUT);
    pinMode(PIN_STP_EN_UC,OUTPUT);
    pinMode(PIN_BIAS_IREAD,INPUT);
    pinMode(PIN_BIAS_VREAD,INPUT);
    pinMode(PIN_EXTMON_UC,INPUT);

    /* Start off with the stepper in sleep mode. */
    digitalWrite(PIN_STP_SLEEP,false);
    /* Enable the driver by bringing the reset high. */
    digitalWrite(PIN_STP_RESET,true);
    /* Right now things have to be hooked up such that we always drive this
     * high and hook up the limit switches in the normally closed configuration.
     * This isn't ideal since if they get disconnected it will be enabled.
     *
     * FIXME: Change this if we ever change the design. */
    // Currently limit switches are not in use
    digitalWrite(PIN_STP_EN_UC,false);

    reset();
}

/* Read an ADC value on the AD5593R.
 *
 * Arguments:
 *     - bus_index: The number of the 3 module board, i.e. 0 for 0b000, 1 for 0b001, etc.
 *     - address: The pin number on the chip, i.e. TEC_CTRL1, TEC_CTRL2, TEC_CTRL3.
 *     - value: pointer to a float
 *
 * Returns 0 on success, -1 on error. */
int gpio_read(int bus_index, int address, float *value)
{
    /* Enable the ADC pin so we can talk to this chip. */
    if (bus_write(bus_index, ADC, LOW)) return -1;
    *value = gpio.read_ADC(address);
    if (bus_write(bus_index, ADC, HIGH)) return -1;
    return 0;
}

/* Turn on/off an output on the AD5593R.
 *
 * Arguments:
 *     - bus_index: The number of the 3 module board, i.e. 0 for 0b000, 1 for 0b001, etc.
 *     - address: The pin number on the chip, i.e. TEC_CTRL1, TEC_CTRL2, TEC_CTRL3.
 *     - value: HIGH/LOW
 *
 * Returns 0 on success, -1 on error. */
int gpio_write(int bus_index, int address, bool value)
{
    /* Enable the ADC pin so we can talk to this chip. */
    if (bus_write(bus_index, ADC, LOW)) return -1;
    if (gpio.write_DAC(address, value ? 3.3 : 0) < 0) {
        Serial.print("error setting DAC!\n");
        bus_write(bus_index, ADC, HIGH);
        return -1;
    }
    if (bus_write(bus_index, ADC, HIGH)) return -1;
    return 0;
}

/* Turn on/off an output on the PCA9557.
 *
 * Arguments:
 *     - bus_index: The number of the 3 module board, i.e. 0 for 0b000, 1 for 0b001, etc.
 *     - address: The pin number on the chip, i.e. KC1, KC2, ADC, etc.
 *     - value: HIGH/LOW
 *
 * Returns 0 on success, -1 on error. */
int bus_write(int bus_index, int address, int value)
{
    if (debug) {
        sprintf(msg,"Setting bus 0x%1x address 0x%08x to %s\n", bus_index, address, value ? "HIGH" : "LOW");
        Serial.print(msg);
    }
    if (!bus[bus_index].digitalWrite(address,value))
        return -1;
    else
        return 0;
}

bool poll[8] = {false,false,false,false,false,false,false,false};

int strtobool(const char *str, bool *value)
{
    if (!strcasecmp(str,"on") || !strcasecmp(str,"1"))
        *value = true;
    else if (!strcasecmp(str,"off") || !strcasecmp(str,"0"))
        *value = false;
    else
        return -1;
    return 0;
}

int mystrtoi(const char *str, int *value)
{
    long lvalue;
    char *endptr;
    errno = 0;
    lvalue = strtol(str,&endptr,0);
    if (errno)
        return -1;
    if (endptr == str)
        return -1;
    if (lvalue > INT_MAX || lvalue < INT_MIN)
        return -1;
    *value = (int) lvalue;
    return 0;
}

/* Returns the temperature in Celsius of the thermistor `address` on card
 * `bus_address`. */
int thermistor_read(int bus_address, int address, float *value)
{
    /* This is the gain of the op amp. Can be calculated as:
     *
     *     G = 2*R2/R1
     *
     * See the docs for which resistor is which. */
    float gain = 40;
    float iref = 100e-6;
    float r0 = 1000;
    float vout;
    gpio_read(bus_address,thermistors[address],&vout);
    float r3 = 778.1;
    float r = vout/(gain*iref) + r3;

    /* Now, we use the simplified Callendar-Van Dusen coefficients from
     * https://www.ti.com/seclit/eb/slyw038c/slyw038c.pdf. */
    float A0 = 3.9083e-3;

    *value = (r/r0 - 1)/A0;
    return 0;
}

/* Returns the total resistance of the TEC `address` on card `bus_address`.
 * Here, we try to do this very accurately by taking the reading very quick.
 * Arjan mentioned that the resistance of the TEC will change rapidly when you
 * start passing current through it. Therefore, we only turn on the TEC relay
 * for a few milliseconds and take the measurement. We also average 10
 * measurements to try and get a more reliable measurement. */
int tec_check(int bus_address, int address, float *value)
{
    int i;
    float sum, sense;
    int naverage = 1;

    /* First, make sure all the TEC relays are open. */
    for (i = 0; i < (int) LEN(tec_relays); i++)
        gpio_write(bus_address,tec_relays[i],false);

    delay(DELAY);

    /* Now, we close the relay */
    gpio_write(bus_address,tec_relays[address],true);

    /* Datasheet says it takes about 3 ms to open, but I've tested it and it
     * seems even without a delay here the relay is already open. We wait 1 ms
     * just to be safe. */
    delay(1);

    /* Now, read the sense relay a few times and average the result. */
    sum = 0.0;
    for (i = 0; i < naverage; i++) {
        if (gpio_read(bus_address,TEC_SENSE,&sense)) {
            sprintf(err, "failed to read tec sense relay on bus %i", bus_address);
            return -1;
        }
        sum += sense;
    }

    /* Now, we open the relay */
    gpio_write(bus_address,tec_relays[address],false);

    /* Compute the voltage across the sense resistor by averaging the readings.
     * Divide by 20 due to the gain of the INA180 current-sense amplifier (see
     * https://www.ti.com/lit/ds/symlink/ina180.pdf). */
    float vsense = (sum/naverage)/20;
    /* Compute the current through the 25 mOhm sense resistor. */
    float isense = vsense/25e-3;
    /* The supply voltage we are sending across the TECs.
     *
     * Note: This isn't guaranteed to be 5.0 V by the board design. It requires
     * that the user hooks up the 5.0 V rail to the TEC voltage. */
    float supply_voltage = 5.0;

    *value = supply_voltage/isense;
    return 0;
}

double mystrtod(const char *nptr, double *value)
{
    char *endptr;
    errno = 0;
    *value = strtod(nptr,&endptr);

    if (endptr == nptr) {
        sprintf(err, "error converting '%s' to a double", nptr);
        return -1;
    } else if (errno != 0) {
        sprintf(err, "error converting '%s' to a double", nptr);
        return -1;
    }

    return 0;
}

/* The current monitor output is sent through a transimpedance amplifier with
 * gain equal to R4. */
double HV_R4 = 1e9;

/* The voltage monitor is read out from a voltage divider with R5 and R6. I
 * can't read the notes exactly to see what R6 is, it might be 2.44 MOhms? */
double HV_R5 = 100e9;
double HV_R6 = 2.94e9;

double HV_R7 = 150e3;
double HV_R8 = 10e3;

/* Returns the current provided by the DC DC boost converter. From the manual,
 * it says:
 *
 * > Current Monitor Output Pin. It sources a
 * > current equal to 20% of the APD current and converts to
 * > a reference voltage through an external resistor. */
int get_bias_iread(double *value)
{
    *value = 5*analogRead(PIN_BIAS_IREAD)*(TEENSY_3V/1023.0)/HV_R4;
    return 0;
}

/* Returns the output voltage from the DC DC boost converter.
 *
 * The voltage monitor output is read out from a voltage divider with R5 and
 * R6. The total current is:
 * 
 *     I = V/(R5+R6)
 *
 * The voltage across R6 which is what we read is:
 *
 *     V6 = I*R6 = V*R6/(R5+R6)
 *
 * So we can calculate the output voltage as:
 *
 *     V = V6*(R5+R6)/R6
 */
int get_bias_vread(double *value)
{
    *value = analogRead(PIN_BIAS_VREAD)*(TEENSY_3V/1023.0)*(HV_R5+HV_R6)/HV_R6;
    /* Subtract off the voltage drop from the 300 + 50 ohm resistors. */
    double r = 350 + 500e3/8.0;
    double v = *value;
    double i = v/r;
    *value -= i*350;
    /* Subtract off the voltage drop from the BAP65-02 diode. */
    *value -= BAP65_02_VOLTAGE_DROP;
    return 0;
}

/* Returns the bias voltage independent of whether it's coming from the DC DC
 * boost converter or an external supply.
 *
 * The voltage monitor output is read out from a voltage divider with R7 and
 * R8. The total current is:
 * 
 *     I = V/(R7+R8)
 *
 * The voltage across R8 which is what we read is:
 *
 *     V6 = I*R8 = V*R8/(R7+R8)
 *
 * So we can calculate the output voltage as:
 *
 *     V = V6*(R7+R8)/R8
 */
int extmon_vread(double *value)
{
    *value = analogRead(PIN_EXTMON_UC)*(TEENSY_3V/1023.0)*(HV_R7+HV_R8)/HV_R8;
    /* Subtract off the voltage drop from the 300 + 50 ohm resistors. */
    double r = 350 + 500e3/8.0;
    double v = *value;
    double i = v/r;
    *value -= i*350;
    /* Subtract off the voltage drop from the BAP65-02 diode. */
    *value -= BAP65_02_VOLTAGE_DROP;
    return 0;
}

/* Performs a user command based on a string. Examples of commands:
 *
 * "tec_write 1 0 on" - turn TEC 0 on on board 1
 * "tec_write 2 1 off" - turn TEC 1 off on board 2
 * "hv_write 2 1 off" - turn HV relay 1 off on board 2
 * "thermistor_read [bus] [address]" - read thermistor voltage
 * "tec_sense_read [bus]" - read tec sensor current
 * "reset" - reset all boards to their nominal state
 * "poll [bus] [on/off]" - start polling thermistors and tec current reading
 * "set_active_bitmask [bitmask]" - set which boards are currently plugged in
 * "debug [on/off]" - turn debugging on or off
 * "set_attenuation [on/off]" - turn attenuation on or off
 * "bias_iread" - read out the current supplied by the DC DC boost converter
 * "bias_vread" - read out the voltage supplied by the DC DC boost converter
 * "extmon_vread" - read out the bias voltage
 * "set_hv [voltage]" - set the high voltage on the DC DC boost converter
 * "disable_hv" - disable DC DC boost converter
 * "set_pin 2 1" - set teensy pin#2 to HIGH (use with caution)
 * "read_pin 17" - read the value of pin#17 (use with caution)
 *
 * Returns 0, 1, or 2 on success, -1 on error. Returns 0 if there is no return
 * value, 1 if there is an integer return value, and 2 if there is a floating
 * point return value. Note that since the return value is passed as a float,
 * we just convert any integer return values to a float. If there is an error,
 * the global `err` string contains an error message. */
int do_command(char *cmd, float *value)
{
    int i;
    int ntok = 0;
    char *tokens[10];
    char *tok;
    bool ison;
    int bus_index, address;
    int bitmask;
    double temp;

    if (cmd[strlen(cmd)-1] == '\n')
        cmd[strlen(cmd)-1] = '\0';

    if (debug) {
        sprintf(msg, "received command: %s\n", cmd);
        Serial.print(msg);
    }

    tok = strtok(cmd, " ");
    while (tok != NULL && ntok < (int) LEN(tokens)) {
        tokens[ntok++] = tok;
        tok = strtok(NULL, " ");
    }

    if (!strcmp(tokens[0], "bias_vread")) {
        if (ntok != 1) {
            sprintf(err, "bias_vread command expects 0 arguments");
            return -1;
        }

        if (get_bias_vread(&temp)) {
            sprintf(err, "error reading the bias voltage");
            return -1;
        }

        *value = temp;

        return 2;
    } else if (!strcmp(tokens[0], "bias_iread")) {
        if (ntok != 1) {
            sprintf(err, "bias_iread command expects 0 arguments");
            return -1;
        }

        if (get_bias_iread(&temp)) {
            sprintf(err, "error reading the bias current");
            return -1;
        }

        *value = temp;

        return 2;
    } else if (!strcmp(tokens[0], "step")) {
        if (ntok != 2) {
            sprintf(err, "step command expects 1 argument: step [steps]");
            return -1;
        }

        if (mystrtoi(tokens[1],&bus_index)) {
            sprintf(err, "expected argument 1 to be integer but got '%s'", tokens[1]);
            return -1;
        }

        if (step(bus_index)) return -1;

        return 1;
    } else if (!strcmp(tokens[0], "step_home")) {
        if (ntok != 1) {
            sprintf(err, "step_home command expects 0 arguments");
            return -1;
        }

        if (step_home()) return -1;

        return 1;
    } else if (!strcmp(tokens[0], "set_pin")){
        if (ntok != 3){
            sprintf(err, "set_pin command expect 2 arguments: set_pin [pin index] [1/0]");
            return -1;
        }

        int _idx;
        int _state;
        if (mystrtoi(tokens[1],&_idx)) {
            sprintf(err, "expected argument 1 to be integer but got '%s'", tokens[1]);
            return -1;
        }
        if (mystrtoi(tokens[2],&_state)) {
            sprintf(err, "expected argument 2 to be integer but got '%s'", tokens[2]);
            return -1;
        }
        if(set_pin(_idx,_state)) return -1;

        return 1;
        
    } else if (!strcmp(tokens[0], "read_pin")){
        if (ntok != 2){
            sprintf(err, "read_pin command expect 1 arguments: read_pin [pin index]");
            return -1;
        }

        int _idx;
        if (mystrtoi(tokens[1],&_idx)) {
            sprintf(err, "expected argument 1 to be integer but got '%s'", tokens[1]);
            return -1;
        }
        int rv = read_pin(_idx);
        *value = rv;

        return 1;
        
    } else if (!strcmp(tokens[0], "set_attenuation")) {
        if (ntok != 2) {
            sprintf(err, "set_attenuation command expects 1 argument: set_attenuation [on/off]");
            return -1;
        }

        if (strtobool(tokens[1],&ison)) {
            sprintf(err, "expected argument 3 to be yes/no but got '%s'", tokens[1]);
            return -1;
        }

        set_attenuation(ison);
    } else if (!strcmp(tokens[0], "tec_write")) {
        if (ntok != 4) {
            sprintf(err, "tec_write command expects 3 arguments: tec_write [bus] [address] [value]");
            return -1;
        }

        if (strtobool(tokens[3],&ison)) {
            sprintf(err, "expected argument 3 to be yes/no but got '%s'", tokens[3]);
            return -1;
        } else if (mystrtoi(tokens[1],&bus_index)) {
            sprintf(err, "expected argument 1 to be integer but got '%s'", tokens[1]);
            return -1;
        } else if (mystrtoi(tokens[2],&address)) {
            sprintf(err, "expected argument 2 to be integer but got '%s'", tokens[2]);
            return -1;
        }

        if (bus_index < 0 || bus_index >= (int) LEN(bus)) {
            sprintf(err, "bus index %i is not valid", bus_index);
            return -1;
        } else if (!active[bus_index]) {
            sprintf(err, "bus index %i is not active", bus_index);
            return -1;
        } else if (address < 0 || address >= (int) LEN(tec_relays)) {
            sprintf(err, "address %i is not valid", address);
            return -1;
        }
        gpio_write(bus_index,tec_relays[address],ison);
    } else if (!strcmp(tokens[0], "hv_write")) {
        if (ntok != 4) {
            sprintf(err, "hv_write command expects 3 arguments: hv_write [bus] [address] [value]");
            return -1;
        }

        if (strtobool(tokens[3],&ison)) {
            sprintf(err, "expected argument 3 to be yes/no but got '%s'", tokens[3]);
            return -1;
        } else if (mystrtoi(tokens[1],&bus_index)) {
            sprintf(err, "expected argument 1 to be integer but got '%s'", tokens[1]);
            return -1;
        } else if (mystrtoi(tokens[2],&address)) {
            sprintf(err, "expected argument 2 to be integer but got '%s'", tokens[2]);
            return -1;
        }

        if (bus_index < 0 || bus_index >= (int) LEN(bus)) {
            sprintf(err, "bus index %i is not valid", bus_index);
            return -1;
        } else if (!active[bus_index]) {
            sprintf(err, "bus index %i is not active", bus_index);
            return -1;
        } else if (address < 0 || address >= (int) LEN(hv_relays)) {
            sprintf(err, "address %i is not valid", address);
            return -1;
        }
        bus_write(bus_index,hv_relays[address],ison);
    } else if (!strcmp(tokens[0], "thermistor_read")) {
        if (ntok != 3) {
            sprintf(err, "thermistor_read command expects 2 arguments: thermistor_read [bus] [address]");
            return -1;
        }

        if (mystrtoi(tokens[1],&bus_index)) {
            sprintf(err, "expected argument 1 to be integer but got '%s'", tokens[1]);
            return -1;
        } else if (mystrtoi(tokens[2],&address)) {
            sprintf(err, "expected argument 2 to be integer but got '%s'", tokens[2]);
            return -1;
        }

        if (bus_index < 0 || bus_index >= (int) LEN(bus)) {
            sprintf(err, "bus index %i is not valid", bus_index);
            return -1;
        } else if (!active[bus_index]) {
            sprintf(err, "bus index %i is not active", bus_index);
            return -1;
        } else if (address < 0 || address >= (int) LEN(thermistors)) {
            sprintf(err, "address %i is not valid", address);
            return -1;
        } else if (value == NULL) {
            sprintf(err, "value must not be set to NULL");
            return -1;
        }
        if (thermistor_read(bus_index,thermistors[address],value)) return -1;
        return 2;
    } else if (!strcmp(tokens[0], "tec_check")) {
        if (ntok != 3) {
            sprintf(err, "tec_check command expects 2 argument: tec_check [bus] [address]");
            return -1;
        }

        if (mystrtoi(tokens[1],&bus_index)) {
            sprintf(err, "expected argument 1 to be integer but got '%s'", tokens[1]);
            return -1;
        } else if (mystrtoi(tokens[2],&address)) {
            sprintf(err, "expected argument 2 to be integer but got '%s'", tokens[2]);
            return -1;
        }

        if (bus_index < 0 || bus_index >= (int) LEN(bus)) {
            sprintf(err, "bus index %i is not valid", bus_index);
            return -1;
        } else if (!active[bus_index]) {
            sprintf(err, "bus index %i is not active", bus_index);
            return -1;
        } else if (address < 0 || address >= (int) LEN(tec_relays)) {
            sprintf(err, "address %i is not valid", address);
            return -1;
        } else if (value == NULL) {
            sprintf(err, "value must not be set to NULL");
            return -1;
        }
        if (tec_check(bus_index,address,value)) return -1;
        return 2;
    } else if (!strcmp(tokens[0], "tec_sense_read")) {
        if (ntok != 2) {
            sprintf(err, "tec_sense_read command expects 1 argument: tec_sense_read [bus]");
            return -1;
        }

        if (mystrtoi(tokens[1],&bus_index)) {
            sprintf(err, "expected argument 1 to be integer but got '%s'", tokens[1]);
            return -1;
        }

        if (bus_index < 0 || bus_index >= (int) LEN(bus)) {
            sprintf(err, "bus index %i is not valid", bus_index);
            return -1;
        } else if (!active[bus_index]) {
            sprintf(err, "bus index %i is not active", bus_index);
            return -1;
        } else if (value == NULL) {
            sprintf(err, "value must not be set to NULL");
            return -1;
        }
        gpio_read(bus_index,TEC_SENSE,value);
        return 2;
    } else if (!strcmp(tokens[0], "reset")) {
        if (ntok != 1) {
            sprintf(err, "reset command expects 0 arguments");
            return -1;
        }
        if (reset())
            return -1;
    } else if (!strcmp(tokens[0], "poll")) {
        if (ntok != 3) {
            sprintf(err, "poll command expects 2 arguments: poll [bus] [on/off]");
            return -1;
        }

        if (mystrtoi(tokens[1],&bus_index)) {
            sprintf(err, "expected argument 1 to be integer but got '%s'", tokens[1]);
            return -1;
        } else if (strtobool(tokens[2],&ison)) {
            sprintf(err, "expected argument 2 to be yes/no but got '%s'", tokens[2]);
            return -1;
        }

        if (bus_index < 0 || bus_index >= (int) LEN(bus)) {
            sprintf(err, "bus index %i is not valid", bus_index);
            return -1;
        } else if (!active[bus_index]) {
            sprintf(err, "bus index %i is not active", bus_index);
            return -1;
        }

        poll[bus_index] = ison;
    } else if (!strcmp(tokens[0], "set_active_bitmask")) {
        if (ntok != 2) {
            sprintf(err, "set_active_bitmask command expects 1 argument: set_active_bitmask [bitmask]");
            return -1;
        }

        if (mystrtoi(tokens[1],&bitmask)) {
            sprintf(err, "expected argument 1 to be integer but got '%s'", tokens[1]);
            return -1;
        }

        for (i = 0; i < (int) LEN(bus); i++) {
            if (bitmask & (1 << i))
                active[i] = 1;
            else
                active[i] = 0;
        }
    } else if (!strcmp(tokens[0], "debug")) {
        if (ntok != 2) {
            sprintf(err, "debug command expects 1 argument: debug [on/off]");
            return -1;
        }

        if (strtobool(tokens[1],&ison)) {
            sprintf(err, "expected argument 1 to be yes/no but got '%s'", tokens[1]);
            return -1;
        }

        debug = ison;
    } else if (!strcmp(tokens[0], "set_dac")) {
        if (ntok != 2) {
            sprintf(err, "set_dac command expects 1 argument: set_dac [value]");
            return -1;
        }

        if (mystrtod(tokens[1],&temp))
            return -1;

        set_dac(temp);
    } else if (!strcmp(tokens[0], "extmon_vread")) {
        if (ntok != 1) {
            sprintf(err, "extmon_vread command expects no arguments");
            return -1;
        }

        if (extmon_vread(&temp)) {
            sprintf(err, "error reading the bias voltage");
            return -1;
        }

        *value = (float) temp;
        return 2;
    } else if (!strcmp(tokens[0], "enable_dac")) {
        if (ntok != 1) {
            sprintf(err, "enable_dac command expects no arguments");
            return -1;
        }

        if (enable_dac()) {
            sprintf(err, "error enabling the dac for HV boost converter");
            return -1;
        }

        return 0;
    } else if (!strcmp(tokens[0], "set_hv")) {
        if (ntok != 2) {
            sprintf(err, "set_hv command expects 1 argument: set_hv [voltage]");
            return -1;
        }

        if (mystrtod(tokens[1], &temp))
            return -1;

        if ((temp < 0) || (temp > 50)) {
            sprintf(err, "voltage must be between 0 and 50 V");
            return -1;
        }

        if (set_hv_feedback(temp)) {
            return -1;
        }
    } else if (!strcmp(tokens[0], "disable_hv")) {
        if (ntok != 1) {
            sprintf(err, "disable_hv command expects no arguments");
            return -1;
        }

        if (disable_hv()) {
            sprintf(err, "error setting the high voltage");
            return -1;
        }
    } else if (!strcmp(tokens[0], "help")) {
        sprintf(err,"tec_write [card] [tec] [on/off]\n"
                    "hv_write [card] [relay] [on/off]\n"
                    "thermistor_read [bus] [address]\n"
                    "tec_sense_read [bus]\n"
                    "tec_check [bus] [address]\n"
                    "reset\n"
                    "poll [bus] [on/off]\n"
                    "set_active_bitmask [bitmask]\n"
                    "debug [on/off]\n"
                    "set_attenuation [on/off]\n"
                    "step_home\n"
                    "step [steps]\n"
                    "bias_iread\n"
                    "bias_vread\n"
                    "extmon_vread\n"
                    "set_hv [voltage]\n"
                    "disable_hv\n"
                    "enable_dac\n"
                    "set_pin [pin#] [0|1]\n"
                    "read_pin [pin#]");
        return -1;
    } else {
        sprintf(err, "unknown command '%s'", tokens[0]);
        return -1;
    }

    return 0;
}

int step(int steps)
{
    int i = 0;

    /* Start off with the stepper in sleep mode. */
    digitalWrite(PIN_STP_RESET,true);
    delay(DELAY);
    digitalWrite(PIN_STP_SLEEP,true);
    delay(DELAY);

    // The direction is determined in the following wiring
    // motor A+ -> RDV8825 A1
    // motor A- -> RDV8825 A2
    // motor B+ -> RDV8825 B1
    // motor B- -> RDV8825 B2
    if (steps > 0)
        digitalWrite(PIN_STP_DIR,false);
    else
        digitalWrite(PIN_STP_DIR,true);

    steps = abs(steps);

    while (++i < MAX_STEPS && i < steps) {
        if (digitalRead(PIN_STP_FAULT) == false) {
            /* Put the stepper back in sleep mode. */
            digitalWrite(PIN_STP_SLEEP,false);

            sprintf(err, "stepper driver detected a fault");
            return -1;
        }
        digitalWrite(PIN_STP_STEP,true);
        delay(DELAY);
        digitalWrite(PIN_STP_STEP,false);
        delay(DELAY);
    }

    /* Put the stepper back in sleep mode. */
    digitalWrite(PIN_STP_SLEEP,false);

    return 0;
}

int step_home(void)
{
    int i = 0;

    /* Start off with the stepper in sleep mode. */
    digitalWrite(PIN_STP_RESET,true);
    delay(DELAY);
    digitalWrite(PIN_STP_SLEEP,true);
    delay(DELAY);

    // The direction is determined in the following wiring
    // motor A+ -> RDV8825 A1
    // motor A- -> RDV8825 A2
    // motor B+ -> RDV8825 B1
    // motor B- -> RDV8825 B2
    digitalWrite(PIN_STP_DIR,true);
    delay(DELAY);

    while (++i < MAX_STEPS && digitalRead(PIN_STP_HOME) == true) {
        if (digitalRead(PIN_STP_FAULT) == false) {
            /* Put the stepper back in sleep mode. */
            digitalWrite(PIN_STP_SLEEP,false);

            sprintf(err, "stepper driver detected a fault");
            return -1;
        }
        digitalWrite(PIN_STP_STEP,true);
        delay(DELAY);
        digitalWrite(PIN_STP_STEP,false);
        delay(DELAY);
    }

    /* Put the stepper back in sleep mode. */
    digitalWrite(PIN_STP_SLEEP,false);

    return 0;
}

// Set/read pin values. Please use with caution.
int set_pin(int pin, int state){
    digitalWrite(pin,state);
    return 0;
}
int read_pin(int pin){
    int rv = digitalRead(pin);
    return rv;
}


/* Formats the return message based on the return value of do_command(). The
 * return message is printed to the global variable msg. */
void format_message(int rv, float value)
{
    if (rv < 0) {
        sprintf(msg, "-%s\n", err);
    } else if (rv == 1) {
        sprintf(msg, ":%i\n", (int) value);
    } else if (rv == 2) {
        sprintf(msg, ",%.18f\n", value);
    } else {
        sprintf(msg, "+ok\n");
    }
}

void loop()
{
    int i;
    float temp = 0;

    while (Serial.available() > 0) {
        if (k >= (int) LEN(cmd) - 1) {
            Serial.print("Error: too many characters in command!\n");
            k = 0;
        }
        cmd[k++] = Serial.read();
        if (cmd[k-1] == '\n') {
            cmd[k-1] = '\0';

            temp = 0;
            int rv = do_command(cmd, &temp);
            format_message(rv,temp);
            Serial.print(msg);
            k = 0;
        }
    }

#ifdef ETHERNET
    // if there's data available, read a packet
    int packetSize = Udp.parsePacket();
    if (packetSize) {
        if (packetBuffer[packetSize-1] == '\n')
            packetBuffer[packetSize-1] = '\0';

        packetBuffer[packetSize] = '\0';

        if (debug) {
            Serial.print("Received packet of size ");
            Serial.println(packetSize);
            Serial.print("From ");
            IPAddress remote = Udp.remoteIP();
            for (int i=0; i < 4; i++) {
                Serial.print(remote[i], DEC);
                if (i < 3) {
                    Serial.print(".");
                }
            }

            Serial.print(", port ");
            Serial.println(Udp.remotePort());
        }

        // read the packet into packetBufffer
        Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
        Serial.println("Contents:");
        Serial.println(packetBuffer);

        temp = 0;
        int rv = do_command(packetBuffer, &temp);
        format_message(rv,temp);
        Serial.print(msg);

        // send a reply to the IP address and port that sent us the packet we received
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write(msg);
        Udp.endPacket();
    }
#endif

    for (i = 0; i < (int) LEN(bus); i++) {
        if (active[i] && poll[i] && millis() % 1000 == 0) {
            gpio_read(i, THERMISTOR1, &temp);
            sprintf(msg, "%i Thermistor 1: %.2f\n", i, temp);
            Serial.print(msg);
            gpio_read(i, THERMISTOR2, &temp);
            sprintf(msg, "%i Thermistor 2: %.2f\n", i, temp);
            Serial.print(msg);
            gpio_read(i, THERMISTOR3, &temp);
            sprintf(msg, "%i Thermistor 3: %.2f\n", i, temp);
            Serial.print(msg);
            gpio_read(i, TEC_SENSE, &temp);
            sprintf(msg, "%i TEC SENSE:    %.2f\n", i, temp);
            Serial.print(msg);
        }
    }
}
