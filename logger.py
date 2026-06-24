import csv
import os
import json
import paho.mqtt.client as mqtt

BROKER = "160.187.144.142"
PORT = 1883

TOPICS = [
    "pcr/24trkb/kelompok4/toilet/127",
    "pcr/24trkb/kelompok4/toilet/324",
    "pcr/24trkb/kelompok4/toilet/mesjid",
    "pcr/24trkb/kelompok4/toilet/kantin",
    "pcr/24trkb/kelompok4/toilet/273"
]

LOG_DIR = "logs"

os.makedirs(LOG_DIR, exist_ok=True)

def on_connect(client, userdata, flags, rc):
    print("Connected:", rc)

    for topic in TOPICS:
        client.subscribe(topic)

def on_message(client, userdata, msg):

    toilet = msg.topic.split("/")[-1]

    try:
        data = json.loads(msg.payload.decode())

        status = data["status"]
        timestamp = data["timestamp"]

    except Exception as e:
        print("Payload Error:", e)
        return

    tanggal = timestamp.split(" ")[0]
    waktu = timestamp.split(" ")[1]

    csv_file = f"{LOG_DIR}/{tanggal}.csv"

    file_baru = not os.path.exists(csv_file)

    with open(csv_file, "a", newline="") as f:

        writer = csv.writer(f)

        if file_baru:
            writer.writerow([
                "Tanggal",
                "Waktu",
                "Toilet",
                "Status"
            ])

        writer.writerow([
            tanggal,
            waktu,
            toilet,
            status
        ])

    print(
        f"[{timestamp}] "
        f"Toilet {toilet} -> {status}"
    )

client = mqtt.Client()

client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, PORT, 60)

client.loop_forever()
