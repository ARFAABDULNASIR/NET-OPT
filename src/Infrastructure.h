// ============================================================
// Infrastructure.h — ADVANCED v5
// NEW: Switch congestion tracking, Router QoS, Smart Monitor
// ============================================================
#ifndef __NETOPT_INFRASTRUCTURE_H
#define __NETOPT_INFRASTRUCTURE_H

#include <omnetpp.h>
#include <map>
using namespace omnetpp;

class CampusSwitch : public cSimpleModule
{
  private:
    double      capacity;
    std::string switchType;
    long        framesForwarded;
    double      totalLoad;
    int         congestionDrops;    // NEW
    double      peakLoad;           // NEW

    simsignal_t throughputSignal;
    simsignal_t switchLoadSignal;
    simsignal_t congestionSignal;   // NEW

    cMessage *statsTimer;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
};

class CoreRouter : public cSimpleModule
{
  private:
    double linkSpeed;
    long   packetsRouted;
    double congestionLevel;
    long   priorityPackets;         // NEW: high-priority packets
    double peakCongestion;          // NEW

    simsignal_t routerLoadSignal;
    simsignal_t congestionSignal;
    simsignal_t prioritySignal;     // NEW

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
};

// ============================================================
// ADVANCED BottleneckMonitor — now tracks per-building stats
// and generates optimization recommendations
// ============================================================
class BottleneckMonitor : public cSimpleModule
{
  private:
    double samplingInterval;
    cMessage *sampleTimer;
    int    sampleCount;
    int    bottleneckCount;
    double totalDropRate;
    double totalLatency;

    // NEW: per-building health tracking
    std::map<std::string, int> buildingBottleneckCount;         // transition events
    std::map<std::string, int> buildingBottleneckSampleCount;   // samples in bottleneck state
    std::map<std::string, double> buildingAvgHealth;
    // NEW: track previous bottleneck state and durations
    bool prevNetworkBottleneck;
    std::map<std::string, bool> prevBuildingBottleneck;
    std::map<std::string, double> buildingBottleneckStart;
    std::map<std::string, double> buildingBottleneckDuration;
    // NEW: debounce counters to avoid instantaneous spikes
    int networkConsecBelow;
    std::map<std::string, int> buildingConsecBelow;
    int debounceSamples; // number of consecutive samples required

    // NEW: network health phases
    double peakLoadTime;
    double recoveryTime;
    bool   peakDetected;

    simsignal_t networkHealthSignal;
    simsignal_t adminHealthSignal;      // NEW
    simsignal_t csHealthSignal;         // NEW
    simsignal_t libraryHealthSignal;    // NEW
    double prevNetworkHealth;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    void printNetworkReport();
    void printOptimizationRecommendations(); // NEW
    double calculateBuildingHealth(const std::string& building, double t);
};

#endif
