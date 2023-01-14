/* Firmware to control the BTL QA/QC board at Caltech.
 * 
 * Anthony LaTorre <alatorre@caltech.edu>
 * Lautaro Narvaez <lautaro@caltech.edu> */
#include <Wire.h>
#include "PCA9557.h"
#include "AD5593R.h"

#define LEN(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

#define PCA9557_DEBUG

/* Generic delay after setting pins high/low. */
#define DELAY 100

char cmd[256];
char err[256];
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

/* Array of HV relay pins and names. */
int hv_relays[6] = {KC1,KC2,KC3,KC4,KC5,KC6};
int hv_relay_names[6] = {"KC1","KC2","KC3","KC4","KC5","KC6"};
int tec_relays[3] = {TEC_CTRL1, TEC_CTRL2, TEC_CTRL3};
int tec_relay_names[3] = {"TEC_CTRL1", "TEC_CTRL2", "TEC_CTRL3"};
int thermistors[4] = {THERMISTOR1, THERMISTOR2, THERMISTOR3, TEC_SENSE};
int thermistor_names[4] = {"THERMISTOR1", "THERMISTOR2", "THERMISTOR3", "TEC_SENSE"};

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
    char msg[100];

    rv = 0;

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
            bus_write(i, hv_relays[j], LOW);
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
        bus_write(i, ADC, HIGH);
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
        bus_write(i, ADC, LOW);
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
        bus_write(i, ADC, HIGH);
    }

    return rv;
}

void setup()
{
    Serial.begin(9600);
    Wire.begin();

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
        char msg[100];
        sprintf(msg,"Setting bus 0x%1x address 0x%08x to %s\n", bus_index, address, value ? "HIGH" : "LOW");
        Serial.print(msg);
    }
    if (!bus[bus_index].digitalWrite(address,value))
        return -1;
    else
        return 0;
}

bool poll[8] = {false,false,false,false,false,false,false,false};

/* Performs a user command based on a string. Examples of commands:
 *
 * "tec_write 1 0 1" - turn TEC 0 on on board 1
 * "tec_write 2 1 0" - turn TEC 1 off on board 2
 * "hv_write 2 1 0" - turn HV relay 1 off on board 2
 *
 * Returns 0 or 1 on success, -1 on error. Returns 0 if there is no return
 * value, 1 if there is a return value. If there is an error, the global `err`
 * string contains an error message. */
int do_command(char *cmd, float *value)
{
    int i = 0;
    char *tokens[10];
    char *tok;

    tok = strtok(cmd, " ");
    while (tok != NULL && i <= LEN(tokens) - 1) {
        tokens[i++] = tok;
        tok = strtok(NULL, " ");
    }

    if (!strcmp(tokens[0], "tec_write")) {
        int bus_index = atoi(tokens[1]);
        int address = atoi(tokens[2]);
        int value = atoi(tokens[3]);
        if (bus_index < 0 || bus_index > LEN(bus)) {
            sprintf(err, "bus index %i is not valid", bus_index);
            return -1;
        } else if (!active[bus_index]) {
            sprintf(err, "bus index %i is not active", bus_index);
            return -1;
        } else if (address < 0 || address > LEN(tec_relays)) {
            sprintf(err, "address %i is not valid", address);
            return -1;
        } else if (value < 0 || value > 1) {
            sprintf(err, "value %i must be 0 or 1", value);
            return -1;
        }
        gpio_write(bus_index,tec_relays[address],value);
    } else if (!strcmp(tokens[0], "hv_write")) {
        int bus_index = atoi(tokens[1]);
        int address = atoi(tokens[2]);
        int value = atoi(tokens[3]);
        if (bus_index < 0 || bus_index > LEN(bus)) {
            sprintf(err, "bus index %i is not valid", bus_index);
            return -1;
        } else if (!active[bus_index]) {
            sprintf(err, "bus index %i is not active", bus_index);
            return -1;
        } else if (address < 0 || address > LEN(hv_relays)) {
            sprintf(err, "address %i is not valid", address);
            return -1;
        } else if (value < 0 || value > 1) {
            sprintf(err, "value %i must be 0 or 1", value);
            return -1;
        }
        bus_write(bus_index,hv_relays[address],value);
    } else if (!strcmp(tokens[0], "thermistor_read")) {
        int bus_index = atoi(tokens[1]);
        int address = atoi(tokens[2]);
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
        int bus_index = atoi(tokens[1]);
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
        if (reset())
            return -1;
    } else if (!strcmp(tokens[0], "poll")) {
        int bus_index = atoi(tokens[1]);
        int value = atoi(tokens[2]);

        if (bus_index < 0 || bus_index > LEN(bus)) {
            sprintf(err, "bus index %i is not valid", bus_index);
            return -1;
        } else if (!active[bus_index]) {
            sprintf(err, "bus index %i is not active", bus_index);
            return -1;
        }

        if (value)
            poll[bus_index] = true;
        else
            poll[bus_index] = false;
    } else if (!strcmp(tokens[0], "set_active_bitmask")) {
        int bitmask = atoi(tokens[1]);

        for (i = 0; i < LEN(bus); i++) {
            if (bitmask & (1 << i))
                active[i] = 1;
            else
                active[i] = 0;
        }
    } else if (!strcmp(tokens[0], "debug")) {
        int value = atoi(tokens[1]);

        if (value)
            debug = true;
        else
            debug = false;
        }
    } else {
        sprintf(err, "unknown command '%s'", tokens[0]);
        return -1;
    }

    return 0;
}

void loop()
{
    int i;
    char msg[100];
    float temp = 0;

    while (Serial.available() > 0) {
        if (k >= LEN(cmd) - 1) {
            Serial.print("Error: too many characters in command!\n");
            k = 0;
        }
        cmd[k++] = Serial.read();
        if (cmd[k-1] == '\n') {
            cmd[k-1] = '\0';
            temp = 0;
            int rv = do_command(cmd, &temp);
            if (rv < 0) {
                sprintf(msg, "%s\n", err);
                Serial.print(msg);
            } else if (rv > 0) {
                sprintf(msg, "%.2f\n", temp);
                Serial.print(msg);
            } else {
                Serial.print("ok\n");
            }
            k = 0;
        }
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
