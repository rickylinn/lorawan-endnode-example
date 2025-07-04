#include <Arduino.h>
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

#define init_button 8
#define dht_pin A0
#define dht_type DHT11

// Schedule TX every this many seconds
const unsigned TX_INTERVAL = 10;

// LoRa Shield Pin mapping
const lmic_pinmap lmic_pins = {
  .nss = 10,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = 7,
  .dio = {2, 5, 6},
};

// Track join state
static bool joined = false;

// DEV EUI: 4f 9f 04 d0 7f d5 b3 70 (Reverse Byte Order for Chirpstack)
static const u1_t PROGMEM DEVEUI[8]={0x70, 0xB3, 0xD5, 0x7F, 0xD0, 0x04, 0x9F, 0x4F};
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}

// APP EUI: 00 00 00 00 00 00 00 00 
static const u1_t PROGMEM APPEUI[8]={0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}

// APP KEY: d5 03 6e bb 00 ae 31 1a 33 60 ed 80 c6 a2 6c 67
static const u1_t PROGMEM APPKEY[16]={0xD5, 0x03, 0x6E, 0xBB, 0x00, 0xAE, 0x31, 0x1A, 0x33, 0x60, 0xED, 0x80, 0xC6, 0xA2, 0x6C, 0x67};
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}

// Job objects for different tasks
static osjob_t sendjob;
static osjob_t retryjob; // Optional

float readTemp();
float readHumid();
bool btnPressCounter(int pin, int count);
bool btnPressTimer(int pin, unsigned long time);
void printHex2(unsigned v);
void onEvent(ev_t ev);
void doSend(osjob_t* j);
void retryJoin(osjob_t* j);

// DHT sensor initialization
DHT dht(dht_pin, dht_type);

// LCD initialization
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
    Serial.begin(9600);
    lcd.init();
	lcd.backlight();
    
    // Show initial screen
    lcd.setCursor(0, 0);
    lcd.print("PICOBOX LORAWAN");
    lcd.setCursor(0, 1);
    lcd.print("Starting...");
    Serial.println(F("Starting..."));
    delay(1000);

    pinMode(init_button, INPUT_PULLUP);

    // Initialize LoRaWAN first to avoid DHT interference
    os_init();
    LMIC_reset(); 
    LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100);
    
    lcd.setCursor(0, 1);
    lcd.print("               ");
    lcd.setCursor(0, 1);
    lcd.print("LoRa init ...");
    Serial.println(F("LoRa init ..."));
    delay(1000);

    // Initialize DHT after LoRaWAN
    dht.begin();
    lcd.setCursor(0, 1);
    lcd.print("               ");
    lcd.setCursor(0, 1);
    lcd.print("DHT init ...");
    Serial.println(F("DHT init ..."));
    delay(1000);
    
    // Start Join Process
    lcd.setCursor(0, 1);
    lcd.print("               ");
    lcd.setCursor(0, 1);
    lcd.print("Start Join ...");
    Serial.println(F("Start Join ..."));
    delay(1000);
    LMIC_startJoining();
}

void loop() {    
    os_runloop_once();
    
    if (btnPressTimer(init_button, 3000)) {
        Serial.println("Button Pressed");
    }

    if (btnPressCounter(init_button, 3)) {
        Serial.println("Hello World");
    }
}

float readTemp() {
    float temp = dht.readTemperature();
    
    // Retry once if first reading fails
    if (isnan(temp)) {
        delay(50);
        temp = dht.readTemperature();
    }

    // Use dummy value if failed
    if (isnan(temp)) {
        return 24.3;
    }

    return temp;
}

float readHumid() {
    float humid = dht.readHumidity();
    
    // Retry once if first reading fails
    if (isnan(humid)) {
        delay(50);
        humid = dht.readHumidity();
    }

    // Use dummy value if failed
    if (isnan(humid)) {
        return 75.3;
    }

    return humid;
}

bool btnPressCounter(int pin, int count) {
	static int press_counter = 0;
	static unsigned long last_press_time = 0;
	static unsigned long first_press_time = 0;
	static bool last_state = HIGH;
	
	const unsigned long debounce_delay = 50;    // 50ms debounce
	const unsigned long window_timeout = 2000;  // 2 second window to complete all presses

	bool current_state = digitalRead(pin);

	// Detect button press (falling edge with debounce)
	if (last_state == HIGH && current_state == LOW) {
		if (millis() - last_press_time > debounce_delay) {
			press_counter++;
			last_press_time = millis();
			
			// Record first press time to start timeout window
			if (press_counter == 1) {
				first_press_time = millis();
			}
		}
	}

	last_state = current_state;

	// Check if we reached the target count
	if (press_counter >= count) {
		press_counter = 0;
		return true;
	}
	
	// Check if timeout window has expired
	if (press_counter > 0 && (millis() - first_press_time > window_timeout)) {
		press_counter = 0;
	}

	return false;
}

