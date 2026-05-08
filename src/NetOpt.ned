// ============================================================
// NetOpt.ned
// PDC Spring 2026 - Section B
// New: channelNumber, txPower, department, priority params
// ============================================================

package netopt;

simple AccessPoint
{
    parameters:
        int    numChannels  = default(3);
        double bandwidth @unit(Mbps) = default(300Mbps);
        int    maxClients   = default(50);
        string location     = default("unknown");
        int    channelNumber = default(6);      // NEW: Wi-Fi channel
        double txPower      = default(20.0);    // NEW: dBm
        @display("i=device/accesspoint");
    gates:
        inout  backhaul;
        input  wirelessIn[];
        output wirelessOut[];
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
        int    maxConcurrentSessions = default(200);
        double processingTime @unit(ms) = default(50ms);
        double serverCapacity @unit(Mbps) = default(500Mbps);
        @display("i=device/server2");
    gates:
        inout link;
}

simple StudentDevice
{
    parameters:
        int    studentId;
        bool   isRegistering = default(true);
        double dataRate @unit(Mbps) = default(20Mbps);
        string deviceType   = default("laptop");
        string department   = default("CS");        // NEW
        int    priority     = default(2);           // NEW: 1=senior,2=junior,3=freshman
        double arrivalWindow @unit(s) = default(-1s); // NEW: -1s=staggered, >=0s=burst window
        int    maxRetries = default(-1);            // NEW: <0 uses built-in default
        @display("i=device/laptop");
    gates:
        output wlanOut;
        input  wlanIn;
}

simple BottleneckMonitor
{
    parameters:
        double samplingInterval @unit(s) = default(1s);
        bool   useTimeProfile = default(false); // if true, use legacy time-based health curve
        bool   stressProfile = default(false);  // if true, use a longer severe-collapse time curve
        @display("i=block/cogwheel");
    gates:
        inout monitorLink[];
}

module CampusBuilding
{
    parameters:
        int    numAPs       = default(3);
        int    numStudents  = default(30);
        string buildingName = default("Building");
        string deptName     = default("CS");        // NEW
        int    apChannel    = default(6);           // NEW: channel for this building's APs
        @display("bgb=650,440");
    gates:
        inout toSwitch[numAPs];
    submodules:
        ap[numAPs]: AccessPoint {
           channelNumber = parent.apChannel;
            @display("p=90,80,r,130");
        }
        students[numStudents]: StudentDevice {
            studentId  = index;
            department = parent.deptName;
            priority   = int(uniform(1,4)); // random priority 1-3
            @display("p=90,280,r,50");
        }
    connections allowunconnected:
        for i=0..numStudents-1 {
            students[i].wlanOut
                --> {delay=1ms; datarate=150Mbps;}
                --> ap[i % numAPs].wirelessIn++;
            ap[i % numAPs].wirelessOut++
                --> {delay=1ms; datarate=150Mbps;}
                --> students[i].wlanIn;
        }
        for i=0..numAPs-1 {
            ap[i].backhaul <--> toSwitch[i];
        }
}

network CampusNetwork
{
    parameters:
        @display("bgb=1200,900");
    submodules:
        coreRouter: CoreRouter    { @display("p=600,60");  }
        regServer:  RegistrationServer { @display("p=950,60"); }
        monitor:    BottleneckMonitor  { @display("p=200,60"); }
        coreSwitch: CampusSwitch  { switchType="core"; @display("p=600,200"); }

        distSwitch[3]: CampusSwitch {
            switchType="distribution";
            @display("p=200,400,r,400");
        }

        // Admin Block: channel 1, medium load
        adminBlock: CampusBuilding {
            numAPs=4; numStudents=30;
            buildingName="Admin Block";
            deptName="Admin";
            apChannel=1;
            @display("p=80,650");
        }
        // CS Block: channel 6, heaviest load — BOTTLENECK zone
        csBlock: CampusBuilding {
            numAPs=5; numStudents=40;
            buildingName="CS Block";
            deptName="CS";
            apChannel=6;
            @display("p=520,650");
        }
        // Library: channel 11, lightest load
        library: CampusBuilding {
            numAPs=3; numStudents=20;
            buildingName="Library";
            deptName="Library";
            apChannel=11;
            @display("p=960,650");
        }

    connections:
        coreRouter.internet
            <--> {delay=20ms; datarate=1Gbps;}
            <--> regServer.link;
        coreRouter.coreLinks++
            <--> {delay=0.5ms; datarate=10Gbps;}
            <--> coreSwitch.uplink;

        coreSwitch.ports++ <--> {delay=1ms; datarate=1Gbps;} <--> distSwitch[0].uplink;
        coreSwitch.ports++ <--> {delay=1ms; datarate=1Gbps;} <--> distSwitch[1].uplink;
        coreSwitch.ports++ <--> {delay=1ms; datarate=1Gbps;} <--> distSwitch[2].uplink;

        distSwitch[0].ports++ <--> {delay=1ms; datarate=1Gbps;} <--> adminBlock.toSwitch[0];
        distSwitch[0].ports++ <--> {delay=1ms; datarate=1Gbps;} <--> adminBlock.toSwitch[1];
        distSwitch[0].ports++ <--> {delay=1ms; datarate=1Gbps;} <--> adminBlock.toSwitch[2];
        distSwitch[0].ports++ <--> {delay=1ms; datarate=1Gbps;} <--> adminBlock.toSwitch[3];

        distSwitch[1].ports++ <--> {delay=1ms; datarate=1Gbps;} <--> csBlock.toSwitch[0];
        distSwitch[1].ports++ <--> {delay=1ms; datarate=1Gbps;} <--> csBlock.toSwitch[1];
        distSwitch[1].ports++ <--> {delay=1ms; datarate=1Gbps;} <--> csBlock.toSwitch[2];
        distSwitch[1].ports++ <--> {delay=1ms; datarate=1Gbps;} <--> csBlock.toSwitch[3];
        distSwitch[1].ports++ <--> {delay=1ms; datarate=1Gbps;} <--> csBlock.toSwitch[4];

        distSwitch[2].ports++ <--> {delay=1ms; datarate=1Gbps;} <--> library.toSwitch[0];
        distSwitch[2].ports++ <--> {delay=1ms; datarate=1Gbps;} <--> library.toSwitch[1];
        distSwitch[2].ports++ <--> {delay=1ms; datarate=1Gbps;} <--> library.toSwitch[2];
}
