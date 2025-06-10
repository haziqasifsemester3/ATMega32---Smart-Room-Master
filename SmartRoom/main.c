// Include necessary AVR libraries and headers
#include <avr/io.h>
#define F_CPU 8000000UL
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Include custom libraries for RTC, I2C, LCD, and Serial communication
#include "lib/DS3232_lib.h"
#include "lib/i2c_lib.h"
#include "lib/liquid_crystal_i2c_lib.h"
#include "lib/Serial_lib.h"

// Define bit manipulation macros for easier code
#define SBIT(r, b) (r |= (1 << b))
#define CBIT(r, b) (r &= ~(1 << b))
#define GBIT(r, b) (r & (1 << b))
#define TOGGLE(r, b) (r ^= (1 << b))

// Define stepper motor and lamp port and pin configurations
#define STEPPER_PORT PORTA
#define STEPPER_DDR DDRA

#define LAMP_PORT PORTB
#define LAMP_DDR DDRB
#define LAMP 3

// Max buffer length and stepper rotation directions
#define MAX 200
#define CW 1
#define CCW 0

// Global variables
int counter = 0;

char cmd_key[MAX];
char cmd_first_value[MAX];
char cmd_second_value[MAX];

// Boolean flags for different actions
bool set_time = false;
bool set_lamp = false;
bool open_curtain = false;
bool close_curtain = false;
bool help = false;
bool is_open = false;
bool after_one_second = false;
bool after_one_minute = false;
bool login = false;

LiquidCrystalDevice_t lcd1; // LCD object
DateTime_t t;               // DateTime object for RTC

// Initialize Stepper Motor pins as output
void stepper_init(void)
{
    SBIT(STEPPER_DDR, 0);
    SBIT(STEPPER_DDR, 1);
    SBIT(STEPPER_DDR, 2);
    SBIT(STEPPER_DDR, 3);
}

// Initialize Lamp control pin as output
void lamp_init()
{
    SBIT(LAMP_DDR, LAMP);
}

// Configure Timer1 to overflow every 1 second
void timer1_init()
{
    TIMSK |= (1 << TOIE1);                     // Enable Timer1 overflow interrupt
    TCCR1B = (1 << CS12) | (1 << CS10);        // Prescaler 1024
    TCNT1 = 57723;                             // Initial counter value for 1s overflow
}

// Initialize PWM for lamp brightness using Timer0
void pwm_init()
{
    TCCR0 |= (1 << WGM00) | (1 << COM01) | (1 << CS00) | (1 << CS02); // Fast PWM, non-inverting, prescaler 1024
    OCR0 = 127; // 50% duty cycle
}

// Initialize LCD and RTC and display default time/date and lamp status
void lcd_init(void)
{
    i2c_master_init(I2C_SCL_FREQUENCY_400);
    lcd1 = lq_init((0b01000110 >> 1), 16, 2, LCD_5x8DOTS);

    // Set initial time
    t.Second = 55;
    t.Minute = 30;
    t.Hour = 9;
    t.Day = Sunday;
    t.Date = 12;
    t.Month = 5;
    t.Year = 2025;
    RTC_Set(t);

    // LCD indicators for curtain and lamp
    lq_setCursor(&lcd1, 0, 13);
    lq_print(&lcd1, "OFF");

    lq_setCursor(&lcd1, 1, 13);
    lq_print(&lcd1, "50%");

    // Display current time and date
    t = RTC_Get();
    if (RTC_Status() == RTC_OK)
    {
        lq_setCursor(&lcd1, 0, 0);
        char time_arr[10];
        sprintf(time_arr, "%02d:%02d:%02d", t.Hour, t.Minute, t.Second);
        lq_print(&lcd1, time_arr);

        lq_setCursor(&lcd1, 1, 0);
        char date_arr[10];
        sprintf(date_arr, "%02d/%02d/%02d", t.Year, t.Month, t.Date);
        lq_print(&lcd1, date_arr);
    }
}

// Initialize Serial Communication and prompt for password
void serial_init_final(void)
{
    serial_init();
    serial_send_string("Hello there!");
    serial_send_string("\rEnter your password to continue.\r");
}