bool btnPressTimer(int pin, unsigned long time) {
	static unsigned long start_time = 0;
	static bool triggered = false;
	
	bool current_state = (digitalRead(pin) == LOW);
	
	if (current_state) {
		// Button is pressed
		if (start_time == 0) {
			// First time detecting press - record start time
			start_time = millis();
			triggered = false;
		}
		
		// Check if enough time has elapsed
		unsigned long elapsed = millis() - start_time;
		if (elapsed >= time && !triggered) {
			triggered = true;
			return true;
		}
	} else {
		// Reset everything
		start_time = 0;
		triggered = false;
	}
	
	return false;
}

void printHex2(unsigned v) {
    v &= 0xff; // just makes sure v is within 8 bits (0–255)
    if (v < 16) {
        Serial.print('0');
    }
    Serial.print(v, HEX);
}

void onEvent(ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    
    switch(ev) {
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            lcd.setCursor(0, 1);
            lcd.print("               ");
            lcd.setCursor(0, 1);
            lcd.print("Joining...");
            break;
            
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            joined = true; // Set join flag
            lcd.setCursor(0, 1);
            lcd.print("               ");
            lcd.setCursor(0,1);
            lcd.print("Network Joined!");
            delay(2000);
            LMIC_setLinkCheckMode(0);
            // Start automatic transmission after successful join
            delay(3000); // Give time for join to fully complete
            os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), doSend);
            break;
            
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            joined = false; // Reset join flag
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print(F("Join Failed!"));
            lcd.setCursor(0, 1);
            lcd.print(F("Retrying..."));
            delay(2000);
            // Retry joining after 30 seconds
            os_setTimedCallback(&retryjob, os_getTime() + sec2osticks(30), retryJoin);
            break;
            
        case EV_JOIN_TXCOMPLETE:
            Serial.println(F("EV_JOIN_TXCOMPLETE - No JoinAccept"));
            // Don't reset joined flag here - this just means no response yet
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print(F("No JoinAccept"));
            lcd.setCursor(0, 1);
            lcd.print(F("Check Gateway"));
            delay(3000);
            // Retry joining after 10 seconds
            os_setTimedCallback(&retryjob, os_getTime() + sec2osticks(10), retryJoin);
            break;
            
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE"));
            lcd.setCursor(0, 1);
            lcd.print("               ");
            lcd.setCursor(0, 1);
            lcd.print("Ready for next..");
            
            if (LMIC.txrxFlags & TXRX_ACK) {
                Serial.println(F("ACK received"));
            }

            if (LMIC.dataLen) {
                Serial.print(F("RX: "));
                Serial.print(LMIC.dataLen);
                Serial.println(F(" bytes"));
            }
            
            // Schedule next automatic transmission
            os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), doSend);
            break;
            
        case EV_TXSTART:
            Serial.println(F("EV_TXSTART"));
            lcd.setCursor(0, 1);
            lcd.print("               ");
            lcd.setCursor(0, 1);
            lcd.print("Transmitting...");
            break;
            
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
            
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
            
        case EV_RXCOMPLETE:
            Serial.println(F("EV_RXCOMPLETE"));
            break;
            
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
            
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
            
        default:
            Serial.print(F("Unknown Event: "));
            Serial.println((unsigned) ev);
            if (ev == 11) {
                Serial.println(F("This might be EV_JOIN_TXCOMPLETE or radio error"));
            }
            break;
    }
}

void doSend(osjob_t* j) { 
    if (LMIC.opmode & OP_JOINING) {
        Serial.println(F("Still joining - please wait"));
        return;
    }
    
    if (!joined) {
        Serial.println(F("Not joined - starting OTAA"));
        LMIC_startJoining();
        return;
    }
   
    // float humidity = readHumid();
    // float temperature = readTemp();

    float humidity = 73.3;
    float temperature = 23.3;


    // Convert to integers for transmission (multiply by 100 for 2 decimal precision)
    uint16_t humidityInt = (uint16_t)(humidity * 100);
    uint16_t temperatureInt = (uint16_t)(temperature * 100);

    byte payload[4];
    payload[0] = highByte(humidityInt);
    payload[1] = lowByte(humidityInt);
    payload[2] = highByte(temperatureInt);
    payload[3] = lowByte(temperatureInt);

    LMIC_setTxData2(1, payload, sizeof(payload), 0);
}

void retryJoin(osjob_t* j) {
    Serial.println(F("Retrying join..."));
    joined = false;
    LMIC_startJoining();
}
