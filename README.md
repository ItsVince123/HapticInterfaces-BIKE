# Restoring Spatial Awareness for Elderly Cyclists: A Haptic Interface Prototype

## Introduction

Cycling provides measurable cognitive and physical benefits for older adults [3], [4], yet it remains inherently dangerous in complex traffic environments [11]. Many older cyclists gradually lose the two traffic senses they rely on most: peripheral vision (often resulting from glaucoma) and sound localization (due to age-related hearing loss). This sensory decline makes elderly cyclists incredibly vulnerable to unseen lateral hazards. Furthermore, driving and mobility studies demonstrate that around 1 in 4 people aged 60-80 with glaucoma stop driving [1], [2], often disengaging from active transportation entirely. The recent surge in e-bike popularity compounds this issue; their heavier weight and faster speeds have led to a massive spike in severe crashes among older populations.

Current commercial safety solutions have notable gaps. Traditional bike radars provide a binary "behind-you" warning without conveying specific left/right spatial information. More importantly, attempting to solve this by adding another screen to the handlebars introduces a dangerous visual distraction, increasing driver perception-brake times [8], [9]. Mirror glances cost valuable forward attention, and auditory beeps are easily lost in loud city traffic.

Our project addresses these limitations by utilizing haptic technology [5], [10]. A short vibration on the left or right handlebar grip utilizes the tactile channel—which is almost always free while cycling—to naturally convey threat direction without visual or auditory load [6], [7]. To complement the rider's spatial awareness, the system also incorporates an inertial measurement unit (IMU) that provides automated brake and crash detection, engaging a rear LED to proactively alert trailing traffic. This prototype acts as a comprehensive, screen-free hardware solution that restores spatial awareness and communicates intent, keeping the rider's eyes firmly on the road forward.

---

## Supplies

The total estimated cost for this prototype is approximately €160. To replicate this build, you will need the following components:

| Component | Role / Justification | Qty | Estimated Price |
| :--- | :--- | :--- | :--- |
| **HC-SR04 ultrasonic sensors** | Left/right side-proximity detection; 1 cm – 300 cm range. | 2 | €3 |
| **MPU-6050 IMU (accel + gyro)** | Forward-axis brake detection and crash sensing. | 1 | €4 |
| **TacHammer vibrotactile actuators** | Left/right side alerts via haptic patterns. Driven by DRV2605. | 2 | €80 |
| **Arduino Uno (ATmega328P)** | Master controller for blind spot sensors and Hall effect interrupts. | 1 | €25 |
| **Arduino Micro (ATmega32U4)** | Dedicated IMU and brake-light loop (relies on Timer4 PWM). | 1 | €25 |
| **TCA9548A I2C Multiplexer** | Routes I2C commands to the dual DRV2605 drivers. | 1 | €4 |
| **Red LED + 220-ohm resistor** | Auto-engaged brake light; flashes on crash-latch events. | 1 | €1 |
| **Hall sensor (KY-003) + magnet** | Wheel-magnet speed and distance tracking. | 1 | €5 |
| **9V battery connector** | Portable supply for ~8 hours of continuous operation. | 1 | €3 |
| **Jumper Cables (F-to-F)** | Mechanical integration on handlebar stem and seat post. | 40 | €5 |
| **3D Printed Enclosures** | Custom frame mounts to secure electronics to the bike chassis. | 1 | €0 (Custom) |

*Note: All necessary CAD files for the 3D-printed enclosures and complete electrical schematics are available in this repository to ensure full reproducibility.*

---

## Methods

### Step 1: Conceptual Framework and Component Rationale
The architectural design of this system is fundamentally structured around multimodal sensory integration, prioritizing real-time environmental monitoring without imposing cognitive load on the user. The components were selected based on the following rationales:
* **Ultrasonic Sensors (HC-SR04):** Chosen for cost-effective, time-of-flight acoustic proximity detection. By emitting high-frequency sound waves and measuring the echo return time, these sensors accurately determine the distance of lateral hazards (like overtaking vehicles) in the rider's blind spots.
* **Hall Effect Sensor (KY-003):** Selected for robust, non-contact rotational telemetry. A permanent magnet is affixed to the wheel spokes; as the wheel rotates, the magnet passes the sensor, inducing a measurable fluctuation in the magnetic field. This allows the system to calculate precise speed and distance metrics independent of GPS signals.
* **Inertial Measurement Unit (MPU-6050):** A 6-axis MEMS (Micro-Electromechanical Systems) sensor utilized to capture the kinetic state of the bicycle. It provides high-resolution acceleration and angular velocity data, enabling the system to deduce intentional deceleration (braking) and uncontrolled kinetic events (crashes).
* **TacHammer Actuators:** Deployed on the steering wheel to leverage the tactile communication channel. These specific voice-coil actuators provide distinct, high-fidelity haptic patterns that are easily distinguishable from standard road vibration.

