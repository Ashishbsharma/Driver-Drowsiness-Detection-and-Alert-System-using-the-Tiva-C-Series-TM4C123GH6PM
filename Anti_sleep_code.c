#include "TM4C123.h"

#define UART0_BAUDRATE 115200
#define IR_SENSOR_PIN 0x04  // PA2 (IR Sensor input, active-low)
#define BUZZER_PIN 0x04     // PF2 (Onboard Buzzer)
#define RELAY_PIN 0x08      // PB3 (Relay Control)
#define VIBRATOR_PIN 0x20   // PB5 (Vibrator Control)
#define LED_RED 0x02        // PF1 (Onboard Red LED for debugging)
#define LED_GREEN 0x08      // PF3 (Onboard Green LED for debugging)

// Function prototypes
void delayMs(int n);
void delayUs(int n);
void initSystem(void);
void initGPIO(void);
void initUART(void);
void UART0_SendChar(char data);
void UART0_SendString(const char *string);
void beepBuzzerDiscrete(int frequency, int duration, int interval, int repetitions);
void controlRelay(uint8_t state);
void controlVibrator(uint8_t state);

int main(void) {
    initSystem();
    initGPIO();
    initUART();

    UART0_SendString("System initialized. Monitoring started.\n");

    uint32_t eyeClosedStartTime = 0;
    uint8_t currentLevel = 0;
    uint8_t previousState = 0; // 0: Eyes open, 1: Eyes closed

    while (1) {
        uint8_t eyesClosed = (GPIOA->DATA & IR_SENSOR_PIN) == 0; // Active-low IR sensor

        if (eyesClosed) {
            if (previousState == 0) {
                UART0_SendString("Eyes closed detected.\n");
                previousState = 1; // Update state to eyes closed
            }

            GPIOF->DATA |= LED_RED; // Turn ON Red LED (Eyes closed)

            if (eyeClosedStartTime == 0) {
                eyeClosedStartTime = 1; // Start timing
            } else {
                eyeClosedStartTime++;
            }

            if (eyeClosedStartTime >= 30 && eyeClosedStartTime < 60) { // 3 to 6 seconds
                if (currentLevel < 1) {
                    currentLevel = 1;
                    beepBuzzerDiscrete(100, 100, 150, 10); // Faster beeping at 1.5 kHz
                    UART0_SendString("Level 1 Alert: Eyes closed for 3 seconds.\n");
                }
                GPIOF->DATA |= LED_GREEN; // Turn ON Green LED for Level 1
                controlRelay(1);          // Keep Relay ON
                controlVibrator(0);       // Vibrator OFF
            } else if (eyeClosedStartTime >= 60 && eyeClosedStartTime < 90) { // 6 to 9 seconds
                if (currentLevel < 2) {
                    currentLevel = 2;
                    beepBuzzerDiscrete(200, 80, 100, 15); // Even faster beeping at 2.5 kHz
                    UART0_SendString("Level 2 Alert: Eyes closed for 6 seconds.\n");
                }
                GPIOF->DATA |= LED_GREEN; // Turn ON Green LED for Level 2
                controlRelay(1);          // Keep Relay ON
                controlVibrator(0);       // Vibrator OFF
            } else if (eyeClosedStartTime >= 90 && eyeClosedStartTime < 120) { // 9 to 12 seconds
                if (currentLevel < 3) {
                    currentLevel = 3;
                    beepBuzzerDiscrete(300, 50, 70, 20); // Rapid beeping at 3.5 kHz
                    controlVibrator(1);    // Vibrator ON
                    UART0_SendString("Level 3 Alert: Eyes closed for 9 seconds.\n");
                }
                GPIOF->DATA |= LED_GREEN; // Turn ON Green LED for Level 3
                controlRelay(1);          // Keep Relay ON
            } else if (eyeClosedStartTime >= 120) { // After 12 seconds
                controlRelay(0);          // Turn OFF Relay (Motor OFF)
                controlVibrator(0);       // Turn OFF Vibrator
                UART0_SendString("Level 4 Alert: Eyes closed for 12 seconds. Motor turned OFF.\n");
            }
        } else {
            if (previousState == 1) {
                UART0_SendString("Eyes open. Resetting alerts.\n");
                previousState = 0; // Update state to eyes open
            }

            GPIOF->DATA &= ~(LED_RED | LED_GREEN); // Turn OFF LEDs
            controlRelay(1);                      // Turn ON Relay (Motor ON)
            controlVibrator(0);                   // Turn OFF Vibrator
            eyeClosedStartTime = 0;
            currentLevel = 0;
        }

        delayMs(100); // Small delay for stabilization
    }
}

// Initialize system clock and peripherals
void initSystem(void) {
    SYSCTL->RCGCGPIO |= 0x23; // Enable clock for Port A, B, and F
    while ((SYSCTL->PRGPIO & 0x23) == 0); // Wait until ready
}

void initGPIO(void) {
    GPIOA->DIR &= ~IR_SENSOR_PIN; // PA2 as input
    GPIOA->DEN |= IR_SENSOR_PIN;  // Enable digital function on PA2
    GPIOB->DIR |= RELAY_PIN | VIBRATOR_PIN; // PB3, PB5 as outputs
    GPIOB->DEN |= RELAY_PIN | VIBRATOR_PIN;
    GPIOF->DIR |= LED_RED | LED_GREEN | BUZZER_PIN; // PF1, PF2, PF3 as outputs
    GPIOF->DEN |= LED_RED | LED_GREEN | BUZZER_PIN;
}

void initUART(void) {
    SYSCTL->RCGCUART |= 0x01; // Enable UART0
    SYSCTL->RCGCGPIO |= 0x01; // Enable Port A
    UART0->CTL &= ~0x01; // Disable UART
    UART0->IBRD = 27;    // Integer baud rate
    UART0->FBRD = 8;     // Fractional baud rate
    UART0->LCRH = 0x70;  // 8-bit, no parity, 1 stop bit
    UART0->CTL |= 0x301; // Enable UART, TX, RX
    GPIOA->AFSEL |= 0x03;
    GPIOA->DEN |= 0x03;
    GPIOA->PCTL = (GPIOA->PCTL & 0xFFFFFF00) + 0x00000011;
    GPIOA->AMSEL &= ~0x03;
}

void UART0_SendChar(char data) {
    while ((UART0->FR & (1U << 5)) != 0);
    UART0->DR = data;
}

void UART0_SendString(const char *string) {
    while (*string) {
        UART0_SendChar(*string);
        string++;
    }
    // Add newline and carriage return
    UART0_SendChar('\r');
    UART0_SendChar('\n');
}


void beepBuzzerDiscrete(int frequency, int duration, int interval, int repetitions) {
    int halfPeriod = 1000000 / frequency;
    for (int i = 0; i < repetitions; i++) {
        GPIOF->DATA |= BUZZER_PIN;
        delayUs(halfPeriod);
        GPIOF->DATA &= ~BUZZER_PIN;
        delayUs(halfPeriod);
        delayMs(interval);
    }
}

void controlRelay(uint8_t state) {
    if (state)
        GPIOB->DATA |= RELAY_PIN;
    else
        GPIOB->DATA &= ~RELAY_PIN;
}

void controlVibrator(uint8_t state) {
    if (state)
        GPIOB->DATA |= VIBRATOR_PIN;
    else
        GPIOB->DATA &= ~VIBRATOR_PIN;
}

void delayMs(int n) {
    for (int i = 0; i < n * 3180; i++)
        __asm("NOP");
}

void delayUs(int n) {
    for (int i = 0; i < n * 3; i++)
        __asm("NOP");
}
