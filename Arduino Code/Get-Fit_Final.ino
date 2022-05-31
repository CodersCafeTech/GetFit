
/* Includes ---------------------------------------------------------------- */
#include <Get_Fit_inferencing.h>
#include <Arduino_LSM9DS1.h>
#include <ArduinoBLE.h>


/* Constant defines -------------------------------------------------------- */
#define CONVERT_G_TO_MS2    9.80665f
#define MAX_ACCEPTED_RANGE  2.0f        // starting 03/2022, models are generated setting range to +-2, but this example use Arudino library which set range to +-4g. If you are using an older model, ignore this value and use 4.0f instead

/* Private variables ------------------------------------------------------- */
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal

// Global Variables

float threshold = 0.0;
float confidence = 0.8;
bool flag = false;
String Label;



//Blutooth service activating

BLEService fitness_service("e267751a-ae76-11eb-8529-0242ac130003");

BLEIntCharacteristic exercise("2A19", BLERead | BLENotify); 
BLEByteCharacteristic start("19b10012-e8f2-537e-4f6c-d104768a1214", BLERead | BLEWrite);
BLEByteCharacteristic pause("6995b940-b6f4-11eb-8529-0242ac130003", BLERead | BLEWrite);

BLEDevice central;


void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);
    Serial.println("Edge Impulse Inferencing Demo");

    if (!IMU.begin()) {
        ei_printf("Failed to initialize IMU!\r\n");
    }
    else {
        ei_printf("IMU initialized\r\n");
    }

    if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 3) {
        ei_printf("ERR: EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME should be equal to 3 (the 3 sensor axes)\n");
        return;
    }
      if (!BLE.begin()) {
      Serial.println("starting BLE failed!");

      while (1);
    }

    // BLE spremenljivke

    BLE.setLocalName("Get-Fit");
    
    BLE.setAdvertisedService(fitness_service);

    //Karakteristike
    
    start.setValue(0);
    pause.setValue(0);

    
    fitness_service.addCharacteristic(exercise); 
    fitness_service.addCharacteristic(start);
    fitness_service.addCharacteristic(pause);



    BLE.addService(fitness_service);

    BLE.advertise();

    Serial.println("Bluetooth device active, waiting for connections...");

    while(1) {
      central = BLE.central();
      if (central) {
        Serial.print("Connected to central: ");
        // print the central's BT address:
        Serial.println(central.address());
        // turn on the LED to indicate the connection:
        digitalWrite(LED_BUILTIN, HIGH);

        break;
      }
    }
}

/**
 * @brief Return the sign of the number
 * 
 * @param number 
 * @return int 1 if positive (or 0) -1 if negative
 */
float ei_get_sign(float number) {
    return (number >= 0.0) ? 1.0 : -1.0;
}


void loop()
{
    if (central.connected()) 
    {
        start.read();
        pause.read();
    }
    if (start.value()) 
    {
        flag = true;
        //Serial.println("Started");
        start.setValue(0);
    }
    if(pause.value()) 
    {
         flag = false;
         //Serial.println("Stopped");
         pause.setValue(0);
    }
    if(flag == true)
    {
      //ei_printf("\nStarting inferencing in 2 seconds...\n");

      //delay(100);

      //ei_printf("Sampling...\n");

    // Allocate a buffer here for the values we'll read from the IMU
    float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = { 0 };

    for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += 3) {
        // Determine the next tick (and then sleep later)
        uint64_t next_tick = micros() + (EI_CLASSIFIER_INTERVAL_MS * 700);

        IMU.readAcceleration(buffer[ix], buffer[ix + 1], buffer[ix + 2]);

        for (int i = 0; i < 3; i++) {
            if (fabs(buffer[ix + i]) > MAX_ACCEPTED_RANGE) {
                buffer[ix + i] = ei_get_sign(buffer[ix + i]) * MAX_ACCEPTED_RANGE;
            }
        }

        buffer[ix + 0] *= CONVERT_G_TO_MS2;
        buffer[ix + 1] *= CONVERT_G_TO_MS2;
        buffer[ix + 2] *= CONVERT_G_TO_MS2;

        delayMicroseconds(next_tick - micros());
    }

    // Turn the raw buffer in a signal which we can the classify
    signal_t signal;
    int err = numpy::signal_from_buffer(buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
    if (err != 0) {
        ei_printf("Failed to create signal from buffer (%d)\n", err);
        return;
    }

    // Run the classifier
    ei_impulse_result_t result = { 0 };

    err = run_classifier(&signal, &result, debug_nn);
    if (err != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", err);
        return;
    }

    // print the predictions
    ei_printf("Predictions ");
    ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
        result.timing.dsp, result.timing.classification, result.timing.anomaly);
    ei_printf(": \n");

    
    
#if EI_CLASSIFIER_HAS_ANOMALY == 1
    ei_printf("    anomaly score: %.3f\n", result.anomaly);
        if(result.anomaly < threshold)
        {
            for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)  
              {      
                 ei_printf("    %s: %.5f\n", result.classification[ix].label, result.classification[ix].value);  
                 if(result.classification[ix].value > confidence )
                 {
                     Label = String(result.classification[ix].label);
                     Serial.println(Label);
                     if (Label == "Arm Circle")
                     {
                       if (central.connected()) exercise.writeValue(0);
                       Serial.println("Data sent");
                     }
                     else if (Label == "Squats")
                     {
                        if (central.connected()) exercise.writeValue(1);
                        Serial.println("Data sent");
                     }
                     else if (Label == "Pushup")
                     {
                        if (central.connected()) exercise.writeValue(2);
                        Serial.println("Data sent");
                     }
        
                  }
               }
      
         }
        
        }    
#endif
    }
     


#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_ACCELEROMETER
#error "Invalid model for current sensor"
#endif
