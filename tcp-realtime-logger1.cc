#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <ctime>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstdlib>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpDynamicLogger");

std::ofstream csvFile;
std::string tcpVariant;

std::vector<double> throughputHistory;
std::vector<double> delayHistory;

// Generate timestamped filename
std::string GetTimestampedFilename() {
    std::time_t now = std::time(nullptr);
    std::tm *ltm = std::localtime(&now);
    std::ostringstream oss;
    oss << "tcp_metrics_"
        << 1900 + ltm->tm_year << "-"
        << 1 + ltm->tm_mon << "-"
        << ltm->tm_mday << "_"
        << ltm->tm_hour << "-"
        << ltm->tm_min << "-"
        << ltm->tm_sec << ".csv";
    return oss.str();
}

// Compute percentile
double GetPercentile(std::vector<double> values, double percentile) {
    if (values.empty()) return 0.0;
    size_t idx = static_cast<size_t>(percentile * values.size());
    idx = std::min(idx, values.size() - 1);
    std::nth_element(values.begin(), values.begin() + idx, values.end());
    return values[idx];
}

// Log metrics every second
void LogMetrics(Ptr<FlowMonitor> monitor, FlowMonitorHelper &helper) {
    monitor->CheckForLostPackets();
    auto stats = monitor->GetFlowStats();
    auto classifier = DynamicCast<Ipv4FlowClassifier>(helper.GetClassifier());

    double currentTime = Simulator::Now().GetSeconds();

    for (const auto &flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);

        double timeLastRx = flow.second.timeLastRxPacket.GetSeconds();
        double timeFirstTx = flow.second.timeFirstTxPacket.GetSeconds();
        double throughput = (timeLastRx > timeFirstTx)
            ? (flow.second.rxBytes * 8.0) / (timeLastRx - timeFirstTx) / 1e6
            : 0.0;

        double delay = (flow.second.rxPackets > 0)
            ? (flow.second.delaySum.GetSeconds() / flow.second.rxPackets)
            : 0.0;

        throughputHistory.push_back(throughput);
        delayHistory.push_back(delay);

        double dynamicThroughputThresh = GetPercentile(throughputHistory, 0.25);
        double dynamicDelayThresh = GetPercentile(delayHistory, 0.75);

        std::string status = (throughput < dynamicThroughputThresh || delay > dynamicDelayThresh)
                             ? "FAILURE" : "OK";

        csvFile << currentTime << ","
                << flow.first << ","
                << t.sourceAddress << "->" << t.destinationAddress << ","
                << throughput << ","
                << delay << ","
                << status << ","
                << tcpVariant << "\n";
    }

    Simulator::Schedule(Seconds(1.0), &LogMetrics, monitor, std::ref(helper));
}

int main(int argc, char *argv[]) {
    // 🔀 Randomly pick a TCP variant
    std::vector<std::string> tcpVariants = {
        "TcpNewReno", "TcpTahoe", "TcpReno", "TcpWestwood", "TcpVegas"
    };

    unsigned int seed = std::time(nullptr);
    SeedManager::SetSeed(seed);
    SeedManager::SetRun(seed % 1000);
    std::srand(seed);

    tcpVariant = tcpVariants[std::rand() % tcpVariants.size()];
    std::string fullType = "ns3::" + tcpVariant;

    // 🛠️ Set TCP type
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName(fullType)));
    std::cout << "✅ Using TCP Variant: " << fullType << std::endl;

    std::string filename = GetTimestampedFilename();
    csvFile.open(filename);
    csvFile << "Time,FlowID,Source->Dest,Throughput(Mbps),Delay(s),LinkStatus,TCPVariant\n";

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    int delayMs = 2 + std::rand() % 3;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(delayMs)));

    NetDeviceContainer devices = p2p.Install(nodes);
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 50000;
    Address sinkAddr(InetSocketAddress(interfaces.GetAddress(1), port));

    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddr);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddr);
    clientHelper.SetAttribute("DataRate", StringValue("5Mbps"));

    uint32_t pktSize = 1000 + (std::rand() % 1401);
    double startTime = 1.0 + (std::rand() % 4);
    double stopTime = 15.0 + (std::rand() % 6);

    clientHelper.SetAttribute("PacketSize", UintegerValue(pktSize));
    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(startTime));
    clientApp.Stop(Seconds(stopTime));

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    Simulator::Schedule(Seconds(1.0), &LogMetrics, monitor, std::ref(flowHelper));

    Simulator::Stop(Seconds(22.0));
    Simulator::Run();

    monitor->SerializeToXmlFile("tcp_metrics_latest.xml", true, true);
    Simulator::Destroy();
    csvFile.close();

    std::cout << "✅ Output saved to: " << filename << std::endl;
    return 0;
}
