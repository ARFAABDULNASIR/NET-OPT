// ============================================================
// StudentDevice.cc — ADVANCED v5
// Improvements:
//   1. Department-based staggered arrival times
//   2. Priority classes (senior/junior/freshman)
//   3. Device type affects data rate (phone vs laptop)
//   4. Total wait time tracking
//   5. Priority-aware retry (seniors retry faster)
// ============================================================
#include "StudentDevice.h"

Define_Module(StudentDevice);

void StudentDevice::initialize()
{
    studentId     = par("studentId");
    isRegistering = par("isRegistering");
    dataRate      = par("dataRate").doubleValue() / 1e6;
    deviceType    = par("deviceType").stdstringValue();
    department    = par("department").stdstringValue();
    priority      = par("priority");
    double arrivalWindow = par("arrivalWindow");
    int configuredMaxRetries = par("maxRetries");

    state             = IDLE;
    retryCount        = 0;
    maxRetries        = (configuredMaxRetries >= 0) ? configuredMaxRetries : (5 + (3 - priority)); // seniors get more retries
    registrationStart = 0.0;
    firstAttemptTime  = 0.0;
    lastWaitTime      = 0.0;

    regTimeSignal     = registerSignal("registrationTime");
    connAttemptSignal = registerSignal("connectionAttempts");
    regSuccessSignal  = registerSignal("registrationSuccess");
    waitTimeSignal    = registerSignal("totalWaitTime");
    prioritySignal    = registerSignal("studentPriority");

    connectTimer      = new cMessage("connectTimer");
    registrationTimer = nullptr;
    retryTimer        = nullptr;

    if (isRegistering) {
        double arrival = calculateArrivalTime(arrivalWindow); // Pass the parameter
        scheduleAt(simTime() + arrival, connectTimer);
    }
}

// NEW: Department-based staggered arrival
// CS students arrive first (30-60s), Admin next (45-75s), Library last (60-90s)
// Within each dept, seniors arrive before juniors before freshmen
double StudentDevice::calculateArrivalTime(double arrivalWindow)
{
    // If arrivalWindow is set (e.g., for StressTest), compress all arrivals into that window
    if (arrivalWindow > 0) {
        // All students arrive within [0, arrivalWindow] seconds
        double compressed = (studentId % 120) * (arrivalWindow / 120.0) + uniform(0, 0.1 * arrivalWindow);
        return compressed;
    }

    // Otherwise, use staggered department-based arrival (original behavior)
    double base;
    if      (department == "CS")    base = 30.0;
    else if (department == "Admin") base = 45.0;
    else                            base = 60.0;  // Library

    // Priority offset: seniors go first
    double priorityOffset = (priority - 1) * 8.0; // 0, 8, 16 seconds

    // Spread within same priority class
    double spread = (studentId % 10) * 0.5 + uniform(0, 2.0);

    return base + priorityOffset + spread;
}

void StudentDevice::handleMessage(cMessage *msg)
{
    if (msg == connectTimer) {
        startRegistration();
        return;
    }
    if (msg == retryTimer) {
        retryTimer = nullptr;
        startRegistration();
        return;
    }
    if (msg == registrationTimer) {
        registrationTimer = nullptr;
        EV_WARN << "[STUDENT " << studentId << "] Timeout — retrying\n";
        retry();
        return;
    }
    handleRegistrationResponse(msg);
}

void StudentDevice::startRegistration()
{
    state             = CONNECTING;
    registrationStart = simTime().dbl();
    if (firstAttemptTime == 0.0)
        firstAttemptTime = simTime().dbl();
    retryCount++;

    bubble("Connecting!");

    emit(connAttemptSignal, 1);
    emit(prioritySignal,    priority);

    EV << "[STUDENT " << studentId << "] ["
       << department << "/P" << priority << "/"
       << deviceType << "] Connecting at t=" << simTime() << "\n";

    sendRegistrationRequest();
}

void StudentDevice::sendRegistrationRequest()
{
    if (!gate("wlanOut")->isConnected()) {
        EV_WARN << "[STUDENT " << studentId << "] No connection!\n";
        state = FAILED;
        return;
    }

    cMessage *req = new cMessage("RegRequest");
    req->setKind(100);
    req->addPar("studentId")  = studentId;
    req->addPar("timestamp")  = simTime().dbl();
    req->addPar("priority")   = priority;
    req->addPar("department") = department.c_str();

    state = REGISTERING;
    send(req, "wlanOut");

    // Priority affects timeout: seniors wait slightly longer before giving up, but keep retries moving
    double timeout = 4.0 + (3 - priority) * 2.0; // 4s, 6s, 8s
    registrationTimer = new cMessage("regTimeout");
    scheduleAt(simTime() + timeout, registrationTimer);
}

