// Code for SAE 2019 motorsports car
// Created by Dominik Chraca

#include <IFCT.h>

CAN_message_t RX_msg, TX_msg;
// Pins
//Aero
byte sig_8_2_on_off = 0;
byte aero_button = 5; // To signal the flaps
byte servo_pwm = 8;
//electrical
byte sig_start_button = 2;
byte sig_buzzer = 6;
byte sig_break_light = 7;
byte precharge = 9;
byte sig_AMS_LED = 10;
byte sig_IMD_LED = 10;
byte sig_R_FAN = 25;
byte sig_L_FAN = 24;
byte MC_POWER = 26;
byte accelerator_1 = A2;
byte accelerator_2 = A4;
byte prcharge_sense = A3;
byte steering = A1;
byte BREAK = A0;
byte PUMP_out = A21;

// gyro data
byte accel_SDA = 38;
byte accel_SCL = 37;
//Pump
byte SIG_PUMP_ANALOG = A21;
byte sig_pump_on_off = 1;

// ID'S
byte ID_temp_1 = 0x0A0;
byte ID_temp_2 = 0x0A1;
byte ID_temp_3 = 0x0A2;
byte ID_motor_poition = 0x0A5;
byte ID_current = 0x0A6;
byte ID_voltage = 0x0A7;
byte ID_faults = 0x0AB;

struct Motor_controller_CAN_data {
    byte id_offset = 0;

    int temp_phase_A = 0;
    int temp_phase_B = 0;
    int temp_phase_C = 0;
    int temp_driver_board = 0;

    int temp_control_board = 0;

    int temp_motor = 0;

    int sensor_angle = 0;
    int angular_velocity = 0;
    int electrical_frequncy = 0;

    int current_PA = 0;
    int current_PB = 0;
    int current_PC = 0;
    int current_DC = 0;

    int voltage_DC = 0;
    int voltage_output = 0;
    int voltage_AB = 0;
    int voltage_BC = 0;

    bool faults[8][8];

} motor_0, motor_1;

char *faults_decoder[8][8] = {
    {"Harawre Gate/Desaturation Fault",
     "HW over-current Fault",
     "Accelerator shorted",
     "Accelerator Open",
     "Current sensor Low",
     "Current sensor High",
     "Module Tempature Low",
     "Module Tempature High"},
    {"Control PCB Temperature Low",
     "Control PCB Temperature High",
     "Gate Dive PCB Temperature Low",
     "Gate Dive PCB Temperature High",
     "5V Sense Voltage Low",
     "5V Sense Voltage High",
     "12V Sense Voltage Low",
     "12V Sense Voltage High"},
    {"2.5V Sense Voltage Low",
     "2.5V Sense Voltage High",
     "1.5V Sense Voltage Low",
     "2.5V Sense Voltage High",
     "DC Bus Voltage High",
     "DC Bus Voltage Low",
     "Precharge Timeout",
     "Precharge Voltage Failure"},
    {"EEPROM Checksum Invalid",
     "EEPROM Data Out of Range",
     "EEPROM Update Required",
     "Reserved",
     "Reserved",
     "Reserved",
     "Brake Shorted",
     "Brake Open"},
    {"Motor Over-speed Fault",
     "Over-current Fault",
     "Over-voltage Fault",
     "Inverter Over-temperature Fault",
     "Accelerator Input Shorted Fault",
     "Accelerator Input Open Fault",
     "Direction Command Fault",
     "Inverter Response Time-out Fault"},
    {"Hardware Gate/Desaturation Fault_2",
     "Hardware Over-current Fault_2",
     "Under-voltage Fault",
     "CAN Command Message Lost Fault",
     "Motor Over-temperature Fault",
     "Reserved",
     "Reserved",
     "Reserved"},
    {"Brake Input Shorted Fault",
     "Brake Input Open Fault",
     "Module A Over-temperature Fault7",
     "Module B Over-temperature Fault7",
     "Module C Over-temperature Fault7",
     "PCB Over-temperature Fault7",
     "Gate Drive Board 1 Over-temperature Fault",
     "Gate Drive Board 2 Over-temperature Fault7"},
    {"Gate Drive Board 3 Over-temperature Fault7",
     "Current Sensor Fault",
     "Reserved",
     "Reserved",
     "Reserved",
     "Reserved",
     "Resolver Not Connected",
     "Inverter Discharge Active"}};

