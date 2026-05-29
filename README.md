# Restoring Spatial Awareness for Elderly Cyclists: A Haptic Interface Prototype

## Introduction

Cycling provides measurable cognitive and physical benefits for older adults, yet it remains inherently dangerous in complex traffic environments. Many older cyclists gradually lose the two traffic senses they rely on most: peripheral vision (often resulting from glaucoma) and sound localization (due to age-related hearing loss). This sensory decline makes elderly cyclists incredibly vulnerable to unseen lateral hazards. Furthermore, driving and mobility studies demonstrate that around 1 in 4 people aged 60-80 with glaucoma stop driving, often disengaging from active transportation entirely. The recent surge in e-bike popularity compounds this issue; their heavier weight and faster speeds have led to a massive spike in severe crashes among older populations.

Current commercial safety solutions have notable gaps. Traditional bike radars provide a binary "behind-you" warning without conveying specific left/right spatial information. More importantly, attempting to solve this by adding another screen to the handlebars introduces a dangerous visual distraction, increasing driver perception-brake times. Mirror glances cost valuable forward attention, and auditory beeps are easily lost in loud city traffic.

Our project addresses these limitations by utilizing haptic technology. A short vibration on the left or right handlebar grip utilizes the tactile channel—which is almost always free while cycling—to naturally convey threat direction without visual or auditory load. This prototype acts as a proactive hardware solution that restores spatial awareness instantly, keeping the rider's eyes firmly on the road forward.

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

---

## Methods

### Step 1: The Concurrency Engine (Arduino Uno)
The crown jewel of the Arduino Uno's implementation is the seamless, concurrent execution of distance sensing and high-speed wheel tracking. Standard Arduino tutorials for ultrasonic sensors rely on the `pulseIn()` function, which halts the entire processor while waiting for an echo to return. If we used `pulseIn()`, the entire system would freeze for up to 18 milliseconds per ping, completely destroying the timing of the haptic feedback patterns and causing the system to miss wheel rotations.

To solve this, we engineered a highly responsive, non-blocking architecture:
* **Asynchronous State Machine:** The two HC-SR04 sensors are managed by a microsecond-level state machine (`US_IDLE`, `US_WAIT_RISE`, `US_WAIT_FALL`, `US_GAP`). The Uno rapidly checks pin states against `micros()` timestamps, advancing the state only when a hardware pin changes, leaving the main loop running at breakneck speed.
* **Hardware Interrupts (Hall Effect):** While the state machine juggles the ultrasonic pings, the Hall effect speed sensor is wired directly to `D2 (INT0)`. This utilizes a dedicated hardware interrupt. Whenever the wheel magnet passes the sensor, the hardware physically interrupts the CPU, freezing the main loop for just a few clock cycles to increment the wheel tick counter in the background, and then instantly resumes the main loop. 
* **The Result:** The Uno flawlessly manages an alternating 40 ms ping-pong between the two ultrasonic sensors, instantly catches a wheel spinning at 40+ km/h, and fires complex I2C haptic patterns to the handlebars—all simultaneously, without a single millisecond of blocking delay.

### Step 2: Resolving Haptic Hardware Constraints
A significant hardware challenge emerged during integration: both left and right TacHammer motors would buzz simultaneously regardless of the threat's direction. The TCA9548A I2C multiplexer only switches the control registers of the DRV2605 drivers, not the actual PWM drive signal.
* **The PWM Split:** We electrically separated the PWM lines. Using the ATmega328P's Timer2, we configured two independent PWM outputs: Pin 11 (`OC2A`) for the left motor and Pin 3 (`OC2B`) for the right motor.
* **Signed PWM Scheme:** The haptic library is configured for a signed PWM scheme where a ~50% duty cycle (`PWM_NEUTRAL = 127`) equates to zero physical drive. When a left-side threat is detected, the right motor's PWM is parked exactly at neutral, ensuring complete mechanical isolation between the left and right alerts.

