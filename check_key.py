import sys
sys.path.append('/home/aether/copilot')
try:
    from copilot import CopilotApp
    app = CopilotApp()
    print("LOADED API KEY:", app.api_key)
except Exception as e:
    import traceback
    traceback.print_exc()
