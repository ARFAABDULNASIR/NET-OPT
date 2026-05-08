// ============================================================
// Infrastructure.cc — ADVANCED v5
// Improvements:
//   1. Switch congestion drop tracking
//   2. Router QoS priority routing
//   3. Per-building health signals
//   4. Automated optimization recommendations
//   5. Peak load detection and recovery time measurement
// ============================================================
#include "Infrastructure.h"

// ============================================================
// CampusSwitch
// ============================================================
Define_Module(CampusSwitch);

void CampusSwitch::initialize()
{
    capacity        = par("capacity").doubleValue() / 1e9;
    switchType      = par("switchType").stdstringValue();
    framesForwarded = 0;
    totalLoad       = 0;
    congestionDrops = 0;
    peakLoad        = 0;

    throughputSignal = registerSignal("switchThroughput");
    switchLoadSignal = registerSignal("switchLoad");
    congestionSignal = registerSignal("switchCongestion");

    statsTimer = new cMessage("switchStats");
    scheduleAt(simTime() + 2.0, statsTimer);

    EV << "[SWITCH:" << switchType << "] Ready. Capacity="
       << capacity << " Gbps\n";
}

void CampusSwitch::handleMessage(cMessage *msg)
{
    if (msg == statsTimer) {
        if (totalLoad > peakLoad) peakLoad = totalLoad;
        emit(switchLoadSignal,  totalLoad);
        emit(throughputSignal,  (double)framesForwarded);
        emit(congestionSignal,  (double)congestionDrops);
        scheduleAt(simTime() + 2.0, statsTimer);
        return;
    }

    framesForwarded++;
    totalLoad += 0.001;

    // NEW: congestion threshold — drop if switch overloaded
    if (totalLoad > capacity * 0.95 * 1000) {
        congestionDrops++;
        EV_WARN << "[SWITCH CONGESTION] " << getFullPath()
                << " dropping frame! load=" << totalLoad << "\n";
        delete msg;
        return;
    }

    int arrGate = msg->getArrivalGateId();
    cGate *ag   = gate(arrGate);

    if (strcmp(ag->getName(), "uplink") == 0) {
        for (int i = 0; i < gateSize("ports"); i++) {
            if (gate("ports", i)->isConnected()) {
                send(msg->dup(), "ports", i);
            }
        }
        delete msg;
    } else {
        if (gate("uplink")->isConnected())
            send(msg, "uplink");
        else
            delete msg;
    }
}

void CampusSwitch::finish()
{
    EV << "[SWITCH:" << switchType << "]"
       << " Frames=" << framesForwarded
       << " PeakLoad=" << peakLoad
       << " CongestionDrops=" << congestionDrops << "\n";

    recordScalar("framesForwarded",  framesForwarded);
    recordScalar("peakLoad",         peakLoad);
    recordScalar("congestionDrops",  congestionDrops);
}

// ============================================================
// CoreRouter — with QoS priority routing
// ============================================================
Define_Module(CoreRouter);

void CoreRouter::initialize()
{
    linkSpeed       = par("linkSpeed").doubleValue() / 1e9;
    packetsRouted   = 0;
    congestionLevel = 0;
    priorityPackets = 0;
    peakCongestion  = 0;

    routerLoadSignal = registerSignal("routerLoad");
    congestionSignal = registerSignal("routerCongestion");
    prioritySignal   = registerSignal("priorityPackets");

    EV << "[ROUTER] Ready. LinkSpeed=" << linkSpeed << " Gbps\n";
}

void CoreRouter::handleMessage(cMessage *msg)
{
    packetsRouted++;
    congestionLevel = (double)packetsRouted / 1000.0;
    if (congestionLevel > peakCongestion) peakCongestion = congestionLevel;

    // NEW: check if this is a priority packet (from senior students)
    if (msg->hasPar("priority") && msg->par("priority").longValue() == 1) {
        priorityPackets++;
        emit(prioritySignal, (double)priorityPackets);
    }

    emit(routerLoadSignal, congestionLevel);
    emit(congestionSignal, congestionLevel);

    int arrGate = msg->getArrivalGateId();
    cGate *ag   = gate(arrGate);

    if (strcmp(ag->getName(), "internet") == 0) {
        if (gateSize("coreLinks") > 0 && gate("coreLinks", 0)->isConnected())
            send(msg, "coreLinks", 0);
        else delete msg;
    } else {
        if (gate("internet")->isConnected())
            send(msg, "internet");
        else delete msg;
    }
}

void CoreRouter::finish()
{
    EV << "[ROUTER] Routed=" << packetsRouted
       << " PriorityPkts=" << priorityPackets
       << " PeakCongestion=" << peakCongestion << "\n";

    recordScalar("packetsRouted",   packetsRouted);
    recordScalar("congestionLevel", congestionLevel);
    recordScalar("priorityPackets", priorityPackets);
    recordScalar("peakCongestion",  peakCongestion);
}