### Step 2: Physical Construction, Power, and Integration
The mechanical assembly required strategic distribution of components across the bicycle frame. 
* **Frame Integration:** The Arduino Micro, the MPU-6050 IMU, both HC-SR04 ultrasonic sensors, the safety LED (with its 220-ohm resistor), the 9V battery, and the Hall effect sensor are physically secured and glued to the bicycle frame using custom 3D-printed mounts. Internal data and power pathways are established using standard female-to-female jumper cables.
* **Haptic Placement:** The two TacHammer actuators (referred to internally as "drakes") are mounted directly onto the left and right grips of the steering wheel (handlebars).
* **Power and Testing Configuration:** To manage power constraints and facilitate debugging, the system employs a dual-power strategy. The onboard Arduino Micro is powered independently by a 9V battery. Conversely, the Arduino Uno is maintained separately on a testing breadboard and is powered directly via a PC USB connection. During testing and calibration phases, the ultrasonic sensors, the Hall effect sensor, and the haptic actuators can be quickly routed to the breadboard/Uno for live serial monitoring and algorithm refinement.

### Step 3: The Concurrency Engine (Arduino Uno)
The primary software challenge was the simultaneous execution of distance sensing and high-speed wheel tracking on the Arduino Uno. Standard methodologies for ultrasonic sensors (e.g., the `pulseIn()` function) block the processor while waiting for an acoustic echo, which would destroy the precise timing required for I2C haptic patterns and cause the system to miss wheel rotations.

To achieve microsecond-level concurrency, we engineered a non-blocking architecture:
* **Asynchronous State Machine:** The two HC-SR04 sensors are managed by a custom state machine (`US_IDLE`, `US_WAIT_RISE`, `US_WAIT_FALL`, `US_GAP`). The Uno rapidly compares pin states against `micros()` timestamps, advancing the state only upon hardware pin changes. This ping-pongs the sensors with a 40 ms settle gap to prevent acoustic cross-interference, leaving the main loop running unhindered.
* **Hardware Interrupts:** Concurrently, the Hall effect sensor is wired to `D2 (INT0)`. Whenever the wheel magnet passes the sensor, a hardware interrupt is triggered. This physically interrupts the CPU for a fraction of a millisecond to increment the wheel tick counter in the background before instantly resuming the main loop. 

### Step 4: Resolving Haptic Hardware Constraints
During integration, a hardware conflict arose: both left and right TacHammer motors buzzed simultaneously regardless of the threat's location. The TCA9548A I2C multiplexer only switches the control registers of the DRV2605 haptic drivers, not the actual PWM drive signal.
* **The PWM Split:** We resolved this by electrically isolating the PWM lines. Using the ATmega328P's internal Timer2, we configured two independent PWM outputs: Pin 11 (`OC2A`) for the left motor and Pin 3 (`OC2B`) for the right motor.
* **Signed PWM Scheme:** The haptic library operates on a signed PWM scheme where a ~50% duty cycle (`PWM_NEUTRAL = 127`) equates to zero mechanical drive. When a left-side threat is detected, the right motor's PWM is parked exactly at neutral, ensuring complete mechanical isolation between the left and right tactile alerts.

### Step 5: Adaptive IMU Algorithm (Arduino Micro)
Detecting a braking bicycle is notoriously complex because a static deceleration threshold will trigger false positives when riding uphill and fail to trigger when riding downhill. The Arduino Micro runs a sophisticated, self-correcting adaptive algorithm on the MPU-6050 data at 50 Hz to solve this:
* **The Dynamic Baseline:** The code applies a slow low-pass filter (`forwardBaseline += 0.01 * (forward - forwardBaseline)`) to the forward acceleration axis. This creates a floating baseline that constantly adjusts to gravity (hill inclines) and average cruising speed, acting as a ~2-second time constant.
* **The "Quiet Band" Lockout:** To prevent the baseline from absorbing sudden stops, the algorithm implements a `BASELINE_QUIET_BAND`. The baseline is only permitted to update when the acceleration signal is exceptionally quiet. When the rider brakes hard, the baseline mathematically locks into place, providing a rock-solid reference point for the duration of the stop. If the acceleration drops below this locked baseline by `0.6 m/s²` for three consecutive samples, the LED jumps to full brightness.
* **Omnidirectional Crash Detection:** The crash logic calculates the raw vector magnitudes (`sqrt(x² + y² + z²)`) for both acceleration and angular velocity. If the bike experiences a shock greater than `30.0 m/s²` or a violent spin over `6.0 rad/s` on any axis, the system latches into an emergency state, rapidly flashing the rear LED to alert surrounding drivers.

