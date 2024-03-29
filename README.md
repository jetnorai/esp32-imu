## What is this?
This is a GyroFlow-compatible data logger firmware for the esp32 and esp32-c3 microcontrollers (and probably others supported by esp-idf)

### Required hardware
* An esp32 or esp32-c3 with 4MB flash
* A BMI270, BMI160 or MPU6050 IMU
* 4 wires

You can get these as separate modules or on a single board (EspLog)

## Parameter docs

[HERE](docs/setings.md)

## How to flash and get started with the DIY version?
[YouYube tutorial](https://www.youtube.com/watch?v=SZeJi4hdDFE)

1. Connect your IMU board to the ESP board. For esp32 it is recommended to connect SDA and SCL to pins 16 and 17 accordingly. For esp32-c3 - to pins 6 and 7.
1. Download the firmware from github. You can find the latest build in "Artifacts" of the latest build from the "Actions" tab.
The zip file will contain a readme file with a command line for flashing using [esptool](https://github.com/espressif/esptool/releases/tag/v4.1).
2. After flashing the firmware you should see a wifi network with SSID starting with "esplog_". Connect to that network with "12345678" password.
3. Go to [http://192.168.4.1/settings](http://192.168.4.1/settings), enter the SDA and SCL pin numbers you have connected the IMU to, click "Apply".
4. Reboot the board and go to [http://192.168.4.1](http://192.168.4.1). If the IMU is OK, "Avg gyro sample int. (ns)" should be non-zero.
5. Calibrate the accelerometer. For that you need to go to [http://192.168.4.1/calibration](http://192.168.4.1/calibration), add some calibration points (for example x-up, x-down, y-up, y-down, z-up, z-down). Hold your IMU stationary for some time before adding a points as the accelerometer data is low-pass filtered. Then click "Calculate offsets" and then "Save to flash".
5. You can connect a button and a led to any free pins and assign the pin numbers in settings. NOTE: when recording is started using the button, wifi is disabled until you stop the recording using the button. This is done intentionally.

## Screenshots

<img src="img/screenshot_main.png" width="540"></img>

<img src="img/screenshot_settings.png" width="540"></img>

<img src="img/screenshot_calibration.png" width="540"></img>

## Other resources

[Test flight](https://www.youtube.com/watch?v=B9Z8ehW_aUw)

[Video about workflow](https://www.youtube.com/watch?v=ldi0L7vc7pM)

### EspLog PCB (the oldest one, 4 layers, proper ceramic antenna)

Four-layer 11 x 14 mm PCB with esp32-c3, 3.3v ldo and either bmi160, bmi270 or lsm6dsr gyro.
The side with the components can be submerged in epoxy, making the logger almost a perfect 11x14x2.5mm cube, so it is easier to glue to a camera.

[EasyEDA project](https://oshwlab.com/vladimir.pinchuk01/gyro-logger-esp32c3_copy)

<img src="img/esplog_02_bot.svg" width="320"></img>
<img src="img/esplog_02_top.svg" width="320"></img>

Assembled:

<img src="img/esplog_02_front.jpg" width="320"></img>
<img src="img/esplog_02_back.jpg" width="320"></img>

### 2-layer PCB designs
There also are two alternative designs in this repository:
https://github.com/VladimirP1/esplog-hardware

To open the project files, use KiCad 6

1) `esplog_hv` is a 2-layer deisgn with a built-in switching regulator supporting 4.5-30v input voltage range.
2) `esplog_ldo` is a 2-layer design, which is almost exacly the same as the older 4-layer design.

Both 2-layer designs omit the ceramic antenna(because it can be very hard to buy) and implement the antenna as a random piece of trace. Wifi TX power over 7dbm cannot be used (otherwise the signal gets distorted too much to be recieved).
