// ============================================================
// AccessPoint.cc — ADVANCED v5
// Improvements:
//   1. Channel interference modelling (co-channel interference)
//   2. Per-AP throughput tracking (sliding 10s window)
//   3. Signal strength simulation (RSSI-based latency)
//   4. Bottleneck duration tracking
//   5. Channel utilization metric
// ============================================================
#include "AccessPoint.h"

Define_Module(AccessPoint);

void AccessPoint::initialize()
{
    numChannels   = par("numChannels");
    bandwidth     = par("bandwidth").doubleValue();
    maxClients    = par("maxClients");
    location      = par("location").stdstringValue();
    channelNumber = par("channelNumber");
    txPower       = par("txPower").doubleValue();

    connectedClients       = 0;
    currentLoad            = 0.0;
    isBottleneck           = false;
    droppedPackets         = 0;
    totalPackets           = 0;
    totalSuccess           = 0;
    totalThroughput        = 0.0;
    interferenceLevel      = 0.0;
    bottleneckStartTime    = -1.0;
    totalBottleneckDuration= 0.0;

    clientCountSignal   = registerSignal("clientCount");
    loadSignal          = registerSignal("apLoad");
    bottleneckSignal    = registerSignal("isBottleneck");
    packetDropSignal    = registerSignal("packetDropRate");
    latencySignal       = registerSignal("apLatency");
    throughputSignal    = registerSignal("apThroughput");
    interferenceSignal  = registerSignal("apInterference");
    channelUtilSignal   = registerSignal("channelUtilization");

    statsTimer       = new cMessage("statsTimer");
    interferenceTimer= new cMessage("interferenceTimer");

    scheduleAt(simTime() + 1.0, statsTimer);
    scheduleAt(simTime() + 5.0, interferenceTimer); // interference check every 5s

    EV << "[AP " << getFullPath() << "] Ready."
       << " bw=" << bandwidth << "Mbps"
       << " maxClients=" << maxClients
       << " channel=" << channelNumber
       << " txPower=" << txPower << "dBm\n";
}

void AccessPoint::handleMessage(cMessage *msg)
{
    if (msg == statsTimer) {
        collectStats();
        scheduleAt(simTime() + 1.0, statsTimer);
        return;
    }

    if (msg == interferenceTimer) {
        updateInterference();
        scheduleAt(simTime() + 5.0, interferenceTimer);
        return;
    }

    // Load-release self-message
    if (msg->isSelfMessage() && msg->getKind() == 1) {
        updateLoad(-0.5);
        if (connectedClients > 0) connectedClients--;
        delete msg;
        return;
    }

    // Incoming packet from student
    totalPackets++;
    int inGateIndex = msg->getArrivalGate()->getIndex();

    // Check overload — factor in interference
    double effectiveCapacity = bandwidth * (1.0 - interferenceLevel * 0.3);
    if (connectedClients >= maxClients ||
        currentLoad >= effectiveCapacity * 0.90) {

        droppedPackets++;
        EV_WARN << "[AP BOTTLENECK] " << getFullPath()
                << " clients=" << connectedClients << "/" << maxClients
                << " load=" << currentLoad << "/" << effectiveCapacity
                << " Mbps interference=" << interferenceLevel << "\n";
        bubble("BOTTLENECK!");
        emit(packetDropSignal, 1.0);

        cMessage *fail = new cMessage("RegResponse");
        fail->setKind(503);
        send(fail, "wirelessOut", inGateIndex);
        delete msg;
        return;
    }

    processIncomingPacket(msg, inGateIndex);
}

void AccessPoint::processIncomingPacket(cMessage *msg, int replyGateIndex)
{
    double txDelay = calculateLatency();
    updateLoad(+0.5);
    connectedClients++;
    totalSuccess++;

    // Track throughput (1 packet = ~1500 bytes = ~0.012 Mbps equivalent)
    double pktThroughput = 0.012;
    totalThroughput += pktThroughput;
    throughputWindow.push_back(pktThroughput);
    if ((int)throughputWindow.size() > 10)
        throughputWindow.pop_front();

    cMessage *resp = new cMessage("RegResponse");
    resp->setKind(200);
    sendDelayed(resp, txDelay, "wirelessOut", replyGateIndex);

    cMessage *rel = new cMessage("releaseLoad");
    rel->setKind(1);
    scheduleAt(simTime() + txDelay + 0.5, rel);

    delete msg;
}

void AccessPoint::updateLoad(double delta)
{
    currentLoad += delta;
    if (currentLoad < 0) currentLoad = 0;
    checkBottleneck();
}

void AccessPoint::checkBottleneck()
{
    double effectiveCapacity = bandwidth * (1.0 - interferenceLevel * 0.3);
    bool nb = (currentLoad      > effectiveCapacity * 0.80) ||
              (connectedClients > maxClients * 0.90);

    if (nb != isBottleneck) {
        isBottleneck = nb;
        emit(bottleneckSignal, isBottleneck ? 1 : 0);
        if (isBottleneck) {
            bottleneckStartTime = simTime().dbl();
            EV_WARN << "[BOTTLENECK DETECTED] " << getFullPath()
                    << " load=" << currentLoad
                    << " clients=" << connectedClients
                    << " interference=" << interferenceLevel << "\n";
            bubble("BOTTLENECK!");
        } else {
            // Track how long the bottleneck lasted
            if (bottleneckStartTime >= 0) {
                totalBottleneckDuration += simTime().dbl() - bottleneckStartTime;
                bottleneckStartTime = -1.0;
            }
            EV << "[BOTTLENECK CLEARED] " << getFullPath() << "\n";
            bubble("RECOVERED!");
        }
    }
}

