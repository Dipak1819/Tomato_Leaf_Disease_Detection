from fastapi import FastAPI, File, UploadFile
import numpy as np
import io
from PIL import Image
from tensorflow import keras

app = FastAPI()

model = keras.models.load_model('app/tomato_disease.h5')

# Simplified class mapping
HEALTHY_LABELS = {
    'Tomato_healthy', 
    'Tomato__Tomato_YellowLeaf__Curl_Virus',
    'Tomato__Tomato_mosaic_virus'
}

def read_image(image):
    img = Image.open(io.BytesIO(image))
    img = img.convert('RGB').resize((256, 256))
    return np.array(img) / 255.0  # Normalize pixel values

@app.post('/detect')
async def detect(file: UploadFile = File(...)):
    # Process image
    image = read_image(await file.read())
    img_batch = np.expand_dims(image, axis=0)
    
    # Get prediction
    prediction = model.predict(img_batch)
    confidence = float(np.max(prediction))
    class_idx = np.argmax(prediction)
    
    # Original class labels (ensure these match your model's training order!)
    original_classes = [
        'Tomato_Bacterial_spot', 'Tomato_Early_blight', 'Tomato_Late_blight',
        'Tomato_Leaf_Mold', 'Tomato_Septoria_leaf_spot',
        'Tomato_Spider_mites_Two_spotted_spider_mite', 'Tomato__Target_Spot',
        'Tomato__Tomato_YellowLeaf__Curl_Virus', 'Tomato__Tomato_mosaic_virus',
        'Tomato_healthy'
    ]
    
    # Categorize prediction
    detected_class = original_classes[class_idx]
    category = "healthy" if detected_class in HEALTHY_LABELS else "unhealthy"
    
    # Generic prevention advice
    prevention = {
        "healthy": [
            "Maintain current care practices",
            "Monitor for early signs of disease"
        ],
        "unhealthy": [
            "Isolate affected plants",
            "Apply appropriate fungicides/pesticides",
            "Improve air circulation"
        ]
    }

    return {
        "status": category,
        "confidence": round(confidence, 4),
        "recommendations": prevention[category]
    }
