// ============================================================
// StudentDevice.h — Student Laptop/Phone Module
// NET-OPT Project | PDC Spring 2026
// ============================================================
#ifndef __NETOPT_STUDENTDEVICE_H
#define __NETOPT_STUDENTDEVICE_H

#include <omnetpp.h>
using namespace omnetpp;

class StudentDevice : public cSimpleModule
{
  private:
    int    studentId;
    bool   isRegistering;
    double dataRate;       // Mbps
    std::string deviceType;

    // State
    enum State { IDLE, CONNECTING, REGISTERING, DONE, FAILED };
    State state;

    int    retryCount;
    int    maxRetries;
    double registrationStart;

    // Self-message timers
    cMessage *connectTimer;
    cMessage *registrationTimer;
    cMessage *retryTimer;

    // Statistics
    simsignal_t regTimeSignal;
    simsignal_t connAttemptSignal;
    simsignal_t regSuccessSignal;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

    void startRegistration();
    void sendRegistrationRequest();
    void handleRegistrationResponse(cMessage *msg);
    void retry();
};

#endif
