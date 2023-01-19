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

#define LEN(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

//#define PCA9557_DEBUG

#define MAX_MSG_LENGTH 1024

/* Maximum number of stepper steps to take when trying to find home.
 *
 * FIXME: Need to determine an actual number for this. */
#define MAX_STEPS 100

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA
};
IPAddress ip(192, 168, 1, 177);

unsigned int localPort = 8888;      // local port to listen on

// buffers for receiving and sending data
char packetBuffer[UDP_TX_PACKET_MAX_SIZE];  // buffer to hold incoming packet,
char ReplyBuffer[] = "acknowledged";        // a string to send back

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

/* Generic delay after setting pins high/low. */
#define DELAY 100

char cmd[MAX_MESSAGE_LENGTH];
char err[MAX_MESSAGE_LENGTH];
char msg[MAX_MESSAGE_LENGTH];
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
#define PIN_MR1       PIN_D36
#define PIN_MR2       PIN_D32
#define PIN_ATT       PIN_D38
#define PIN_THRU      PIN_D37
/* Pins to set the step and microstep size. */
#define PIN_STP_FAULT PIN_D0
#define PIN_STP_SLEEP PIN_D1
#define PIN_STP_RESET PIN_D2
#define PIN_STP_M2    PIN_D3
#define PIN_STP_M1    PIN_D4
#define PIN_STP_M0    PIN_D5
#define PIN_STP_STEP  PIN_D6
#define PIN_STP_DIR   PIN_D7
#define PIN_DAC_CLEAR PIN_D8
#define PIN_BI_SD     PIN_D9
#define PIN_CS        PIN_D10
#define PIN_MOSI      PIN_D11
/* Analog pins. */
/* HV voltage and current. */
#define PIN_BIAS_IREAD PIN_A0
#define PIN_BIAS_VREAD PIN_A1
#define PIN_STP_HOME   PIN_D17
/* Stepper enable. Goes through two limit switches first. */
#define PIN_STP_EN_UC  PIN_D23

