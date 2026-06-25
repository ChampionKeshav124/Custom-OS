import os
import json
import urllib.request
import base64
import time
import re

def generate(prompt, save_path=None):
    api_key = os.environ.get("GEMINI_API_KEY")
    if not api_key:
        # Try reading from config file
        api_path = os.path.expanduser("~/.config/gemini_key.txt")
        if os.path.exists(api_path):
            try:
                with open(api_path, "r") as f:
                    api_key = f.read().strip()
            except:
                pass
    
    if not api_key:
        print("Error: Gemini API key not found in environment or config file.")
        return None

    # Use Google Generative Language API Imagen 4 model (predict endpoint is v1beta)
    url = f"https://generativelanguage.googleapis.com/v1beta/models/imagen-4.0-generate-001:predict?key={api_key}"
    headers = {'Content-Type': 'application/json'}
    payload = {
        "instances": [{"prompt": prompt}],
        "parameters": {"sampleCount": 1, "aspectRatio": "1:1"}
    }
    
    try:
        req = urllib.request.Request(url, data=json.dumps(payload).encode(), headers=headers, method="POST")
        with urllib.request.urlopen(req, timeout=45) as response:
            res_data = json.loads(response.read().decode())
        
        if 'predictions' in res_data and len(res_data['predictions']) > 0:
            img_b64 = res_data['predictions'][0]['bytesBase64Encoded']
            img_data = base64.b64decode(img_b64)
            
            if not save_path:
                clean_prompt = re.sub(r'[^a-zA-Z0-9]', '_', prompt)[:30].strip('_')
                if not clean_prompt:
                    clean_prompt = "generated"
                filename = f"image_{clean_prompt}_{int(time.time())}.png"
                save_path = os.path.expanduser(f"~/Desktop/{filename}")
            
            os.makedirs(os.path.dirname(save_path), exist_ok=True)
            with open(save_path, "wb") as f:
                f.write(img_data)
            
            # Print the special image tag that the copilot script parses
            print(f"[IMAGE: {save_path}]")
            print(f"Success: Image generated and saved to {save_path}")
            return save_path
        else:
            print("Error: No image was returned. Response:", res_data)
            return None
    except Exception as e:
        print(f"Error during image generation: {str(e)}")
        return None
