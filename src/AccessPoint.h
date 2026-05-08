// ============================================================
// AccessPoint.h — ADVANCED v5
// NEW: Channel interference, smart load balancing signal,
//      per-AP throughput tracking, signal strength simulation
// ============================================================
#ifndef __NETOPT_ACCESSPOINT_H
#define __NETOPT_ACCESSPOINT_H

#include <omnetpp.h>
#include <deque>
using namespace omnetpp;

class AccessPoint : public cSimpleModule
{
  private:
    // Parameters
    int         numChannels;
    double      bandwidth;
    int         maxClients;
    std::string location;
    int         channelNumber;      // NEW: which Wi-Fi channel (1,6,11)
    double      txPower;            // NEW: transmit power in dBm

    // State
    int         connectedClients;
    double      currentLoad;
    bool        isBottleneck;
    int         droppedPackets;
    int         totalPackets;
    int         totalSuccess;

    // NEW: throughput tracking (sliding window)
    double      totalThroughput;
    std::deque<double> throughputWindow; // last 10 seconds

    // NEW: interference level from neighboring APs (0.0 - 1.0)
    double      interferenceLevel;

    // NEW: consecutive bottleneck duration
    double      bottleneckStartTime;
    double      totalBottleneckDuration;

    // Signals
    simsignal_t clientCountSignal;
    simsignal_t loadSignal;
    simsignal_t bottleneckSignal;
    simsignal_t packetDropSignal;
    simsignal_t latencySignal;
    simsignal_t throughputSignal;       // NEW
    simsignal_t interferenceSignal;     // NEW
    simsignal_t channelUtilSignal;      // NEW

    cMessage *statsTimer;
    cMessage *interferenceTimer;        // NEW

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void refreshDisplay() const override;
    virtual void finish() override;

    void processIncomingPacket(cMessage *msg, int replyGateIndex);
    void updateLoad(double delta);
    void checkBottleneck();
    void collectStats();
    double calculateLatency();
    void updateInterference();          // NEW
    double getChannelUtilization();     // NEW

  public:
    bool   getBottleneckStatus() const { return isBottleneck; }
    double getCurrentLoad()      const { return currentLoad; }
    int    getConnectedClients() const { return connectedClients; }
    int    getChannelNumber()    const { return channelNumber; }
};

#endif