/* Array of HV relay pins and names. */
int hv_relays[6] = {KC1,KC2,KC3,KC4,KC5,KC6};
const char *hv_relay_names[6] = {"KC1","KC2","KC3","KC4","KC5","KC6"};
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
    for (i = 0; i < LEN(bus); i++) {
        if (!active[i]) {
            sprintf(err, "Skipping board %i because it's not active\n", i);
            Serial.print(err);
            continue;
        }

        /* Loop over the six HV relays. */
        for (j = 0; j < LEN(hv_relays); j++) {
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
    for (i = 0; i < LEN(bus); i++) {
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
        gpio.set_ADC_max_2x_Vref();
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

    delay(100);

    /* Bring the master reset low to enable us to clock in the coils. */
    digitalWrite(PIN_MR1,LOW);
    digitalWrite(PIN_MR2,LOW);

    digitalWrite(ison ? PIN_ATT : PIN_THRU,LOW);

    delay(100);

    for (i = 0; i < 100; i++) {
        /* Clock in either the attenuated or unattenuated relays. */
        digitalWrite(ison ? PIN_ATT : PIN_THRU,HIGH);
        delay(100);
        digitalWrite(ison ? PIN_ATT : PIN_THRU,LOW);
        delay(100);
    }

    /* Bring the master reset high to zero all the outputs. */
    digitalWrite(PIN_MR1,HIGH);
    digitalWrite(PIN_MR2,HIGH);

    return 0;
}

void setup()
{
    Serial.begin(9600);
    Wire.begin();

    // start the Ethernet
    Ethernet.begin(mac, ip);

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

    /* Pins to set the step and microstep size. */
    pinMode(PIN_STP_M0,OUTPUT);
    pinMode(PIN_STP_M1,OUTPUT);
    pinMode(PIN_STP_M2,OUTPUT);
    /* Master reset. */
    pinMode(PIN_MR,OUTPUT);
    /* Relay clock attenuation. */
    pinMode(PIN_ATT,OUTPUT);
    /* Relay clock through. */
    pinMode(PIN_THRU,OUTPUT);
    /* Stepper home limit switch. */
    pinMode(PIN_STP_HOME,INPUT);
    /* Should be pulled down by default, but just to make sure. */
    pinMode(pin, INPUT_PULLDOWN);
    pinMode(PIN_STP_STEP,OUTPUT);
    pinMode(PIN_STP_DIR,OUTPUT);
    pinMode(PIN_STP_FAULT,INPUT);
    pinMode(PIN_STP_SLEEP,OUTPUT);
    pinMode(PIN_STP_RESET,OUTPUT);
    pinMode(PIN_STP_EN_UC,OUTPUT);
    pinMode(PIN_BIAS_IREAD,INPUT);
    pinMode(PIN_BIAS_VREAD,INPUT);

    /* Start off with the stepper in sleep mode. */
    digitalWrite(PIN_STP_SLEEP,false);
    /* Enable the driver by bringing the reset high. */
    digitalWrite(PIN_STP_RESET,true);
    /* Right now things have to be hooked up such that we always drive this
     * high and hook up the limit switches in the normally closed configuration.
     * This isn't ideal since if they get disconnected it will be enabled.
     *
     * FIXME: Change this if we ever change the design. */
    digitalWrite(PIN_STP_EN_UC,false);

    reset();
}

int bias_iread(int *value)
{
    *value = analogRead(PIN_BIAS_IREAD);
}

int bias_vread(int *value)
{
    *value = analogRead(PIN_BIAS_VREAD);
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

int mystrtol(const char *str, long *value)
{
    char *endptr;
    errno = 0;
    *value = strtol(str,&endptr,0);
    if (errno)
        return -1;
    if (endptr == str)
        return -1;
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
 *
 * Returns 0 or 1 on success, -1 on error. Returns 0 if there is no return
 * value, 1 if there is a return value. If there is an error, the global `err`
 * string contains an error message. */
int do_command(char *cmd, float *value)
{
    int i;
    int ntok = 0;
    char *tokens[10];
    char *tok;
    bool ison;
    long bus_index, address;
    long bitmask;

    tok = strtok(cmd, " ");
    while (tok != NULL && ntok <= LEN(tokens) - 1) {
        tokens[ntok++] = tok;
        tok = strtok(NULL, " ");
    }

    if (!strcmp(tokens[0], "bias_vread")) {
        if (ntok != 1) {
            sprintf(err, "bias_vread command expects 0 arguments");
            return -1;
        }

        if (bias_vread(&bus_index)) return -1;

        *value = bus_index;

        return 0;
    } else if (!strcmp(tokens[0], "bias_iread")) {
        if (ntok != 1) {
            sprintf(err, "bias_iread command expects 0 arguments");
            return -1;
        }

        if (bias_iread(&bus_index)) return -1;

        *value = bus_index;

        return 0;
    } else if (!strcmp(tokens[0], "step")) {
        if (ntok != 2) {
            sprintf(err, "step command expects 1 argument: step [steps]");
            return -1;
        }

        if (mystrtol(tokens[1],&bus_index)) {
            sprintf(err, "expected argument 1 to be integer but got '%s'", tokens[1]);
            return -1;

        if (step(bus_index)) return -1;

        return 0;
    } else if (!strcmp(tokens[0], "step_home")) {
        if (ntok != 1) {
            sprintf(err, "step_home command expects 0 arguments");
            return -1;
        }

        if (step_home()) return -1;

        return 0;
    } else if (!strcmp(tokens[0], "set_attenuation")) {
        if (ntok != 2) {
            sprintf(err, "set_attenuation command expects 1 argument: set_attenuation [on/off]");
            return -1;
        }

        if (strtobool(tokens[3],&ison)) {
            sprintf(err, "expected argument 3 to be yes/no but got '%s'", tokens[3]);
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
        } else if (mystrtol(tokens[1],&bus_index)) {
            sprintf(err, "expected argument 1 to be integer but got '%s'", tokens[1]);
            return -1;
        } else if (mystrtol(tokens[2],&address)) {
            sprintf(err, "expected argument 2 to be integer but got '%s'", tokens[2]);
            return -1;
        }

        if (bus_index < 0 || bus_index > LEN(bus)) {
            sprintf(err, "bus index %i is not valid", bus_index);
            return -1;
        } else if (!active[bus_index]) {
            sprintf(err, "bus index %i is not active", bus_index);
            return -1;
        } else if (address < 0 || address > LEN(tec_relays)) {
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
        } else if (mystrtol(tokens[1],&bus_index)) {
            sprintf(err, "expected argument 1 to be integer but got '%s'", tokens[1]);
            return -1;
        } else if (mystrtol(tokens[2],&address)) {
            sprintf(err, "expected argument 2 to be integer but got '%s'", tokens[2]);
            return -1;
        }

        if (bus_index < 0 || bus_index > LEN(bus)) {
            sprintf(err, "bus index %i is not valid", bus_index);
            return -1;
        } else if (!active[bus_index]) {
            sprintf(err, "bus index %i is not active", bus_index);
            return -1;
        } else if (address < 0 || address > LEN(hv_relays)) {
            sprintf(err, "address %i is not valid", address);
            return -1;
        }
        bus_write(bus_index,hv_relays[address],ison);
    } else if (!strcmp(tokens[0], "thermistor_read")) {
        if (ntok != 3) {
            sprintf(err, "thermistor_read command expects 2 arguments: thermistor_read [bus] [address]");
            return -1;
        }

        if (mystrtol(tokens[1],&bus_index)) {
            sprintf(err, "expected argument 1 to be integer but got '%s'", tokens[1]);
            return -1;
        } else if (mystrtol(tokens[2],&address)) {
            sprintf(err, "expected argument 2 to be integer but got '%s'", tokens[2]);
            return -1;
        }

        if (bus_index < 0 || bus_index > LEN(bus)) {
            sprintf(err, "bus index %i is not valid", bus_index);
            return -1;
        } else if (!active[bus_index]) {
            sprintf(err, "bus index %i is not active", bus_index);
            return -1;
        } else if (address < 0 || address > LEN(hv_relays)) {
            sprintf(err, "address %i is not valid", address);
            return -1;
        } else if (value == NULL) {
            sprintf(err, "value must not be set to NULL");
            return -1;
        }
        gpio_read(bus_index,thermistors[address],value);
        return 1;
    } else if (!strcmp(tokens[0], "tec_sense_read")) {
        if (ntok != 2) {
            sprintf(err, "tec_sense_read command expects 1 argument: tec_sense_read [bus]");
            return -1;
        }

        if (mystrtol(tokens[1],&bus_index)) {
            sprintf(err, "expected argument 1 to be integer but got '%s'", tokens[1]);
            return -1;
        }

        if (bus_index < 0 || bus_index > LEN(bus)) {
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
        return 1;
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

        if (mystrtol(tokens[1],&bus_index)) {
            sprintf(err, "expected argument 1 to be integer but got '%s'", tokens[1]);
            return -1;
        } else if (strtobool(tokens[2],&ison)) {
            sprintf(err, "expected argument 2 to be yes/no but got '%s'", tokens[2]);
            return -1;
        }

        if (bus_index < 0 || bus_index > LEN(bus)) {
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

        if (mystrtol(tokens[1],&bitmask)) {
            sprintf(err, "expected argument 1 to be integer but got '%s'", tokens[1]);
            return -1;
        }

        for (i = 0; i < LEN(bus); i++) {
            if (bitmask & (1 << i))
                active[i] = 1;
            else
                active[i] = 0;
        }
    } else if (!strcmp(tokens[0], "debug")) {
        if (ntok != 1) {
            sprintf(err, "debug command expects 1 argument: debug [on/off]");
            return -1;
        }

        if (strtobool(tokens[1],&ison)) {
            sprintf(err, "expected argument 1 to be yes/no but got '%s'", tokens[1]);
            return -1;
        }

        debug = ison;
    } else if (!strcmp(tokens[0], "help")) {
        sprintf(err,"tec_write [card] [tec] [on/off]\n"
                    "hv_write [card] [relay] [on/off]\n"
                    "thermistor_read [bus] [address]\n"
                    "tec_sense_read [bus]\n"
                    "reset\n"
                    "poll [bus] [on/off]\n"
                    "set_active_bitmask [bitmask]\n"
                    "debug [on/off]\n"
                    "set_attenuation [on/off]\n",
                    "step_home\n",
                    "step [steps]\n",
                    "bias_iread\n",
                    "bias_vread\n");
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
    digitalWrite(PIN_STP_SLEEP,true);

    /* FIXME: Need to determine direction. */
    if (steps < 0)
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
        delay(100);
        digitalWrite(PIN_STP_STEP,false);
        delay(100);
    }

    /* Put the stepper back in sleep mode. */
    digitalWrite(PIN_STP_SLEEP,false);

    return 0;
}

int step_home(void)
{
    int i = 0;

    /* Start off with the stepper in sleep mode. */
    digitalWrite(PIN_STP_SLEEP,true);

    /* FIXME: Need to determine direction. */
    digitalWrite(PIN_STP_DIR,true);

    while (++i < MAX_STEPS && digitalRead(PIN_STP_HOME) == false) {
        if (digitalRead(PIN_STP_FAULT) == false) {
            /* Put the stepper back in sleep mode. */
            digitalWrite(PIN_STP_SLEEP,false);

            sprintf(err, "stepper driver detected a fault");
            return -1;
        }
        digitalWrite(PIN_STP_STEP,true);
        delay(100);
        digitalWrite(PIN_STP_STEP,false);
        delay(100);
    }

    /* Put the stepper back in sleep mode. */
    digitalWrite(PIN_STP_SLEEP,false);

    return 0;
}

void loop()
{
    int i;
    float temp = 0;

    while (Serial.available() > 0) {
        if (k >= LEN(cmd) - 1) {
            Serial.print("Error: too many characters in command!\n");
            k = 0;
        }
        cmd[k++] = Serial.read();
        if (cmd[k-1] == '\n') {
            cmd[k-1] = '\0';

            sprintf(msg, "received command: %s\n", cmd);
            Serial.print(msg);

            temp = 0;
            int rv = do_command(cmd, &temp);
            if (rv < 0) {
                sprintf(msg, "-%s\n", err);
                Serial.print(msg);
            } else if (rv > 0) {
                sprintf(msg, "+%.2f\n", temp);
                Serial.print(msg);
            } else {
                sprintf("+ok\n");
                Serial.print(msg);
            }
            k = 0;
        }
    }

    // if there's data available, read a packet
    int packetSize = Udp.parsePacket();
    if (packetSize) {
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
        int rv = do_command(cmd, &temp);
        if (rv < 0) {
            sprintf(msg, "-%s\n", err);
            Serial.print(msg);
        } else if (rv > 0) {
            sprintf(msg, "+%.2f\n", temp);
            Serial.print(msg);
        } else {
            sprintf(msg, "+ok\n");
            Serial.print(msg);
        }

        // send a reply to the IP address and port that sent us the packet we received
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write(msg);
        Udp.endPacket();
    }

    for (i = 0; i < LEN(bus); i++) {
        if (active[i] && poll[i]) {
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
    delay(1000);
}