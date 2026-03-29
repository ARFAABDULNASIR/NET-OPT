// ============================================================
// CampusSwitch.h / CoreRouter.h — Network Infrastructure
// NET-OPT Project | PDC Spring 2026
// ============================================================
#ifndef __NETOPT_INFRASTRUCTURE_H
#define __NETOPT_INFRASTRUCTURE_H

#include <omnetpp.h>
using namespace omnetpp;

// -------------------------------------------------------
// CampusSwitch: Layer-2/3 switch — forwards all frames
// -------------------------------------------------------
class CampusSwitch : public cSimpleModule
{
  private:
    double capacity;
    std::string switchType;
    long  framesForwarded;
    double totalLoad;

    simsignal_t throughputSignal;
    simsignal_t switchLoadSignal;

    cMessage *statsTimer;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
};

// -------------------------------------------------------
// CoreRouter: Routes packets between buildings and internet
// -------------------------------------------------------
class CoreRouter : public cSimpleModule
{
  private:
    double linkSpeed;
    long  packetsRouted;
    double congestionLevel;

    simsignal_t routerLoadSignal;
    simsignal_t congestionSignal;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
};

// -------------------------------------------------------
// BottleneckMonitor: Collects global stats, generates report
// -------------------------------------------------------
class BottleneckMonitor : public cSimpleModule
{
  private:
    double samplingInterval;
    cMessage *sampleTimer;
    int  sampleCount;

    // Aggregated metrics
    double totalDropRate;
    double totalLatency;
    int    bottleneckCount;

    simsignal_t networkHealthSignal;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    void printNetworkReport();
};

#endif
