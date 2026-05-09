import { useCallback, useEffect, useRef, useState, type ReactElement } from "react";
import type { ScenarioId } from "./engine/types";
import { SCENARIOS, scenarioIds } from "./engine/scenarios";
import { NetOptSimulator } from "./engine/simulation";
import { LineChart } from "./components/LineChart";
import { TopologyView } from "./components/TopologyView";
import { ActivityLog } from "./components/ActivityLog";

/** Edge padding + max readable width — uses nearly full ultrawide / 4K */
const SHELL = "mx-auto w-full max-w-[1600px] px-3 py-7 sm:px-5 lg:px-8";

function useSimRef() {
  const ref = useRef<NetOptSimulator | null>(null);
  if (!ref.current) ref.current = new NetOptSimulator();
  return ref;
}

export function App(): ReactElement {
  const simRef = useSimRef();
  const [scenarioId, setScenarioId] = useState<ScenarioId>("RegistrationPeak");
  const cfg = SCENARIOS[scenarioId];

  const [playing, setPlaying] = useState(false);
  const [speed, setSpeed] = useState(64);
  const [seed, setSeed] = useState(4242);
  const [, setTick] = useState(0);
  const [report, setReport] = useState("");
  const [showBuildingLines, setShowBuildingLines] = useState(false);

  const reinit = useCallback(() => {
    simRef.current!.reset(SCENARIOS[scenarioId], seed);
    setPlaying(false);
    setReport("");
    setTick((n) => n + 1);
  }, [scenarioId, seed, simRef]);

  useEffect(() => {
    reinit();
  }, [reinit]);

  useEffect(() => {
    if (!playing) return;
    let raf = 0;
    let last = performance.now();
    const loop = (t: number) => {
      const dtWall = (t - last) / 1000;
      last = t;
      const sim = simRef.current!;
      const maxStep = 2.5;
      let simDt = Math.min(maxStep, dtWall * speed);
      const cap = sim.cfg.simTimeLimit - sim.time;
      if (simDt > cap) simDt = Math.max(0, cap);
      if (simDt > 0) sim.step(simDt);
      setTick((n) => n + 1);
      if (sim.isFinished()) {
        setPlaying(false);
        setReport(sim.finalizeMessage());
      }
      raf = requestAnimationFrame(loop);
    };
    raf = requestAnimationFrame(loop);
    return () => cancelAnimationFrame(raf);
  }, [playing, speed, simRef]);

  const snap = simRef.current!.snapshot();

  const regTotal = snap.students.filter((s) => s.phase === "registered").length;
  const failTotal = snap.students.filter((s) => s.phase === "failed").length;

  const serverLoadPct = Math.round((snap.server.activeSessions / Math.max(1, snap.server.maxConcurrent)) * 100);

  return (
    <div className="bg-grid min-h-screen w-full pb-12">
      <header className="border-b border-white/[0.08] bg-[var(--surface-1)]/80 backdrop-blur-sm">
        <div className={`${SHELL} flex flex-col gap-6 pt-8 pb-8 sm:flex-row sm:items-start sm:justify-between`}>
          <div className="min-w-0 flex-1">
            <p className="text-[11px] font-medium uppercase tracking-[0.18em] text-[var(--accent)]">
              Parallel &amp; Distributed Computing · project
            </p>
            <h1 className="mt-2 text-2xl font-semibold tracking-tight text-white sm:text-3xl">NET-OPT simulator</h1>
            <p className="mt-3 max-w-4xl text-[14px] leading-relaxed text-[var(--muted)]">
              This page replays the <strong className="font-medium text-white/90">five fixed scenarios</strong> from{" "}
              <code className="rounded bg-white/[0.06] px-1.5 py-px text-[13px] text-teal-200/90">simulations/omnetpp.ini</code> with the same
              limits, monitor sampling, debounce, and full <code className="text-white/75">sim-time-limit</code>. The engine is discrete-time
              TypeScript (not OMNeT++’s scheduler), so individual message times can differ, but bottlenecks, retries, and health curves follow the
              same formulas as in <code className="text-white/75">Infrastructure.cc</code> / <code className="text-white/75">StudentDevice.cc</code>.
            </p>
          </div>

          <div className="flex w-full flex-wrap gap-2 font-mono text-[11px] text-[var(--muted)] sm:w-auto sm:max-w-md sm:justify-end">
            <span className="rounded-lg border border-white/10 px-3 py-1.5 text-white">t · {snap.time.toFixed(0)} / {snap.timeLimit}s</span>
            <span className="rounded-lg border border-emerald-500/20 bg-emerald-500/[0.07] px-3 py-1.5 text-emerald-300/95">
              ✓ {regTotal}/{snap.students.length}
            </span>
            {failTotal > 0 ? (
              <span className="rounded-lg border border-red-500/25 bg-red-500/[0.08] px-3 py-1.5 text-red-300">{failTotal} fail</span>
            ) : null}
            <span className="rounded-lg border border-white/10 px-3 py-1.5">srv {serverLoadPct}%</span>
          </div>
        </div>
      </header>

      <main className={`${SHELL} space-y-5 sm:space-y-6`}>
        <section className="w-full rounded-xl border border-white/10 bg-[var(--surface-1)] p-4 sm:p-5">
          <div className="flex flex-col gap-4 lg:flex-row lg:items-start lg:justify-between">
            <label className="block min-w-0 flex-1 lg:max-w-md">
              <span className="text-[11px] font-medium uppercase tracking-wider text-[var(--muted)]">Scenario · omnetpp.ini</span>
              <select
                value={scenarioId}
                disabled={playing}
                onChange={(e) => setScenarioId(e.target.value as ScenarioId)}
                className="mt-2 w-full max-w-full appearance-none rounded-lg border border-white/15 bg-black/35 py-2.5 pl-3 pr-8 text-[14px] text-white outline-none focus:border-teal-500/50 disabled:opacity-45"
              >
                {scenarioIds().map((id) => (
                  <option key={id} value={id}>
                    {SCENARIOS[id].title} — {SCENARIOS[id].simTimeLimit}s
                  </option>
                ))}
              </select>
            </label>
            <p className="min-w-0 flex-1 text-[13px] leading-relaxed text-[var(--muted)] lg:max-w-2xl lg:text-right">{cfg.description}</p>
          </div>

          <div className="mt-5 flex flex-col gap-4 border-t border-white/[0.07] pt-5 sm:flex-row sm:flex-wrap sm:items-center sm:justify-between">
            <div className="flex flex-wrap gap-2">
              <button
                type="button"
                onClick={() => setPlaying((p) => !p)}
                className="rounded-lg bg-gradient-to-r from-teal-400 to-cyan-500 px-5 py-2.5 text-[13px] font-semibold text-slate-900 shadow-lg shadow-teal-500/15 transition hover:brightness-105"
              >
                {playing ? "Pause" : "Run"}
              </button>
              <button
                type="button"
                onClick={() => {
                  simRef.current!.step(cfg.monitorSamplingIntervalSec);
                  setTick((n) => n + 1);
                }}
                disabled={playing}
                className="rounded-lg border border-white/14 bg-white/[0.04] px-3 py-2.5 text-[13px] text-white/90 hover:bg-white/[0.08] disabled:opacity-40"
              >
                +{cfg.monitorSamplingIntervalSec}s
              </button>
              <button
                type="button"
                onClick={reinit}
                disabled={playing}
                className="rounded-lg border border-white/12 px-3 py-2.5 text-[13px] text-[var(--muted)] hover:border-white/20 hover:text-white disabled:opacity-40"
              >
                Reset
              </button>
            </div>

            <div className="flex flex-wrap items-center gap-x-4 gap-y-2 sm:justify-end">
              <label className="flex min-w-0 flex-1 items-center gap-2 text-[12px] text-[var(--muted)] sm:flex-initial sm:min-w-[200px]">
                <span className="shrink-0 whitespace-nowrap">Speed</span>
                <input
                  type="range"
                  min={4}
                  max={200}
                  value={speed}
                  onChange={(e) => setSpeed(Number(e.target.value))}
                  className="min-w-0 flex-1 accent-teal-400 sm:w-36"
                />
                <span className="shrink-0 font-mono text-[12px] text-white/90">{speed}×</span>
              </label>
              <label className="flex items-center gap-2 text-[12px] text-[var(--muted)]">
                <span className="shrink-0">Seed</span>
                <input
                  type="number"
                  value={seed}
                  disabled={playing}
                  onChange={(e) => setSeed(Number(e.target.value) || 0)}
                  className="w-full min-w-[4.5rem] max-w-[6rem] rounded-lg border border-white/12 bg-black/40 px-2 py-1.5 font-mono text-[13px] text-white outline-none disabled:opacity-45"
                />
              </label>
            </div>
          </div>
        </section>

        <details className="w-full rounded-xl border border-white/[0.08] bg-[var(--surface-1)]/60 px-4 py-3 sm:px-5">
          <summary className="cursor-pointer list-none text-[13px] text-white/90 [&::-webkit-details-marker]:hidden">
            <span className="text-[var(--muted)]">▸ Why no sliders for APs / ini parameters?</span>
          </summary>
          <div className="mt-3 border-t border-white/[0.06] pt-3 text-[13px] leading-relaxed text-[var(--muted)]">
            <p className="mb-2">
              OMNeT++ lets you edit <code className="text-white/75">omnetpp.ini</code>, drag modules in the IDE, and inspect every gate. This site
              is a <strong className="text-white/85">read-only twin</strong>: each dropdown option is a frozen copy of one <code className="text-white/75">[Config …]</code> block so your report can
              say “same numbers as the C++ run.”
            </p>
            <p>
              Adding live sliders (extra APs, custom <code className="text-white/75">maxClients</code>, etc.) means maintaining a second parameter
              surface and validating it against OMNeT—doable, but not in this build. To experiment: change <code className="text-white/75">scenarios.ts</code> or the ini in
              the OMNeT project, then rebuild.
            </p>
          </div>
        </details>

        <div className="flex w-full min-w-0 flex-col gap-5 xl:flex-row xl:items-start xl:gap-8">
          <div className="min-w-0 flex-1 space-y-4 sm:space-y-5">
            <LineChart
              history={snap.monitorHistory}
              timeLimit={snap.timeLimit}
              sampleIntervalSec={snap.scenario.monitorSamplingIntervalSec}
              showBuildingLines={showBuildingLines}
            />

            <label className="flex cursor-pointer select-none items-center gap-2 text-[12px] text-[var(--muted)]">
              <input
                type="checkbox"
                checked={showBuildingLines}
                onChange={(e) => setShowBuildingLines(e.target.checked)}
                className="rounded border-white/20 accent-teal-400"
              />
              Overlay per-building health (Admin / CS / Library)
            </label>

            {report ? (
              <pre className="w-full overflow-x-auto rounded-xl border border-emerald-500/20 bg-emerald-500/[0.06] px-4 py-3 font-mono text-[11px] leading-relaxed text-emerald-100/95 whitespace-pre-wrap break-words">
                {report}
              </pre>
            ) : null}
          </div>

          <aside className="w-full shrink-0 xl:sticky xl:top-5 xl:w-[min(100%,20rem)] xl:self-start">
            <TopologyView snapshot={snap} />
          </aside>
        </div>

        <details className="group w-full rounded-xl border border-white/[0.08] bg-[var(--surface-1)] open:bg-[#0f1620]">
          <summary className="cursor-pointer list-none px-4 py-3 text-[13px] font-medium text-white/90 sm:px-5 [&::-webkit-details-marker]:hidden">
            <span className="text-[var(--muted)] group-open:hidden">▸ Bottleneck summary &amp; activity log</span>
            <span className="hidden text-[var(--muted)] group-open:inline">▾ Bottleneck summary &amp; activity log</span>
          </summary>
          <div className="space-y-4 border-t border-white/[0.06] px-4 pb-4 pt-3 sm:px-5">
            <div className="grid gap-2 sm:grid-cols-2 lg:grid-cols-3">
              <SummaryCell label="Monitor samples (1 Hz)" value={String(snap.summary.sampleCount)} />
              <SummaryCell label="Network bottleneck events" value={String(snap.summary.bottleneckEvents)} />
              <SummaryCell label="Peak (monitor)" value={snap.summary.peakLoadTime != null ? `${snap.summary.peakLoadTime.toFixed(0)} s` : "—"} />
              <SummaryCell
                label="Admin · bottleneck samples (~s)"
                value={`${snap.summary.adminBottleneckSamples} (~${snap.summary.adminBottleneckDuration.toFixed(0)})`}
              />
              <SummaryCell
                label="CS · bottleneck samples (~s)"
                value={`${snap.summary.csBottleneckSamples} (~${snap.summary.csBottleneckDuration.toFixed(0)})`}
              />
              <SummaryCell
                label="Library · bottleneck samples (~s)"
                value={`${snap.summary.libBottleneckSamples} (~${snap.summary.libBottleneckDuration.toFixed(0)})`}
              />
            </div>
            <div>
              <p className="mb-2 text-[11px] uppercase tracking-wider text-[var(--muted)]">Registration server (counters)</p>
              <div className="grid grid-cols-2 gap-2 sm:grid-cols-4 font-mono text-[11px] text-white/80 sm:text-[12px]">
                <div className="rounded-lg border border-white/8 bg-black/25 px-2 py-1.5">active · {snap.server.activeSessions}/{snap.server.maxConcurrent}</div>
                <div className="rounded-lg border border-white/8 bg-black/25 px-2 py-1.5">queue · {snap.server.queue}</div>
                <div className="rounded-lg border border-white/8 bg-black/25 px-2 py-1.5">served · {snap.server.totalServed}</div>
                <div className="rounded-lg border border-white/8 bg-black/25 px-2 py-1.5">reject · {snap.server.totalRejected}</div>
              </div>
            </div>
            <ActivityLog logs={snap.logs} />
          </div>
        </details>
      </main>

      <footer className="border-t border-white/[0.07] px-3 py-5 text-center text-[11px] text-[var(--muted)] sm:px-6">
        NET-OPT web · paired with OMNeT++ sources in repo root ·{" "}
        <code className="rounded bg-white/[0.05] px-1 text-[11px] text-white/75">npm run build</code>
      </footer>
    </div>
  );
}

function SummaryCell({ label, value }: { label: string; value: string }): ReactElement {
  return (
    <div className="rounded-lg border border-white/[0.06] bg-black/22 px-3 py-2">
      <p className="text-[10px] uppercase tracking-wide text-[var(--muted)]">{label}</p>
      <p className="break-words font-mono text-[12px] text-white sm:text-[13px]">{value}</p>
    </div>
  );
}
