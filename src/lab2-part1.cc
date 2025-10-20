#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Lab2Part1");

static Ptr<OutputStreamWrapper> cWndStream;
static uint32_t cWndValue;

static void
CwndTracer(uint32_t oldval, uint32_t newval)
{
    *cWndStream->GetStream() << Simulator::Now().GetSeconds() << " " << newval << std::endl;
    cWndValue = newval;
}

static void
TraceCwnd(std::string cwnd_tr_file_name)
{
    AsciiTraceHelper ascii;
    cWndStream = ascii.CreateFileStream(cwnd_tr_file_name.c_str());
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback(&CwndTracer));
}

int
main(int argc, char* argv[])
{
    std::string dataRate = "1Mbps";
    std::string delay = "20ms";
    double errorRate = 0.00001;
    uint32_t nFlows = 1;
    std::string transport_prot = "TcpNewReno";

    CommandLine cmd(__FILE__);
    cmd.AddValue("dataRate", "Bottleneck link data rate", dataRate);
    cmd.AddValue("delay", "Bottleneck link delay", delay);
    cmd.AddValue("errorRate", "Bottleneck link error rate", errorRate);
    cmd.AddValue("nFlows", "Number of TCP flows (max 20)", nFlows);
    cmd.AddValue("transport_prot", "Transport protocol to use (TcpCubic or TcpNewReno)", transport_prot);
    cmd.Parse(argc, argv);

    if (transport_prot != "TcpNewReno" && transport_prot != "TcpCubic")
    {
        NS_LOG_ERROR("Invalid transport protocol. Use TcpNewReno or TcpCubic.");
        return 1;
    }

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::" + transport_prot));

    NodeContainer nodes;
    nodes.Create(4);
    NodeContainer source_r1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer r1_r2 = NodeContainer(nodes.Get(1), nodes.Get(2));
    NodeContainer r2_dest = NodeContainer(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    PointToPointHelper p2pAccess;
    p2pAccess.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pAccess.SetChannelAttribute("Delay", StringValue("0.01ms"));

    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2pBottleneck.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer d_source_r1 = p2pAccess.Install(source_r1);
    NetDeviceContainer d_r1_r2 = p2pBottleneck.Install(r1_r2);
    NetDeviceContainer d_r2_dest = p2pAccess.Install(r2_dest);

    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(errorRate));
    d_r1_r2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_source_r1 = address.Assign(d_source_r1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    address.Assign(d_r1_r2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r2_dest = address.Assign(d_r2_dest);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(i_r2_dest.GetAddress(1), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(3));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(20.0));

    for (uint32_t i = 0; i < nFlows; ++i)
    {
        BulkSendHelper sourceHelper("ns3::TcpSocketFactory", sinkAddress);
        sourceHelper.SetAttribute("MaxBytes", UintegerValue(0));
        ApplicationContainer sourceApps = sourceHelper.Install(nodes.Get(0));
        sourceApps.Start(Seconds(1.0));
        sourceApps.Stop(Seconds(20.0));
    }

    if (nFlows == 1)
    {
        std::string cwnd_fname = "lab2-part1-" + transport_prot + "-cwnd.txt";
        // CORREÇÃO: Agenda a conexão do rastreador para depois do início da aplicação
        Simulator::Schedule(Seconds(1.00001), &TraceCwnd, cwnd_fname);
    }
    
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    
    double totalGoodput = 0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator it = stats.begin(); it != stats.end(); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        if (t.sourceAddress == i_source_r1.GetAddress(0) && t.destinationAddress == i_r2_dest.GetAddress(1))
        {
            double flowDuration = (it->second.timeLastRxPacket - it->second.timeFirstTxPacket).GetSeconds();
            if (flowDuration > 0)
            {
                double goodput = it->second.rxBytes * 8.0 / flowDuration / 1000; // kbps
                std::cout << "Flow " << it->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ") Goodput: " << goodput << " kbps" << std::endl;
                totalGoodput += goodput;
            }
        }
    }
    std::cout << "---------------------------------" << std::endl;
    std::cout << "Total Aggregate Goodput: " << totalGoodput << " kbps" << std::endl;
    
    Simulator::Destroy();
    return 0;
}