//Including libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient);

Servo myservo;

//Defining OLED screen parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3c

//Defining other pins
#define buzzer 5
#define LED 15

#define PB_CANCEL 13
#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 14
#define DHTPIN 12
#define LDRPIN 34
#define SERVOPIN 26

//buzzer sound notes
int n_notes = 8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};


#define NTP_SERVER    "asia.pool.ntp.org"
//Sri Lanka Standard Time is UTC+5:30
#define UTC_OFFSET_DST 0
float UTC_OFFSET = 5.5;

//Declare OLED display and DHT22 sensor
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

//time related variables
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;
int time_zone_hour = 5;
int time_zone_minute = 30;

bool alarm_enabled[] = {true, true};
int n_alarms = 2;
int alarm_hours[] = {10, 13};
int alarm_minutes[] = {30, 35};
bool alarm_triggered[] = {false, false};

int current_mode = 0;
int max_mode = 5;
String modes[] = {"1 -   Set Time zone", "2-Set     Alarm", "3-   View Alarm List", "4 - Delete Alarm", "5-Disable Alarms"};

//variables and default values of angle equation
float offset =30;
int Tmed;
float lightIntensity = 0.0;
float controllingFactor =0.75;
int Ts = 5000; //sampling interval default 5 seconds
int Tu = 120000; //data sending interval 2 mintues default
float measuredT;
float medT =30;

unsigned long lastSampleTime = 0;
unsigned long lastUpdateTime = 0;
float sampleSum = 0;
int sampleCount = 0;


void setup() {
  Serial.begin(115200);

  pinMode(buzzer, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);
  pinMode(LED, OUTPUT);
  pinMode(LDRPIN, INPUT);
  pinMode(SERVOPIN, OUTPUT);

  dhtSensor.setup(DHTPIN, DHTesp::DHT22);
  myservo.attach(SERVOPIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    display.clearDisplay();
    print_line("Connecting to WIFI", 0, 0, 2);
  }

  display.clearDisplay();
  print_line("Connected to WIFI", 0, 0, 2);
  delay(500);
  configTime(UTC_OFFSET * 3600, UTC_OFFSET_DST, NTP_SERVER);
  //UTC offset in seconds
  setupMqtt();

  display.display();
  delay(2000);
  display.clearDisplay();
  print_line("Welcome to Medibox!", 0, 20, 2);
  delay(3000);

  display.clearDisplay();
  delay(2000);
}

void loop() {
  if(!mqttClient.connected()){
    connectToBroker();
  }
  mqttClient.loop();//looping to check messages
  float lightIntensity=readLightIntensity(Ts,Tu);
  adjustServo(offset, lightIntensity, controllingFactor, Ts,Tu, measuredT, medT);

  update_time_with_checked_alarms();
  if (digitalRead(PB_OK) == LOW) {
    delay(50);
    display.clearDisplay();
    print_line("MENU", 40, 20, 2);
    delay(750);
    display.clearDisplay();
    to_the_menu();
  }
  check_temp();
}

void set_time_zone() {
  display.clearDisplay();
  print_line("Current Time Zone : UCT + " + String(time_zone_hour) + ":" + String(time_zone_minute), 0, 0, 2);
  delay(2500);
  display.clearDisplay();
  print_line("Set the   time zone   hour", 0, 0, 2);

  while (true) {
    int pressed = wait_for_response();
    if (pressed == PB_UP) {
      delay(200);
      time_zone_hour += 1;
      time_zone_hour = time_zone_hour % 24;
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      time_zone_hour -= 1;
      if (time_zone_hour < 0) {
        time_zone_hour = 23;
      }

    }
    else if (pressed == PB_OK) {
      delay(200);
      break;
    }
    clearSection(0, 48, 40, 64);
    print_line(String(time_zone_hour), 0, 50, 2);

  }
  display.clearDisplay();

  print_line("Set the time zone   minute", 0, 0, 2);
  while (true) {

    int pressed = wait_for_response();
    if (pressed == PB_UP) {
      delay(200);
      time_zone_minute += 1;
      time_zone_minute = time_zone_minute % 60;
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      time_zone_minute -= 1;
      if (time_zone_minute < 0) {
        time_zone_minute = 59;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      break;
    }

    clearSection(30, 48, 70, 64);
    print_line(String(time_zone_hour), 0, 50, 2);
    print_line(":", 25, 50, 2);
    print_line(String(time_zone_minute), 40, 50, 2);
  }
  display.clearDisplay();
  print_line("time_zone is set to:", 0, 0, 2);
  print_line(String(time_zone_hour), 0, 50, 2);
  print_line(String(":"), 30, 50, 2);
  print_line(String(time_zone_minute), 40, 50, 2);
  delay(2000);

  UTC_OFFSET = float(time_zone_hour) + float(time_zone_minute) / 60;
  configTime(UTC_OFFSET * 3600, UTC_OFFSET_DST, NTP_SERVER);
}

int wait_for_response() {
  while (true) {
    if (digitalRead(PB_UP) == LOW) {
      delay(200);
      return PB_UP;
    }
    else if (digitalRead(PB_DOWN) == LOW) {
      delay(200);
      return PB_DOWN;
    }
    else if (digitalRead(PB_OK) == LOW) {
      delay(200);
      return PB_OK;
    }
    else if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);
      return PB_CANCEL;
    }
    update_time();
  }
}

