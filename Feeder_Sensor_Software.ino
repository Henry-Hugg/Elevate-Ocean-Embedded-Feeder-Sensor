
#include <Wire.h>

uint8_t SW_VERSION = 2;

#define ADC_BUFFER_BEFORE_SIZE    100
#define ADC_BUFFER_AFTER_SIZE     1900

#define ADC_CHARGE_DELAY_US       (int)(400)

#define PHOTORESISTOR_ADC_NO      A7

uint16_t PHOTORESISTOR_ADC_AVG_INV;

uint8_t SIGNAL_GOING_HIGH;

const int PELLET_DETECTED_PIN = 14;
const int PIN_CAPTURE_SIGNAL = A1;
const int PIN_TRIGGER_I2C_COM = A2;

float average = 0;

uint16_t ADC_SAMPLING_BUFFER[ADC_BUFFER_BEFORE_SIZE];

float photores_threshold;

void setup()
{
  Serial.begin(115200);
  delay(100);

  Serial.println("\n\nFeeder Sensor board - Init started.");

  Serial.println("SW version: " + String(SW_VERSION));

  Wire.begin();
  delay(10);
  Wire.setClock(100000); // Explicitly setting the speed to avoid a misconfiguration

  Serial.println("Sending init byte to verify that the I2C communication works");
  Wire.beginTransmission(0x02); // transmit to device #0x02
  Wire.write(0xBC);
  Wire.endTransmission();    // stop transmitting

  pinMode(PHOTORESISTOR_ADC_NO, INPUT); // reading analog voltage input from photoresistor

  pinMode(PELLET_DETECTED_PIN, OUTPUT);
  digitalWrite(PELLET_DETECTED_PIN, LOW);

  pinMode(PIN_CAPTURE_SIGNAL, INPUT);
  pinMode(PIN_TRIGGER_I2C_COM, INPUT);

  SIGNAL_GOING_HIGH = 0;

  for (int i = 0; i < 1000; i++)
  {
    int adc_bits = analogRead(PHOTORESISTOR_ADC_NO); // initial reading
    delayMicroseconds(ADC_CHARGE_DELAY_US); // delay (value based on experiments) to charge the analog channel with current
    average = average + analogRead(PHOTORESISTOR_ADC_NO); // read again for a more accurate reading
  }
  average = average / 1000; // e.g. 725
  PHOTORESISTOR_ADC_AVG_INV = 1023 - average;

  photores_threshold = (float)(PHOTORESISTOR_ADC_AVG_INV * 1.035);

  //Serial.println("Average adc bits: " + String((int)average));
  Serial.println("adc_buffer_inv: " + String((int)(1023 - average)));
  
  //Serial.println("Sensor board - Init finished.\n");
}

float adc_bits_inv;
int adc_bits;

uint16_t SAMPLING_IDX = 0;
uint16_t SAMPLING_IDX_STOP = 0;

uint16_t ADC_SAMPLING_BUFFER_AFTER[ADC_BUFFER_AFTER_SIZE];

void loop()
{
  (void)analogRead(PHOTORESISTOR_ADC_NO); // initial reading
  delayMicroseconds(ADC_CHARGE_DELAY_US); // delay (value based on experiments) to charge the analog channel with current
  ADC_SAMPLING_BUFFER[SAMPLING_IDX] = 1023 - analogRead(PHOTORESISTOR_ADC_NO); // read again for a more accurate reading  

  if (ADC_SAMPLING_BUFFER[SAMPLING_IDX] >= photores_threshold)
  {
    // Set a pin high, to stop rotating the wheel, in order to capture the full signal with all the pellets that are dispensed.
    
    SAMPLING_IDX_STOP = SAMPLING_IDX;

    for (int i = 0; i < ADC_BUFFER_AFTER_SIZE; i++)
    {
      (void)analogRead(PHOTORESISTOR_ADC_NO); // initial reading
      delayMicroseconds(ADC_CHARGE_DELAY_US); // delay (value based on experiments) to charge the analog channel with current
      ADC_SAMPLING_BUFFER_AFTER[i] = 1023 - analogRead(PHOTORESISTOR_ADC_NO); // read again for a more accurate reading
    }
    
    digitalWrite(PELLET_DETECTED_PIN, HIGH);
    //Serial.println("Pellet detected");
    //Serial.println("adc_bits_inv: " + String(adc_bits_inv));
    //Serial.println("photores_threshold: " + String(photores_threshold));

    float surface_covered = 0.0;

    Wire.beginTransmission(0x02); // transmit to device #0x02

    for (int i = SAMPLING_IDX_STOP + 1; i < ADC_BUFFER_BEFORE_SIZE; i++)
    {
      Wire.write((int)ADC_SAMPLING_BUFFER[i]);              // sends one byte

      Serial.println("adc_buffer_inv: " + String(ADC_SAMPLING_BUFFER[i]));

      if (ADC_SAMPLING_BUFFER[i] > PHOTORESISTOR_ADC_AVG_INV)
      {
        surface_covered += ADC_SAMPLING_BUFFER[i] - PHOTORESISTOR_ADC_AVG_INV;
      }
      else
      {
        surface_covered -= PHOTORESISTOR_ADC_AVG_INV - ADC_SAMPLING_BUFFER[i];
      }

      delay(1);
    }

    for (int i = 0; i < SAMPLING_IDX_STOP + 1; i++)
    {
      Wire.write((int)ADC_SAMPLING_BUFFER[i]);              // sends one byte

      Serial.println("adc_buffer_inv: " + String(ADC_SAMPLING_BUFFER[i]));

      if (ADC_SAMPLING_BUFFER[i] > PHOTORESISTOR_ADC_AVG_INV)
      {
        surface_covered += ADC_SAMPLING_BUFFER[i] - PHOTORESISTOR_ADC_AVG_INV;
      }
      else
      {
        surface_covered -= PHOTORESISTOR_ADC_AVG_INV - ADC_SAMPLING_BUFFER[i];
      }

      delay(1);
    }

    for (int i = 0; i < ADC_BUFFER_AFTER_SIZE; i++)
    {
      Wire.write((int)ADC_SAMPLING_BUFFER_AFTER[i]);              // sends one byte

      Serial.println("adc_buffer_inv: " + String(ADC_SAMPLING_BUFFER_AFTER[i]));

      if (ADC_SAMPLING_BUFFER_AFTER[i] > PHOTORESISTOR_ADC_AVG_INV)
      {
        surface_covered += ADC_SAMPLING_BUFFER_AFTER[i] - PHOTORESISTOR_ADC_AVG_INV;
      }
      else
      {
        surface_covered -= PHOTORESISTOR_ADC_AVG_INV - ADC_SAMPLING_BUFFER_AFTER[i];
      }

      delay(1);
    }

    Wire.endTransmission();    // stop transmitting

    SAMPLING_IDX = 0;
    SAMPLING_IDX_STOP = 0;

    digitalWrite(PELLET_DETECTED_PIN, LOW);

    //Serial.println("surface_covered = " + String(surface_covered / 10));
  }

  if (SAMPLING_IDX < ADC_BUFFER_BEFORE_SIZE - 1)
  {
    SAMPLING_IDX += 1;
  }
  else
  {
    SAMPLING_IDX = 0;
  }

}
