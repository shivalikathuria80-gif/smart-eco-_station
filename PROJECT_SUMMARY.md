# Smart Eco Station: Project Summary

## 1. The idea in one line
A smart dustbin station that **unlocks with an RFID card**, **uses AI to recognise wet or dry waste**, **sorts it automatically into the right bin**, **gives the user reward points**, and **tells the authorities on a website when a bin is getting full**.

## 2. The parts and what each one does

| Part | Job |
|---|---|
| **RFID reader (RC522) + card** | Identifies the person using the station |
| **Main ESP32** | The "brain": controls everything and connects to the internet over Wi-Fi |
| **Solenoid lock** (through a relay) | Keeps the lid locked until a registered card is scanned |
| **ESP32-CAM** | Takes a photo of the waste |
| **Laptop running `app.py`** | Runs the Teachable Machine AI model that decides wet or dry |
| **Servo motor (MG996R)** | Tilts the flap to drop the waste into the wet or dry bin |
| **2 ultrasonic sensors (HC-SR04)** | One inside each bin, measuring how full it is |
| **16×2 LCD screen** | Shows messages and fill levels at the station |
| **Buzzer** | Beeps for success and errors |
| **Firebase Realtime Database** | Online storage for users, points, bin levels and logs |
| **Website** | Public page for checking points, plus the Authority Portal for staff |

## 3. Step by step: what happens when someone uses it

### Step 1: scanning the card
- The person taps their RFID card. The RC522 reads the card's unique number (UID), e.g. `A3 5F 8C 12`.
- The main ESP32 asks Firebase over Wi-Fi: "Is there a user saved under this card number?"
- **Not registered:** the LCD shows **ACCESS DENIED**, the buzzer beeps 3 times, and a "denied" entry goes to the website's log.
- **Registered:** the LCD shows **"Hi <name>, Lid unlocked!"**

### Step 2: unlocking the lid
- The ESP32 switches the relay on and the solenoid lock opens for **5 seconds**, then locks again.
- The person puts their waste on the flap inside.

### Step 3: recognising the waste
- The main ESP32 sends the message `CAPTURE` to the ESP32-CAM through wires (a serial connection).
- The camera turns on its flash, takes a photo, and sends it over Wi-Fi to the laptop.
- On the laptop, `app.py` runs the **Teachable Machine model**, which answers "Wet" or "Dry" and how confident it is.
- The camera passes the answer back to the main ESP32, e.g. `RESULT:WET:0.93` (93% sure it's wet).

### Step 4: sorting
- The ESP32 first checks that the target bin isn't completely full.
- The servo tilts the flap **left for wet** or **right for dry**, then returns to the centre.
- The LCD shows e.g. **"Detected: WET → Wet bin"**.

### Step 5: reward points
- The ESP32 adds **+10 points** to the user's account in Firebase.
- The LCD shows **"+10 points! Total: 160"**, and a "reward" entry goes to the website's log.

### Step 6: checking how full the bins are
- Each ultrasonic sensor sends a sound pulse down into its bin and times the echo. A shorter time means the waste is closer, so the bin is fuller.
- The distance becomes a percentage. For a 40 cm deep bin, 40 cm away is 0% and 5 cm away is 100%.
- The levels are:
  - shown on the LCD: `Wet:45% Dry:80%`
  - sent to Firebase after every item and every 15 seconds
- At **90%**, an **alert** goes to the Authority Portal saying the bin should be emptied.

## 4. The AI part (training)
- **Where:** the model is trained on Google's **Teachable Machine** website, with no coding.
- **How:**
  1. Take many photos of wet waste (food, peels) and dry waste (paper, plastic), ideally with the ESP32-CAM itself.
  2. Upload them into two classes, **Wet** and **Dry**, and click **Train**.
  3. Export the model as **Keras**, which gives `keras_model.h5` and `labels.txt`.
- **Running it:** put the two files next to `app.py` and run `python app.py`. It prints the address the camera should send photos to.

## 5. The database (Firebase)
The ESP32 and the website both read and write the **same Firebase Realtime Database**, so any change shows up everywhere instantly. It holds three main things:

| Location | What's stored | Who writes it |
|---|---|---|
| `/users/{cardUID}` | name and points | Website (registering cards), ESP32 (adding points) |
| `/bins/STATION-01` | wet % and dry % fill levels | ESP32 |
| `/logs` | events: scans, rewards, alerts, with the time | ESP32 and the website |

## 6. The website

### Public section
- Explains the project and how it works.
- **Points checker:** type your card number to see your name, points, rank and roughly how many items you've recycled.

### Authority Portal (PIN protected)
- **Register new cards:** enter a card UID and a name, and that card can then unlock the station.
- **Adjust points:** manually add or remove points for any card.
- **Bin monitoring:** live bars showing each bin's fill level. They turn orange at 75% and red at 95%, and an alert banner appears when a bin is full.
- **Live event log:** the latest 20 scans, rewards and alerts, as they happen.

## 7. The complete flow

```
RFID card → Main ESP32 → checks Firebase → registered? → solenoid unlocks
                                                        ↓
             ESP32-CAM takes photo → laptop AI (Teachable Machine) → WET / DRY
                                                        ↓
             Servo sorts waste → +10 points in Firebase → LCD shows total
                                                        ↓
     Ultrasonic sensors → fill % → LCD + Firebase → Website Authority Portal
```

## 8. Where the files are

| Folder | Contents |
|---|---|
| `Desktop\eco station\Nyrc` | The website (`smart eco station.html`) |
| `D:\NYRC 2627\EcoStation_Main` | Main ESP32 code (C++) |
| `D:\NYRC 2627\EcoStation_Cam_Server` | ESP32-CAM code for the Teachable Machine version |
| `D:\NYRC 2627\ClassifierServer` | Python AI server (`app.py`) |
| `D:\NYRC 2627\EcoStation_Cam` | Backup option: AI running on the camera itself (Edge Impulse) |

## 9. Why it's useful
- **Cleaner recycling:** wet and dry waste are separated automatically, so there's no human mistake.
- **Motivation:** reward points make people want to use it.
- **Faster emptying:** authorities get an alert and empty bins before they overflow.
- **Responsible use:** only registered users can open the lid.
