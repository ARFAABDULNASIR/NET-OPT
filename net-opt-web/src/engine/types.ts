export type BuildingId = "Admin" | "CS" | "Library";

export type ScenarioId =
  | "NormalDay"
  | "RegistrationPeak"
  | "OptimizedNetwork"
  | "StaggeredRegistration"
  | "StressTest";

export type StudentPhase =
  | "idle"
  | "connecting"
  | "at_ap"
  | "waiting_server"
  | "retry_backoff"
  | "registered"
  | "failed";

export interface ScenarioConfig {
  id: ScenarioId;
  title: string;
  description: string;
  /** Simulated seconds wall clock for this scenario */
  simTimeLimit: number;
  useTimeProfile: boolean;
  stressProfile: boolean;
  /** When false (NormalDay), students only generate light background traffic */
  registering: boolean;
  adminStudents: number;
  csStudents: number;
  libStudents: number;
  /** >=0 compressed burst arrival; negative = dept-staggered (StudentDevice parity) */
  arrivalWindowSec: number;
  dataRateMbps: number;
  apMaxClients: number;
  apBandwidthMbps: number;
  txPower: number;
  regMaxSessions: number;
  /** Seconds (ini uses ms units in OMNeT par — we normalize to seconds here) */
  regProcessingSec: number;
  /**
   * OMNeT `BottleneckMonitor.samplingInterval` (seconds). Default 1s matches NED default.
   */
  monitorSamplingIntervalSec: number;
  /**
   * `maxRetries` NED parameter: omnetpp.ini only sets RegistrationPeak (=50).
   * Use `-1` for OMNeT default `(5 + (3 - priority))` per StudentDevice.cc.
   */
  maxRetries: number;

}

export interface BuildingTopo {
  id: BuildingId;
  dept: string;
  numAPs: number;
  apChannel: number;
  numStudents: number;
}

export interface APRuntime {
  key: string;
  building: BuildingId;
  index: number;
  channelNumber: number;
  bandwidth: number;
  maxClients: number;
  connectedClients: number;
  currentLoadMbps: number;
  interference: number;
  droppedPackets: number;
  totalPackets: number;
  isBottleneck: boolean;
  /** Pending load/client releases keyed by completion time */
  releases: Array<{ until: number; load: number; clients: number }>;
}

export interface StudentRuntime {
  id: number;
  building: BuildingId;
  apIndex: number;
  dept: string;
  priority: number;
  phase: StudentPhase;
  retries: number;
  attempts: number;
  bubble: string;
  arrivalPending: boolean;
  nextActionAt: number;
  backoffUntil: number;
  /** Per-student RNG stream — derived from seed+id */
  regStartTime: number;
}

export interface ServerRuntime {
  activeSessions: number;
  queue: number;
  maxConcurrent: number;
  processingSec: number;
  totalServed: number;
  totalRejected: number;
  /** Sim time when each active session completes */
  completions: number[];
}

export interface MonitorSample {
  t: number;
  networkHealth: number;
  adminHealth: number;
  csHealth: number;
  libHealth: number;
  bottleneckNetwork: boolean;
}

export interface SimSnapshot {
  time: number;
  timeLimit: number;
  scenario: ScenarioConfig;
  buildings: BuildingTopo[];
  aps: APRuntime[];
  students: StudentRuntime[];
  server: ServerRuntime;
  monitorHistory: MonitorSample[];
  logs: string[];
  summary: {
    sampleCount: number;
    bottleneckEvents: number;
    adminBottleneckSamples: number;
    csBottleneckSamples: number;
    libBottleneckSamples: number;
    adminBottleneckDuration: number;
    csBottleneckDuration: number;
    libBottleneckDuration: number;
    peakLoadTime: number | null;
    recoveryTime: number | null;
  };
  finished: boolean;
}
