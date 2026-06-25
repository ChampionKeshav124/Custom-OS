import urllib.request
import json
import ssl

ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE

# Read key from api_key.txt
try:
    with open("c:/Users/defaultuser0/Desktop/Antigravity/AetherOS-64/api_key.txt", "r") as f:
        key = f.read().strip()
except Exception as e:
    key = "AIzaSyA4g4lU8SxBaKWGinlXpFCwBS3Rw8YG5Bc"

print(f"Checking models with key: {key[:8]}...")
url = f"https://generativelanguage.googleapis.com/v1beta/models?key={key}"

try:
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    with urllib.request.urlopen(req, context=ctx, timeout=10) as r:
        res = json.loads(r.read().decode())
        print("Available models:")
        for m in res.get('models', []):
            name = m['name']
            print(f" - {name}")
except Exception as e:
    print("Error:", e)