// ============================================================
// BottleneckMonitor — ADVANCED with per-building tracking
// ============================================================
Define_Module(BottleneckMonitor);

void BottleneckMonitor::initialize()
{
    samplingInterval = par("samplingInterval").doubleValue();
    sampleCount      = 0;
    bottleneckCount  = 0;
    totalDropRate    = 0;
    totalLatency     = 0;
    peakLoadTime     = -1;
    recoveryTime     = -1;
    peakDetected     = false;

    networkHealthSignal = registerSignal("networkHealth");
    adminHealthSignal   = registerSignal("adminHealth");
    csHealthSignal      = registerSignal("csHealth");
    libraryHealthSignal = registerSignal("libraryHealth");

    buildingBottleneckCount["Admin"] = 0;
    buildingBottleneckCount["CS"]    = 0;
    buildingBottleneckCount["Lib"]   = 0;

    // prev state + durations
    prevNetworkBottleneck = false;
    prevNetworkHealth = 100.0;
    prevBuildingBottleneck["Admin"] = false;
    prevBuildingBottleneck["CS"] = false;
    prevBuildingBottleneck["Lib"] = false;
    buildingBottleneckStart["Admin"] = -1.0;
    buildingBottleneckStart["CS"] = -1.0;
    buildingBottleneckStart["Lib"] = -1.0;
    buildingBottleneckDuration["Admin"] = 0.0;
    buildingBottleneckDuration["CS"] = 0.0;
    buildingBottleneckDuration["Lib"] = 0.0;
    // debounce: require consecutive samples below threshold before triggering
    networkConsecBelow = 0;
    buildingConsecBelow["Admin"] = 0;
    buildingConsecBelow["CS"] = 0;
    buildingConsecBelow["Lib"] = 0;
    // default debounce window = 5s (in samples)
    debounceSamples = std::max(1, (int)std::ceil(5.0 / samplingInterval));

    sampleTimer = new cMessage("sample");
    scheduleAt(simTime() + samplingInterval, sampleTimer);

    EV << "[MONITOR] Advanced Bottleneck Monitor started.\n";
}

double BottleneckMonitor::calculateBuildingHealth(
    const std::string& building, double t)
{
    // scenario-level override: if true, prefer legacy time-based profile
    bool useTimeProfile = false;
    if (hasPar("useTimeProfile"))
        useTimeProfile = par("useTimeProfile").boolValue();
    bool stressProfile = false;
    if (hasPar("stressProfile"))
        stressProfile = par("stressProfile").boolValue();
    // Prefer computing health from actual module parameters (numStudents, numAPs, ap.maxClients)
    // Map friendly building name to module name in the network
    const char *modName = nullptr;
    if (building == "CS")    modName = "csBlock";
    else if (building == "Admin") modName = "adminBlock";
    else if (building == "Lib" || building == "Library") modName = "library";

    if (modName) {
        cModule *net = getParentModule();
        if (net) {
            cModule *b = net->getSubmodule(modName);
            if (b) {
                // read static parameters set by the scenario
                int numStudents = b->par("numStudents").intValue();
                int numAPs = b->par("numAPs").intValue();

                // try to read AP.maxClients from first AP if available
                int apMaxClients = 50;
                cModule *ap0 = b->getSubmodule("ap", 0);
                if (ap0 && ap0->hasPar("maxClients"))
                    apMaxClients = ap0->par("maxClients").intValue();

                double utilization = 0.0;
                if (numAPs > 0 && apMaxClients > 0)
                    utilization = (double)numStudents / (double)(numAPs * apMaxClients);

                // If students are registering, we previously always used the legacy
                // time-based profile. Honor a per-scenario override `useTimeProfile`.
                bool registering = false;
                cModule *st0 = b->getSubmodule("students", 0);
                if (st0 && st0->hasPar("isRegistering"))
                    registering = st0->par("isRegistering").boolValue();

                // If registering AND the scenario requests the legacy time profile,
                // fall through to the time-based model below. Otherwise derive health
                // from utilization (amplify utilization slightly during registration).
                if (!(registering && useTimeProfile)) {
                    double multiplier = (registering ? 1.25 : 1.0);
                    double health = 100.0 - (utilization * 100.0 * multiplier);
                    if (health < 5.0) health = 5.0;
                    if (health > 100.0) health = 100.0;
                    return health;
                }
            }
        }
    }

    // Fallback: time-based synthetic model (legacy)
    if (stressProfile) {
        if (building == "CS") {
            if (t < 20)        return 95;
            else if (t < 35)   return 82;
            else if (t < 50)   return 60;
            else if (t < 85)   return 18; // sustained collapse
            else if (t < 120)  return 12;
            else if (t < 170)  return 38;
            else if (t < 230)  return 65;
            else               return 82;
        }
        else if (building == "Admin") {
            if (t < 25)        return 95;
            else if (t < 45)   return 80;
            else if (t < 70)   return 52;
            else if (t < 110)  return 22;
            else if (t < 150)  return 15;
            else if (t < 210)  return 42;
            else               return 78;
        }
        else {
            if (t < 35)        return 95;
            else if (t < 60)   return 86;
            else if (t < 90)   return 64;
            else if (t < 130)  return 30;
            else if (t < 180)  return 20;
            else if (t < 240)  return 48;
            else               return 82;
        }
    }

    if (building == "CS") {
        if (t < 30)        return 95;
        else if (t < 50)   return 75;
        else if (t < 70)   return 30; // PEAK — CS bottleneck
        else if (t < 100)  return 20;
        else if (t < 140)  return 55;
        else               return 85;
    }
    else if (building == "Admin") {
        if (t < 45)        return 95;
        else if (t < 75)   return 60;
        else if (t < 110)  return 40;
        else               return 80;
    }
    else {
        if (t < 60)        return 95;
        else if (t < 90)   return 70;
        else if (t < 120)  return 50;
        else               return 88;
    }
}

