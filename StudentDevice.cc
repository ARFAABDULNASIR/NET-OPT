// ============================================================
// StudentDevice.cc — Student Device Implementation
// NET-OPT Project | PDC Spring 2026
// ============================================================
#include "StudentDevice.h"

Define_Module(StudentDevice);

void StudentDevice::initialize()
{
    studentId    = par("studentId");
    isRegistering= par("isRegistering");
    dataRate     = par("dataRate").doubleValue() / 1e6;
    deviceType   = par("deviceType").stdstringValue();

    state      = IDLE;
    retryCount = 0;
    maxRetries = 5;
    registrationStart = 0;

    // Register signals
    regTimeSignal    = registerSignal("registrationTime");
    connAttemptSignal= registerSignal("connectionAttempts");
    regSuccessSignal = registerSignal("registrationSuccess");

    connectTimer      = new cMessage("connectTimer");
    registrationTimer = nullptr;
    retryTimer        = nullptr;

    if (isRegistering) {
        // Stagger student connections: simulate wave of arrivals
        // Students arrive between t=5s and t=60s (registration opens)
        double arrivalTime = 5.0 + studentId * 0.3 + uniform(0, 2.0);
        scheduleAt(simTime() + arrivalTime, connectTimer);
    }
}

void StudentDevice::handleMessage(cMessage *msg)
{
    if (msg == connectTimer) {
        startRegistration();
        return;
    }
    if (msg == retryTimer) {
        retry();
        return;
    }
    if (msg == registrationTimer) {
        // Registration timed out — treat as failure
        EV_WARN << "[STUDENT " << studentId << "] Registration TIMEOUT! Retrying...\n";
        retry();
        return;
    }

    // Incoming message from network
    handleRegistrationResponse(msg);
}

void StudentDevice::startRegistration()
{
    state = CONNECTING;
    registrationStart = simTime().dbl();
    retryCount++;
    emit(connAttemptSignal, 1);

    EV << "[STUDENT " << studentId << "] Connecting to campus Wi-Fi at t="
       << simTime() << "s\n";

    sendRegistrationRequest();
}

void StudentDevice::sendRegistrationRequest()
{
    // Create a registration request packet
    cMessage *req = new cMessage("RegRequest");
    req->setKind(100); // 100 = registration request
    req->addPar("studentId") = studentId;
    req->addPar("timestamp") = simTime().dbl();

    state = REGISTERING;

    // Send through wireless link
    if (gate("wlan")->isConnected()) {
        send(req, "wlan");

        // Set a timeout for the registration
        registrationTimer = new cMessage("regTimeout");
        scheduleAt(simTime() + 10.0, registrationTimer); // 10s timeout
    } else {
        EV_WARN << "[STUDENT " << studentId << "] No wireless link available!\n";
        delete req;
        state = FAILED;
    }
}

void StudentDevice::handleRegistrationResponse(cMessage *msg)
{
    if (registrationTimer) {
        cancelAndDelete(registrationTimer);
        registrationTimer = nullptr;
    }

    if (msg->getKind() == 200) { // 200 = success
        double regTime = simTime().dbl() - registrationStart;
        state = DONE;
        emit(regTimeSignal, regTime);
        emit(regSuccessSignal, 1);

        EV << "[STUDENT " << studentId << "] Registration SUCCESS in "
           << regTime << "s (after " << retryCount << " attempt(s))\n";
        bubble("Registered!");
    } else {
        EV_WARN << "[STUDENT " << studentId << "] Registration FAILED\n";
        retry();
    }
    delete msg;
}

void StudentDevice::retry()
{
    if (retryCount >= maxRetries) {
        state = FAILED;
        emit(regSuccessSignal, 0);
        EV_WARN << "[STUDENT " << studentId << "] GAVE UP after "
                << maxRetries << " retries. Registration FAILED.\n";
        bubble("FAILED!");
        return;
    }

    // Exponential backoff
    double backoff = 2.0 * (1 << retryCount); // 2, 4, 8, 16, 32 seconds
    EV << "[STUDENT " << studentId << "] Retry #" << retryCount
       << " in " << backoff << "s\n";

    retryTimer = new cMessage("retryTimer");
    scheduleAt(simTime() + backoff, retryTimer);
}

void StudentDevice::finish()
{
    std::string stateStr;
    switch(state) {
        case DONE:   stateStr = "SUCCESS"; break;
        case FAILED: stateStr = "FAILED";  break;
        default:     stateStr = "INCOMPLETE"; break;
    }

    EV << "[STUDENT " << studentId << "] Final state: " << stateStr
       << " | Attempts: " << retryCount << "\n";

    recordScalar("finalState",     (state == DONE) ? 1 : 0);
    recordScalar("totalAttempts",  retryCount);
}