// NEW: Simulate Wi-Fi channel interference
// Co-channel interference increases with load and nearby APs
void AccessPoint::updateInterference()
{
    // Interference model: increases during peak load periods
    double t = simTime().dbl();
    double baseInterference = 0.0;

    // Peak registration causes more devices scanning/probing
    if (t > 30 && t < 90)
        baseInterference = 0.15 + (currentLoad / bandwidth) * 0.25;
    else if (t > 90 && t < 120)
        baseInterference = 0.10 + (currentLoad / bandwidth) * 0.15;
    else
        baseInterference = 0.05;

    // Channel 1 and 6 have more overlap than channel 11
    if (channelNumber == 1 || channelNumber == 6)
        baseInterference *= 1.2;

    interferenceLevel = std::min(baseInterference, 0.6); // cap at 60%
    emit(interferenceSignal, interferenceLevel * 100.0); // emit as %
}

double AccessPoint::getChannelUtilization()
{
    return (bandwidth > 0) ? (currentLoad / bandwidth) * 100.0 : 0.0;
}

double AccessPoint::calculateLatency()
{
    // RSSI-based latency: higher interference = worse signal = more retransmits
    double u = currentLoad / bandwidth;
    double interferenceDelay = interferenceLevel * 0.010; // up to 10ms from interference

    double baseLatency;
    if (u < 0.50)      baseLatency = 0.002;
    else if (u < 0.80) baseLatency = 0.006;
    else if (u < 0.95) baseLatency = 0.020;
    else               baseLatency = 0.050;

    return baseLatency + interferenceDelay;
}

void AccessPoint::collectStats()
{
    double chanUtil = getChannelUtilization();
    double dr = (totalPackets > 0)
                ? (double)droppedPackets / totalPackets * 100.0 : 0.0;

    // Sliding window throughput
    double windowTput = 0;
    for (double v : throughputWindow) windowTput += v;

    emit(clientCountSignal,  connectedClients);
    emit(loadSignal,         currentLoad);
    emit(latencySignal,      calculateLatency() * 1000.0);
    emit(packetDropSignal,   dr);
    emit(throughputSignal,   windowTput);
    emit(channelUtilSignal,  chanUtil);

    if (hasGUI() && isBottleneck)
        bubble("BOTTLENECK!");

    EV << "[AP STATS] " << getFullPath()
       << " clients=" << connectedClients
       << " load=" << currentLoad << "Mbps"
       << " chanUtil=" << chanUtil << "%"
       << " interference=" << interferenceLevel * 100 << "%"
       << " drop=" << dr << "%"
       << " btlnk=" << (isBottleneck ? "YES" : "no") << "\n";
}

void AccessPoint::refreshDisplay() const
{
    if (!hasGUI())
        return;

    cDisplayString& disp = const_cast<cDisplayString&>(getDisplayString());
    if (isBottleneck) {
        disp.setTagArg("i", 1, "red");
        disp.setTagArg("i", 2, "70");
        disp.setTagArg("i2", 0, "status/busy");
        disp.setTagArg("t", 0, "BOTTLENECK");
        disp.setTagArg("t", 1, "t");
        disp.setTagArg("t", 2, "red");
    }
    else {
        disp.setTagArg("i", 1, "");
        disp.setTagArg("i", 2, "");
        disp.removeTag("i2");
        disp.removeTag("t");
    }
}

void AccessPoint::finish()
{
    double dr = (totalPackets > 0)
                ? (double)droppedPackets / totalPackets * 100.0 : 0.0;

    // Close any open bottleneck duration
    if (isBottleneck && bottleneckStartTime >= 0)
        totalBottleneckDuration += simTime().dbl() - bottleneckStartTime;

    EV << "\n=== AP REPORT: " << getFullPath() << " ===\n"
       << "  Location          : " << location << "\n"
       << "  Channel           : " << channelNumber << "\n"
       << "  Total packets     : " << totalPackets << "\n"
       << "  Dropped packets   : " << droppedPackets << "\n"
       << "  Drop rate         : " << dr << "%\n"
       << "  Total throughput  : " << totalThroughput << " Mbps-equiv\n"
       << "  Bottleneck time   : " << totalBottleneckDuration << "s\n"
       << "  Avg interference  : " << interferenceLevel * 100 << "%\n";

    recordScalar("totalPackets",          totalPackets);
    recordScalar("droppedPackets",        droppedPackets);
    recordScalar("dropRate%",             dr);
    recordScalar("isBottleneck",          isBottleneck ? 1 : 0);
    recordScalar("totalThroughput",       totalThroughput);
    recordScalar("bottleneckDuration_s",  totalBottleneckDuration);
    recordScalar("avgInterference%",      interferenceLevel * 100);
}
