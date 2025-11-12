from flask import Flask, render_template_string, request, redirect
import random  # Para simular voltajes

app = Flask(__name__)

# Variables simuladas (como en ESP32)
armed = True
intrusion = False
solar_volt = round(random.uniform(3.5, 5.0), 2)  # Simula solar
auto_volt = round(random.uniform(11.5, 14.5), 2)  # Simula auto
dynamic_chat_id = ""  # Dinámico

# HTML de la interfaz (igual al ESP32)
HTML_TEMPLATE = '''
<!DOCTYPE html>
<html><head><meta name="viewport" content="width=device-width, initial-scale=1"><style>
body { font-family: Arial; background: #222; color: #fff; text-align: center; padding: 20px; }
h1 { color: #0f0; }
button { background: #0f0; color: black; border: none; padding: 15px; font-size: 18px; margin: 10px; border-radius: 10px; width: 80%; }
button:disabled { background: #666; }
p { font-size: 16px; }
a { color: #0f0; text-decoration: none; }
</style></head><body>
<h1>🌿 FloraLock App (Demo Server)</h1>
<p>Voltaje Solar: {{ solar_volt }} V</p>
<p>Voltaje Auto: {{ auto_volt }} V</p>
<p>Intrusión: {{ "SÍ ⚠️" if intrusion else "No ✅" }}</p>
<p>Modo: {{ "ARMADO 🔒" if armed else "DESARMADO 🔓" }}</p>
<p>Chat ID Actual: {{ dynamic_chat_id if dynamic_chat_id else "No registrado" }}</p>
<a href="/setchat?chatid=123456789"><button>Registrar Chat ID (ejemplo - cambia por el tuyo)</button></a>
<button onclick="arm()">ARMAR Alarma</button>
<button onclick="disarm()">DESARMAR Alarma</button>
<button onclick="simulate_intrusion()">Simular Intrusión</button>
<script>
function arm() { fetch('/arm').then(() => location.reload()); }
function disarm() { fetch('/disarm').then(() => location.reload()); }
function simulate_intrusion() { fetch('/intrusion').then(() => alert('¡Alerta! Sirena simulada + Telegram (si registrado)')); location.reload(); }
setInterval(() => location.reload(), 5000);  // Refresh auto cada 5s
</script>
</body></html>
'''

@app.route('/')
def index():
    global armed, intrusion, solar_volt, auto_volt
    solar_volt = round(random.uniform(3.5, 5.0), 2)  # Actualiza simulado
    auto_volt = round(random.uniform(11.5, 14.5), 2)
    return render_template_string(HTML_TEMPLATE, armed=armed, intrusion=intrusion, solar_volt=solar_volt, auto_volt=auto_volt, dynamic_chat_id=dynamic_chat_id)

@app.route('/arm')
def arm():
    global armed
    armed = True
    return "Alarma ARMADA - Recargando..."

@app.route('/disarm')
def disarm():
    global armed
    armed = False
    return "Alarma DESARMADA - Recargando..."

@app.route('/setchat')
def setchat():
    global dynamic_chat_id
    chatid = request.args.get('chatid')
    if chatid:
        dynamic_chat_id = chatid
        return f"Chat ID registrado: {chatid} - ¡Ahora las alertas van a tu Telegram!"
    return "Error: Envía ?chatid=TU_NUMERO"

@app.route('/intrusion')
def simulate_intrusion():
    global intrusion
    intrusion = True
    # Aquí simularías Telegram (usa requests para HTTP si quieres real)
    print("¡INTRUSIÓN SIMULADA! Enviar a Telegram: Alerta con voltaje", solar_volt)
    return "Intrusión detectada - Sirena ON (simulada)"

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=80, debug=True)  # Corre en puerto 80 (o 5000 si 80 falla)