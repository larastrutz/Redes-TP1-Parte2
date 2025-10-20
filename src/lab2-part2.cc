#include <fstream>
#include <string>
#include <vector>
#include <numeric>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h" // CORREÇÃO: Incluído o módulo completo
#include "ns3/rng-seed-manager.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Lab2Part2");

int
main(int argc, char* argv[])
{
    std::string dataRate = "1Mbps";
    std::string delay = "20ms";
    double errorRate = 0.00001;
    uint32_t nFlows = 2;
    std::string transport_prot = "TcpNewReno";

    CommandLine cmd(__FILE__);
    cmd.AddValue("dataRate", "Bottleneck link data rate", dataRate);
    cmd.AddValue("delay", "Bottleneck link delay", delay);
    cmd.AddValue("errorRate", "Bottleneck link error rate", errorRate);
    cmd.AddValue("nFlows", "Number of TCP flows (must be even)", nFlows);
    cmd.AddValue("transport_prot", "Transport protocol to use (TcpCubic or TcpNewReno)", transport_prot);
    cmd.Parse(argc, argv);

    if (nFlows % 2 != 0)
    {
        NS_LOG_ERROR("Number of flows must be an even number.");
        return 1;
    }
    
    std::vector<double> avgGoodputsDest1;
    std::vector<double> avgGoodputsDest2;

    for (uint32_t run = 0; run < 10; ++run)
    {
        RngSeedManager::SetRun(run);

        if (transport_prot != "TcpNewReno" && transport_prot != "TcpCubic")
        {
            NS_LOG_ERROR("Invalid transport protocol. Use TcpNewReno or TcpCubic.");
            return 1;
        }

        Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::" + transport_prot));

        NodeContainer nodes;
        nodes.Create(5);
        NodeContainer source_r1 = NodeContainer(nodes.Get(0), nodes.Get(1));
        NodeContainer r1_r2 = NodeContainer(nodes.Get(1), nodes.Get(2));
        NodeContainer r2_dest1 = NodeContainer(nodes.Get(2), nodes.Get(3));
        NodeContainer r2_dest2 = NodeContainer(nodes.Get(2), nodes.Get(4));

        InternetStackHelper stack;
        stack.Install(nodes);

        PointToPointHelper p2pAccess;
        p2pAccess.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
        p2pAccess.SetChannelAttribute("Delay", StringValue("0.01ms"));

        PointToPointHelper p2pBottleneck;
        p2pBottleneck.SetDeviceAttribute("DataRate", StringValue(dataRate));
        p2pBottleneck.SetChannelAttribute("Delay", StringValue(delay));

        PointToPointHelper p2pDest2;
        p2pDest2.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
        p2pDest2.SetChannelAttribute("Delay", StringValue("50ms"));

        NetDeviceContainer d_source_r1 = p2pAccess.Install(source_r1);
        NetDeviceContainer d_r1_r2 = p2pBottleneck.Install(r1_r2);
        NetDeviceContainer d_r2_dest1 = p2pAccess.Install(r2_dest1);
        NetDeviceContainer d_r2_dest2 = p2pDest2.Install(r2_dest2);

        Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
        em->SetAttribute("ErrorRate", DoubleValue(errorRate));
        d_r1_r2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer i_source_r1 = address.Assign(d_source_r1);

        address.SetBase("10.1.2.0", "255.255.255.0");
        address.Assign(d_r1_r2);

        address.SetBase("10.1.3.0", "255.255.255.0");
        Ipv4InterfaceContainer i_r2_dest1 = address.Assign(d_r2_dest1);

        address.SetBase("10.1.4.0", "255.255.255.0");
        Ipv4InterfaceContainer i_r2_dest2 = address.Assign(d_r2_dest2);

        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        uint16_t sinkPort = 8080;
        
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
        ApplicationContainer sinkApps1 = sinkHelper.Install(nodes.Get(3));
        sinkApps1.Start(Seconds(0.0));
        sinkApps1.Stop(Seconds(20.0));
        
        ApplicationContainer sinkApps2 = sinkHelper.Install(nodes.Get(4));
        sinkApps2.Start(Seconds(0.0));
        sinkApps2.Stop(Seconds(20.0));

        for (uint32_t i = 0; i < nFlows; ++i)
        {
            Address sinkAddress;
            if (i % 2 == 0) {
                sinkAddress = InetSocketAddress(i_r2_dest1.GetAddress(1), sinkPort);
            } else {
                sinkAddress = InetSocketAddress(i_r2_dest2.GetAddress(1), sinkPort);
            }
            BulkSendHelper sourceHelper("ns3::TcpSocketFactory", sinkAddress);
            sourceHelper.SetAttribute("MaxBytes", UintegerValue(0));
            ApplicationContainer sourceApps = sourceHelper.Install(nodes.Get(0));
            sourceApps.Start(Seconds(1.0));
            sourceApps.Stop(Seconds(20.0));
        }

        FlowMonitorHelper flowmon;
        Ptr<FlowMonitor> monitor = flowmon.InstallAll();

        Simulator::Stop(Seconds(20.0));
        Simulator::Run();

        double totalGoodputDest1 = 0;
        double totalGoodputDest2 = 0;
        uint32_t flowsDest1 = 0;
        uint32_t flowsDest2 = 0;

        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
        for (auto const& stat : monitor->GetFlowStats())
        {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(stat.first);
            double flowDuration = (stat.second.timeLastRxPacket - stat.second.timeFirstTxPacket).GetSeconds();
            if (flowDuration > 0)
            {
                if (t.destinationAddress == i_r2_dest1.GetAddress(1)) {
                    totalGoodputDest1 += stat.second.rxBytes * 8.0 / flowDuration;
                    flowsDest1++;
                } else if (t.destinationAddress == i_r2_dest2.GetAddress(1)) {
                    totalGoodputDest2 += stat.second.rxBytes * 8.0 / flowDuration;
                    flowsDest2++;
                }
            }
        }
        
        if (flowsDest1 > 0) avgGoodputsDest1.push_back(totalGoodputDest1 / flowsDest1);
        if (flowsDest2 > 0) avgGoodputsDest2.push_back(totalGoodputDest2 / flowsDest2);
        
        Simulator::Destroy();
    }
    
    double finalAvgDest1 = std::accumulate(avgGoodputsDest1.begin(), avgGoodputsDest1.end(), 0.0) / avgGoodputsDest1.size();
    double finalAvgDest2 = std::accumulate(avgGoodputsDest2.begin(), avgGoodputsDest2.end(), 0.0) / avgGoodputsDest2.size();
    
    std::cout << "\n--- Final Averaged Results over 10 runs ---" << std::endl;
    std::cout << "Protocol: " << transport_prot << ", nFlows: " << nFlows << std::endl;
    std::cout << "Average Goodput to dest1 (short RTT): " << finalAvgDest1 / 1000 << " kbps" << std::endl;
    std::cout << "Average Goodput to dest2 (long RTT): " << finalAvgDest2 / 1000 << " kbps" << std::endl;

    return 0;
}