### Step 3: Adaptive IMU Algorithm (Arduino Micro)
Detecting a braking bicycle is notoriously difficult because bicycles lean into turns and tilt up and down hills. A static deceleration threshold would constantly trigger false brake lights when riding uphill or fail to trigger when riding downhill. To solve this, the Arduino Micro runs a sophisticated, self-correcting adaptive algorithm on the MPU-6050 data:

* **The Dynamic Baseline:** Instead of assuming "0" is flat, the code uses a slow low-pass filter (`forwardBaseline += 0.01 * (forward - forwardBaseline)`) running at 50 Hz. This creates a floating baseline that represents the current combination of gravity (hill incline) and average cruising speed, acting as a ~2-second time constant.
* **The "Quiet Band" Lockout:** The most clever part of this algorithm is the `BASELINE_QUIET_BAND`. If the rider brakes hard or accelerates quickly, updating the baseline would cause the algorithm to "absorb" the braking event and turn the light off prematurely. The algorithm only updates the baseline when the `brakeSignal` is exceptionally quiet (less than half the trigger threshold). When the rider actually brakes, the baseline locks into place, ensuring a rock-solid reference point for the duration of the stop.
* **Debounced Trigger:** If the instantaneous forward acceleration drops below this locked baseline by `0.6 m/s²` for three consecutive samples, the LED jumps to full brightness.
* **Omnidirectional Crash Detection:** The crash logic entirely overrides the brake logic. It calculates the raw vector magnitudes (`sqrt(x² + y² + z²)`) for both acceleration and angular velocity. If the bike experiences a shock greater than `30.0 m/s²` or a violent spin over `6.0 rad/s` on any axis, the system immediately latches into an emergency state, flashing the LED to alert surrounding drivers.

---

## Discussion

The prototype effectively translates spatial and telemetry data into tactile feedback, directly addressing the sensory deficits common in older cyclists. By utilizing independent left/right haptic channels, the system successfully eliminates the need for visual dashboard checks, directly mitigating the cognitive load associated with mirror checking and screen reading. 

The software architecture proved highly successful. Implementing a non-blocking state machine alongside a hardware interrupt on the Uno completely resolved the stuttering issues typically seen in Arduino sensor arrays, resulting in instant, fluid haptic responses. Furthermore, the Micro's adaptive IMU algorithm elegantly solves the "hill problem," proving that a simple low-pass filter with a quiet-band lockout can produce an incredibly reliable, auto-calibrating brake light without complex trigonometry.

However, the current build has limitations. Distributing the architecture across two microcontrollers (Uno and Micro) made the wiring harness complex and the physical footprint bulky. Combining these functions requires a microcontroller with multiple hardware timers and an RTOS (Real-Time Operating System) to manage the blocking I2C calls alongside high-frequency PWM generation.

---

## Conclusion and Future Work

This project demonstrates a highly viable proof-of-concept: haptic feedback can seamlessly replace visual and auditory dashboards to restore spatial awareness for elderly and vulnerable cyclists. The prototype successfully integrates blind-spot monitoring, speed pacing, and automated safety lighting into a completely screen-free interface.

Future development should focus on several key areas:
1.  **Hardware Consolidation:** Migrating the codebase to a single, powerful microcontroller (such as an ESP32) housed in a weatherproof, stem-mounted enclosure with an integrated lithium-ion battery management system.
2.  **Sensor Upgrades:** Swapping the HC-SR04 ultrasonic sensors for short-range millimeter-wave radar to improve reliability in dense traffic, heavy rain, and varied lighting conditions.
3.  **App Integration & Emergency Response:** Utilizing Bluetooth to sync ride telemetry to a companion app. This would allow clinicians to prescribe structured cycling rehabilitation plans, and enable the IMU's crash-latch state to automatically trigger an SMS alert to emergency contacts.
4.  **Clinical Validation:** Conducting structured on-road user studies with elderly, glaucoma, and hearing-impaired demographics to fine-tune the haptic intensity and detection thresholds.

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
