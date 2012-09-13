Author: Jesse Brannon

This simple program demonstrates basic features of the BeagleBone.
The Bone communicates via I2C to a temperature sensor. The measured temperature is displayed to the console every time that a switch is pressed (I just used a wire and touched it to Vdd to simulate a switch). If the temperature goes above 26 C, a red LED turns on. This is done via a GPIO output. An external LED acts as a secondary hearbeat by being PWMed with a frequency of 1 Hz. Finally, the Vdd supplied to the temperature sensor is read by an analog input to verify that the device is getting power. The user will be alerted via the command line if the sensor is not powered.

To use, just compile and run. The I2C tools are required.
