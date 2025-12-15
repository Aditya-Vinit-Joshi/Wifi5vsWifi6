# Dense Wi-Fi Simulation (802.11ac/802.11ax) Using ns-3

This repository contains an ns-3 simulation script for evaluating dense Wi-Fi network performance using **802.11ac** or **802.11ax**, configurable channel widths, and both UDP and TCP traffic models.  
The simulation uses a single Access Point (AP) with multiple Stations (STAs), positioned randomly around the AP, and computes detailed performance metrics.

---

##  Features

- Supports **802.11ac (Wi-Fi 5)** and **802.11ax (Wi-Fi 6)**
- Channel widths: **20 / 40 / 80 / 160 MHz**
- Traffic models:
  - **UDP OnOff (CBR)** per STA  
  - **TCP BulkSend**
- Automatic parameter scaling for dense networks (RTS/CTS, packet size adjustment)
- Mobility with uniformly random STA placement
- FlowMonitor-based metric collection
- Optional PCAP tracing
- Computes:
  - Aggregate throughput
  - Average per-packet delay
  - Packet loss rate
  - Jain’s fairness index
  - Total Tx/Rx packets and success rate

---

##  File Description

| File | Description |
|------|-------------|
| `dense-wifi.cc` | Main simulation source code (the file in this repository) |

Place this file inside your **ns-3 `scratch/` directory** if you want to run it without modifying waf builds.

---

##  Requirements

- **ns-3.40 or newer**  
  (because the script uses the `ChannelSettings` API)
- Standard ns-3 modules:
  - `wifi`
  - `internet`
  - `applications`
  - `mobility`
  - `flow-monitor`

---

##  Building the Simulation

Inside the ns-3 root directory:

```bash
./waf configure --enable-examples --enable-tests
./waf build

## Example Runs
802.11ax, 20 STAs, 80 MHz, UDP:

./waf --run "scratch/dense-wifi --standard=ax --nStas=20 --channelWidth=80 --useUdp=true"