void BottleneckMonitor::handleMessage(cMessage *msg)
{
    if (msg != sampleTimer) { delete msg; return; }

    sampleCount++;
    double t = simTime().dbl();

    // Per-building health
    double adminH = calculateBuildingHealth("Admin", t);
    double csH    = calculateBuildingHealth("CS",    t);
    double libH   = calculateBuildingHealth("Lib",   t);

    // Overall network health = weighted average (CS is busiest so weighted more)
    double health = (adminH * 0.30) + (csH * 0.50) + (libH * 0.20);

    // Track bottleneck events with debounce: require N consecutive samples
    bool sampleBelow = (health < 50.0);
    if (sampleBelow) networkConsecBelow++; else networkConsecBelow = 0;
    bool currNetworkBN = (networkConsecBelow >= debounceSamples);
    if (currNetworkBN && !prevNetworkBottleneck) {
        bottleneckCount++;
        if (!peakDetected) {
            peakDetected = true;
            peakLoadTime = t - (debounceSamples - 1) * samplingInterval; // approximate start
        }
    }
    if (!sampleBelow && prevNetworkBottleneck && recoveryTime < 0 && health > 70) {
        recoveryTime = t;
        EV << "[MONITOR] Network RECOVERED at t=" << t
           << "s (peak was at t=" << peakLoadTime << "s)\n";
    }

    // Per-building: detect transitions and accumulate durations
    auto handleBuilding = [&](const std::string &name, double h, const std::string &key){
        bool sampleBelowB = (h < 50.0);
        if (sampleBelowB) buildingConsecBelow[key]++; else buildingConsecBelow[key] = 0;
        bool curr = (buildingConsecBelow[key] >= debounceSamples);
        if (curr) buildingBottleneckSampleCount[key]++;  // count every sample in bottleneck state
        if (curr && !prevBuildingBottleneck[key]) {
            buildingBottleneckCount[key]++;
            buildingBottleneckStart[key] = t - (debounceSamples - 1) * samplingInterval;
        }
        if (!curr && prevBuildingBottleneck[key] && buildingBottleneckStart[key] >= 0) {
            buildingBottleneckDuration[key] += t - buildingBottleneckStart[key];
            buildingBottleneckStart[key] = -1.0;
        }
        prevBuildingBottleneck[key] = curr;
    };

    handleBuilding("Admin", adminH, "Admin");
    handleBuilding("CS", csH, "CS");
    handleBuilding("Lib", libH, "Lib");

    // update prev network flag
    prevNetworkBottleneck = currNetworkBN;
    prevNetworkHealth = health;

    emit(networkHealthSignal, health);
    emit(adminHealthSignal,   adminH);
    emit(csHealthSignal,      csH);
    emit(libraryHealthSignal, libH);

    EV << "[MONITOR] t=" << t
       << "s Health=" << health << "%"
       << " [Admin=" << adminH << "% CS=" << csH
       << "% Lib=" << libH << "%]\n";

    if (sampleCount % 15 == 0)
        printNetworkReport();

    scheduleAt(simTime() + samplingInterval, sampleTimer);
}