//Global variables
bool enable_bit = 0;

void setup() {
    // Set ID offset
    motor_1.id_offset = 0x30;
    motor_0.id_offset = 0x0;

    //Setup for pinouts and pinins
    pinMode(sig_8_2_on_off, OUTPUT);
    pinMode(sig_buzzer, OUTPUT);
    pinMode(sig_break_light, OUTPUT);
    pinMode(servo_pwm, OUTPUT);
    pinMode(precharge, OUTPUT); // PWM before precharge is done
    pinMode(sig_AMS_LED, OUTPUT);
    pinMode(sig_IMD_LED, OUTPUT);
    pinMode(sig_R_FAN, OUTPUT);
    pinMode(sig_L_FAN, OUTPUT);
    pinMode(MC_POWER, OUTPUT);
    pinMode(sig_pump_on_off, OUTPUT);

    pinMode(aero_button, INPUT); // Front of dash
    pinMode(sig_start_button, INPUT);

    // Sets all faults to zero
    for (int i = 0; i < 8; ++i) {
        for (int k = 0; k < 8; ++k) {
            motor_1.faults[i][k] = 0;
            motor_0.faults[i][k] = 0;
        }
    }

    pinMode(13, OUTPUT); // On board LED to know if code is running
    digitalWrite(13, HIGH);
    Can1.setBaudRate(500000);
    Can1.enableFIFO();
}

void loop() {
    // Reads all CAN data
    read_can();
    // Completes all the logic for the car
    all_logic();
    // Takes avarge with error check and sends the position for the motor controller.
    int accelerator_value = read_accelerator_value();
    
    write_speed(accelerator_value, 1, enable_bit, motor_1.id_offset); // Motor 1(2) right
    write_speed(accelerator_value, 0, enable_bit, motor_0.id_offset); // Motor 0(1) left
}

void all_logic() {
    // Check if start button is pressed
    if (digitalRead(sig_start_button) && !enable_bit) // The button is not pressed then the car should not be enabled
    {
        enable_bit = 1; // Let the car be able to move
    }
    // Moves the flaps when the button is pressed
    if (digitalRead(aero_button)) {
        analogWrite(servo_pwm, 0);
        digitalWrite(sig_8_2_on_off, HIGH);
    } else {
        digitalWrite(sig_8_2_on_off, LOW);
        analogWrite(servo_pwm, 0);
    }
}

int read_accelerator_value() {
    int avg_sensor = 0;
    int sensorValue_1 = analogRead(accelerator_1);
    int sensorValue_2 = analogRead(accelerator_2);
    if ((sensorValue_1 < 5) || (sensorValue_2 < 5)) // To check and clean if the value is jumping around
    {
        sensorValue_1 = 0;
        sensorValue_2 = 0;
    }
    float errorcheck = ((abs(sensorValue_1 - sensorValue_2)) / sensorValue_2); // Percent error
    if ((errorcheck <= 0.1))                                                   // if the error is less than 10%
    {
        avg_sensor = (sensorValue_1 + sensorValue_2) / 2;
    } else {
        Serial.println("Error accelerator reading");
    }
    return avg_sensor;
}

void write_speed(int m_speed, bool m_direction, bool enable_pin, int id_off) // Max torque speed is 100 NM || 0 = Clockwise  1 = CounterClockwise
{
    (m_speed > 860) ? m_speed = 860 : 1;
    int percent_speed = map(m_speed, 0, 860, 0, 100);   // Converts analog to motor values (NM) || 100NM = 1000 in Code
    if ((percent_speed < 1000) && (percent_speed >= 0)) // Checks if negative or above 100 NM
    {
        //Calculations value = (high_byte x 256) + low_byte
        byte low_byte = percent_speed % 256;
        byte high_byte = percent_speed / 256;

        //Setting up sending data parameters
        TX_msg.ext = 0;
        TX_msg.id = 0x0C0 + id_off; // Command message ID
        TX_msg.len = 8;
        TX_msg.buf[0] = low_byte; // NM
        TX_msg.buf[1] = high_byte;
        TX_msg.buf[2] = 0; // Speed
        TX_msg.buf[3] = 0;
        TX_msg.buf[4] = m_direction; // Direction
        TX_msg.buf[5] = enable_pin;  // Inverter enable byte
        TX_msg.buf[6] = 0;           // Last two are the maximum torque values || if 0 then defualt values are set
        TX_msg.buf[7] = 0;

        Can1.write(TX_msg);
    }

    else {
        Serial.println("Exceeding max torque value within write speed function.");
    }
}

