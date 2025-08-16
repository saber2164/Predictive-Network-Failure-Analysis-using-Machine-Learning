# Install required packages: pip install Flask pandas scikit-learn joblib
from flask import Flask, request, jsonify
from flask_cors import CORS
import joblib
import pandas as pd
import numpy as np

# Initialize the Flask application
app = Flask(__name__)
# Enable Cross-Origin Resource Sharing (CORS)
CORS(app)

# --- Load the Trained Model ---
try:
    model = joblib.load("link_failure_classifier.joblib")
    print("✅ Model loaded successfully!")
except FileNotFoundError:
    model = None
    print("🚨 Error: 'link_failure_classifier.joblib' not found.")
except Exception as e:
    model = None
    print(f"🚨 An error occurred while loading the model: {e}")

# --- (Simulated) Model Performance Stats ---
# In a real-world scenario, your training script (predict4.py) would save these
# stats to a JSON file, and this server would read that file.
# For this example, we are hardcoding the values for simplicity.
MODEL_STATS = {
    "cross_validation_accuracy": 0.925,
    "metrics": {
        "failure": {"precision": 0.88, "recall": 0.91, "f1-score": 0.89},
        "ok": {"precision": 0.94, "recall": 0.92, "f1-score": 0.93}
    },
    "model_name": "Random Forest",
    "data_split": {"training": 80, "testing": 20}
}

# --- API Endpoint for Model Statistics ---
@app.route('/stats', methods=['GET'])
def get_stats():
    if model is None:
        return jsonify({'error': 'Model is not available.'}), 500
    return jsonify(MODEL_STATS)

# --- API Endpoint for Predictions ---
@app.route('/predict', methods=['POST'])
def predict():
    if model is None:
        return jsonify({'error': 'Model is not available.'}), 500

    try:
        data = request.get_json()
        throughput = float(data['throughput'])
        delay = float(data['delay'])

        input_df = pd.DataFrame([[throughput, delay]], columns=['Throughput(Mbps)', 'Delay(s)'])
        
        prediction_raw = model.predict(input_df)[0]
        prediction_proba = model.predict_proba(input_df)[0]

        if prediction_raw == 1:
            prediction_label = 'OK'
            confidence = prediction_proba[1]
        else:
            prediction_label = 'FAILURE'
            confidence = prediction_proba[0]
        
        return jsonify({
            'prediction': prediction_label,
            'confidence': confidence,
            'input': {'throughput': throughput, 'delay': delay} # Return input for plotting
        })

    except Exception as e:
        return jsonify({'error': 'An error occurred during prediction.'}), 400

# --- Run the Flask App ---
if __name__ == '__main__':
    app.run(port=5000, debug=True)