void update_time() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char timeHour[3];
  strftime(timeHour, 3, "%H", &timeinfo);
  hours = atoi(timeHour);

  char timeMinute[3];
  strftime(timeMinute, 3, "%M", &timeinfo);
  minutes = atoi(timeMinute);

  char timeSecond[3];
  strftime(timeSecond, 3, "%S", &timeinfo);
  seconds = atoi(timeSecond);

  char timeDay[3];
  strftime(timeDay, 3, "%d", &timeinfo);
  days = atoi(timeDay);
}

void print_line(String text, int column, int row, int text_size) {
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(text);

  display.display();
}

void print_time(void) {
  display.clearDisplay(); // Clear display before printing time
  String time = String(hours) + ":" + String(minutes) + ":" + String(seconds);
  print_line(time, 10, 5, 2);
  display.display(); // Ensure the display is updated
}
void to_the_menu() {
  while (digitalRead(PB_CANCEL) == HIGH) {
    display.clearDisplay();
    print_line(modes[current_mode], 0, 20, 2);

    int pressed = wait_for_response();
    if (pressed == PB_UP) {
      delay(200);
      current_mode += 1;
      current_mode = current_mode % max_mode;
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      current_mode -= 1;
      if (current_mode < 0) {
        current_mode = max_mode - 1;
      }

    }
    else if (pressed == PB_OK) {
      delay(200);
      run_mode(current_mode);
    }
  }
}

void run_mode(int mode) {
  if (mode == 0) {
    set_time_zone();
  }
  else if (mode == 1) {
    choose_alarm();
  }
  else if (mode == 2) {
    view_alarm_list();
  }
  else if (mode == 3) {
  delete_alarm();
  }
  else if (mode == 4) {
    for (int i = 0; i < n_alarms; i++) {
      alarm_enabled[i] = false;
      // Also reset the alarm times
      alarm_hours[i] = 0;
      alarm_minutes[i] = 0;
    }
    display.clearDisplay();
    print_line("Alarms    Disabled", 0, 0, 2);
    delay(1000);
  }

}

