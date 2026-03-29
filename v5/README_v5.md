# GEO-SENSE AFRICA v5.0 — THE RESILIENT MESH GATEWAY
## Multi-Hazard Early Warning System (MHEWS) — Professional Edition

**Geo-Sense v5.0** is the ultimate evolution of the system, transforming from a simple monitoring station into a **Disaster-Resilient Mesh Gateway**. Built for the final hackathon sprint, it features a high-density analytics suite and a self-healing network simulation.

---

### 🕸️ Mesh Technology Simulation
The system now features a fully functional **Mesh Topology** tab on the web console.
*   **Self-Healing:** If the Risk Index exceeds 8.5 (Extreme Danger), the system simulates an "Edge Node Failure" to demonstrate how the gateway tracks and reports network health during disasters.
*   **Routing Intel:** Displays RSSI (Signal Strength), Hop counts (Network Latency), and Node Load for every device in the mesh.

### 📊 Professional Analytics
The web dashboard has been overhauled with **Chart.js v4** to provide:
1.  **Risk Trend:** Calculated risk fusion over time.
2.  **Hydrological:** Real-time river level monitoring.
3.  **Environment (v5 Exclusive):** Room Temperature and Soil Moisture comparison analytics.
4.  **Remote Console:** Live event logs and manual hardware overrides.

### 🎮 Command & Control
*   **Manual Overrides:** Remotely trigger the Siren or Danger Flag for testing or manual evacuation alerts.
*   **Emergency Reset:** A single click returns the entire mesh to autonomous "Safe" mode.
*   **High-Density OLED:** On-board SSD1306 displays Mesh Node counts, Risk Index, and Hazard classification simultaneously.

### 🛠️ Directory Structure (Final)
*   `GeoSense_ESP32_Master_v5.ino`: The Mesh Gateway firmware.
*   `GeoSense_Arduino_Slave_v5.ino`: The Edge Sensor Node firmware.

---
*Created for the Hack Keele Hackathon — Sunday, March 29, 2026*
