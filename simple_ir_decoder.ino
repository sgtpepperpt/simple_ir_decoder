#define BUF 500
#define IR_PIN 9
#define PRINT_RAW 0
#define PRINT_BIN 0

int start_micros;
int now_micros;
int value;
int prev_val;
int num_ones;

// holds the received IR pulses
short comm[BUF];
short comm_pos;

int bin_string_to_byte(String s) {
    byte value = 0;
    for (int i = 0; i < s.length(); i++) {
        value *= 2;
        if (s[i] == '1')
            value++;
    }

    return value;
}

String byte_to_hex(byte num) {
    char tmp[16];
    sprintf(tmp, "%02X", num);
    return String(tmp);
}

void print_pulse() {
    // this function assumes the modified "NEC-like" protocol a Whirlpool A/C uses
    // the bounds and type of signals probably need to change in other IR protocols
  
    // lower and upper bounds for on and off pairs, representing bits
    // {{on-lower-bound, on-upper-bound}, {off-lower-bound, off-upper-bound}}
    const int start[2][2] = {{8000, 9500}, {4000, 5000}}; // 9000, 4500
    const int space[2][2] = {{450, 700}, {7000, 9000}};   // 500, 8000
    const int finish[2][2] = {{300, 750}, {-2, 0}};       // 560
    const int zero[2][2] = {{300, 750}, {300, 750}};      // 560, 560
    const int one[2][2] = {{300, 750}, {1000, 3000}};     // 560, 1680
    
    String curr_byte = "";
    
    String hex_command = "";
    
    for (int i = 1; i < comm_pos; i+=2) {
        int on = comm[i];
        int off = comm[i+1];

        if (on == -1) {
            break;
        }

        int in_bounds_start_on = start[0][0] < on && on < start[0][1];
        int in_bounds_start_off = start[1][0] < off && off < start[1][1];

        int in_bounds_space_on = space[0][0] < on && on < space[0][1];
        int in_bounds_space_off = space[1][0] < off && off < space[1][1];

        int in_bounds_finish_on = finish[0][0] < on && on < finish[0][1];
        int in_bounds_finish_off = finish[1][0] < off && off < finish[1][1];
        
        int in_bounds_zero_on = zero[0][0] < on && on < zero[0][1];
        int in_bounds_zero_off = zero[1][0] < off && off < zero[1][1];

        int in_bounds_one_on = one[0][0] < on && on < one[0][1];
        int in_bounds_one_off = one[1][0] < off && off < one[1][1];

        if (in_bounds_zero_on && in_bounds_zero_off) {
            curr_byte += '0';

            #if PRINT_BIN
            Serial.print('0');
            #endif
        } else if (in_bounds_one_on && in_bounds_one_off) {
            curr_byte += '1';
            #if PRINT_BIN
            Serial.print('1');
            #endif
        } else if (in_bounds_start_on && in_bounds_start_off) {
            // ignore signal start except if received mid-command
            curr_byte = "";
            if (i > 1)
                hex_command  += "s";
            #if PRINT_BIN
            Serial.println("Signal start");
            #endif
        } else if (in_bounds_finish_on && in_bounds_finish_off) {
            // ignore signal finish except if received mid-command
            curr_byte = "";
            if (i + 1 < comm_pos)
                hex_command  += "f";
            #if PRINT_BIN
            Serial.println("Signal end");
            #endif
        } else if (in_bounds_space_on && in_bounds_space_off) {
            // "pause" signal, count as a space
            curr_byte = "";
            hex_command  += " ";
            #if PRINT_BIN
            Serial.println();
            #endif
        } else {
          // unrecognised sequence, print it out
            curr_byte = "";
            hex_command += "x";
            #if PRINT_BIN
            Serial.println();
            Serial.print(i);
            Serial.print(": ");
            Serial.print(on);
            Serial.print(" ");
            Serial.println(off);
            #endif
        }

        if (curr_byte.length() == 8) {
            byte bin_val = bin_string_to_byte(curr_byte);
            #if PRINT_BIN
            Serial.print(" ");
            Serial.println(byte_to_hex(bin_val));
            #endif
            hex_command += byte_to_hex(bin_val);

            curr_byte = "";
        }
    }

    #if PRINT_BIN
    Serial.println();
    #endif

    int len = hex_command.length();
    Serial.write((char*)&len, 4);
    Serial.write(hex_command.c_str());
}

void setup() {
    Serial.begin(115200);
    pinMode(IR_PIN, INPUT);
}

void loop() {
    // variable initialisation
    for (int i = 0; i < BUF; i++)
        comm[i] = -1;
        
    num_ones = 0;
    prev_val = 1;
    comm_pos = 0;

    // wait until we receive a pulse
    value = 1;
    while (value) {
        value = digitalRead(IR_PIN);
    }

    start_micros = micros();

    while (true) {
        if (value != prev_val) {
            now_micros = micros();

            // pulse is the distance from last state change to now
            comm[comm_pos++] = now_micros - start_micros;

            start_micros = now_micros;
        }

        if (value)
            num_ones++;
        else
            num_ones = 0;

        if (comm_pos >= BUF)
            Serial.println(F("Buffer overflow detected... Raise BUF if possible"));
  
        if (num_ones > 10000)
            break;

        prev_val = value;
        value = digitalRead(IR_PIN);
    }

    // post command processing (printing)
    #if PRINT_RAW
    for (int i = 0; i < comm_pos; i++) {
        Serial.print(comm[i]);
        
        if (i < comm_pos -1)
            Serial.print(", ");
        else
            Serial.println();
    }

    Serial.println("elements: " + String(comm_pos));
    #endif
    
    print_pulse();
    #if PRINT_RAW
    Serial.println("--------------------------------------------------------------------------------");
    #endif
}
