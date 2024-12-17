#include <Arduino.h>
#include <WiFi.h>
#include <ESP_Mail_client.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include "credentials.h"

#define DHT22_PIN 4
#define LED_PIN 2

// Wi-Fi credentials
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// Email credentials
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465

DHT dht22(DHT22_PIN, DHT22);

// Define a static IP address for this Node
IPAddress staticIP(192, 168, 1, 160);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

SMTPSession smtp;
Session_Config config;

// Measurement storage
float tempSum = 0;
float humiSum = 0;
int measurementCount = 0;

unsigned long lastMeasurementTime = 0;
unsigned long lastMinuteBlink = 0;

const unsigned long MEASUREMENT_INTERVAL = 10 * 60 * 1000; // 10 minutes
const unsigned long MINUTE_BLINK_INTERVAL = 60 * 1000;     // 1 minute
// const unsigned long MEASUREMENT_INTERVAL = 60 * 1000; // 1 minute
// const unsigned long MINUTE_BLINK_INTERVAL = 1000;     // 1 second

void smtpCallback(SMTP_Status status);
void sendEmail(ESP_Mail_Session &session, SMTP_Message &message);
void connectToWiFi();
void blinkLED(int times, int delayTime);

void setup()
{
    Serial.begin(9600);
    pinMode(LED_PIN, OUTPUT);
    dht22.begin();

    connectToWiFi();

    // Configure SMTP settings
    smtp.debug(1);
    smtp.callback(smtpCallback);

    config.server.host_name = SMTP_HOST;
    config.server.port = SMTP_PORT;
    config.login.email = EMAIL_SENDER;      // set to empty for no SMTP Authentication
    config.login.password = EMAIL_PASSWORD; // set to empty for no SMTP Authentication
    config.login.user_domain = "";
    config.secure.mode = esp_mail_secure_mode_ssl_tls;
    config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
    config.time.gmt_offset = 3;
    config.time.day_light_offset = 0;
}

void loop()
{
    unsigned long currentTime = millis();

    // LED blink every minute when connected
    if (WiFi.status() == WL_CONNECTED && currentTime - lastMinuteBlink >= MINUTE_BLINK_INTERVAL)
    {
        blinkLED(1, 500);
        lastMinuteBlink = currentTime;
    }

    // Take a measurement every 10 minutes
    if (currentTime - lastMeasurementTime >= MEASUREMENT_INTERVAL)
    {
        float tempC;
        float humi;

        // Retry measurement until successful
        do
        {
            tempC = dht22.readTemperature();
            humi = dht22.readHumidity();
            if (isnan(tempC) || isnan(humi))
            {
                Serial.println("Failed to read from DHT22 sensor, retrying...");
                delay(2000);
            }
        } while (isnan(tempC) || isnan(humi));

        Serial.print("Temperature: ");
        Serial.print(tempC);
        Serial.print(" °C, Humidity: ");
        Serial.print(humi);
        Serial.println(" %");

        // Blink LED 4 times to indicate successful measurement
        blinkLED(5, 100);

        tempSum += tempC;
        humiSum += humi;
        measurementCount++;
        lastMeasurementTime = currentTime;

        // Send email after 6 measurements (1 hour)
        if (measurementCount >= 6)
        {
            float avgTemp = tempSum / measurementCount;
            float avgHumi = humiSum / measurementCount;

            // Prepare email message
            SMTP_Message message;
            message.sender.name = "ESP32";
            message.sender.email = EMAIL_SENDER;
            message.subject = "Hourly ESP32 Update";
            message.addRecipient(EMAIL_RECIPIENT_NAME, EMAIL_RECIPIENT);
            message.addRecipient(EMAIL_RECIPIENT2_NAME, EMAIL_RECIPIENT2);

            char emailContent[200];
            snprintf(emailContent, sizeof(emailContent), "Average Temperature: %.2f °C\nAverage Humidity: %.2f %%", avgTemp, avgHumi);
            message.text.content = emailContent;

            // Blink LED rapidly to indicate email is being sent
            blinkLED(10, 100);

            // Send email
            if (!smtp.connect(&config))
            {
                Serial.println("Error connecting to SMTP server.");
                return;
            }

            if (!MailClient.sendMail(&smtp, &message))
            {
                Serial.print("Error sending Email: ");
                Serial.println(smtp.errorReason());
            }
            else
            {
                Serial.println("Email sent successfully!");
            }

            // Reset counters
            tempSum = 0;
            humiSum = 0;
            measurementCount = 0;
        }
    }
}

void connectToWiFi()
{
    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(ssid);

    WiFi.config(staticIP, gateway, subnet, IPAddress(8, 8, 8, 8), IPAddress(8, 8, 4, 4));
    WiFi.begin(ssid, password);

    // Fast blink while connecting
    while (WiFi.status() != WL_CONNECTED)
    {
        blinkLED(1, 200);
        delay(200);
    }

    Serial.println("Wi-Fi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void blinkLED(int times, int delayTime)
{
    for (int i = 0; i < times; i++)
    {
        digitalWrite(LED_PIN, HIGH);
        delay(delayTime);
        digitalWrite(LED_PIN, LOW);
        delay(delayTime);
    }
}

void smtpCallback(SMTP_Status status)
{
    Serial.println(status.info());
}