#include <Wire.h>
#include "PCA9557.h"
#include "AD5593R.h"

#define PCA9557_DEBUG

int debug = 1;

#define KC1 6
#define KC2 7
#define KC3 4
#define KC4 5
#define KC5 2
#define KC6 3
#define ADC 1
#define TEC_CTRL1 4
#define TEC_CTRL2 5
#define TEC_CTRL3 6

int hv_relays[6] = {KC1,KC2,KC3,KC4,KC5,KC6};
int hv_relay_names[6] = {"KC1","KC2","KC3","KC4","KC5","KC6"};

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

AD5593R gpio(-1);
bool my_DACs[8] = {0,0,0,0,1,1,1,1};
bool my_ADCs[8] = {1,1,1,1,0,0,0,0};

void setup()
{
    int i, j;
    char msg[100];

    Serial.begin(9600);
    Wire.begin();

    /* Loop over each of the 3 module boards. */
    for (i = 0; i < 2; i++) {
        /* Loop over the six HV relays. */
        for (j = 0; j < 6; j++) {
            /* Set the pin to be an output.

             * Note: I have the code here set up to be able to detect an error,
             * but it turns out for some reason that it never errors and if it
             * can't talk to the chip it just resets the whole Teensy. I tried
             * setting a timeout, but it seems the wire library for the teensy
             * doesn't have the timeout function added yet. I'm going to keep
             * this code here in case we figure out some way to update it. */
            if (!bus[i].pinMode(hv_relays[j],OUTPUT)) {
                sprintf("Error setting pin mode for address 0x%1x %s. Aborting...\n", i, hv_relay_names[j]);
                Serial.print(msg);
                break;
            }
            delay(500);
            set_relay(i, hv_relays[j], LOW);
            delay(500);
        }
        /* Set the pin which sets the address for the GPIO chip to be an
         * output. */
        if (!bus[i].pinMode(ADC,OUTPUT)) {
            sprintf("Error setting pin mode for address 0x%1x ADC. Aborting...\n", i);
            Serial.print(msg);
        }

        /* Set the address pin HIGH so we don't talk to multiple chips at once.
         */
        set_relay(i, ADC, HIGH);
        delay(500);
    }

    /* Loop over each of the 3 module boards. */
    for (i = 0; i < 2; i++) {
        /* Enable the ADC pin so we can talk to this chip. */
        set_relay(i, ADC, LOW);
        delay(500);
        gpio.configure_DACs(my_DACs);
        gpio.configure_ADCs(my_ADCs);
        gpio.enable_internal_Vref();
        gpio.set_DAC_max_2x_Vref();
        gpio.set_ADC_max_2x_Vref();
        if (gpio.write_DAC(TEC_CTRL1,0) < 0) {
            Serial.print("error setting DAC!\n");
        }
        if (gpio.write_DAC(TEC_CTRL2,0) < 0) {
            Serial.print("error setting DAC!\n");
        }
        if (gpio.write_DAC(TEC_CTRL3,0) < 0) {
            Serial.print("error setting DAC!\n");
        }
        delay(500);
        /* Always need to set it back low at the end to avoid talking to
         * multiple chips. */
        set_relay(i, ADC, HIGH);
    }
}

int i = 0;

void set_tec_relay(int bus_index, int address, bool value)
{
    /* Enable the ADC pin so we can talk to this chip. */
    set_relay(bus_index, ADC, LOW);
    if (gpio.write_DAC(address,value ? 3.3 : 0) < 0) {
        Serial.print("error setting DAC!\n");
    }
    set_relay(bus_index, ADC, HIGH);
}

/* Turn on/off a relay on the PCA9557. Address should be the address of the
 * relay, and value should be ON/OFF. */
void set_relay(int bus_index, int address, int value)
{
    if (debug) {
        char msg[100];
        sprintf(msg,"Setting bus 0x%1x address 0x%08x to %s\n", bus_index, address, value ? "HIGH" : "LOW");
        Serial.print(msg);
    }
    bus[bus_index].digitalWrite(address,value);
}

void loop()
{
    set_relay(0, ADC,i++ % 2 ? LOW : HIGH);
    set_tec_relay(0,TEC_CTRL1,true);
    delay(1000);
}
