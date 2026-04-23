<div align="center">

<img src="https://readme-typing-svg.herokuapp.com?font=Fira+Code&size=30&pause=1000&color=F7B731&center=true&vCenter=true&width=600&lines=☀️+SOL-NODE;Solar+Panel+Fault+Detection;Real-Time+IoT+Monitoring+System" alt="Typing SVG" />

<br/>

![GitHub repo size](https://img.shields.io/github/repo-size/ssanjay-be/SOL-NODE?color=orange&style=for-the-badge)
![GitHub stars](https://img.shields.io/github/stars/ssanjay-be/SOL-NODE?style=for-the-badge&color=yellow)
![GitHub forks](https://img.shields.io/github/forks/ssanjay-be/SOL-NODE?style=for-the-badge&color=green)
![Platform](https://img.shields.io/badge/Platform-ESP32-blue?style=for-the-badge&logo=espressif)
![Language](https://img.shields.io/badge/Language-C++-orange?style=for-the-badge&logo=cplusplus)
![Status](https://img.shields.io/badge/Status-Active-brightgreen?style=for-the-badge)
![Conference](https://img.shields.io/badge/Conference-Paper%20Published-red?style=for-the-badge)

</div>

---

## 📌 Overview

**SOL-NODE** is a real-time solar panel fault detection and health monitoring system built on the **ESP32 microcontroller**. It uses a perturbation-based diagnostic method to automatically identify and classify faults such as wiring failures, load faults, battery issues, and panel degradation — all displayed on a live web dashboard and OLED screen.

> 💡 This project was developed as part of a **conference research paper** at M Kumarasamy College of Engineering.

---

## ✨ Key Features

| Feature | Description |
|---|---|
| ⚡ Real-time Monitoring | Continuous voltage, current & power tracking |
| 🧠 Baseline Learning | Auto-learns normal operating conditions |
| 🔍 Fault Detection | Detects deviations from baseline automatically |
| 🔬 Perturbation Diagnosis | MOSFET-based switching to classify fault type |
| 🌐 Web Dashboard | Live updates via ESP32 hosted web server |
| 📟 OLED Display | On-site fault status and readings |
| 💰 Low Cost | Built with affordable, compact hardware |

---

## 🔧 Hardware Components

| Component | Model | Purpose |
|---|---|---|
| Microcontroller | ESP32 | Main processing unit |
| Current/Voltage Sensor | INA219 | Electrical parameter sensing |
| Display | OLED SSD1306 | Local status display |
| Voltage Regulator | LM2596 | Stable 5V power supply |
| Load Switch | MOSFET | Perturbation injection |
| Fault Simulator | Toggle Switches | Controlled fault testing |
| Load Unit | 12V Automotive Bulb | Load representation |

---

## 🗂️ System Architecture

```
Solar Panel
    │
    ▼
INA219 Sensor (Voltage & Current)
    │
    ▼
ESP32 Microcontroller
    ├──► Baseline Learning Module
    ├──► Behavioural Drift Detection
    ├──► Perturbation-Based Diagnosis
    ├──► OLED Display (Local Status)
    └──► Web Dashboard (Remote Access)
```

---

## 🔄 How It Works

```
START
  │
  ▼
Data Acquisition ──► Learn Baseline
  │
  ▼
Detect Behavioural Drift
  │
  ├── Within Threshold? ──► NO ──► Back to Acquisition
  │
  └── YES
        │
        ▼
   Apply Diagnosis
        │
        ▼
Fault & Health Estimation
        │
        ▼
Display System Status
        │
        ▼
       END
```

---

## 📊 Fault Detection Results

The system was tested under 4 controlled fault conditions:

### 1️⃣ Normal Operating Condition
> System stabilizes voltage and current simultaneously. Baseline measurements show no deviation — system functioning correctly.

### 2️⃣ Wiring Fault
> Breaking the current path caused sudden voltage drop and zero current output. System confirmed wiring fault with zero recovery signs.

### 3️⃣ Load Fault
> Higher load caused current spike and voltage fluctuation. Unstable power output flagged as load fault.

### 4️⃣ Panel Degradation
> Gradual reduction in voltage and current confirmed panel degradation via steady power output decrease.

<div align="center">
<img src="images/fig3_fault_signatures.png" width="80%" alt="Fault Signatures"/>
<br/>
<em>Fig — Fault signatures captured by SOL-NODE system</em>
</div>

---

## ⚙️ Setup & Installation

### Prerequisites
- Arduino IDE installed
- ESP32 board package installed
- Required libraries:
  - `Adafruit INA219`
  - `Adafruit SSD1306`
  - `ESPAsyncWebServer`
  - `ArduinoJson`

### Steps

**1. Clone the repository**
```bash
git clone https://github.com/ssanjay-be/SOL-NODE.git
cd SOL-NODE
```

**2. Open Arduino IDE**
```
File → Open → code/sol_health_node.ino
```

**3. Configure WiFi credentials**
```cpp
// In sol_health_node.ino, update:
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
```

**4. Select Board & Upload**
```
Tools → Board → ESP32 Dev Module
Tools → Port → Select your COM port
Click Upload ▶
```

**5. Access Web Dashboard**
```
Open Serial Monitor → Copy ESP32 IP Address
Open browser → http://[ESP32-IP-ADDRESS]
```

---

## 📁 Project Structure

```
SOL-NODE/
│
├── README.md
├── code/
│   ├── sol_health_node.ino       ← Main ESP32 firmware
│   └── dashboard_index.html      ← Web dashboard UI
├── images/
│   └── fig3_fault_signatures.png ← Fault signature plots
├── paper/
│   └── THESES-10.pdf             ← Conference research paper
└── docs/
    └── circuit_diagram.png       ← Hardware schematic
```

---

## 📄 Research Paper

This project was presented at a conference as a research paper.

📎 [Download Full Paper (PDF)](paper/THESES-10.pdf)

**Abstract:** SOL-NODE proposes a low-cost, real-time solar panel fault detection system using ESP32 with perturbation-based diagnostics. The system accurately detects and classifies wiring faults, load faults, battery issues, and panel degradation with minimal hardware complexity.

---

## 📈 Comparison with Existing Systems

| Parameter | Existing Systems | SOL-NODE |
|---|---|---|
| Monitoring | Passive | ✅ Active |
| Fault Detection | External analysis | ✅ Real-time on-device |
| Fault Diagnosis | Complex/Unavailable | ✅ Perturbation-based |
| Cost | High | ✅ Low-cost |
| Processing | Cloud/External | ✅ Local ESP32 |
| Real-time | Limited | ✅ Fully real-time |
| Dashboard | Some systems | ✅ Live web dashboard |
| Deployment | Complex | ✅ Easy integration |

---

## 👨‍💻 Author

<div align="center">

| | |
|---|---|
| **Name** | Sanjay S |
| **Institution** | M Kumarasamy College of Engineering |
| **Email** | sanjaysece7@gmail.com |
| **LinkedIn** | [sanjay-s-296162362](https://www.linkedin.com/in/sanjay-s-296162362/) |
| **GitHub** | [ssanjay-be](https://github.com/ssanjay-be) |

</div>

---

## 📜 License

This project is licensed under the **MIT License** — feel free to use, modify, and distribute with attribution.

---

<div align="center">

### 🌟 If this project helped you, please give it a star!

[![GitHub stars](https://img.shields.io/github/stars/ssanjay-be/SOL-NODE?style=social)](https://github.com/ssanjay-be/SOL-NODE)

**Made with ❤️ by Sanjay S | M Kumarasamy College of Engineering**

</div>
