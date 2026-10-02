"""
Smart AI Eco-Station v2 — Teachable Machine inference server

Runs on your computer (or a Raspberry Pi) on the same WiFi network as the
ESP32-CAM. The CAM posts a JPEG frame here; this loads your exported
Google Teachable Machine model and returns the predicted class.

--- How to get the model files ---
In Teachable Machine (teachablemachine.withgoogle.com):
  Export Model -> Tensorflow -> "Keras" -> Download my model

That gives you a .zip containing:
  keras_model.h5
  labels.txt        (one class name per line, e.g. "0 Dry" / "1 Wet")

Unzip both files into this same folder before running this script.

--- Setup ---
  python -m venv venv
  source venv/bin/activate        (Windows: venv\\Scripts\\activate)
  pip install -r requirements.txt
  python app.py

The server prints the local IP + port to use in the ESP32-CAM sketch.

--- Label mapping ---
Teachable Machine lets you name your classes anything. This server only
needs to know which of your class names counts as "DRY" and which counts
as "WET" — edit DRY_LABELS / WET_LABELS below to match what you typed in
Teachable Machine (matching is case-insensitive, and checks if your label
CONTAINS these words, so "Dry Recyclables" still matches "dry").
"""

import io
import numpy as np
from flask import Flask, request, jsonify
from PIL import Image
from tensorflow.keras.models import load_model
import socket

MODEL_PATH = "keras_model.h5"
LABELS_PATH = "labels.txt"
IMAGE_SIZE = 224  # Teachable Machine's default input size

# Edit these to match the class names you actually typed into Teachable Machine.
DRY_LABELS = ["dry", "recyclable", "plastic", "paper", "metal"]
WET_LABELS = ["wet", "organic", "food", "compost"]
# Optional third class trained on photos of the EMPTY flap. Stops people
# scanning a card with nothing on the flap and still getting points.
EMPTY_LABELS = ["empty", "nothing", "background", "none"]

app = Flask(__name__)
model = load_model(MODEL_PATH, compile=False)

with open(LABELS_PATH, "r") as f:
    # labels.txt lines look like "0 Dry" — strip the leading index.
    class_names = [line.strip().split(" ", 1)[-1] for line in f if line.strip()]

# Warm-up: the first predict() is slow (several seconds) and would make the
# first real item time out on the ESP32, so run one dummy prediction now.
model.predict(np.zeros((1, IMAGE_SIZE, IMAGE_SIZE, 3), dtype=np.float32), verbose=0)


def preprocess(image_bytes):
    img = Image.open(io.BytesIO(image_bytes)).convert("RGB")
    img = img.resize((IMAGE_SIZE, IMAGE_SIZE))
    arr = np.asarray(img, dtype=np.float32)
    arr = (arr / 127.5) - 1.0  # Teachable Machine's standard MobileNet preprocessing
    return np.expand_dims(arr, axis=0)


def map_to_stream(label):
    lower = label.lower()
    if any(word in lower for word in EMPTY_LABELS):
        return "NONE"
    if any(word in lower for word in DRY_LABELS):
        return "DRY"
    if any(word in lower for word in WET_LABELS):
        return "WET"
    return None  # unrecognized label — caller decides the fallback


@app.route("/classify", methods=["POST"])
def classify():
    image_bytes = request.get_data()
    if not image_bytes:
        return jsonify({"error": "No image data received"}), 400

    try:
        input_tensor = preprocess(image_bytes)
    except Exception as e:
        return jsonify({"error": f"Could not read image: {e}"}), 400

    predictions = model.predict(input_tensor, verbose=0)[0]
    top_index = int(np.argmax(predictions))
    top_label = class_names[top_index]
    confidence = float(predictions[top_index])

    stream = map_to_stream(top_label)
    if stream is None:
        # Model returned a class name that isn't in DRY_LABELS/WET_LABELS —
        # tell the caller plainly rather than silently guessing.
        return jsonify({
            "label": top_label,
            "confidence": confidence,
            "stream": None,
            "warning": "Label not mapped to DRY/WET — edit DRY_LABELS/WET_LABELS in app.py"
        }), 200

    return jsonify({"label": top_label, "confidence": confidence, "stream": stream})


@app.route("/health", methods=["GET"])
def health():
    return jsonify({"status": "ok", "classes": class_names})


def print_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
    except Exception:
        ip = "127.0.0.1"
    finally:
        s.close()
    print(f"\nServer starting. Point the ESP32-CAM sketch at:")
    print(f"  http://{ip}:5000/classify\n")


if __name__ == "__main__":
    print_local_ip()
    app.run(host="0.0.0.0", port=5000)