void read_can() {
    //canSniff(RX_msg); // Check data
    if (Can1.read(RX_msg)) {
        //Serial.println("Motor 1");
        //canSniff(RX_msg); // Check data

        motor_1.temp_phase_A = read_signed_data(ID_temp_1 + motor_1.id_offset, 0, motor_1.temp_phase_A); // C
        motor_1.temp_phase_B = read_signed_data(ID_temp_1 + motor_1.id_offset, 1, motor_1.temp_phase_B);
        motor_1.temp_phase_C = read_signed_data(ID_temp_1 + motor_1.id_offset, 2, motor_1.temp_phase_C);
        motor_1.temp_driver_board = read_signed_data(ID_temp_1 + motor_1.id_offset, 3, motor_1.temp_driver_board);

        motor_1.temp_control_board = read_signed_data(ID_temp_2 + motor_1.id_offset, 0, motor_1.temp_control_board);

        motor_1.temp_motor = read_signed_data(ID_temp_3 + motor_1.id_offset, 2, motor_1.temp_motor);

        motor_1.sensor_angle = read_signed_data(ID_motor_poition + motor_1.id_offset, 0, motor_1.sensor_angle);                    // degrees
        motor_1.angular_velocity = read_signed_data(ID_motor_poition + motor_1.id_offset, 1, motor_1.angular_velocity);            // RPM
        motor_1.electrical_frequncy = read_signed_data(ID_motor_poition + motor_1.id_offset, 2, motor_1.electrical_frequncy) / 10; // Hz

        motor_1.current_PA = read_signed_data(ID_current + motor_1.id_offset, 0, motor_1.current_PA); // Amps
        motor_1.current_PB = read_signed_data(ID_current + motor_1.id_offset, 1, motor_1.current_PB);
        motor_1.current_PC = read_signed_data(ID_current + motor_1.id_offset, 2, motor_1.current_PC);
        motor_1.current_DC = read_signed_data(ID_current + motor_1.id_offset, 3, motor_1.current_DC);

        motor_1.voltage_DC = read_signed_data(ID_voltage + motor_1.id_offset, 0, motor_1.voltage_DC); // Volts
        motor_1.voltage_output = read_signed_data(ID_voltage + motor_1.id_offset, 1, motor_1.voltage_output);
        motor_1.voltage_AB = read_signed_data(ID_voltage + motor_1.id_offset, 2, motor_1.voltage_AB);
        motor_1.voltage_BC = read_signed_data(ID_voltage + motor_1.id_offset, 3, motor_1.voltage_BC);

        // Read faults
        read_fault_data_motor_1();
    } else if (Can0.read(RX_msg)) {
        //Serial.println("Motor 0");
        //canSniff(RX_msg); // Check data

        motor_0.temp_phase_A = read_signed_data(ID_temp_1 + motor_0.id_offset, 0, motor_0.temp_phase_A); // C
        motor_0.temp_phase_B = read_signed_data(ID_temp_1 + motor_0.id_offset, 1, motor_0.temp_phase_B);
        motor_0.temp_phase_C = read_signed_data(ID_temp_1 + motor_0.id_offset, 2, motor_0.temp_phase_C);
        motor_0.temp_driver_board = read_signed_data(ID_temp_1 + motor_0.id_offset, 3, motor_0.temp_driver_board);

        motor_0.temp_control_board = read_signed_data(ID_temp_2 + motor_0.id_offset, 0, motor_0.temp_control_board);

        motor_0.temp_motor = read_signed_data(ID_temp_3 + motor_0.id_offset, 2, motor_0.temp_motor);

        motor_0.sensor_angle = read_signed_data(ID_motor_poition + motor_0.id_offset, 0, motor_0.sensor_angle);                    // degrees
        motor_0.angular_velocity = read_signed_data(ID_motor_poition + motor_0.id_offset, 1, motor_0.angular_velocity);            // RPM
        motor_0.electrical_frequncy = read_signed_data(ID_motor_poition + motor_0.id_offset, 2, motor_0.electrical_frequncy) / 10; // Hz

        motor_0.current_PA = read_signed_data(ID_current + motor_0.id_offset, 0, motor_0.current_PA); // Amps
        motor_0.current_PB = read_signed_data(ID_current + motor_0.id_offset, 1, motor_0.current_PB);
        motor_0.current_PC = read_signed_data(ID_current + motor_0.id_offset, 2, motor_0.current_PC);
        motor_0.current_DC = read_signed_data(ID_current + motor_0.id_offset, 3, motor_0.current_DC);

        motor_0.voltage_DC = read_signed_data(ID_voltage + motor_0.id_offset, 0, motor_0.voltage_DC); // Volts
        motor_0.voltage_output = read_signed_data(ID_voltage + motor_0.id_offset, 1, motor_0.voltage_output);
        motor_0.voltage_AB = read_signed_data(ID_voltage + motor_0.id_offset, 2, motor_0.voltage_AB);
        motor_0.voltage_BC = read_signed_data(ID_voltage + motor_0.id_offset, 3, motor_0.voltage_BC);

        // Read faults
        read_fault_data_motor_0();
    }
}

