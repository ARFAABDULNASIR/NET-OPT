# NET-OPT — Campus Wi-Fi Bottleneck Simulation
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

## 🎬 Four Simulation Scenarios

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

1. **AP bubbles** = `BOTTLENECK!` / `RECOVERED!` — AP saturation and recovery states
2. **Student bubbles** = `Connecting!`, `Registered!`, `Retrying`, or `FAILED!` — registration progress
3. The exact bubble color is controlled by OMNeT++/Qtenv, but the status text is shown directly on the module

### Monitor Output (Final Report):

At simulation end, the **BottleneckMonitor** module prints a detailed report:

```
=================================================
  NET-OPT ADVANCED SIMULATION COMPLETE
  Total Samples     : 600
  Bottleneck Events : 1
  Admin Bottlenecks : 31 samples (35s)
  CS Bottlenecks    : 46 samples (50s)
  Lib Bottlenecks   : 0 samples (0s)
  Peak Load At      : 60s
=================================================
```

**What each metric means:**
- **Total Samples**: Monitoring intervals (1 per second)
- **Bottleneck Events**: Transition count (health dropped below 50%)
- **XXX Bottlenecks**: Number of samples where building was congested + total duration

### RegistrationPeak Scenario (Main Demo):

**Timeline:**
```
t=0-30s    → Students start arriving, network healthy (95%)
t=30-60s   → Registration opens, load rising (60%)
t=60-90s   → PEAK: CS Block APs overwhelmed, BOTTLENECK alerts (25%)
t=90-120s  → Demand slightly reduces, partial recovery (40%)
t=120-300s → Post-peak, network stabilizes (80%)
```

**Expected Monitor Output:**
```
=================================================
  NET-OPT ADVANCED SIMULATION COMPLETE
  Total Samples     : 600
  Bottleneck Events : 1
  Admin Bottlenecks : 31 samples (35s)
  CS Bottlenecks    : 46 samples (50s)
  Lib Bottlenecks   : 0 samples (0s)
  Peak Load At      : 60s
=================================================
```

**What this tells us:**
- **Admin & CS** experienced ~30-50 sampling intervals of congestion (each ~1s, so ~30-50s total bottleneck duration)
- **Library** stayed uncongested (0 samples)
- **Peak arrived at t=60s** and lasted ~50 seconds before recovery
- Students show 1-2 retry attempts due to timeouts during peak

### OptimizedNetwork Scenario:

**Expected Monitor Output:**
```
=================================================
  NET-OPT ADVANCED SIMULATION COMPLETE
  Total Samples     : 300
  Bottleneck Events : 0
  Admin Bottlenecks : 0 samples (0s)
  CS Bottlenecks    : 0 samples (0s)
  Lib Bottlenecks   : 0 samples (0s)
=================================================
```

**What this demonstrates:**
- Upgrading AP capacity (80 maxClients vs 5 maxClients in Peak) completely **eliminates bottlenecks**
- All students register on **first attempt** (no retries needed)
- Network health stays > 70% throughout

### StressTest Scenario (Worst Case):

**Expected Monitor Output:**
```
=================================================
  NET-OPT ADVANCED SIMULATION COMPLETE
  Total Samples     : 300
  Bottleneck Events : 1
  Admin Bottlenecks : 136 samples (140s)
  CS Bottlenecks    : 116 samples (120s)
  Lib Bottlenecks   : 146 samples (150s)
  Peak Load At      : 50s
=================================================
```

**What this demonstrates:**
- **Severe, sustained congestion** across all buildings (120-150s duration)
- Students require **3-4 retry attempts** to finally succeed
- This shows why staggered registration is critical for real deployments

## Web companion (`net-opt-web/`)

Discrete-time **browser simulator** kept in lockstep with **`simulations/omnetpp.ini`**: same **`sim-time-limit`** per config (Registration **600s**, most others **300s**, NormalDay **180s**), five scenario names, **`useTimeProfile` / `stressProfile`**, **`maxRetries`** (**50** only on Registration Peak; otherwise OmNeT **`5 + (3 − priority)`**), monitor **1s** sampling **·** debounce **`ceil(5/Δt)`** like `BottleneckMonitor`, and **`StudentDevice` / `Infrastructure.cc`** health formulas. Runs the **full OMNeT horizon** (simulation **does not** stop when every student completes). Discrete-event fidelity matches at the scenario/metric layer; the TS kernel differs from OMNeT C++ internally.

### Run locally

```bash
cd net-opt-web/
npm install
npm run dev
```

### Production build / deploy

```bash
cd net-opt-web/
npm run build
```

Static files emit to **`net-opt-web/dist/`**. Host on GitHub Pages, Netlify, or Vercel with **publish directory** = `dist` (or `net-opt-web/dist` when using a mono-repo aware host). `vite.config.ts` uses `base: './'` so file-based open and sub-path hosting work.

---