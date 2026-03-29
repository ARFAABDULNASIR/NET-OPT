// ============================================================
// RegistrationServer.cc — University Registration Server
// NET-OPT Project | PDC Spring 2026
// ============================================================
#include "RegistrationServer.h"

Define_Module(RegistrationServer);

void RegistrationServer::initialize()
{
    maxConcurrentSessions = par("maxConcurrentSessions");
    processingTime        = par("processingTime").doubleValue();
    serverCapacity        = par("serverCapacity").doubleValue() / 1e6;

    activeSessions   = 0;
    totalServed      = 0;
    totalRejected    = 0;
    totalResponseTime= 0;

    sessionCountSignal  = registerSignal("activeSessions");
    queueLengthSignal   = registerSignal("queueLength");
    responseTimeSignal  = registerSignal("responseTime");
    rejectionRateSignal = registerSignal("rejectionRate");
    serverLoadSignal    = registerSignal("serverLoad");

    serviceTimer = new cMessage("serviceTimer");
    scheduleAt(simTime() + processingTime, serviceTimer);

    EV << "[SERVER] Registration Server initialized. "
       << "MaxSessions=" << maxConcurrentSessions
       << ", ProcessingTime=" << processingTime*1000 << "ms\n";
}

void RegistrationServer::handleMessage(cMessage *msg)
{
    if (msg == serviceTimer) {
        serveNextInQueue();
        scheduleAt(simTime() + processingTime, serviceTimer);
        return;
    }

    // Incoming registration request
    processRequest(msg);

    // Emit stats
    emit(sessionCountSignal, activeSessions);
    emit(queueLengthSignal,  (int)requestQueue.getLength());
    double load = (double)activeSessions / maxConcurrentSessions * 100.0;
    emit(serverLoadSignal, load);

    EV << "[SERVER] Active sessions=" << activeSessions
       << "/" << maxConcurrentSessions
       << " | Queue=" << requestQueue.getLength()
       << " | Load=" << load << "%\n";
}

void RegistrationServer::processRequest(cMessage *req)
{
    if (activeSessions < maxConcurrentSessions) {
        // Accept immediately
        activeSessions++;
        double respTime = processingTime + (activeSessions * 0.005); // load factor
        sendDelayed(req->dup(), respTime, "link");
        emit(responseTimeSignal, respTime * 1000); // ms
        totalServed++;
        totalResponseTime += respTime;
        delete req;
    } else if ((int)requestQueue.getLength() < 100) {
        // Queue the request
        requestQueue.insert(req);
        EV_WARN << "[SERVER] Session limit reached. Request queued. "
                << "Queue length=" << requestQueue.getLength() << "\n";
    } else {
        // Queue full — reject
        totalRejected++;
        EV_WARN << "[SERVER] Queue FULL! Rejecting request. "
                << "Total rejected=" << totalRejected << "\n";

        // Send failure response
        cMessage *fail = new cMessage("RegResponse");
        fail->setKind(503); // 503 = server unavailable
        send(fail, "link");
        delete req;

        double rejRate = (double)totalRejected / (totalServed + totalRejected) * 100;
        emit(rejectionRateSignal, rejRate);
    }
}

void RegistrationServer::serveNextInQueue()
{
    if (!requestQueue.isEmpty()) {
        cMessage *req = (cMessage *)requestQueue.pop();
        activeSessions++;

        double respTime = processingTime;
        cMessage *resp = new cMessage("RegResponse");
        resp->setKind(200); // success
        sendDelayed(resp, respTime, "link");
        emit(responseTimeSignal, respTime * 1000);
        totalServed++;
        activeSessions--;
    } else if (activeSessions > 0) {
        activeSessions--;
    }
}

void RegistrationServer::sendResponse(cMessage *req, bool success)
{
    cMessage *resp = new cMessage("RegResponse");
    resp->setKind(success ? 200 : 503);
    send(resp, "link");
    delete req;
}

void RegistrationServer::finish()
{
    double avgRespTime = (totalServed > 0) ? totalResponseTime / totalServed : 0;
    double rejRate     = (totalServed + totalRejected > 0) ?
        (double)totalRejected / (totalServed + totalRejected) * 100 : 0;

    EV << "\n============================================\n";
    EV << "REGISTRATION SERVER FINAL REPORT\n";
    EV << "  Total Served   : " << totalServed    << "\n";
    EV << "  Total Rejected : " << totalRejected   << "\n";
    EV << "  Rejection Rate : " << rejRate        << "%\n";
    EV << "  Avg Resp Time  : " << avgRespTime*1000 << " ms\n";
    EV << "============================================\n";

    recordScalar("totalServed",       totalServed);
    recordScalar("totalRejected",     totalRejected);
    recordScalar("rejectionRate%",    rejRate);
    recordScalar("avgResponseTime_ms",avgRespTime * 1000);
}
