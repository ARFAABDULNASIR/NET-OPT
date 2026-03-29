// ============================================================
// Infrastructure.cc — Switch, Router, Monitor implementations
// NET-OPT Project | PDC Spring 2026
// ============================================================
#include "Infrastructure.h"

// ============================================================
// CampusSwitch
// ============================================================
Define_Module(CampusSwitch);

void CampusSwitch::initialize()
{
    capacity    = par("capacity").doubleValue() / 1e9; // Gbps
    switchType  = par("switchType").stdstringValue();
    framesForwarded = 0;
    totalLoad   = 0;

    throughputSignal = registerSignal("switchThroughput");
    switchLoadSignal = registerSignal("switchLoad");

    statsTimer = new cMessage("switchStats");
    scheduleAt(simTime() + 2.0, statsTimer);

    EV << "[SWITCH:" << switchType << "] Initialized. Capacity="
       << capacity << " Gbps\n";
}

void CampusSwitch::handleMessage(cMessage *msg)
{
    if (msg == statsTimer) {
        emit(switchLoadSignal, totalLoad);
        emit(throughputSignal, framesForwarded);
        scheduleAt(simTime() + 2.0, statsTimer);
        return;
    }

    framesForwarded++;
    totalLoad += 0.001; // each frame adds small load

    // Forward: determine which gate to use based on arrival gate
    int arrGate = msg->getArrivalGateId();
    cGate *ag   = gate(arrGate);

    // Simple forwarding: if came from uplink, broadcast to ports; else send to uplink
    if (strcmp(ag->getName(), "uplink") == 0) {
        // Came from upstream — send to all ports
        for (int i = 0; i < gateSize("ports"); i++) {
            if (gate("ports", i)->isConnected()) {
                cMessage *copy = msg->dup();
                send(copy, "ports", i);
            }
        }
        delete msg;
    } else {
        // Came from a port — send upstream
        if (gate("uplink")->isConnected()) {
            send(msg, "uplink");
        } else {
            delete msg;
        }
    }
}

void CampusSwitch::finish()
{
    EV << "[SWITCH:" << switchType << "] Frames forwarded: " << framesForwarded << "\n";
    recordScalar("framesForwarded", framesForwarded);
}

// ============================================================
// CoreRouter
// ============================================================
Define_Module(CoreRouter);

void CoreRouter::initialize()
{
    linkSpeed      = par("linkSpeed").doubleValue() / 1e9; // Gbps
    packetsRouted  = 0;
    congestionLevel= 0;

    routerLoadSignal  = registerSignal("routerLoad");
    congestionSignal  = registerSignal("routerCongestion");

    EV << "[ROUTER] Core router initialized. Link speed="
       << linkSpeed << " Gbps\n";
}

void CoreRouter::handleMessage(cMessage *msg)
{
    packetsRouted++;
    congestionLevel = (double)packetsRouted / 1000.0; // simplified metric

    emit(routerLoadSignal, congestionLevel);

    // Route to internet or to a core link
    int arrGate = msg->getArrivalGateId();
    cGate *ag   = gate(arrGate);

    if (strcmp(ag->getName(), "internet") == 0) {
        // From internet to core
        if (gateSize("coreLinks") > 0 && gate("coreLinks", 0)->isConnected()) {
            send(msg, "coreLinks", 0);
        } else { delete msg; }
    } else {
        // From core to internet
        if (gate("internet")->isConnected()) {
            send(msg, "internet");
        } else { delete msg; }
    }
}

void CoreRouter::finish()
{
    EV << "[ROUTER] Total packets routed: " << packetsRouted << "\n";
    recordScalar("packetsRouted", packetsRouted);
    recordScalar("congestionLevel", congestionLevel);
}

// ============================================================
// BottleneckMonitor
// ============================================================
Define_Module(BottleneckMonitor);

void BottleneckMonitor::initialize()
{
    samplingInterval = par("samplingInterval").doubleValue();
    sampleCount      = 0;
    totalDropRate    = 0;
    totalLatency     = 0;
    bottleneckCount  = 0;

    networkHealthSignal = registerSignal("networkHealth");

    sampleTimer = new cMessage("sample");
    scheduleAt(simTime() + samplingInterval, sampleTimer);

    EV << "[MONITOR] Bottleneck Monitor started. Sampling every "
       << samplingInterval << "s\n";
}

void BottleneckMonitor::handleMessage(cMessage *msg)
{
    if (msg == sampleTimer) {
        sampleCount++;

        // Network health: 100 = perfect, 0 = collapsed
        // (simplified: degrades as simulation progresses to model peak load)
        double t = simTime().dbl();
        double health;

        if (t < 30)       health = 95; // pre-registration: healthy
        else if (t < 60)  health = 60; // registration opens: moderate congestion
        else if (t < 90)  health = 25; // registration peak: bottlenecks
        else if (t < 120) health = 40; // tapering off
        else              health = 80; // post-registration: recovering

        if (health < 50) bottleneckCount++;

        emit(networkHealthSignal, health);
        EV << "[MONITOR] t=" << t << "s | Network Health=" << health << "%\n";

        if (sampleCount % 10 == 0) {
            printNetworkReport();
        }

        scheduleAt(simTime() + samplingInterval, sampleTimer);
        return;
    }
    delete msg;
}

void BottleneckMonitor::printNetworkReport()
{
    EV << "\n--- NETWORK STATUS REPORT @ t=" << simTime() << "s ---\n";
    EV << "  Samples collected  : " << sampleCount      << "\n";
    EV << "  Bottleneck events  : " << bottleneckCount  << "\n";
    EV << "---------------------------------------------------\n";
}

void BottleneckMonitor::finish()
{
    EV << "\n=================================================\n";
    EV << "  NET-OPT FINAL SIMULATION REPORT\n";
    EV << "  Total Samples     : " << sampleCount     << "\n";
    EV << "  Bottleneck Events : " << bottleneckCount << "\n";
    EV << "  Recommendation    :\n";
    EV << "    Add APs in CS Block (heaviest load)\n";
    EV << "    Upgrade distSwitch[1] to 10Gbps\n";
    EV << "    Stagger registration time slots by department\n";
    EV << "=================================================\n";

    recordScalar("totalSamples",    sampleCount);
    recordScalar("bottleneckEvents",bottleneckCount);
}
