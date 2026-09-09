
#include <Wire.h>

uint8_t SW_VERSION = 4;

#define ADC_BUFFER_BEFORE_SIZE    100
#define ADC_BUFFER_AFTER_SIZE     400

#define ADC_CHARGE_DELAY_US       (int)(400)

#define PHOTORESISTOR_ADC_NO      A7

uint16_t PHOTORESISTOR_ADC_AVG_INV;

uint8_t SIGNAL_GOING_HIGH;

const int PIN_DISPENSING_SUCCESSFUL = 14;

const int PIN_CAPTURE_SIGNAL = A1;
const int PIN_TRIGGER_I2C_COM = A2;

uint8_t NUMBER_OF_PELLETS;

float average = 0;

uint16_t ADC_SAMPLING_BUFFER[ADC_BUFFER_BEFORE_SIZE + ADC_BUFFER_AFTER_SIZE];

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

  pinMode(PHOTORESISTOR_ADC_NO, INPUT); // reading analog voltage input from photoresistor

  pinMode(PIN_DISPENSING_SUCCESSFUL, OUTPUT);
  digitalWrite(PIN_DISPENSING_SUCCESSFUL, LOW);

  pinMode(PIN_CAPTURE_SIGNAL, INPUT);
  pinMode(PIN_TRIGGER_I2C_COM, INPUT);

  NUMBER_OF_PELLETS = 0;

  SIGNAL_GOING_HIGH = 0;

  for (int i = 0; i < 1000; i++)
  {
    int adc_bits = analogRead(PHOTORESISTOR_ADC_NO); // initial reading
    delayMicroseconds(ADC_CHARGE_DELAY_US); // delay (value based on experiments) to charge the analog channel with current
    average = average + analogRead(PHOTORESISTOR_ADC_NO); // read again for a more accurate reading
  }
  average = average / 1000; // e.g. 725
  PHOTORESISTOR_ADC_AVG_INV = 1023 - average;

  Wire.beginTransmission(0x02); // transmit to device #0x02
  Wire.write((uint16_t)(PHOTORESISTOR_ADC_AVG_INV - 100));
  Wire.endTransmission();    // stop transmitting

  photores_threshold = (float)(PHOTORESISTOR_ADC_AVG_INV * 1.05);

  for (int i = 0; i < ADC_BUFFER_BEFORE_SIZE; i++)
  {
    ADC_SAMPLING_BUFFER[i] = (uint16_t)PHOTORESISTOR_ADC_AVG_INV;
  }

  //Serial.println("Average adc bits: " + String((int)average));
  Serial.println("PHOTORESISTOR_ADC_AVG_INV: " + String(PHOTORESISTOR_ADC_AVG_INV));

  Serial.println("Sensor board - Init finished.\n");
}

void loop()
{
  uint16_t adc_sample_inv_tmp;
  
  (void)analogRead(PHOTORESISTOR_ADC_NO); // initial reading
  delayMicroseconds(ADC_CHARGE_DELAY_US); // delay (value based on experiments) to charge the analog channel with current
  adc_sample_inv_tmp = (uint16_t)(1023 - analogRead(PHOTORESISTOR_ADC_NO)); // read again for a more accurate reading  

  // At index 0 we we always hold the oldest sample collected

  // The for loop shift cycle of 100 elements uses approx. It takes approx. 50us execution time on Arduino Nano Every.
  for (int i = 0; i < ADC_BUFFER_BEFORE_SIZE - 1; i++)
  {
    ADC_SAMPLING_BUFFER[i] = ADC_SAMPLING_BUFFER[i + 1];
  }

  ADC_SAMPLING_BUFFER[ADC_BUFFER_BEFORE_SIZE - 1] = adc_sample_inv_tmp;

  if (adc_sample_inv_tmp >= photores_threshold)
  {
    // Set a pin high, to stop rotating the wheel, in order to capture the full signal with all the pellets that are dispensed.
    digitalWrite(PIN_DISPENSING_SUCCESSFUL, HIGH); // Less than 1s execution time on Arduino Nano Every. This is negligeable for our case.
    delayMicroseconds(ADC_CHARGE_DELAY_US);

    //Serial.println("Pellet detected");
    //Serial.println("adc_bits_inv: " + String(adc_bits_inv));
    //Serial.println("photores_threshold: " + String(photores_threshold));

    for (int i = ADC_BUFFER_BEFORE_SIZE; i < ADC_BUFFER_BEFORE_SIZE + ADC_BUFFER_AFTER_SIZE; i++)
    {
      (void)analogRead(PHOTORESISTOR_ADC_NO); // initial reading
      delayMicroseconds(ADC_CHARGE_DELAY_US); // delay (value based on experiments) to charge the analog channel with current
      ADC_SAMPLING_BUFFER[i] = 1023 - analogRead(PHOTORESISTOR_ADC_NO); // read again for a more accurate reading
    }

    digitalWrite(PIN_DISPENSING_SUCCESSFUL, LOW);

    NUMBER_OF_PELLETS = 0;

    // Processing the entire signal, to determine the number of pellets.
    for (int i = 0; i < ADC_BUFFER_BEFORE_SIZE + ADC_BUFFER_AFTER_SIZE; i++)
    {
      if (SIGNAL_GOING_HIGH == 0)
      {
        float photores_threshold = (float)(PHOTORESISTOR_ADC_AVG_INV * 1.05);
        if (ADC_SAMPLING_BUFFER[i] >= (uint16_t)photores_threshold)
        {
          SIGNAL_GOING_HIGH = 1;

          NUMBER_OF_PELLETS++;
          Serial.println("NUMBER_OF_PELLETS = " + String(NUMBER_OF_PELLETS));
        }
      }
      else
      {
        float photores_threshold = (float)(PHOTORESISTOR_ADC_AVG_INV * 1.025);
        if (ADC_SAMPLING_BUFFER[i] <= (uint16_t)photores_threshold)
        {
          SIGNAL_GOING_HIGH = 0;
        }
      }
    }

    Wire.beginTransmission(0x02); // transmit to device #0x02
    Wire.write((int)NUMBER_OF_PELLETS);              // sends one byte
    Wire.endTransmission();    // stop transmitting

    // Sending the entire signal on the Serial port (including the buffer collected before trigger)
    for (int i = 0; i < ADC_BUFFER_BEFORE_SIZE + ADC_BUFFER_AFTER_SIZE; i++)
    {
      Serial.println("ADC_SAMPLING_BUFFER: " + String(ADC_SAMPLING_BUFFER[i]));
    }

    // Cleaning the buffer before trigger
    for (int i = 0; i < ADC_BUFFER_BEFORE_SIZE; i++)
    {
      ADC_SAMPLING_BUFFER[i] = (uint16_t)PHOTORESISTOR_ADC_AVG_INV;
    }
  }
}