// Split input command string into 3 parts
void split_string_by_space(char *rec)
{
    char first_word[MAX], second_word[MAX], third_word[MAX], fourth_word[MAX];

    strcpy(first_word, "");
    strcpy(second_word, "");
    strcpy(third_word, "");
    strcpy(fourth_word, "");

    sscanf(rec, "%s %s %s %s", first_word, second_word, third_word, fourth_word);

    sprintf(cmd_key, "%s %s", first_word, second_word);
    sprintf(cmd_first_value, "%s", third_word);
    sprintf(cmd_second_value, "%s", fourth_word);
}

// Extract and set date and time values from string input
void set_date_and_time(char *time, char *date)
{
    int hour, minute, second;
    int day, month, year;

    sscanf(time, "%d:%d:%d", &hour, &minute, &second);
    sscanf(date, "%d/%d/%d", &month, &day, &year);

    t.Second = second;
    t.Minute = minute;
    t.Hour = hour;
    t.Day = Sunday;
    t.Date = day;
    t.Month = month;
    t.Year = year + 2000;
}

// Update PWM duty cycle
void set_duty_cycle(int duty_cycle)
{
    OCR0 = 255 * duty_cycle / 100;
}

// Rotate stepper motor 4-step full sequence
void rotate_stepper(int steps, int direction)
{
    int step_sequence[4] = {0x04, 0x02, 0x08, 0x01};
    for (int i = 0; i < steps; i++)
    {
        if (direction == CW)
        {
            for (int step = 0; step < 4; step++)
            {
                STEPPER_PORT = step_sequence[step];
                _delay_ms(50);
            }
        }
        else if (direction == CCW)
        {
            for (int step = 3; step >= 0; step--)
            {
                STEPPER_PORT = step_sequence[step];
                _delay_ms(50);
            }
        }
    }
}

// Rotate stepper motor 3-step sequence (used for fine movement)
void rotate_stepper_three_quarter(int steps, int direction)
{
    int step_sequence[3] = {0x04, 0x02, 0x08};
    for (int i = 0; i < steps; i++)
    {
        if (direction == CW)
        {
            for (int step = 0; step < 3; step++)
            {
                STEPPER_PORT = step_sequence[step];
                _delay_ms(50);
            }
        }
        else if (direction == CCW)
        {
            for (int step = 2; step >= 0; step--)
            {
                STEPPER_PORT = step_sequence[step];
                _delay_ms(50);
            }
        }
    }
}

// Execute 'set time' command
void function_set_time(void)
{
    set_date_and_time(cmd_first_value, cmd_second_value);
    RTC_Set(t);
    t = RTC_Get();
    if (RTC_Status() == RTC_OK)
    {
        lq_setCursor(&lcd1, 0, 0);
        char time_arr[10];
        sprintf(time_arr, "%02d:%02d:%02d", t.Hour, t.Minute, t.Second);
        lq_print(&lcd1, time_arr);

        lq_setCursor(&lcd1, 1, 0);
        char date_arr[10];
        sprintf(date_arr, "%02d/%02d/%02d", t.Year, t.Month, t.Date);
        lq_print(&lcd1, date_arr);
    }
    serial_send_string("\rDone.\r");
}

// Execute 'set lamp' command
void function_set_lamp(void)
{
    int duty_cycle = atoi(cmd_first_value);
    set_duty_cycle(duty_cycle);

    lq_setCursor(&lcd1, 1, 12);
    char duty_cycle_arr[10];
    sprintf(duty_cycle_arr, "%3d%%", duty_cycle);
    lq_print(&lcd1, duty_cycle_arr);
    serial_send_string("\rDone.\r");
}

// Execute 'open curtain' command
void function_open_curtain(void)
{
    if (!is_open)
    {
        serial_send_string("\rOpening...\r");
        rotate_stepper(9, CW);
        rotate_stepper_three_quarter(1, CW);
        lq_setCursor(&lcd1, 0, 13);
        lq_print(&lcd1, "ON ");
        serial_send_string("\rDone.\r");
        is_open = true;
    }
    else
    {
        serial_send_string("\rThe curtain is already open!\r");
    }
}

// Execute 'close curtain' command
void function_close_curtain(void)
{
    if (is_open)
    {
        serial_send_string("\rClosing...\r");
        rotate_stepper_three_quarter(1, CCW);
        rotate_stepper(9, CCW);
        lq_setCursor(&lcd1, 0, 13);
        lq_print(&lcd1, "OFF");
        serial_send_string("\rDone.\r");
        is_open = false;
    }
    else
    {
        serial_send_string("\rThe curtain is already close!\r");
    }
}