void choose_alarm() {
  int alarm_number = 0;
  while (true) {
    display.clearDisplay();
    print_line("Choose the Alarm", 0, 0, 2);
    print_line(String(alarm_number + 1), 30, 40, 3);
    int pressed = wait_for_response();

    if (pressed == PB_UP) {
      delay(200);
      alarm_number += 1;
      alarm_number = alarm_number % 2;

    }
    else if (pressed == PB_DOWN) {
      delay(200);
      alarm_number -= 1;
      if (alarm_number < 0) {
        alarm_number = 1;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      display.clearDisplay();
      alarm_enabled[alarm_number] = true;

      display.clearDisplay();
      int temp_alarm_hour = alarm_hours[alarm_number];
      while (true) {
        print_line("Set the Alarm hour", 0, 0, 2);
        int pressed = wait_for_response();
        if (pressed == PB_UP) {
          delay(200);
          temp_alarm_hour += 1;
          temp_alarm_hour = temp_alarm_hour % 24;
        }
        else if (pressed == PB_DOWN) {
          delay(200);
          temp_alarm_hour -= 1;
          if (temp_alarm_hour < 0) {
            temp_alarm_hour = 23;
          }

        }
        else if (pressed == PB_OK) {
          delay(200);
          alarm_hours[alarm_number] = temp_alarm_hour;
          break;
        }
        display.clearDisplay();
        print_line(String(temp_alarm_hour), 0, 50, 2);

      }

      int temp_alarm_minute = alarm_minutes[alarm_number];
      display.clearDisplay();

      if (wait_for_response() == PB_CANCEL) {
        //pass();
      }
      else {
        while (true) {
          print_line("Set the Alarm minute", 0, 0, 2);
          int pressed = wait_for_response();
          if (pressed == PB_UP) {
            delay(200);
            temp_alarm_minute += 1;
            temp_alarm_minute = temp_alarm_minute % 60;
          }
          else if (pressed == PB_DOWN) {
            delay(200);
            temp_alarm_minute -= 1;
            if (temp_alarm_minute < 0) {
              temp_alarm_minute = 59;
            }

          }
          else if (pressed == PB_OK) {
            delay(200);
            alarm_minutes[alarm_number] = temp_alarm_minute;
            break;
          }
          display.clearDisplay();
          print_line(String(temp_alarm_hour), 0, 50, 2);
          print_line(":", 30, 50, 2);
          print_line(String(temp_alarm_minute), 40, 50, 2);
        }
        int pressed = wait_for_response();
        if (pressed != PB_CANCEL) {
          break;
        }
      }
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }
}

void update_time_with_checked_alarms() {
    update_time();
    print_time();
    
    bool alarm_go = false;
    for (int i = 0; i < n_alarms; i++) {
        if (alarm_enabled[i]) {
            alarm_go = true;
            break;
        }
    }

    if (alarm_go) {
        for (int i = 0; i < n_alarms; i++) {
            if (!alarm_triggered[i] && alarm_hours[i] == hours && alarm_minutes[i] == minutes) {
                ring_alarm(i);
                alarm_triggered[i] = true;
            }
        }
    }
}

void ring_alarm(int alarm_index) {
    int snooze_count = 0; // Track snooze attempts
    display.clearDisplay();

    while (snooze_count < 5) {
        // Show alarm message
        display.clearDisplay();
        print_line("Medicine Time", 0, 20, 2);
        digitalWrite(LED, HIGH);

        // Play alarm until a button is pressed
        bool button_pressed = false;
        while (!button_pressed) {
            // Check for cancel button
            if (digitalRead(PB_CANCEL) == LOW) {
                delay(200);
                digitalWrite(LED, LOW);
                display.clearDisplay();
                print_line("Alarm Stopped", 0, 20, 2);
                delay(2000);
                display.clearDisplay(); // Clear display before returning
                return; // Exit the function completely
            }
            
            // Check for snooze buttons
            if (digitalRead(PB_OK) == LOW || digitalRead(PB_UP) == LOW || digitalRead(PB_DOWN) == LOW) {
                snooze_count++;
                button_pressed = true;
                snooze_alarm(snooze_count); // Trigger snooze
                break; // Break out of the inner while loop
            }
            
            // Play the buzzer tone
            for (int i = 0; i < n_notes; i++) {
                tone(buzzer, notes[i]);
                delay(500);
                noTone(buzzer);
                delay(2);
                
                // Check buttons again during tone playing
                if (digitalRead(PB_CANCEL) == LOW) {
                    noTone(buzzer);
                    delay(200);
                    digitalWrite(LED, LOW);
                    display.clearDisplay();
                    print_line("Alarm Stopped", 0, 20, 2);
                    delay(2000);
                    display.clearDisplay();
                    return; // Exit the function completely
                }
                
                if (digitalRead(PB_OK) == LOW || digitalRead(PB_UP) == LOW || digitalRead(PB_DOWN) == LOW) {
                    noTone(buzzer);
                    snooze_count++;
                    button_pressed = true;
                    snooze_alarm(snooze_count);
                    break; // Break out of the for loop
                }
            }
        }
        
        // If we've reached max snooze attempts, show message and exit
        if (snooze_count >= 5) {
            display.clearDisplay();
            print_line("Max Snooze", 0, 20, 2);
            print_line("Reached", 0, 40, 2);
            delay(2000);
            break; // Exit the while loop
        }
    }

    digitalWrite(LED, LOW);
    display.clearDisplay();
}

void snooze_alarm(int snooze_count) {
    display.clearDisplay();
    print_line("Snoozed", 0, 20, 2);
    print_line("Attempts: " + String(snooze_count) + "/5", 0, 40, 1);
    
    for (int i = 0; i < 10; i++) {
        digitalWrite(LED, HIGH);
        delay(250);
        digitalWrite(LED, LOW);
        delay(250);
    }

    // Instead of just showing "Snoozing for 5 minutes" and delaying,
    // we'll show the current time for the duration of the snooze period
    
    unsigned long snooze_start_time = millis();
    unsigned long snooze_duration = 5 * 60 * 1000; // 5 minutes in milliseconds
    
    // Show time during the snooze period
    while (millis() - snooze_start_time < snooze_duration) {
        display.clearDisplay();
        update_time(); // Update current time
        print_time();  // Display current time
        
        // Show snooze status
        print_line("Snoozing...", 0, 30, 1);
        
        // Calculate and show remaining time
        int remaining_seconds = (snooze_duration - (millis() - snooze_start_time)) / 1000;
        int remaining_minutes = remaining_seconds / 60;
        remaining_seconds %= 60;
        String remaining = String(remaining_minutes) + ":" + 
                          (remaining_seconds < 10 ? "0" : "") + String(remaining_seconds);
        print_line("Remaining: " + remaining, 0, 45, 1);
        
        delay(1000); // Update every second
        
        // Check if cancel is pressed to exit snooze early
        if (digitalRead(PB_CANCEL) == LOW) {
            delay(200);
            break;
        }
    }
    
    // After snooze period is over, make sure the display shows current time
    display.clearDisplay();
    update_time();
    print_time();
}

void delete_alarm() {
  int selected_alarm = 0; // Start with alarm 1 selected
  
  while (true) {
    display.clearDisplay();
    print_line("Select Alarm to Delete:", 0, 0, 1);

    // Display all alarms with current selection highlighted
    for (int i = 0; i < n_alarms; i++) {
      String status = alarm_enabled[i] ? 
        String(alarm_hours[i]) + ":" + (alarm_minutes[i] < 10 ? "0" : "") + String(alarm_minutes[i]) : 
        "Disabled";
      
      // Highlight the currently selected alarm
      if (i == selected_alarm) {
        print_line("> Alarm " + String(i + 1) + ": " + status, 0, 15 + i * 12, 1);
      } else {
        print_line("  Alarm " + String(i + 1) + ": " + status, 0, 15 + i * 12, 1);
      }
    }
    
    print_line("UP/DOWN:Select OK:Delete", 0, 52, 1);

    int pressed = wait_for_response();
    if (pressed == PB_UP) {
      delay(200);
      selected_alarm = (selected_alarm + n_alarms - 1) % n_alarms;; // Move up (to alarm 3 from 1)
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      selected_alarm = (selected_alarm + 1) % n_alarms; // Move down (to alarm 2 from 1)
    }
    else if (pressed == PB_OK) {
      delay(200);
      if (alarm_enabled[selected_alarm]) {
        // Confirm deletion
        display.clearDisplay();
        print_line("Delete Alarm " + String(selected_alarm + 1) + "?", 0, 10, 1);
        print_line("Time: " + String(alarm_hours[selected_alarm]) + ":" + 
                  (alarm_minutes[selected_alarm] < 10 ? "0" : "") + 
                  String(alarm_minutes[selected_alarm]), 0, 25, 1);
        print_line("OK:Confirm CANCEL:Back", 0, 45, 1);
        
        // Wait for confirmation
        int confirm = wait_for_response();
        if (confirm == PB_OK) {
          // Delete the alarm
          alarm_enabled[selected_alarm] = false;
          alarm_hours[selected_alarm] = 0;
          alarm_minutes[selected_alarm] = 0;
          
          display.clearDisplay();
          print_line("Alarm " + String(selected_alarm + 1), 0, 15, 2);
          print_line("Deleted!", 0, 35, 2);
          delay(1500);
        }
      } else {
        display.clearDisplay();
        print_line("Alarm " + String(selected_alarm + 1), 0, 15, 2);
        print_line("Already Disabled", 0, 35, 1);
        delay(1500);
      }
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }
}

void view_alarm_list() {
  display.clearDisplay();
  print_line("Alarms List", 0, 0, 2);
  delay(2000);
  for (int i = 0; i < 2; i++) {
    String line = "Alarm ";
    line += String(i + 1);
    line += ": ";
    line += String(alarm_hours[i]);
    line += ":";
    line += String(alarm_minutes[i]);
    print_line(line, 0, 20 + i * 10, 1);
  }
  delay(2000);
}

void check_temp() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  display.clearDisplay();
  measuredT=data.temperature;
  if (data.temperature > 32) {
    digitalWrite(LED, HIGH);
    print_line("TEMPERATURE HIGH", 0, 40, 1);
  }
  else {
    digitalWrite(LED, LOW);
  }
  if (data.temperature < 24) {
    digitalWrite(LED, HIGH);
    print_line("TEMPERATURE LOW", 0, 40, 1);
  }
  else {
    digitalWrite(LED, LOW);
  }
  if (data.humidity > 80) {
    digitalWrite(LED, HIGH);
    print_line("HUMIDITY HIGH", 0, 50, 1);
  }
  else {
    digitalWrite(LED, LOW);
  }
  if (data.humidity < 65) {
    digitalWrite(LED, HIGH);
    print_line("HUMIDITY LOW", 0, 50, 1);
  }
  else {
    digitalWrite(LED, LOW);
  }
  publishToTopic("Medibox_220705M/Temperature",String(measuredT,2));
}

void clearSection(int x1, int x2, int y1, int y2) {
  for (int i = x1; i < x2; i++) {
    for (int j = y1; j < y2; j++) {
      display.drawPixel(i, j, BLACK);
    }
  }
  display.display();
}

void setupMqtt(){
  mqttClient.setServer("test.mosquitto.org",1883);
  mqttClient.setCallback(callback); //function callback is triggered whenever a message arrive
}

void connectToBroker(){
  while (!mqttClient.connected()){
    Serial.print("Atttempting MQTT connection...");
    if (mqttClient.connect("ESP32-324324324")){
      Serial.println("Connected");
      mqttClient.subscribe("Medibox_220705M/Ts");
      mqttClient.subscribe("Medibox_220705M/Tu");
      mqttClient.subscribe("Medibox_220705M/offset");
      mqttClient.subscribe("Medibox_220705M/gamma");
      mqttClient.subscribe("Medibox_220705M/Tmed");

    }else{
      Serial.print("failed");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

float readLightIntensity(unsigned long Ts,unsigned long Tu){
  unsigned long currentTime = millis();
  if (currentTime-lastSampleTime>=Ts){
    lastSampleTime = currentTime;
    int raw = analogRead(LDRPIN);
    lightIntensity = 1-raw/4095.0;
    sampleSum += lightIntensity;
    sampleCount++;
    Serial.print("Sampled: ");
    Serial.println(lightIntensity, 3);
  }
  if (currentTime - lastUpdateTime >= Tu) {
    lastUpdateTime = currentTime;
    if (sampleCount > 0) {
      float averageLightIntensity = sampleSum / sampleCount;
      Serial.print("Averaged Light Intensity: ");
      Serial.println(averageLightIntensity, 3);
      publishToTopic("Medibox_220705M/LightIntensity",String(averageLightIntensity,2));
      sampleSum = 0;
      sampleCount = 0;
    }
  }
  return lightIntensity;
}

void adjustServo(float offset, float lightIntensity, float controllingFactor, int ts, int tu, float measuredT, float medT) {
  float ratio = float(ts) / tu;
  if (ratio <= 0.0f) ratio = 0.01f;  // Prevent log(0) or negative

  float logFactor = log(ratio);
  float angle = offset + (180 - offset) * lightIntensity * controllingFactor * logFactor * (measuredT / medT);
  Serial.print(" LI=");
  Serial.print(lightIntensity);
  Serial.print(" r=");
  Serial.print(controllingFactor);
  Serial.print(" Log=");
  Serial.print(logFactor);
  Serial.print(" T=");
  Serial.print(measuredT);
  Serial.print(" Tmed=");
  Serial.println(medT);
  myservo.write(angle);
  Serial.print("Servo motor angle is ");
  Serial.println(angle);
}

void publishToTopic(String topic, String message) {
  mqttClient.publish(topic.c_str(), message.c_str());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Received on topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(message);

  if (String(topic) == "Medibox_220705M/Ts") {
    Ts = (message.toInt())*1000;
    Serial.print("Ts is ");
    Serial.print(Ts/1000);
    Serial.println("Seconds");

  } else if (String(topic) == "Medibox_220705M/Tu") {
    Tu= (message.toInt())*1000;
    Serial.print("Tu is ");
    Serial.print(Tu/1000);
    Serial.println("Seconds");

  } else if (String(topic) == "Medibox_220705M/offset") {
    offset= message.toFloat();
    Serial.print("now offset is");
    Serial.println(offset);

  } else if (String(topic) == "Medibox_220705M/gamma") {
    controllingFactor= message.toFloat();
    Serial.print("now rfactor is");
    Serial.println(controllingFactor);
  }
  else if (String(topic) == "Medibox_220705M/Tmed") {
    medT= message.toFloat();
    Serial.print("now Tmed is");
    Serial.println(medT);
  }
}