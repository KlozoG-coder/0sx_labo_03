#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define LCD_ADDR 0x27 // Adresse I2C de l'écran LCD
#define TEMP_SENSOR_PIN A0
#define JOYSTICK_X A1
#define JOYSTICK_Y A2
#define BUTTON_PIN 2
#define LED_PIN 8

LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

// Caractères personnalisés 
byte customChar8[8] = {   
    0B01100,  
    0B10010,  
    0B01100,  
    0B10010,  
    0B01100,  
    0B10010,
    0B10010,  
    0B01100   
};

// Tableaux pour stocker les valeurs
float temperatureValues[2] = {0.0, 0.0}; // Température actuelle et précédente
int speedValues[2] = {0, 0};             // Vitesse actuelle et précédente
int directionValues[2] = {0, 0};         // Direction actuelle et précédente
bool climateState[2] = {false, false};   // État de la climatisation actuel et précédent

// Variables pour les plages de valeurs du joystick
const int joystickMin = 0;
const int joystickMax = 1023;
const int joystickNeutralMin = 450;
const int joystickNeutralMax = 550;

// Variables pour les plages de vitesse et de direction
const int speedMin = -25; // Vitesse minimale (recul)
const int speedMax = 120; // Vitesse maximale (avance)
const int directionMin = -90; // Direction minimale (gauche)
const int directionMax = 90;  // Direction maximale (droite)

bool displayPage = true; // true = Page 1 (Température), false = Page 2 (Vitesse/Direction)
bool lastButtonState = HIGH;
unsigned long lastMillis = 0;
const unsigned long interval = 100; // Intervalle de 100 ms pour l'envoi série
unsigned long lastButtonPress = 0;

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    lcd.init();
    lcd.backlight();
    
    // Création des caractères personnalisés
    lcd.createChar(0, customChar8); // Caractère pour "8"

    // Affichage au démarrage
    lcd.setCursor(0, 0);
    lcd.print("Nom: KOUDJONOU J.L.");
    lcd.setCursor(0, 1);
    lcd.write(0); // Affiche "8" (caractère personnalisé 0)
    lcd.setCursor(1, 1); 
    delay(3000);
    lcd.clear();
}

void loop() {
    static unsigned long lastButtonPress = 0;
    unsigned long currentMillis = millis();
    
    // Lecture des valeurs du capteur et du joystick
    int thermistorValue = analogRead(TEMP_SENSOR_PIN);
    float resistance = (1023.0 / thermistorValue - 1) * 10000; // Diviseur de tension avec résistance 10kΩ
    temperatureValues[0] = 1 / (0.001129148 + (0.000234125 * log(resistance)) + (0.0000000876741 * pow(log(resistance), 3))) - 273.15; // Conversion en °C

    int joystickX = analogRead(JOYSTICK_X);
    int joystickY = analogRead(JOYSTICK_Y);
    bool buttonState = digitalRead(BUTTON_PIN);

    // Calcul de la vitesse et de la direction
    speedValues[0] = 0; // Par défaut, la vitesse est à 0 (repos)
    if (joystickY > joystickNeutralMax) { // Joystick vers le haut (avance)
        speedValues[0] = map(joystickY, joystickNeutralMax, joystickMax, 0, speedMax); // Vitesse entre 0 et 120 km/h
    } else if (joystickY < joystickNeutralMin) { // Joystick vers le bas (recule)
        speedValues[0] = map(joystickY, joystickNeutralMin, joystickMin, 0, speedMin); // Vitesse entre 0 et -25 km/h
    }

    directionValues[0] = 0; // Par défaut, la direction est neutre
    if (joystickX > joystickNeutralMax) { // Joystick vers la droite
        directionValues[0] = map(joystickX, joystickNeutralMax, joystickMax, 0, directionMax); // Direction entre 0° et 90°
    } else if (joystickX < joystickNeutralMin) { // Joystick vers la gauche
        directionValues[0] = map(joystickX, joystickNeutralMin, joystickMin, 0, directionMin); // Direction entre 0° et -90°
    }

    // Hystérésis pour éviter l'activation/désactivation rapide de la clim
    if (temperatureValues[0] > 30 && !climateState[0]) {
        if (millis() - lastButtonPress > 5000) {
            climateState[0] = true;
            lastButtonPress = millis();
        }
    } else if (temperatureValues[0] < 29 && climateState[0]) {
        if (millis() - lastButtonPress > 5000) {
            climateState[0] = false;
            lastButtonPress = millis();
        }
    }
    digitalWrite(LED_PIN, climateState[0] ? HIGH : LOW);

    // Gestion du bouton avec anti-rebond
    if (buttonState == LOW && lastButtonState == HIGH && millis() - lastButtonPress > 50) {
        displayPage = !displayPage;
        lcd.clear();
        lastButtonPress = millis();
    }
    lastButtonState = buttonState;

    // Affichage sur l'écran LCD
    if (displayPage) {
        // Page 1 : Température sur la première ligne, état de la climatisation en dessous
        lcd.setCursor(0, 0);
        lcd.print("Temp: ");
        lcd.print(temperatureValues[0], 1);
        lcd.print("C   ");
        
        lcd.setCursor(0, 1);
        lcd.print("AC: ");
        lcd.print(climateState[0] ? "ON " : "OFF");
    } else {
        // Page 2 : Vitesse et direction
        lcd.setCursor(0, 0);
        if (speedValues[0] > 0) {
            lcd.print("Avance: ");
            lcd.print(speedValues[0]);
            lcd.print("km/h  ");
        } else if (speedValues[0] < 0) {
            lcd.print("Recule: ");
            lcd.print(abs(speedValues[0]));
            lcd.print("km/h  ");
        } else {
            lcd.print("Vitesse: 0 km/h");
        }
        
        lcd.setCursor(0, 1);
        lcd.print("Direction: ");
        if (directionValues[0] < 0) {
            lcd.print("G ");
        } else if (directionValues[0] > 0) {
            lcd.print("D ");
        } else {
            lcd.print("N/A");
        }
        lcd.print(abs(directionValues[0]));
        lcd.print("deg ");
    }

    // Envoi des données toutes les 100 ms
    if (currentMillis - lastMillis >= interval) {
        lastMillis = currentMillis;
        Serial.print("etd:2384980,x:");
        Serial.print(joystickX);
        Serial.print(", y:");
        Serial.print(joystickY);
        Serial.print(", sys:");
        Serial.println(climateState[0] ? "1" : "0");
    }

    delay(10);
}