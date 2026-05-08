// ============================================================
// StudentDevice.h — ADVANCED v5
// NEW: Department-based arrival, device types, priority classes
// ============================================================
#ifndef __NETOPT_STUDENTDEVICE_H
#define __NETOPT_STUDENTDEVICE_H

#include <omnetpp.h>
using namespace omnetpp;

class StudentDevice : public cSimpleModule
{
  private:
    int         studentId;
    bool        isRegistering;
    double      dataRate;
    std::string deviceType;
    std::string department;     // NEW: CS, Admin, Library
    int         priority;       // NEW: 1=senior, 2=junior, 3=freshman

    enum State { IDLE, CONNECTING, REGISTERING, DONE, FAILED };
    State  state;
    int    retryCount;
    int    maxRetries;
    double registrationStart;
    double firstAttemptTime;    // NEW: track total wait time
    double lastWaitTime;

    cMessage *connectTimer;
    cMessage *registrationTimer;
    cMessage *retryTimer;

    simsignal_t regTimeSignal;
    simsignal_t connAttemptSignal;
    simsignal_t regSuccessSignal;
    simsignal_t waitTimeSignal;     // NEW
    simsignal_t prioritySignal;     // NEW

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void refreshDisplay() const override;
    virtual void finish() override;

    void startRegistration();
    void sendRegistrationRequest();
    void handleRegistrationResponse(cMessage *msg);
    void retry();
    double calculateArrivalTime(double arrivalWindow);  // NEW: department-based staggering with burst support
};

#endif