// Show help commands to user
void function_help(void)
{
    serial_send_string("\r******  << Help >>  ******\r");
    serial_send_string("\r   set time (XX:YY:ZZ M/D/Y)\r");
    serial_send_string("-> Sets the desired time\r");
    serial_send_string("\r   set lamp (0 to 100)\r");
    serial_send_string("-> Sets the room brightness\r");
    serial_send_string("\r   open curtain\r");
    serial_send_string("-> Opens the curtain\r");
    serial_send_string("\r   close curtain\r");
    serial_send_string("-> Closes the curtain\r");
}

// Update LCD every second
void function_after_one_second(void)
{
    t = RTC_Get();
    if (RTC_Status() == RTC_OK)
    {
        lq_setCursor(&lcd1, 0, 0);
        char time_arr[10];
        sprintf(time_arr, "%02d:%02d:%02d", t.Hour, t.Minute, t.Second);
        lq_print(&lcd1, time_arr);

        lq_setCursor(&lcd1, 1, 0);
        char date_arr[10];
        sprintf(date_arr, "%02d/%02d/%02d", t.Year, t.Month, t.Date);
        lq_print(&lcd1, date_arr);
    }
}

// Logout user after 1-minute inactivity
void function_session_timeout(void)
{
    login = false;
    serial_send_string("\rSession timeout!");
    serial_send_string("\rEnter your password to continue.\r");
    counter = 0;
}

// Handle password input character by character
void password_validation(char c)
{
    static uint8_t index = 0;
    char rec[MAX];

    if (c == '\r')
        rec[index] = '\0';
    else
        rec[index] = c;

    if (rec[index] == '\0')
    {
        if (strcmp(rec, "1234") == 0)
        {
            login = true;
            after_one_minute = false;
            counter = 0;
            serial_send_string("\rWelcome back, I'm ready!\r");
        }
        else
        {
            serial_send_string("\rIncorrect password!\r");
        }
        strcpy(rec, "");
        index = 0;
    }
    else
    {
        index++;
    }
}

// Handle command input after login
void get_user_cmd(char c)
{
    static uint8_t index = 0;
    char rec[MAX];

    if (c == '\r')
        rec[index] = '\0';
    else
        rec[index] = c;

    if (rec[index] == '\0')
    {
        split_string_by_space(rec);
        if (strcmp(cmd_key, "set time") == 0)
            set_time = true;
        else if (strcmp(cmd_key, "set lamp") == 0)
            set_lamp = true;
        else if (strcmp(cmd_key, "open curtain") == 0)
            open_curtain = true;
        else if (strcmp(cmd_key, "close curtain") == 0)
            close_curtain = true;
        else if (strcmp(rec, "help") == 0)
            help = true;
        else
            serial_send_string("\rError!\r");

        strcpy(rec, "");
        index = 0;
    }
    else
    {
        index++;
    }
}

// Main function
int main(void)
{
    stepper_init();
    lamp_init();
    timer1_init();
    pwm_init();
    lcd_init();
    serial_init_final();

    sei(); // Enable global interrupts

    while (1)
    {
        // Execute commands based on user input
        if (set_time)       { function_set_time(); set_time = false; }
        if (set_lamp)       { function_set_lamp(); set_lamp = false; }
        if (open_curtain)   { function_open_curtain(); open_curtain = false; }
        if (close_curtain)  { function_close_curtain(); close_curtain = false; }
        if (help)           { function_help(); help = false; }
        if (after_one_second) { function_after_one_second(); after_one_second = false; }
        if (after_one_minute && login) { function_session_timeout(); after_one_minute = false; }
    }
}

// Timer1 overflow ISR for 1-second tracking
ISR(TIMER1_OVF_vect)
{
    TCNT1 = 57723; // Reset timer
    TIFR = (1 << TOV1); // Clear overflow flag
    counter++;
    after_one_second = true;
    if (counter == 60) after_one_minute = true;
}

// USART interrupt for character receiving
#if SERIAL_INTERRUPT == 1
ISR(USART_RXC_vect)
{
    char c = UDR;
    UDR = c; // Echo
    counter = 0;

    if (login)
        get_user_cmd(c);
    else
        password_validation(c);
}
#endif
