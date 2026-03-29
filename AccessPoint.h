// ============================================================
// AccessPoint.h — Wi-Fi Access Point Module
// NET-OPT Project | PDC Spring 2026
// ============================================================
#ifndef __NETOPT_ACCESSPOINT_H
#define __NETOPT_ACCESSPOINT_H

#include <omnetpp.h>
#include <map>
#include <queue>

using namespace omnetpp;

class AccessPoint : public cSimpleModule
{
  private:
    // Parameters
    int    numChannels;
    double bandwidth;       // in Mbps
    int    maxClients;
    std::string location;

    // State
    int    connectedClients;
    double currentLoad;     // Mbps currently being used
    bool   isBottleneck;

    // Statistics (signals)
    simsignal_t clientCountSignal;
    simsignal_t loadSignal;
    simsignal_t bottleneckSignal;
    simsignal_t packetDropSignal;
    simsignal_t latencySignal;

    // Packet queue (simulates AP buffer)
    cQueue packetQueue;
    int    droppedPackets;
    int    totalPackets;

    // Self-messages
    cMessage *statsTimer;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

    void processIncomingPacket(cMessage *msg);
    void forwardToBackhaul(cMessage *msg);
    void updateLoad(double deltaLoad);
    void checkBottleneck();
    void collectStats();
    double calculateLatency();

  public:
    // Called by students to "associate"
    bool acceptClient();
    void releaseClient();
    double getCurrentLoad() const { return currentLoad; }
    bool getBottleneckStatus() const { return isBottleneck; }
};

#endif