---

## Discussion

The prototype effectively translates spatial, kinetic, and telemetry data into tactile and visual feedback, directly addressing the sensory deficits common in older cyclists. By utilizing independent left/right haptic channels, the system successfully eliminates the need for visual dashboard checks, mitigating the cognitive load associated with mirror checking. 

Bench testing confirms that both the sensing matrix and the safety lighting perform exceptionally well. The IMU-driven brake and crash detection algorithms operate reliably, proving that the adaptive low-pass filter with a quiet-band lockout can produce a highly accurate, auto-calibrating brake light without requiring complex trigonometry. 

The decision to separate the processing load across two microcontrollers (the Uno and the Micro) was dictated by two primary constraints encountered during development:
1. **Mechanical Restraints:** The physical dimensions of our available 3D printer limited the maximum printable volume of the hardware enclosure, preventing the use of a single, larger, consolidated PCB layout. 
2. **Computational Overhead & Task Scheduling:** Task scheduling proved prohibitive on a single 8-bit microcontroller. Managing the microsecond-level timing of two asynchronous ultrasonic sensors alongside a high-priority Hall effect hardware interrupt already saturated the Arduino Uno's processing capabilities. Attempting to add a 50 Hz I2C polling loop for the IMU to the same processor caused unacceptable latency and compromised the haptic feedback's timing accuracy.

---

## Conclusion and Future Work

This project demonstrates a highly viable proof-of-concept: haptic feedback can seamlessly replace visual and auditory dashboards to restore spatial awareness for elderly and vulnerable cyclists. The prototype successfully integrates blind-spot monitoring, speed pacing, and automated safety lighting into a comprehensive interface.

Future development should focus on several key areas:
1. **Hardware Consolidation:** Migrating the codebase to a single, more powerful 32-bit microcontroller with RTOS (Real-Time Operating System) capabilities, housed in a professionally manufactured weatherproof enclosure.
2. **Sensor Upgrades:** Swapping the HC-SR04 ultrasonic sensors for short-range millimeter-wave radar to improve reliability in dense traffic, heavy rain, and varied lighting conditions.
3. **App Integration & Emergency Response:** Utilizing Bluetooth to sync ride telemetry to a companion app. This would allow clinicians to prescribe structured cycling rehabilitation plans, and enable the IMU's crash-latch state to automatically trigger an SMS alert to emergency contacts or an ambulance.
4. **Clinical Validation:** Conducting structured on-road user studies with elderly, glaucoma, and hearing-impaired demographics to fine-tune the haptic intensity and detection thresholds.

---

## References

[1] S. W. van Landingham et al., “Driving patterns in older adults with glaucoma,” BMC Ophthalmol., vol. 13, no. 4, 2013.  
[2] J. M. Wood, A. A. Black, K. Mallon, R. Thomas and C. Owsley, “Glaucoma and driving: on-road driving characteristics,” PLoS ONE, vol. 11, no. 7, e0158318, 2016.  
[3] L.-A. Leyland et al., “The effect of cycling on cognitive function and well-being in older adults,” PLoS ONE, vol. 14, no. 2, e0211779, 2019.  
[4] M. Kardan et al., “Cycling in older adults: a scoping review,” Front. Sports Act. Living, vol. 5, art. 1157503, 2023.  
[5] D. S. Alles, “Information transmission by phantom sensations,” IEEE Trans. Man-Machine Syst., vol. 11, no. 1, pp. 85–91, 1970.  
[6] J. Seiler et al., “Wearable vibrotactile interface using phantom tactile sensation for human–robot interaction,” in Proc. EuroHaptics, Springer LNCS 12272, 2020, pp. 380–388.  
[7] A. Matviienko et al., “Augmenting bicycles and helmets with multimodal warnings for children,” in Proc. MobileHCI ’18, Barcelona, Spain, 2018, art. 15, pp. 1–13.  
[8] M. Green, “‘How long does it take to stop?’ Methodological analysis of driver perception–brake times,” Transp. Hum. Factors, vol. 2, no. 3, pp. 195–216, 2000.  
[9] J. R. Treat et al., “Tri-level study of the causes of traffic accidents,” NHTSA Report DOT-HS-805-099, 1979.  
[10] Y. Gaffary and A. Lécuyer, “The use of haptic and tactile information in the car to improve driving safety: a review of current technologies,” Front. ICT, vol. 5, art. 5, 2018.  
[11] World Health Organization, “Global status report on road safety,” Geneva: WHO, 2023.
