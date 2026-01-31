# Lake-Watch — Particle.publish + Home Assistant Cloud Webhook

Lake-Watch is a **Particle Electron (cellular + LiPo)** firmware that monitors:

- **Crawlspace temperature** (to avoid frozen pipes)
- **Home power status** (on/off, inferred from charging / power source)
- **Electron External LiPo Battery level** (fuel gauge)

This repository is the **Particle.publish / webhook** version. It replaces the
MQTT version so that we need not rely on open ports at the server side.

---

## Design overview

### Old design (MQTT → HA directly)
The original Lake-Watch used MQTT from the Electron to a Home Assistant Mosquitto broker. That works well, but it typically implies:
- a reachable MQTT broker (often via port forwarding or VPN), and/or
- the remote LAN/Home Assistant staying up.

### New design (this repo): Particle Cloud → Nabu Casa Webhook → Home Assistant
This repo sends measurements to the Particle Cloud using **`Particle.publish()`**, then uses a **Particle Webhook** to POST the data to a **Home Assistant Cloud (Nabu Casa) webhook**.

```
Particle Electron (cellular)
   └─ Particle.publish("ha/cabin/lake_watch", JSON)
        └─ Particle Cloud Webhook (HTTPS POST)
             └─ Home Assistant Cloud Webhook trigger (no open ports)
                  ├─ Option A: update entities directly (recommended long-term)
                  └─ Option B: republish to local MQTT topics (compatibility)
```


References:
- `Particle.publish()` API: https://docs.particle.io/reference/device-os/api/publish/particle-publish-publish/
- Particle Webhooks: https://docs.particle.io/integrations/webhooks/
- Home Assistant Cloud Webhooks: https://www.nabucasa.com/config/webhooks/

---

## What gets published

### Particle event name
By default (recommended):
- **Event name:** `ha/cabin/lake_watch`

### Event data (JSON)
The firmware publishes a single compact JSON document, e.g.:

```json
{
  "fuelPercent": 83.4,
  "powerSource": 1,
  "powerIsOn": true,
  "crawlTempF": 36.12,
  "inDanger": false,
  "fw": "1.0.0",
  "reason": "periodic"
}
```

Notes:
- `reason` is useful for debugging (e.g., `periodic`, `power_change`, `temp_alert`).
- You can add more fields, but keep payloads reasonably small.

---

## Home Assistant setup (Nabu Casa)

You have two good ways to use the incoming webhook data:

### Option A (recommended): Webhook → entities directly
Use **trigger-based Template entities** or an automation that writes into Helpers (input_number, input_boolean, etc.).
This avoids MQTT entirely.

### Option B (compatibility): Webhook → republish to your existing MQTT topics
If you already have automations/sensors built around MQTT topics like `ha/cabin/#`, you can keep them by republishing locally from the webhook automation.

---

## Step-by-step: Create the Home Assistant Cloud webhook

1. In Home Assistant: **Settings → Automations & scenes → Create automation**
2. Add a **Webhook** trigger.
   - Give it a Webhook ID such as: `particle_lake_watch`
   - Save the automation.
3. In Home Assistant: **Settings → Home Assistant Cloud → Webhooks**
4. Copy the **public webhook URL** for `particle_lake_watch`.

Keep that URL handy — you will paste it into the Particle Webhook integration.

---

## Step-by-step: Create the Particle Webhook integration

In the Particle Console:

1. **Integrations → New Integration → Webhook**
2. **Event name**: `ha/cabin/lake_watch`
3. **Request type**: `POST`
4. **Request format**: `JSON`
5. **URL**: paste the Home Assistant Cloud webhook URL from the previous section
6. **JSON body** (important: triple braces to avoid escaping):

```json
{
  "device_id": "{{{PARTICLE_DEVICE_ID}}}",
  "event": "{{{PARTICLE_EVENT_NAME}}}",
  "published_at": "{{{PARTICLE_PUBLISHED_AT}}}",
  "data": {{{PARTICLE_EVENT_VALUE}}}
}
```

This causes Home Assistant to receive a JSON payload where the published JSON becomes `trigger.json.data`.

---

## Optional: Republish into MQTT topics (`ha/cabin/#`)

If you want to keep your existing MQTT-based sensors/automations, add an action in your webhook-triggered automation:

```yaml
alias: Lake-Watch ingest (webhook)
trigger:
  - platform: webhook
    webhook_id: particle_lake_watch

variables:
  d: "{{ trigger.json.data }}"
  fuel: "{{ d.fuelPercent | float }}"
  temp: "{{ d.crawlTempF | float }}"
  on: "{{ d.powerIsOn | bool }}"
  src: "{{ d.powerSource | int }}"

action:
  - service: mqtt.publish
    data:
      topic: "ha/cabin/powerLevel"
      payload: "{{ fuel }}"
      retain: true

  - service: mqtt.publish
    data:
      topic: "ha/cabin/crawlTemp"
      payload: "{{ temp }}"
      retain: true

  - choose:
      - conditions: "{{ on }}"
        sequence:
          - service: mqtt.publish
            data: { topic: "ha/cabin/powerOK", payload: "1" }
      - conditions: "{{ not on }}"
        sequence:
          - service: mqtt.publish
            data: { topic: "ha/cabin/powerOUT", payload: "1" }
```

You can extend this to publish your previous warning topics (`crawlWarn`, `crawlFreeze`, etc.) based on `temp`.

---

## Firmware build / flash

You can develop and flash this firmware using:
- Particle Workbench (VS Code extension), or
- Particle Web IDE, or
- Particle CLI

This repo currently focuses on the **design + HA/Particle integration**; see `src/` for the firmware entry point and configuration constants.

---

## Security notes

- Treat the Home Assistant webhook URL (and webhook_id) as a **secret**.
- Use Particle **PRIVATE** events.
- If you republish to MQTT locally, keep your Mosquitto broker restricted to your LAN/Tailscale as you do today.

---

## License
(keep / update as appropriate)
