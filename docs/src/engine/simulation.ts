/**
 * Discrete-time campus Wi-Fi bottleneck lab — aligns with OMNeT++ NET-OPT scenarios.
 */

import type {
  APRuntime,
  BuildingId,
  BuildingTopo,
  MonitorSample,
  ScenarioConfig,
  SimSnapshot,
  StudentRuntime,
  ServerRuntime,
} from "./types";
import { SCENARIOS } from "./scenarios";

/** NetOpt.ned topology */
export const ADMIN_APS = 4;
export const CS_APS = 5;
export const LIB_APS = 3;

function mulberry32(a: number) {
  return function () {
    let t = (a += 0x6d2b79f5);
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function uniform(rng: () => number, lo: number, hi: number) {
  return lo + (hi - lo) * rng();
}

export function deptForBuilding(b: BuildingId): string {
  if (b === "Admin") return "Admin";
  if (b === "Library") return "Library";
  return "CS";
}

export function arrivalTimeForStudent(cfg: ScenarioConfig, b: BuildingId, studentIdLocal: number, rng: () => number): number {
  if (!cfg.registering) return 99999;
  if (cfg.arrivalWindowSec >= 0) {
    const w = cfg.arrivalWindowSec;
    const compressed = (studentIdLocal % 120) * (w / 120.0) + uniform(rng, 0, 0.1 * w);
    return Math.max(0, compressed);
  }
  let base: number;
  if (b === "CS") base = 30;
  else if (b === "Admin") base = 45;
  else base = 60;
  const prio = priorityForScenario(cfg, b, studentIdLocal);
  const priorityOffset = (prio - 1) * 8;
  const spread = (studentIdLocal % 10) * 0.5 + uniform(rng, 0, 2);
  return base + priorityOffset + spread;
}

function priorityForScenario(cfg: ScenarioConfig, b: BuildingId, sid: number): number {
  if (cfg.id === "StaggeredRegistration") {
    if (b === "CS") return 1;
    if (b === "Admin") return 2;
    return 3;
  }
  if (cfg.id === "StressTest") return 2;
  /* NED parity: uniform(1,4) excludes 4 => 1..3 */
  return 1 + (sid % 3);
}

export function calculateBuildingHealth(
  cfg: ScenarioConfig,
  building: BuildingId,
  t: number,
  topo: BuildingTopo[],
): number {
  const blk = topo.find((x) => x.id === building)!;
  const registering = cfg.registering;

  const useTp = registering && cfg.useTimeProfile;

  if (!useTp || !registering) {
    const utilization = blk.numStudents / Math.max(1, blk.numAPs * cfg.apMaxClients);
    const mult = registering ? 1.25 : 1;
    let h = 100 - utilization * 100 * mult;
    return Math.min(100, Math.max(5, h));
  }

  const stressProfile = cfg.stressProfile;

  const cs = (): number => {
    if (stressProfile) {
      if (t < 20) return 95;
      if (t < 35) return 82;
      if (t < 50) return 60;
      if (t < 85) return 18;
      if (t < 120) return 12;
      if (t < 170) return 38;
      if (t < 230) return 65;
      return 82;
    }
    if (t < 30) return 95;
    if (t < 50) return 75;
    if (t < 70) return 30;
    if (t < 100) return 20;
    if (t < 140) return 55;
    return 85;
  };

  const admin = (): number => {
    if (stressProfile) {
      if (t < 25) return 95;
      if (t < 45) return 80;
      if (t < 70) return 52;
      if (t < 110) return 22;
      if (t < 150) return 15;
      if (t < 210) return 42;
      return 78;
    }
    if (t < 45) return 95;
    if (t < 75) return 60;
    if (t < 110) return 40;
    return 80;
  };

  const lib = (): number => {
    if (stressProfile) {
      if (t < 35) return 95;
      if (t < 60) return 86;
      if (t < 90) return 64;
      if (t < 130) return 30;
      if (t < 180) return 20;
      if (t < 240) return 48;
      return 82;
    }
    if (t < 60) return 95;
    if (t < 90) return 70;
    if (t < 120) return 50;
    return 88;
  };

  switch (building) {
    case "CS":
      return cs();
    case "Admin":
      return admin();
    default:
      return lib();
  }
}

function topologyFromScenario(cfg: ScenarioConfig): BuildingTopo[] {
  return [
    {
      id: "Admin",
      dept: "Admin",
      numAPs: ADMIN_APS,
      apChannel: 1,
      numStudents: cfg.adminStudents,
    },
    {
      id: "CS",
      dept: "CS",
      numAPs: CS_APS,
      apChannel: 6,
      numStudents: cfg.csStudents,
    },
    {
      id: "Library",
      dept: "Library",
      numAPs: LIB_APS,
      apChannel: 11,
      numStudents: cfg.libStudents,
    },
  ];
}

function makeAPs(cfg: ScenarioConfig, topo: BuildingTopo[]): APRuntime[] {
  const out: APRuntime[] = [];
  for (const b of topo) {
    for (let i = 0; i < b.numAPs; i++) {
      out.push({
        key: `${b.id}:${i}`,
        building: b.id,
        index: i,
        channelNumber: b.apChannel,
        bandwidth: cfg.apBandwidthMbps,
        maxClients: cfg.apMaxClients,
        connectedClients: 0,
        currentLoadMbps: 0,
        interference: 0.05,
        droppedPackets: 0,
        totalPackets: 0,
        isBottleneck: false,
        releases: [],
      });
    }
  }
  return out;
}

export function seedStudents(cfg: ScenarioConfig, seed: number): StudentRuntime[] {
  const topo = topologyFromScenario(cfg);
  let gid = 0;
  const out: StudentRuntime[] = [];

  function pushBuilding(bid: BuildingId, count: number) {
    const rng = mulberry32(seed + gid * 977 + bid.charCodeAt(0));
    for (let i = 0; i < count; i++) {
      const btopo = topo.find((t) => t.id === bid)!;
      const apIndex = i % btopo.numAPs;
      const arr = arrivalTimeForStudent(cfg, bid, i, rng);
      const prio = priorityForScenario(cfg, bid, i);
      out.push({
        id: gid++,
        building: bid,
        apIndex,
        dept: deptForBuilding(bid),
        priority: prio,
        phase: cfg.registering ? "idle" : "idle",
        retries: 0,
        attempts: 0,
        bubble: "",
        arrivalPending: true,
        nextActionAt: arr,
        backoffUntil: 0,
        regStartTime: -1,
      });
    }
  }

  pushBuilding("Admin", cfg.adminStudents);
  pushBuilding("CS", cfg.csStudents);
  pushBuilding("Library", cfg.libStudents);

  return out;
}

function backoffSec(prio: number, retryCount: number): number {
  const base = prio === 1 ? 1 : prio === 2 ? 1.5 : 2;
  const step = Math.min(retryCount, 3);
  return Math.min(5, base * (1 << step));
}

/** StudentDevice.cc retry cap: ini value if >= 0 else `5 + (3 - priority)`. */
export function omnetRetryCap(cfg: ScenarioConfig, priority: number): number {
  if (cfg.maxRetries >= 0) return cfg.maxRetries;
  return 5 + (3 - priority);
}

function updateInterference(t: number, ap: APRuntime): number {
  let base =
    t > 30 && t < 90 ? 0.15 + (ap.currentLoadMbps / Math.max(ap.bandwidth, 1)) * 0.25
    : t > 90 && t < 120 ? 0.1 + (ap.currentLoadMbps / Math.max(ap.bandwidth, 1)) * 0.15
    : 0.05;

  if (ap.channelNumber === 1 || ap.channelNumber === 6) base *= 1.2;
  return Math.min(0.6, base);
}

function effectiveBw(ap: APRuntime): number {
  return ap.bandwidth * (1 - ap.interference * 0.3);
}

function checkBn(ap: APRuntime): boolean {
  const bw = effectiveBw(ap);
  return ap.currentLoadMbps > bw * 0.8 || ap.connectedClients > ap.maxClients * 0.9;
}

type Job = { until: number; studentId: number };

export class NetOptSimulator {
  cfg!: ScenarioConfig;
  seed!: number;
  time = 0;
  topo: BuildingTopo[] = [];
  aps: APRuntime[] = [];
  students: StudentRuntime[] = [];
  server: ServerRuntime = {
    activeSessions: 0,
    queue: 0,
    maxConcurrent: 200,
    processingSec: 0.05,
    totalServed: 0,
    totalRejected: 0,
    completions: [],
  };

  logs: string[] = [];
  monitorHistory: MonitorSample[] = [];
  lastMonitorT = -1;

  sampleCount = 0;
  bottleneckEvents = 0;
  consecBelowNetwork = 0;
  prevNetworkBn = false;
  adminBnSamples = 0;
  csBnSamples = 0;
  libBnSamples = 0;
  peakLoadTime: number | null = null;
  peakDetected = false;
  recoveryTime: number | null = null;

  consecAdmin = 0;
  consecCs = 0;
  consecLib = 0;
  prevAdminBn = false;
  prevCsBn = false;
  prevLibBn = false;
  durAdmin = 0;
  durCs = 0;
  durLib = 0;
  adminStart = -1;
  csStart = -1;
  libStart = -1;

  serverJobs: Job[] = [];

  waitingAtServerQueue: number[] = [];

  interferenceEvery = 0;
  idlePingAcc = 0;

  /** `ceil(5 / samplingInterval)` — BottleneckMonitor.cc */
  bnDebounceSamples = 5;

  /** First sample scheduled at OmNeT `t = samplingInterval` */
  nextMonitorSampleT = 1;

  constructor() {
    /** Safe before first React paint — App still re-syncs via `useEffect` + `reset`. */
    this.reset(SCENARIOS.RegistrationPeak, 4242);
  }

  reset(cfg: ScenarioConfig, seed = 4242): void {
    this.cfg = cfg;
    this.seed = seed;
    this.time = 0;
    this.topo = topologyFromScenario(cfg);
    this.aps = makeAPs(cfg, this.topo);
    this.students = seedStudents(cfg, seed);
    this.server = {
      activeSessions: 0,
      queue: 0,
      maxConcurrent: cfg.regMaxSessions,
      processingSec: cfg.regProcessingSec,
      totalServed: 0,
      totalRejected: 0,
      completions: [],
    };
    this.serverJobs = [];
    this.waitingAtServerQueue = [];
    this.logs = [];
    this.monitorHistory = [];
    this.lastMonitorT = -1;
    this.sampleCount = 0;
    this.bottleneckEvents = 0;
    this.consecBelowNetwork = 0;
    this.prevNetworkBn = false;
    this.adminBnSamples = this.csBnSamples = this.libBnSamples = 0;
    this.peakLoadTime = null;
    this.peakDetected = false;
    this.recoveryTime = null;
    this.consecAdmin = this.consecCs = this.consecLib = 0;
    this.prevAdminBn = this.prevCsBn = this.prevLibBn = false;
    this.durAdmin = this.durCs = this.durLib = 0;
    this.adminStart = this.csStart = this.libStart = -1;
    this.interferenceEvery = 0;
    this.idlePingAcc = 0;
    const iv = cfg.monitorSamplingIntervalSec;
    this.bnDebounceSamples = Math.max(1, Math.ceil(5 / iv));
    this.nextMonitorSampleT = iv;
    this.pushLog(`${cfg.title} — reset · ${cfg.simTimeLimit}s OMNeT \`sim-time-limit\`, monitor Δ=${iv}s (${this.bnDebounceSamples} debounced samples)`);
  }

  maybeSampleMonitor(untilExclusive: number): void {
    const cap = Math.min(untilExclusive, this.cfg.simTimeLimit);
    const iv = this.cfg.monitorSamplingIntervalSec;
    while (cap + 1e-12 >= this.nextMonitorSampleT) {
      const tS = Math.min(this.nextMonitorSampleT, this.cfg.simTimeLimit);
      this.sampleMonitorSnapshot(tS);
      this.nextMonitorSampleT += iv;
    }
  }

  pushLog(line: string): void {
    this.logs.push(`[${this.time.toFixed(2)}s] ${line}`);
    if (this.logs.length > 200) this.logs.shift();
  }

  getAp(student: StudentRuntime): APRuntime | undefined {
    return this.aps.find((ap) => ap.building === student.building && ap.index === student.apIndex);
  }

  refreshApBn(ap: APRuntime): void {
    const bn = checkBn(ap);
    if (bn !== ap.isBottleneck) {
      ap.isBottleneck = bn;
      ap.bubble = bn ? "BOTTLENECK!" : "";
    }
  }

  /** Apply AP load release events up to current time */
  processReleases(now: number): void {
    for (const ap of this.aps) {
      while (ap.releases.length > 0 && ap.releases[0].until <= now) {
        const r = ap.releases.shift()!;
        ap.currentLoadMbps = Math.max(0, ap.currentLoadMbps - r.load);
        ap.connectedClients = Math.max(0, ap.connectedClients - r.clients);
        this.refreshApBn(ap);
      }
    }
  }

  tryQueueServer(studentId: number, now: number): boolean {
    const srv = this.server;
    if (srv.activeSessions < srv.maxConcurrent) {
      this.startServerJob(studentId, now);
      return true;
    }
    if (this.waitingAtServerQueue.length < 100) {
      this.waitingAtServerQueue.push(studentId);
      srv.queue = this.waitingAtServerQueue.length;
      return true;
    }
    srv.totalRejected++;
    return false;
  }

  /** Start servicing one session now (caller ensures capacity available). */
  startServerJob(studentId: number, now: number): void {
    const srv = this.server;
    srv.activeSessions++;
    const respTime = srv.processingSec + srv.activeSessions * 0.005;
    this.serverJobs.push({ until: now + respTime, studentId });
  }

  completeRegistration(st: StudentRuntime | undefined): void {
    if (st && st.phase === "waiting_server") {
      st.phase = "registered";
      st.bubble = "Registered!";
      this.pushLog(`Student #${st.id} (${st.building}) completed registration`);
    }
  }

  drainServer(now: number): void {
    const srv = this.server;
    while (srv.activeSessions < srv.maxConcurrent && this.waitingAtServerQueue.length > 0) {
      const nextId = this.waitingAtServerQueue.shift()!;
      srv.queue = this.waitingAtServerQueue.length;
      this.startServerJob(nextId, now);
    }
  }

  flushServer(now: number): void {
    this.serverJobs.sort((a, b) => a.until - b.until);
    while (this.serverJobs.length && this.serverJobs[0].until <= now) {
      const done = this.serverJobs.shift()!;
      const st = this.students.find((s) => s.id === done.studentId);
      this.server.activeSessions = Math.max(0, this.server.activeSessions - 1);
      this.server.totalServed++;
      this.completeRegistration(st);
      this.drainServer(now);
    }
  }

  attemptAp(student: StudentRuntime, now: number): void {
    const ap = this.getAp(student);
    if (!ap) return;
    ap.totalPackets++;
    student.attempts++;
    const effBw = effectiveBw(ap);
    if (ap.connectedClients >= ap.maxClients || ap.currentLoadMbps >= effBw * 0.9) {
      ap.droppedPackets++;
      student.retries++;
      student.phase = "retry_backoff";
      student.backoffUntil = now + backoffSec(student.priority, student.retries);
      student.bubble = "Retrying";
      ap.bubble = "BOTTLENECK!";
      this.pushLog(`Student #${student.id} AP overload — backoff`);
      return;
    }

    const txDelayMs = Math.max(2, Math.min(50, student.priority * 3 + Math.random() * 4));
    const loadInc = cfgLoadPacket(this.cfg);
    ap.connectedClients++;
    ap.currentLoadMbps += loadInc;

    const until = now + txDelayMs / 1000 + 0.5;
    const release = {
      until,
      load: loadInc,
      clients: 1,
    };
    ap.releases.push(release);
    ap.releases.sort((a, b) => a.until - b.until);

    student.phase = "waiting_server";
    student.bubble = "Connecting!";
    student.regStartTime = student.regStartTime < 0 ? now : student.regStartTime;

    const admitted = this.tryQueueServer(student.id, now);
    if (!admitted) {
      student.phase = "retry_backoff";
      student.retries++;
      student.backoffUntil = now + backoffSec(student.priority, student.retries);
      student.bubble = "Retrying";
      this.pushLog(`Server queue full → student #${student.id} rejects`);
      ap.connectedClients--;
      ap.currentLoadMbps = Math.max(0, ap.currentLoadMbps - loadInc);
      ap.totalPackets--;
      ap.releases = ap.releases.filter((r) => r !== release);
    }

    this.refreshApBn(ap);
  }

  handleNormalMicroTraffic(dt: number): void {
    this.idlePingAcc += dt * 8;
    if (this.idlePingAcc < 1) return;
    this.idlePingAcc = 0;
    const rng = mulberry32(this.seed + Math.floor(this.time * 10));
    const k = Math.floor(rng() * Math.min(this.students.length, 20));
    for (let j = 0; j < k; j++) {
      const s = this.students[Math.floor(rng() * this.students.length)];
      const ap = this.getAp(s);
      if (!ap || ap.connectedClients >= ap.maxClients) continue;
      ap.connectedClients++;
      ap.currentLoadMbps += rng() * 0.08;
      ap.releases.push({ until: this.time + 0.35 + rng() * 0.2, load: rng() * 0.06, clients: 1 });
    }
  }

  sampleMonitorSnapshot(sampleT: number): void {
    const t = sampleT;
    this.sampleCount++;
    const ah = calculateBuildingHealth(this.cfg, "Admin", t, this.topo);
    const ch = calculateBuildingHealth(this.cfg, "CS", t, this.topo);
    const lh = calculateBuildingHealth(this.cfg, "Library", t, this.topo);
    const health = ah * 0.3 + ch * 0.5 + lh * 0.2;

    const sampleBelow = health < 50;
    if (sampleBelow) this.consecBelowNetwork++;
    else this.consecBelowNetwork = 0;

    const currNetBn = this.consecBelowNetwork >= this.bnDebounceSamples;
    if (currNetBn && !this.prevNetworkBn) {
      this.bottleneckEvents++;
      if (!this.peakDetected && this.cfg.registering) {
        this.peakDetected = true;
        this.peakLoadTime = t;
      }
    }
    if (!sampleBelow && this.prevNetworkBn && health > 70 && this.recoveryTime == null) this.recoveryTime = t;

    const stepBlock = (
      hx: number,
      getConsec: () => number,
      setConsec: (n: number) => void,
      getPrev: () => boolean,
      setPrev: (b: boolean) => void,
      getStart: () => number,
      setStart: (n: number) => void,
      getDur: () => number,
      setDur: (n: number) => void,
      getSamples: () => number,
      setSamples: (n: number) => void,
    ) => {
      let consec = getConsec();
      if (hx < 50) consec++;
      else consec = 0;
      setConsec(consec);

      const currBn = consec >= this.bnDebounceSamples;
      if (currBn) setSamples(getSamples() + 1);

      const prev = getPrev();
      if (currBn && !prev) setStart(t);
      if (!currBn && prev && getStart() >= 0) {
        setDur(getDur() + (t - getStart()));
        setStart(-1);
      }
      setPrev(currBn);
    };

    stepBlock(
      ah,
      () => this.consecAdmin,
      (n) => {
        this.consecAdmin = n;
      },
      () => this.prevAdminBn,
      (b) => {
        this.prevAdminBn = b;
      },
      () => this.adminStart,
      (n) => {
        this.adminStart = n;
      },
      () => this.durAdmin,
      (n) => {
        this.durAdmin = n;
      },
      () => this.adminBnSamples,
      (n) => {
        this.adminBnSamples = n;
      },
    );
    stepBlock(
      ch,
      () => this.consecCs,
      (n) => {
        this.consecCs = n;
      },
      () => this.prevCsBn,
      (b) => {
        this.prevCsBn = b;
      },
      () => this.csStart,
      (n) => {
        this.csStart = n;
      },
      () => this.durCs,
      (n) => {
        this.durCs = n;
      },
      () => this.csBnSamples,
      (n) => {
        this.csBnSamples = n;
      },
    );
    stepBlock(
      lh,
      () => this.consecLib,
      (n) => {
        this.consecLib = n;
      },
      () => this.prevLibBn,
      (b) => {
        this.prevLibBn = b;
      },
      () => this.libStart,
      (n) => {
        this.libStart = n;
      },
      () => this.durLib,
      (n) => {
        this.durLib = n;
      },
      () => this.libBnSamples,
      (n) => {
        this.libBnSamples = n;
      },
    );

    this.prevNetworkBn = currNetBn;
    const row: MonitorSample = {
      t,
      networkHealth: health,
      adminHealth: ah,
      csHealth: ch,
      libHealth: lh,
      bottleneckNetwork: currNetBn,
    };
    const step = Math.max(0.2, Math.min(2, Math.floor(this.cfg.simTimeLimit / 400) || 1));
    if (this.monitorHistory.length === 0 || t - this.monitorHistory[this.monitorHistory.length - 1].t >= step) this.monitorHistory.push(row);
    this.lastMonitorT = sampleT;
  }

  step(dtSim: number): void {
    if (this.time >= this.cfg.simTimeLimit) return;

    const before = this.time;
    const now = Math.min(this.time + dtSim, this.cfg.simTimeLimit);
    const actualDt = now - before;
    this.time = now;

    this.interferenceEvery += actualDt;
    if (this.interferenceEvery >= 0.35) {
      this.interferenceEvery = 0;
      for (const ap of this.aps) {
        ap.interference = updateInterference(now, ap);
        this.refreshApBn(ap);
      }
    }

    this.processReleases(now);
    this.flushServer(now);

    if (!this.cfg.registering) {
      this.handleNormalMicroTraffic(actualDt);
      this.maybeSampleMonitor(now);
      return;
    }

    for (const s of this.students) {
      /* arrival */
      if (s.phase === "idle" && s.arrivalPending && now >= s.nextActionAt) {
        s.arrivalPending = false;
        s.phase = "connecting";
      }
      /* retry */
      if (s.phase === "retry_backoff" && now >= s.backoffUntil) {
        const cap = omnetRetryCap(this.cfg, s.priority);
        if (s.attempts >= cap) {
          s.phase = "failed";
          s.bubble = "FAILED!";
          this.pushLog(`Student #${s.id} gave up (attempt cap ${cap}, OMNeT StudentDevice parity)`);
          continue;
        }
        s.phase = "connecting";
      }
      if (s.phase === "connecting") {
        this.attemptAp(s, now);
      }
    }

    this.maybeSampleMonitor(now);
  }

  /** OmNeT runs until `sim-time-limit`; do not truncate when all registrations settle. */
  isFinished(): boolean {
    return this.time >= this.cfg.simTimeLimit;
  }

  finalizeMessage(): string {
    const s = `
=================================================
NET-OPT WEB RUN COMPLETE (${this.cfg.id})
=================================================
Samples (approx.)     : ${this.sampleCount}
Bottleneck events     : ${this.bottleneckEvents}
Admin bottleneck smpl : ${this.adminBnSamples} (≈ ${this.durAdmin.toFixed(0)}s)
CS bottleneck smpl    : ${this.csBnSamples} (≈ ${this.durCs.toFixed(0)}s)
Library bottleneck smp: ${this.libBnSamples} (≈ ${this.durLib.toFixed(0)}s)`;
    const pk = this.peakLoadTime != null ? `\nPeak (monitor) approx: ${this.peakLoadTime.toFixed(1)}s` : "";
    const rec = this.recoveryTime != null ? `\nRecovery approx: ${this.recoveryTime.toFixed(1)}s` : "";
    const fk = `${this.students.filter((x) => x.phase === "failed").length} failed`;
    const inc = `${this.students.filter((x) => x.phase !== "registered" && x.phase !== "failed").length} unresolved at T`;
    return `${s}${pk}${rec}\nRegistered: ${this.students.filter((x) => x.phase === "registered").length}/${this.students.length}\n${fk}\n${inc}\n=================================================`;
  }

  snapshot(): SimSnapshot {
    return {
      time: this.time,
      timeLimit: this.cfg.simTimeLimit,
      scenario: this.cfg,
      buildings: [...this.topo],
      aps: this.aps.map((a) => ({ ...a, releases: [...a.releases] })),
      students: this.students.map((s) => ({ ...s })),
      server: { ...this.server, completions: [...this.server.completions] },
      monitorHistory: [...this.monitorHistory],
      logs: [...this.logs],
      summary: {
        sampleCount: this.sampleCount,
        bottleneckEvents: this.bottleneckEvents,
        adminBottleneckSamples: this.adminBnSamples,
        csBottleneckSamples: this.csBnSamples,
        libBottleneckSamples: this.libBnSamples,
        adminBottleneckDuration: this.durAdmin,
        csBottleneckDuration: this.durCs,
        libBottleneckDuration: this.durLib,
        peakLoadTime: this.peakLoadTime,
        recoveryTime: this.recoveryTime,
      },
      finished: this.time >= this.cfg.simTimeLimit,
    };
  }
}

function cfgLoadPacket(cfg: ScenarioConfig): number {
  /* packet ~ fraction of student's data appetite */
  return Math.min(cfg.dataRateMbps * 0.04, cfg.apBandwidthMbps * 0.05);
}
