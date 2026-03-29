// ============================================================
// AccessPoint.cc — Wi-Fi Access Point Implementation
// NET-OPT Project | PDC Spring 2026
// ============================================================
#include "AccessPoint.h"

Define_Module(AccessPoint);

void AccessPoint::initialize()
{
    // Load parameters from NED
    numChannels      = par("numChannels");
    bandwidth        = par("bandwidth").doubleValue() / 1e6; // convert to Mbps
    maxClients       = par("maxClients");
    location         = par("location").stdstringValue();

    // Initialize state
    connectedClients = 0;
    currentLoad      = 0.0;
    isBottleneck     = false;
    droppedPackets   = 0;
    totalPackets     = 0;

    // Register output signals (for Qtenv/Analysis)
    clientCountSignal  = registerSignal("clientCount");
    loadSignal         = registerSignal("apLoad");
    bottleneckSignal   = registerSignal("isBottleneck");
    packetDropSignal   = registerSignal("packetDropRate");
    latencySignal      = registerSignal("apLatency");

    // Schedule periodic statistics collection every 1 second
    statsTimer = new cMessage("statsTimer");
    scheduleAt(simTime() + 1.0, statsTimer);

    EV << "[AP:" << getFullPath() << "] Initialized. "
       << "Bandwidth=" << bandwidth << "Mbps, MaxClients=" << maxClients << endl;
}

void AccessPoint::handleMessage(cMessage *msg)
{
    if (msg == statsTimer) {
        collectStats();
        scheduleAt(simTime() + 1.0, statsTimer);
        return;
    }

    totalPackets++;

    // Check if AP is overloaded (bottleneck condition)
    if (connectedClients >= maxClients || currentLoad >= bandwidth * 0.90) {
        // Drop packet — AP is saturated
        droppedPackets++;
        EV_WARN << "[AP BOTTLENECK] " << getFullPath()
                << " dropping packet! Clients=" << connectedClients
                << "/" << maxClients
                << " Load=" << currentLoad << "/" << bandwidth << " Mbps" << endl;
        emit(packetDropSignal, 1);
        delete msg;
        return;
    }

    // Process packet
    processIncomingPacket(msg);
}

void AccessPoint::processIncomingPacket(cMessage *msg)
{
    // Simulate transmission delay based on load
    double txDelay = calculateLatency();

    // Add load to AP
    double pktSizeMbps = 0.5; // average packet contributes 0.5 Mbps equivalent
    updateLoad(+pktSizeMbps);

    // Forward to wired backhaul after delay
    cMessage *fwdMsg = msg->dup();
    sendDelayed(fwdMsg, txDelay, "backhaul");

    // Schedule load release after transmission
    cMessage *releaseMsg = new cMessage("releaseLoad");
    releaseMsg->setKind(1);
    scheduleAt(simTime() + txDelay + 0.001, releaseMsg);

    delete msg;
}

void AccessPoint::forwardToBackhaul(cMessage *msg)
{
    if (gateSize("backhaul") > 0) {
        send(msg, "backhaul");
    } else {
        delete msg;
    }
}

void AccessPoint::updateLoad(double deltaLoad)
{
    currentLoad += deltaLoad;
    if (currentLoad < 0) currentLoad = 0;
    checkBottleneck();
}

void AccessPoint::checkBottleneck()
{
    // Bottleneck: if load exceeds 80% of capacity OR clients exceed 90% of max
    bool newBottleneck = (currentLoad > bandwidth * 0.80) ||
                         (connectedClients > maxClients * 0.90);

    if (newBottleneck != isBottleneck) {
        isBottleneck = newBottleneck;
        if (isBottleneck) {
            EV_WARN << "[BOTTLENECK DETECTED] AP at " << getFullPath()
                    << " | Load: " << currentLoad << "/" << bandwidth << " Mbps"
                    << " | Clients: " << connectedClients << "/" << maxClients << endl;
            bubble("BOTTLENECK!");
        } else {
            EV << "[BOTTLENECK CLEARED] AP at " << getFullPath() << endl;
        }
        emit(bottleneckSignal, isBottleneck ? 1 : 0);
    }
}

double AccessPoint::calculateLatency()
{
    // Base latency + congestion factor
    double utilization = currentLoad / bandwidth;
    double baseLatency = 0.002; // 2ms base Wi-Fi latency

    if (utilization < 0.5)
        return baseLatency;
    else if (utilization < 0.8)
        return baseLatency * 3;   // moderate congestion: 6ms
    else if (utilization < 0.95)
        return baseLatency * 10;  // heavy congestion: 20ms
    else
        return baseLatency * 25;  // near-saturation: 50ms
}

void AccessPoint::collectStats()
{
    emit(clientCountSignal, connectedClients);
    emit(loadSignal, currentLoad);
    emit(latencySignal, calculateLatency() * 1000); // emit in ms

    double dropRate = (totalPackets > 0) ?
        (double)droppedPackets / totalPackets * 100.0 : 0.0;
    emit(packetDropSignal, dropRate);

    // Print periodic summary
    EV << "[AP STATS] " << getFullPath()
       << " | Clients=" << connectedClients
       << " | Load=" << std::fixed << currentLoad << " Mbps"
       << " | DropRate=" << dropRate << "%"
       << " | Bottleneck=" << (isBottleneck ? "YES" : "no") << endl;
}

bool AccessPoint::acceptClient()
{
    if (connectedClients < maxClients) {
        connectedClients++;
        double perClientLoad = 15.0; // 15 Mbps per registering student
        updateLoad(+perClientLoad);
        return true;
    }
    return false;
}

void AccessPoint::releaseClient()
{
    if (connectedClients > 0) {
        connectedClients--;
        updateLoad(-15.0);
    }
}

void AccessPoint::finish()
{
    double finalDropRate = (totalPackets > 0) ?
        (double)droppedPackets / totalPackets * 100.0 : 0.0;

    EV << "\n========================================\n";
    EV << "AP FINAL REPORT: " << getFullPath() << "\n";
    EV << "  Total Packets   : " << totalPackets   << "\n";
    EV << "  Dropped Packets : " << droppedPackets  << "\n";
    EV << "  Drop Rate       : " << finalDropRate   << "%\n";
    EV << "  Peak Clients    : " << connectedClients << "/" << maxClients << "\n";
    EV << "  Bottleneck?     : " << (isBottleneck ? "YES" : "NO") << "\n";
    EV << "========================================\n";

    recordScalar("totalPackets",   totalPackets);
    recordScalar("droppedPackets", droppedPackets);
    recordScalar("dropRate%",      finalDropRate);
    recordScalar("isBottleneck",   isBottleneck ? 1 : 0);
}
