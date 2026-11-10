# 💊 MediRoute
### Autonomous Medicine Delivery Robot with Obstacle Avoidance and Secure Handover

**An ESP32-based, low-cost healthcare robotics prototype integrating ultrasonic obstacle detection, RFID-controlled medicine access, and simulation-based machine learning analysis.**

![Platform](https://img.shields.io/badge/Platform-ESP32-blue)
![Language](https://img.shields.io/badge/Language-C%2B%2B%20%7C%20Python-green)
![ML](https://img.shields.io/badge/ML-Scikit--learn-orange)
![Status](https://img.shields.io/badge/Status-Research%20Prototype-yellow)

---

## 📌 Overview

The [local dashboard companion](mediroute-dashboard/README.md) adds face login, offline speech transcription, and a project assistant with 62 topics. Follow its setup instructions to run it on your computer.

**MediRoute** is a low-cost, two-wheel-drive medicine transport robot developed to explore autonomous indoor delivery in healthcare environments.

The project combines:

- **Ultrasonic obstacle detection** for reactive navigation.
- **ESP32-based motor control** for robot movement.
- **RFID authentication** for controlled access to medicines.
- **Servo-operated compartment** for medicine handover.
- **Machine learning** for analyzing simulated obstacle avoidance and delivery performance.

The goal is to demonstrate an affordable healthcare robotics platform that can be extended with more advanced navigation and monitoring capabilities.

> **Project scope:** MediRoute currently focuses on reactive obstacle avoidance and compartment access. Reliable room-to-room navigation, localization, and clinical deployment are not implemented or validated.

---

## 🎯 Objectives

1. Develop an affordable indoor medicine transport robot.
2. Implement obstacle detection using ultrasonic sensors.
3. Control robot movement using an ESP32 microcontroller.
4. Demonstrate RFID-based access to the medicine compartment.
5. Analyze obstacle avoidance using machine learning.
6. Evaluate how detection distance, payload, and obstacle type affect simulated performance.

---

## ⚙️ Hardware Components

| Component | Purpose |
|---|---|
| ESP32 DevKit V1 | Main microcontroller |
| HC-SR04 ultrasonic sensors | Detect nearby obstacles |
| TB6612FNG motor driver | Control DC motor speed and direction |
| 2 × TT geared DC motors | Robot movement |
| 2WD chassis with caster | Mechanical support and mobility |
| RC522 RFID reader | Read RFID tags for access verification |
| SG90 servo motor | Operate medicine compartment latch |
| MPU6050 | Optional acceleration and orientation sensing |
| 4 × AA battery pack | Portable power source |
| Medicine compartment | Carry small medicine packages |

**Hardware note:** The exact motor-driver installation, IMU integration, GPIO assignments, and regulated power wiring should be checked against the final physical build.

---

## 🏗️ System Architecture

```text
                MediRoute System
                       |
                  ESP32 DevKit V1
                       |
       +---------------+----------------+
       |               |                |
  Ultrasonic       Motor Driver      RFID Reader
   Sensors          TB6612FNG          RC522
       |               |                |
  Obstacle         Left & Right     Authorized Tag
  Detection         DC Motors           |
       |               |             SG90 Servo
       |               |                |
       +---------------+          Medicine Compartment
                       |
               Reactive Navigation
```

### Working Principle

**Step 1 — Movement**

The ESP32 controls the two geared motors through the motor driver.

**Step 2 — Obstacle Detection**

Ultrasonic sensors estimate the distance between the robot and nearby obstacles.

**Step 3 — Obstacle Avoidance**

When an obstacle enters the configured detection range, the robot's intended control logic stops or changes its movement.

**Step 4 — RFID Verification**

An RFID card is scanned and its identifier is checked against authorized identifiers.

**Step 5 — Medicine Handover**

Upon successful verification, the servo can release the medicine compartment.

The system demonstrates local reactive control rather than full autonomous navigation to a verified hospital room.

---

## 💻 Software and Technologies

| Technology | Application |
|---|---|
| Arduino IDE | ESP32 firmware development |
| Embedded C/C++ | Sensor reading and motor control |
| Python | Simulation and data analysis |
| NumPy | Synthetic data generation |
| Pandas | Dataset processing |
| Matplotlib | Graphs and visualizations |
| Scikit-learn | Machine-learning models |
| Jupyter Notebook | ML experimentation |

---

## 🤖 Machine Learning Analysis

We developed a simulation-based ML workflow to study how different operating conditions influence obstacle avoidance.

### Dataset

The dataset contains **135 synthetic trials** generated using a reproducible stochastic model.

| Parameter | Values |
|---|---|
| Detection threshold | 20, 30, 40 cm |
| Payload | 0, 100, 200 g |
| Obstacle type | Cardboard box, chair leg, hanging fabric |
| Repetitions | 5 per combination |
| Total trials | 135 |

**Important:** These observations are simulation-generated and are not measurements collected from the physical robot.

### ML Models

**Logistic Regression**

Predicts whether a simulated trial results in successful obstacle avoidance.

**Random Forest Classifier**

Uses an ensemble of decision trees to classify avoidance success.

**Random Forest Regressor**

Predicts continuous values, including stopping clearance and traversal time.

### Classification Results

| Model | Accuracy | Balanced Accuracy | ROC-AUC |
|---|---:|---:|---:|
| Logistic Regression | 55.9% | 66.7% | 0.741 |
| Random Forest | 67.6% | 54.2% | 0.783 |

These results are from held-out synthetic observations. Neither classifier exceeded the 82.4% majority-class baseline in ordinary test accuracy.

### Regression Results

| Prediction Target | MAE | R² |
|---|---:|---:|
| Stopping clearance | 7.69 cm | -0.045 |
| Traversal time | 1.07 seconds | 0.293 |

The results show limited predictive performance and highlight the need for more data and physical validation.

---

## 📊 Simulation Results

The simulated experiment evaluated 135 trials across three obstacle detection thresholds.

| Threshold | Avoidance Success | Mean Travel Time | Mean Signed Clearance |
|---|---:|---:|---:|
| 20 cm | 82.2% | 19.68 s | 8.72 cm |
| 30 cm | 84.4% | 20.25 s | 18.53 cm |
| 40 cm | 77.8% | 21.32 s | 23.36 cm |

The results suggest a simulated trade-off between stopping clearance and travel time.

However, the differences in success rate are affected by stochastic detection failures. They do not establish a universally optimal threshold for the physical robot.

---

## ✨ Project Contributions

### 1. Low-Cost Healthcare Robotics

Integration of affordable embedded components into a medicine transport prototype.

### 2. Reactive Obstacle Avoidance

Use of ultrasonic sensing and differential-drive motor control for basic obstacle response.

### 3. RFID-Controlled Handover

A simple authorization mechanism linked to a servo-operated medicine compartment.

### 4. Reproducible Simulation Framework

A factorial experiment analyzing detection threshold, payload, and obstacle type.

### 5. Machine-Learning Evaluation

Classification and regression experiments for predicting synthetic robot performance.

These contributions emphasize system integration and evaluation rather than claiming invention of a new obstacle-avoidance or machine-learning algorithm.

---

## 📁 Suggested Repository Structure

```text
MediRoute/
│
├── README.md
│
├── firmware/
│   └── mediroute_esp32.ino
│
├── hardware/
│   ├── circuit_diagram.png
│   └── components.md
│
├── dataset/
│   └── MediRoute_Synthetic_Data.csv
│
├── notebooks/
│   └── MediRoute_ML_Analysis.ipynb
│
├── research/
│   └── MediRoute_IEEE_Paper.pdf
│
├── images/
│   ├── robot_front.jpg
│   └── robot_side.jpg
│
└── requirements.txt
```

*This is a suggested structure. Add or rename files to match the actual repository contents.*

---

## 🚀 Getting Started

### Hardware

1. Assemble the 2WD chassis and mount the motors.
2. Install the ESP32, motor driver, ultrasonic sensors, RFID reader, and servo.
3. Connect the components according to a verified wiring diagram.
4. Provide correctly regulated power and protect ESP32 inputs from 5 V signals.
5. Upload and test the ESP32 firmware using Arduino IDE.
6. Verify individual components before testing integrated robot movement.

### Machine Learning

Install the required Python libraries:

```bash
pip install numpy pandas matplotlib scikit-learn jupyter
```

Start Jupyter Notebook:

```bash
jupyter notebook
```

Open the MediRoute ML notebook and load the synthetic CSV dataset.

Run the cells to generate performance graphs, train models, and evaluate their predictions.

---

## ⚠️ Limitations

- No reliable indoor localization or destination confirmation.
- Ultrasonic sensors have limited obstacle coverage.
- Basic RFID UID matching is not clinical-grade authentication.
- No validated real-world collision avoidance statistics.
- No autonomous charging.
- Machine-learning models are not deployed on the ESP32.
- Synthetic ML results cannot establish physical robot reliability.

---

## 🔮 Future Scope

- Indoor localization using visual markers or mapped landmarks.
- Wheel-encoder feedback and improved motion control.
- Enhanced obstacle sensing and emergency-stop mechanisms.
- Lightweight edge ML for sensor analysis.
- Improved authentication and compartment monitoring.
- Physical experiments to validate the simulation.
- Hospital workflow integration after appropriate safety testing.

---

## 👨‍💻 Project Team

**Department of Computing Technologies**  
School of Computing  
SRM Institute of Science and Technology, Kattankulathur

| Name | Registration Number |
|---|---|
| Rian K Sinu | RA2411003010586 |
| Namish Jindal | RA2411003010622 |
| Mehul Bansal | RA2411003010403 |
| Devashya Kothari | RA2411003011130 |

### Faculty Guide

**Dr. R. Kayalvizhi**  
Department of Networking and Communications  
School of Computing  
SRM Institute of Science and Technology

**Course:** 21CSE305P — SERBOT: Project Based Learning in Robotics  
**Team:** 2

---

## 📄 Research

**Title:** *MediRoute: Factorial Simulation and Machine-Learning Analysis of Obstacle Avoidance in a Low-Cost Medicine Transport Robot*

The research evaluates a low-cost medicine transport prototype through a reproducible stochastic simulation and supervised machine-learning experiments.

The paper includes system architecture, hardware design, simulated performance comparisons, classification results, regression analysis, and limitations.

---

## 📜 Disclaimer

MediRoute is an educational and research prototype.

It is not a certified medical device or a clinically validated autonomous delivery system. Simulation-generated results must not be interpreted as measured hardware performance.

---

**MediRoute — Exploring Affordable Robotics for Smarter Healthcare.**
