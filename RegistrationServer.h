// ============================================================
// RegistrationServer.h — University Registration Server
// NET-OPT Project | PDC Spring 2026
// ============================================================
#ifndef __NETOPT_REGISTRATIONSERVER_H
#define __NETOPT_REGISTRATIONSERVER_H

#include <omnetpp.h>
#include <queue>
using namespace omnetpp;

class RegistrationServer : public cSimpleModule
{
  private:
    int    maxConcurrentSessions;
    double processingTime;
    double serverCapacity;   // Mbps

    // State
    int    activeSessions;
    int    totalServed;
    int    totalRejected;
    double totalResponseTime;

    // Request queue
    cQueue requestQueue;

    // Self-messages
    cMessage *serviceTimer;

    // Statistics signals
    simsignal_t sessionCountSignal;
    simsignal_t queueLengthSignal;
    simsignal_t responseTimeSignal;
    simsignal_t rejectionRateSignal;
    simsignal_t serverLoadSignal;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

    void processRequest(cMessage *req);
    void serveNextInQueue();
    void sendResponse(cMessage *req, bool success);
};

#endif