void StudentDevice::handleRegistrationResponse(cMessage *msg)
{
    if (registrationTimer) {
        cancelAndDelete(registrationTimer);
        registrationTimer = nullptr;
    }

    if (msg->getKind() == 200) {
        double regTime  = simTime().dbl() - registrationStart;
        double waitTime = simTime().dbl() - firstAttemptTime;
        state = DONE;
        lastWaitTime = waitTime;

        emit(regTimeSignal,    regTime);
        emit(regSuccessSignal, 1);
        emit(waitTimeSignal,   waitTime);

        EV << "[STUDENT " << studentId << "] SUCCESS in "
           << regTime << "s, total wait=" << waitTime
           << "s (attempt " << retryCount << ")\n";
        bubble("Registered!");
    } else {
        EV_WARN << "[STUDENT " << studentId
                << "] Failed code=" << msg->getKind() << "\n";
        retry();
    }
    delete msg;
}

void StudentDevice::retry()
{
    if (maxRetries >= 0 && retryCount >= maxRetries) {
        state = FAILED;
        emit(regSuccessSignal, 0);
        lastWaitTime = simTime().dbl() - firstAttemptTime;
        emit(waitTimeSignal, lastWaitTime);
        EV_WARN << "[STUDENT " << studentId << "] GAVE UP after "
                << maxRetries << " retries. Total wait=" << lastWaitTime << "s\n";
        bubble("FAILED!");
        return;
    }

    // Priority-aware backoff: seniors retry faster, but keep the delay short enough to finish within the demo
    double backoffBase = (priority == 1) ? 1.0 : (priority == 2) ? 1.5 : 2.0;
    int backoffStep = (retryCount < 3) ? retryCount : 3;
    double backoff = backoffBase * (1 << backoffStep);
    if (backoff > 5.0)
        backoff = 5.0;

    bubble("Retrying");

    EV << "[STUDENT " << studentId << "] Retry #"
       << retryCount << " in " << backoff << "s\n";

    retryTimer = new cMessage("retryTimer");
    scheduleAt(simTime() + backoff, retryTimer);
}

void StudentDevice::refreshDisplay() const
{
    if (!hasGUI())
        return;

    cDisplayString& disp = const_cast<cDisplayString&>(getDisplayString());
    if (state == CONNECTING || state == REGISTERING) {
        disp.setTagArg("i", 1, "gold");
        disp.setTagArg("i", 2, "60");
        disp.setTagArg("i2", 0, "status/busy");
        disp.setTagArg("t", 0, "CONNECTING");
        disp.setTagArg("t", 1, "t");
        disp.setTagArg("t", 2, "blue");
    }
    else if (state == DONE) {
        disp.setTagArg("i", 1, "green");
        disp.setTagArg("i", 2, "60");
        disp.setTagArg("i2", 0, "status/up");
        disp.setTagArg("t", 0, "REGISTERED");
        disp.setTagArg("t", 1, "t");
        disp.setTagArg("t", 2, "green");
    }
    else if (state == FAILED) {
        disp.setTagArg("i", 1, "red");
        disp.setTagArg("i", 2, "70");
        disp.setTagArg("i2", 0, "status/down");
        disp.setTagArg("t", 0, "FAILED");
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

void StudentDevice::finish()
{
    const char *s = (state == DONE)   ? "SUCCESS"
                  : (state == FAILED) ? "FAILED" : "INCOMPLETE";

    double totalWait = (lastWaitTime > 0)
                     ? lastWaitTime : ((firstAttemptTime > 0) ? simTime().dbl() - firstAttemptTime : 0);

    EV << "[STUDENT " << studentId << "] Final=" << s
       << " dept=" << department
       << " priority=" << priority
       << " attempts=" << retryCount
       << " totalWait=" << totalWait << "s\n";

    recordScalar("finalState",    (state == DONE) ? 1 : 0);
    recordScalar("totalAttempts", retryCount);
    recordScalar("totalWaitTime", totalWait);
    recordScalar("priority",      priority);
}
