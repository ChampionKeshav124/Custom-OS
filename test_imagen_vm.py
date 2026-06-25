import urllib.request
import json
import urllib.error

key = "AIzaSyA4g4lU8SxBaKWGinlXpFCwBS3Rw8YG5Bc"
url = f"https://generativelanguage.googleapis.com/v1beta/models/imagen-4.0-generate-001:predict?key={key}"
headers = {'Content-Type': 'application/json'}
payload = {
    "instances": [{"prompt": "a circle"}],
    "parameters": {"sampleCount": 1}
}

try:
    req = urllib.request.Request(url, data=json.dumps(payload).encode(), headers=headers, method="POST")
    with urllib.request.urlopen(req, timeout=10) as response:
        print("Success!")
except urllib.error.HTTPError as e:
    print("HTTP Error:", e.code, e.reason)
    try:
        print("Body:", e.read().decode())
    except Exception as ex:
        print("Error reading body:", ex)
except Exception as e:
    print("General Error:", e)
