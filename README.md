# NET-OPT — Campus Wi-Fi Bottleneck Simulation
## PDC Spring 2026 | Section B | Serial #37, 38, 39
### Tool: OMNeT++ | Domain: Campus IT

---

## 📌 Project Overview

**NET-OPT** simulates a university campus Wi-Fi network during registration peaks
to identify bottleneck access points (APs) and distribution switches, then proposes
and demonstrates infrastructure optimizations.

**Key Concept:** During registration week, hundreds of students simultaneously
connect to campus Wi-Fi to access the registration portal. This creates congestion
at certain APs (especially in CS Block and Admin Block), causing high latency,
packet drops, and registration failures.

---

## 📁 Project Structure

```
NET-OPT/
├── src/
│   ├── package.ned          ← Package declaration (required)
│   ├── NetOpt.ned           ← MAIN network topology (NED language)
│   ├── AccessPoint.h/.cc    ← Wi-Fi AP logic + bottleneck detection
│   ├── StudentDevice.h/.cc  ← Student laptop/phone behavior
│   ├── RegistrationServer.h/.cc ← University registration server
│   └── Infrastructure.h/.cc ← Switch, Router, Monitor modules
├── simulations/
│   └── omnetpp.ini          ← ALL simulation scenarios (4 configs)
├── results/                 ← Auto-generated simulation results
└── README.md                ← This file
```

---

## 🖥️ System Requirements

- **OMNeT++ 6.x** (recommended: 6.0.3 or later)
  - Download: https://omnetpp.org/download/
- **OS:** Windows 10/11, Ubuntu 20.04+, or macOS
- **RAM:** 4 GB minimum
- **IDE:** OMNeT++ IDE (Eclipse-based, included in OMNeT++ installer)

---

## 🚀 Step-by-Step: How to Run in OMNeT++

### STEP 1 — Install OMNeT++

1. Go to https://omnetpp.org/download/
2. Download **OMNeT++ 6.x** for your OS
3. Extract and follow the install guide (usually just `./configure && make` on Linux)
4. Launch the **OMNeT++ IDE** (run `omnetpp` in terminal or click the shortcut)

---

### STEP 2 — Import the Project

1. In OMNeT++ IDE, go to: **File → Import → General → Existing Projects into Workspace**
2. Click **Browse** and select the `NET-OPT/` folder
3. Click **Finish**
4. The project `NET-OPT` will appear in the **Project Explorer** panel

---

### STEP 3 — Build the Project

1. Right-click on `NET-OPT` in Project Explorer
2. Click **Build Project** (or press `Ctrl+B`)
3. Watch the **Console** tab — it should say `Build Finished` with 0 errors
4. A `NET-OPT` executable will be created

> ⚠️ If you see errors about includes, go to:
> Project → Properties → OMNeT++ → NED Source Folders → Add `src/`

---

### STEP 4 — Run the Simulation (MAIN DEMO)

#### Option A: Using the IDE (Recommended for Professor Demo)

1. Right-click on `simulations/omnetpp.ini` → **Run As → OMNeT++ Simulation**
2. A dialog will appear — select the config: **`RegistrationPeak`**
3. Click **OK** → The **Qtenv** (graphical) simulation window opens
4. Press the **Play** button (▶) or press `F5`

#### Option B: Command Line (faster)

```bash
cd NET-OPT/
./NET-OPT -u Qtenv simulations/omnetpp.ini -c RegistrationPeak
```

---

## 🎬 Four Simulation Scenarios (Show ALL to Professor)

| Config Name       | Description                         | Expected Result          |
|-------------------|-------------------------------------|--------------------------|
| `NormalDay`       | Low traffic, normal campus day      | All green, no bottlenecks|
| `RegistrationPeak`| **MAIN DEMO** — registration opens  | Bottlenecks in CS Block  |
| `OptimizedNetwork`| Upgraded APs and server             | Bottlenecks resolved     |
| `StressTest`      | Absolute worst case                 | Network collapse visible |

### How to switch configs:
In the run dialog, change the **Configuration name** dropdown.

---

## 📊 What to Watch During the Simulation

### In Qtenv (graphical window):

1. **Red bubbles on APs** = `BOTTLENECK!` — AP is saturated
2. **Green bubbles on students** = `Registered!` — successful registration
3. **Red bubbles on students** = `FAILED!` — student couldn't register

