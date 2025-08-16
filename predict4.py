import pandas as pd
import glob
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split, cross_val_score
from sklearn.metrics import classification_report, confusion_matrix, ConfusionMatrixDisplay
import matplotlib.pyplot as plt
import joblib

# ✅ Load all CSVs
file_paths = glob.glob("tcp_metrics_2025-7-3_*.csv")  # Adjust if needed
df = pd.concat([pd.read_csv(f) for f in file_paths], ignore_index=True)

# ✅ Clean + encode
df = df.dropna()
df['LinkStatus'] = df['LinkStatus'].map({'OK': 1, 'FAILURE': 0})

# Optional: Show class balance
print(df['LinkStatus'].value_counts())

# ✅ Features and target
X = df[['Throughput(Mbps)', 'Delay(s)']]
y = df['LinkStatus']

# ✅ Train/test split
X_train, X_test, y_train, y_test = train_test_split(
    X, y, stratify=y, test_size=0.2, random_state=42
)

# ✅ Random Forest model with regularization
model = RandomForestClassifier(
    n_estimators=100,
    max_depth=5,
    min_samples_split=5,
    min_samples_leaf=5,
    class_weight='balanced',
    random_state=42
)
model.fit(X_train, y_train)
y_pred = model.predict(X_test)

# ✅ Cross-validation
scores = cross_val_score(model, X, y, cv=5)
print(f"\n📈 Cross-Validation Accuracy: {scores.mean():.3f} ± {scores.std():.3f}")

# ✅ Classification report
print("\n📊 Classification Report:\n", classification_report(y_test, y_pred))

# ✅ Confusion Matrix
cm = confusion_matrix(y_test, y_pred)
disp = ConfusionMatrixDisplay(confusion_matrix=cm, display_labels=["FAILURE", "OK"])
disp.plot(cmap=plt.cm.Blues)
plt.title("Confusion Matrix")
plt.grid(False)
plt.show()

# ✅ Save model
joblib.dump(model, "link_failure_classifier.joblib")
print("✅ Model saved to 'link_failure_classifier.joblib'")
