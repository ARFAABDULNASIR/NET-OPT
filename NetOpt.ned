// ============================================================
// NET-OPT: Campus Wi-Fi Bottleneck Simulation
// PDC Spring 2026 - Section B
// Tool: OMNeT++ (NED Language)
// Domain: Campus IT
// ============================================================

package src;

simple AccessPoint
{
    parameters:
        int numChannels = default(3);
        double bandwidth @unit(Mbps) = default(300Mbps);
        int maxClients = default(50);
        string location = default("unknown");
        @display("i=device/accesspoint");
    gates:
        inout backhaul;
        inout wireless[];
}

simple CampusSwitch
{
    parameters:
        double capacity @unit(Gbps) = default(1Gbps);
        string switchType = default("distribution");
        @display("i=device/switch");
    gates:
        inout uplink;
        inout ports[];
}

simple CoreRouter
{
    parameters:
        double linkSpeed @unit(Gbps) = default(10Gbps);
        @display("i=device/router");
    gates:
        inout internet;
        inout coreLinks[];
}

simple RegistrationServer
{
    parameters:
        int maxConcurrentSessions = default(200);
        double processingTime @unit(ms) = default(50ms);
        double serverCapacity @unit(Mbps) = default(500Mbps);
        @display("i=device/server2");
    gates:
        inout link;
}

simple StudentDevice
{
    parameters:
        int studentId;
        bool isRegistering = default(true);
        double dataRate @unit(Mbps) = default(20Mbps);
        string deviceType = default("laptop");
        @display("i=device/laptop");
    gates:
        inout wlan;
}

simple BottleneckMonitor
{
    parameters:
        double samplingInterval @unit(s) = default(1s);
        @display("i=block/cogwheel");
    gates:
        inout monitorLink[];
}

// -------------------------------------------------------
// FLAT network — no compound modules, no gate size issues
// -------------------------------------------------------
network CampusNetwork
{
    parameters:
        int numAdminStudents = default(10);
        int numCSStudents    = default(10);
        int numLibStudents   = default(10);
        @display("bgb=1200,900");

    submodules:
        coreRouter: CoreRouter        { @display("p=600,60");  }
        regServer:  RegistrationServer{ @display("p=900,60");  }
        coreSwitch: CampusSwitch      { switchType="core"; @display("p=600,180"); }
        monitor:    BottleneckMonitor { @display("p=250,60");  }

        swAdmin: CampusSwitch { switchType="distribution"; @display("p=200,320"); }
        swCS:    CampusSwitch { switchType="distribution"; @display("p=600,320"); }
        swLib:   CampusSwitch { switchType="distribution"; @display("p=1000,320");}

        apAdmin[4]: AccessPoint { location="AdminBlock"; @display("p=100,460,r,80"); }
        apCS[5]:    AccessPoint { location="CSBlock";    @display("p=420,460,r,80"); }
        apLib[3]:   AccessPoint { location="Library";    @display("p=870,460,r,80"); }

        adminStudents[numAdminStudents]: StudentDevice { studentId=index;       @display("p=100,620,r,35"); }
        csStudents[numCSStudents]:       StudentDevice { studentId=index+100;   @display("p=420,620,r,35"); }
        libStudents[numLibStudents]:     StudentDevice { studentId=index+200;   @display("p=870,620,r,35"); }

    connections allowunconnected:
        coreRouter.internet    <--> {delay=20ms;  datarate=1Gbps;}  <--> regServer.link;
        coreRouter.coreLinks++ <--> {delay=0.5ms; datarate=10Gbps;} <--> coreSwitch.uplink;

        coreSwitch.ports++ <--> {delay=1ms; datarate=1Gbps;} <--> swAdmin.uplink;
        coreSwitch.ports++ <--> {delay=1ms; datarate=1Gbps;} <--> swCS.uplink;
        coreSwitch.ports++ <--> {delay=1ms; datarate=1Gbps;} <--> swLib.uplink;

        swAdmin.ports++ <--> {delay=1ms; datarate=1Gbps;} <--> apAdmin[0].backhaul;
        swAdmin.ports++ <--> {delay=1ms; datarate=1Gbps;} <--> apAdmin[1].backhaul;
        swAdmin.ports++ <--> {delay=1ms; datarate=1Gbps;} <--> apAdmin[2].backhaul;
        swAdmin.ports++ <--> {delay=1ms; datarate=1Gbps;} <--> apAdmin[3].backhaul;

        swCS.ports++ <--> {delay=1ms; datarate=1Gbps;} <--> apCS[0].backhaul;
        swCS.ports++ <--> {delay=1ms; datarate=1Gbps;} <--> apCS[1].backhaul;
        swCS.ports++ <--> {delay=1ms; datarate=1Gbps;} <--> apCS[2].backhaul;
        swCS.ports++ <--> {delay=1ms; datarate=1Gbps;} <--> apCS[3].backhaul;
        swCS.ports++ <--> {delay=1ms; datarate=1Gbps;} <--> apCS[4].backhaul;

        swLib.ports++ <--> {delay=1ms; datarate=1Gbps;} <--> apLib[0].backhaul;
        swLib.ports++ <--> {delay=1ms; datarate=1Gbps;} <--> apLib[1].backhaul;
        swLib.ports++ <--> {delay=1ms; datarate=1Gbps;} <--> apLib[2].backhaul;

        for i=0..numAdminStudents-1 {
            adminStudents[i].wlan <--> {delay=1ms; datarate=150Mbps;} <--> apAdmin[i%4].wireless++;
        }
        for i=0..numCSStudents-1 {
            csStudents[i].wlan <--> {delay=1ms; datarate=150Mbps;} <--> apCS[i%5].wireless++;
        }
        for i=0..numLibStudents-1 {
            libStudents[i].wlan <--> {delay=1ms; datarate=150Mbps;} <--> apLib[i%3].wireless++;
        }
}