### Key Statistics to show Professor:

After simulation finishes, click **Results** tab or run:
```bash
cd results/
# Open .sca (scalar) and .vec (vector) files in the IDE's Analysis tool
```

**What to record and show:**
- `apLoad` → Wi-Fi load per AP over time (should spike at t=60-90s)
- `clientCount` → How many students per AP
- `isBottleneck` → 0/1 flag, shows when bottleneck occurs
- `networkHealth` → Global metric (drops from 95% to 25% at peak)
- `activeSessions` → Server sessions
- `responseTime` → Registration response time in ms

---

## 🔍 Key Results to Explain to Professor

### Registration Peak Scenario:

```
t=0-30s    → Students start arriving, network healthy (95%)
t=30-60s   → Registration opens, load rising (60%)
t=60-90s   → PEAK: CS Block APs overwhelmed, BOTTLENECK alerts (25%)
t=90-120s  → Demand slightly reduces, partial recovery (40%)
t=120-300s → Post-peak, network stabilizes (80%)
```

### Root Causes Identified:
1. **CS Block AP saturation** — 80 students, only 5 APs (16 students/AP average)
2. **Registration Server queue overflow** — 200-session limit exceeded
3. **Distribution switch (distSwitch[1])** — 1Gbps uplink is the bottleneck

### Solutions (shown in OptimizedNetwork config):
1. Add 3 more APs in CS Block (5 → 8)
2. Upgrade AP firmware to support 80 clients (was 50)
3. Double registration server capacity (200 → 400 sessions)
4. Stagger registration by department (not simulated but recommended)

---

## 📋 Assessment Checklist (Per Your Rubric)

| Criteria             | What We Demonstrate                            | Score Target |
|----------------------|------------------------------------------------|--------------|
| Core Logic (CLO 3)   | Bottleneck detection, queue modeling, retries  | Excellent    |
| Tool Integration(CLO4)| Full OMNeT++ with NED, signals, statistics     | Excellent    |
| Fault Tolerance(CLO5)| AP drops → student retries with backoff        | Excellent    |
| Demo & Q&A           | 4 scenarios, live graphs, final report         | Excellent    |

---

## ❓ Expected Professor Questions & Answers

**Q: Why OMNeT++?**
A: OMNeT++ is the industry-standard discrete-event network simulator. It allows
   us to model real network topologies, test thousands of nodes, and generate
   real statistics — impossible to do with real hardware at this scale.

**Q: What is a bottleneck in this context?**
A: An AP is a bottleneck when its client count exceeds 90% of capacity OR when
   its load exceeds 80% of bandwidth. Our monitor detects and flags this in
   real-time using OMNeT++ signals.

**Q: How is this related to Parallel & Distributed Computing?**
A: The network itself IS a distributed system. APs independently manage clients,
   packets are processed in parallel, and the registration server is a shared
   distributed resource — exactly the concepts from PDC.

**Q: How does the student retry mechanism work?**
A: We implement exponential backoff: 2s, 4s, 8s, 16s, 32s between retries —
   same algorithm used in real Wi-Fi (802.11 CSMA/CA backoff).

---

## 👥 Team Roles (Serial #37, 38, 39)

| Serial | Suggested Role              | Main Files                    |
|--------|-----------------------------|-------------------------------|
| 37     | Network Topology + AP Logic | NetOpt.ned, AccessPoint.cc    |
| 38     | Student Behavior + Server   | StudentDevice.cc, RegServer.cc|
| 39     | Simulation Config + Demo    | omnetpp.ini, Results Analysis |

---

## 📞 Quick Troubleshooting

| Problem                         | Solution                                      |
|---------------------------------|-----------------------------------------------|
| "Module not found" error        | Right-click project → Properties → NED paths  |
| Build fails (missing omnetpp.h) | Ensure OMNeT++ is properly installed          |
| Simulation runs but no graphics | Use `-u Qtenv` flag, not `-u Cmdenv`          |
| No results files generated      | Check `result-dir = results/` in omnetpp.ini  |
| APs show no bottleneck          | Switch to `RegistrationPeak` or `StressTest`  |

---

*NET-OPT | Parallel & Distributed Computing | Spring 2026 | Section B*
