# SPECO - Personal Fall Detection System

<p align="center">
  <img src="URL_TO_YOUR_PROJECT_LOGO_OR_DEMO_IMAGE.png" alt="SPECO Device" width="400"/>
</p>

<p align="center">
  A compact, wearable device designed to automatically detect falls and alert caregivers, providing safety and peace of mind for the elderly and their families.
</p>

<p align="center">
  <a href="https://github.com/chidorisasuke/SPECO/issues">Report Bug</a>
  ·
  <a href="https://github.com/chidorisasuke/SPECO/issues">Request Feature</a>
</p>

## About The Project

**SPECO (Safe Pouch)** was created to address a critical challenge in elder care: providing a quick response in the event of a fall. Traditional alert systems often require the user to press a button, which may not be possible after a fall. SPECO solves this by using an onboard IMU sensor to detect falls automatically and instantly trigger a series of alerts.

This repository contains all the necessary code for the embedded device firmware and the companion real-time monitoring website.

## Key Features

* **Automatic Fall Detection:** Utilizes an IMU sensor to accurately detect the sudden change in orientation and impact associated with a fall.
* **Instant Local Alarm:** An onboard buzzer provides immediate audible feedback that a fall has been detected.
* **Broadcast Alert Messages:** Automatically sends alert messages (e.g., via SMS or a web service) to multiple pre-configured contacts.
* **Real-time Web Dashboard:** A companion website allows family or caregivers to monitor the user's status in real-time.
* **Compact & Powerful:** Built on the **ESP32C6** microcontroller, offering a powerful core in a small, wearable form factor.

## Technology Stack

This project is built with a combination of hardware and software:

* **Firmware (Embedded):**
    * [C++](https://isocpp.org/) on the Arduino Framework
    * Libraries for IMU sensor communication and Wi-Fi connectivity.
* **Hardware:**
    * ESP32C6 Microcontroller
    * IMU (Inertial Measurement Unit) Sensor
    * Buzzer
* **Website (Monitoring Dashboard):**
    * HTML5
    * CSS3
    * JavaScript

## Getting Started

To get a local copy up and running, follow these simple steps.

### Prerequisites

* Arduino IDE or PlatformIO installed.
* Necessary libraries for the ESP32 and IMU sensor (you can list them here, e.g., `Adafruit_MPU6050`).

### Installation

1.  **Clone the repo:**
    ```sh
    git clone [https://github.com/chidorisasuke/SPECO.git](https://github.com/chidorisasuke/SPECO.git)
    ```
2.  **Hardware Assembly:**
    * Connect the IMU sensor and buzzer to the correct pins on the ESP32C6 as described in the schematics (you can add schematics to your project later).
3.  **Software Setup:**
    * Open the `.ino` file in the Arduino IDE.
    * Update the `config.h` file with your Wi-Fi credentials and the phone numbers/endpoints for the alert messages.
    * Upload the code to your ESP32C6.

## Usage

Once the device is programmed and powered on, it will automatically connect to Wi-Fi and begin monitoring for falls. If a fall is detected:
1.  The local buzzer will sound.
2.  An alert will be sent to the web server and broadcasted to contacts.
3.  The web dashboard will update to show an "Alert" status.

## Contributing

Contributions are what make the open-source community such an amazing place to learn, inspire, and create. Any contributions you make are **greatly appreciated**.

If you have a suggestion that would make this better, please fork the repo and create a pull request. You can also simply open an issue with the tag "enhancement".
1.  Fork the Project
2.  Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3.  Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4.  Push to the Branch (`git push origin feature/AmazingFeature`)
5.  Open a Pull Request

## License

Distributed under the MIT License. See `LICENSE` file for more information.

## Contact

Yahya Bachtiar - [Your Email or LinkedIn Profile]

Project Link: [https://github.com/chidorisasuke/SPECO](https://github.com/chidorisasuke/SPECO)