int read_signed_data(byte ID, byte position_1, int pre_data) // Position (0,1,2,3) inside data structure of data
{
    byte low_byte = position_1 * 2;
    byte high_byte = low_byte + 1;
    int value = pre_data; // Value set so the number never changes when ID is not true
    if (RX_msg.id == ID)  // ID of motor array
    {
        long full_data = (RX_msg.buf[high_byte] * 255) + RX_msg.buf[low_byte];
        if (RX_msg.buf[high_byte] < 128) {
            value = full_data;
        } else if (RX_msg.buf[high_byte] > 128) {
            value = map(full_data, 65280, 32640, 0, -32640);
        }
    }
    return value;
}

void read_fault_data_motor_1() {
    if (RX_msg.id == ID_faults) {
        for (int col = 0; col < 8; ++col) // for each byte
        {
            if (RX_msg.buf[col]) // If the byte has info
            {
                for (int row = 0; row < 8; ++row) // for each bit
                {
                    if (((RX_msg.buf[col] >> row) & 0B00000001)) // If each bit is true, store value
                    {
                        motor_1.faults[col][row] = 1;
                    } else // No error
                    {
                        motor_1.faults[col][row] = 0;
                    }
                }
            }
        }
    }
}

void read_fault_data_motor_0() {
    if (RX_msg.id == ID_faults) {
        for (int col = 0; col < 8; ++col) // for each byte
        {
            if (RX_msg.buf[col]) // If the byte has info
            {
                for (int row = 0; row < 8; ++row) // for each bit
                {
                    if (((RX_msg.buf[col] >> row) & 0B00000001)) // If each bit is true, store value
                    {
                        motor_0.faults[col][row] = 1;
                    } else // No error
                    {
                        motor_0.faults[col][row] = 0;
                    }
                }
            }
        }
    }
}

void print_faults(Motor_controller_CAN_data motor) {
    delay(10);
    Serial.println();
    for (int i = 0; i < 8; ++i) {
        for (int k = 0; k < 8; ++k) {
            if (motor.faults[i][k]) {
                Serial.println(faults_decoder[i][k]);
            }
        }
    }
}

void canSniff(const CAN_message_t &RX_msg) // Reads all data
{
    Serial.print("MB ");
    Serial.print(RX_msg.mb);
    Serial.print("  LEN: ");
    Serial.print(RX_msg.len);
    Serial.print(" EXT: ");
    Serial.print(RX_msg.flags.extended);
    Serial.print(" REMOTE: ");
    Serial.print(RX_msg.rtr);
    Serial.print(" TS: ");
    Serial.print(RX_msg.timestamp);
    Serial.print(" ID: ");
    Serial.print(RX_msg.id, HEX);
    Serial.print(" Buffer: ");
    for (uint8_t i = 0; i < RX_msg.len; i++) {
        Serial.print(RX_msg.buf[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}