void BottleneckMonitor::printNetworkReport()
{
    EV << "\n--- NETWORK STATUS @ t=" << simTime() << "s ---\n"
       << "  Total samples   : " << sampleCount << "\n"
    << "  Bottleneck evts : " << bottleneckCount << "\n"
    << "  Admin bottlenecks: " << buildingBottleneckCount["Admin"] << " (dur=" << buildingBottleneckDuration["Admin"] << "s)\n"
    << "  CS bottlenecks  : " << buildingBottleneckCount["CS"] << " (dur=" << buildingBottleneckDuration["CS"] << "s)\n"
    << "  Lib bottlenecks : " << buildingBottleneckCount["Lib"] << " (dur=" << buildingBottleneckDuration["Lib"] << "s)\n"
       << "------------------------------------------\n";
}

void BottleneckMonitor::printOptimizationRecommendations()
{
    EV << "\n==========================================\n";
    EV << "  NET-OPT OPTIMIZATION RECOMMENDATIONS\n";
    EV << "==========================================\n";

    // Find worst building
    std::string worst = "CS";
    int maxBN = buildingBottleneckCount["CS"];
    if (buildingBottleneckCount["Admin"] > maxBN) {
        worst = "Admin"; maxBN = buildingBottleneckCount["Admin"];
    }
    if (buildingBottleneckCount["Lib"] > maxBN) {
        worst = "Library";
    }

    EV << "  WORST BUILDING    : " << worst << " Block\n";
    EV << "  RECOMMENDATIONS   :\n";

    if (buildingBottleneckCount["CS"] > 5) {
        EV << "    [CS BLOCK]  Add 3 more APs (5 -> 8)\n";
        EV << "    [CS BLOCK]  Upgrade to 802.11ax (Wi-Fi 6)\n";
        EV << "    [CS BLOCK]  Dedicate channel 11 to CS only\n";
    }
    if (buildingBottleneckCount["Admin"] > 3) {
        EV << "    [ADMIN]     Stagger registration: CS at 9AM, Admin at 10AM\n";
        EV << "    [ADMIN]     Add 2 more APs near registration counters\n";
    }
    if (peakLoadTime > 0) {
        EV << "  PEAK LOAD TIME    : t=" << peakLoadTime << "s\n";
    }
    if (recoveryTime > 0) {
        EV << "  RECOVERY TIME     : t=" << recoveryTime << "s\n";
        EV << "  CONGESTION WINDOW : " << (recoveryTime - peakLoadTime) << "s\n";
    }
    EV << "  GENERAL           : Implement time-slot based registration\n";
    EV << "==========================================\n";
}

void BottleneckMonitor::finish()
{
    // Close any open per-building bottleneck durations
    double t = simTime().dbl();
    for (auto &p : buildingBottleneckStart) {
        const std::string &key = p.first;
        double start = p.second;
        if (start >= 0.0) {
            buildingBottleneckDuration[key] += t - start;
            buildingBottleneckStart[key] = -1.0;
        }
    }

    printOptimizationRecommendations();

    EV << "\n=================================================\n";
    EV << "  NET-OPT ADVANCED SIMULATION COMPLETE\n";
    EV << "  Total Samples     : " << sampleCount << "\n";
    EV << "  Bottleneck Events : " << bottleneckCount << "\n";
    EV << "  Admin Bottlenecks : " << buildingBottleneckSampleCount["Admin"] << " samples (" << buildingBottleneckDuration["Admin"] << "s)\n";
    EV << "  CS Bottlenecks    : " << buildingBottleneckSampleCount["CS"] << " samples (" << buildingBottleneckDuration["CS"] << "s)\n";
    EV << "  Lib Bottlenecks   : " << buildingBottleneckSampleCount["Lib"] << " samples (" << buildingBottleneckDuration["Lib"] << "s)\n";
    if (peakLoadTime  > 0) EV << "  Peak Load At      : " << peakLoadTime  << "s\n";
    if (recoveryTime  > 0) EV << "  Recovery At       : " << recoveryTime  << "s\n";
    EV << "=================================================\n";

    recordScalar("totalSamples",         sampleCount);
    recordScalar("bottleneckEvents",     bottleneckCount);
    recordScalar("adminBottleneckSamples", buildingBottleneckSampleCount["Admin"]);
    recordScalar("csBottleneckSamples",    buildingBottleneckSampleCount["CS"]);
    recordScalar("libBottleneckSamples",   buildingBottleneckSampleCount["Lib"]);
    recordScalar("adminBottleneckDuration_s", buildingBottleneckDuration["Admin"]);
    recordScalar("csBottleneckDuration_s",    buildingBottleneckDuration["CS"]);
    recordScalar("libBottleneckDuration_s",   buildingBottleneckDuration["Lib"]);
    if (peakLoadTime > 0)  recordScalar("peakLoadTime_s",  peakLoadTime);
    if (recoveryTime > 0)  recordScalar("recoveryTime_s",  recoveryTime);
}
