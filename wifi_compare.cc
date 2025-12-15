#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/queue-size.h"

using namespace ns3;

static double
ComputeJainFairness(const std::vector<double>& x)
{
  if (x.empty()) return 0.0;
  double sum = 0.0, sumSq = 0.0;
  for (double v : x) { sum += v; sumSq += v * v; }
  return (sum * sum) / (x.size() * sumSq + 1e-12);
}

int main (int argc, char *argv[])
{
  // ---------- Parameters (overridable from CLI) ----------
  std::string standard = "ax";
  uint32_t nStas = 20;
  uint32_t packetSize = 1000;
  std::string appRate = "10Mbps";
  double simTime = 20.0;
  uint32_t channelWidth = 80;
  bool useUdp = true;
  bool enablePcap = false;
  bool quietLogs = true;
  double txPower = 20.0;    // dBm
  double distance = 10.0;   // meters (side length of square, centered on AP)

  CommandLine cmd;
  cmd.AddValue("standard", "Wi-Fi standard: 'ac' or 'ax'", standard);
  cmd.AddValue("nStas", "Number of stations", nStas);
  cmd.AddValue("packetSize", "Application payload size (bytes)", packetSize);
  cmd.AddValue("appRate", "Per-station offered rate (e.g., '10Mbps')", appRate);
  cmd.AddValue("simTime", "Simulation time (s)", simTime);
  cmd.AddValue("channelWidth", "Channel width (MHz): 20/40/80/160", channelWidth);
  cmd.AddValue("useUdp", "Use UDP CBR (true) or TCP BulkSend (false)", useUdp);
  cmd.AddValue("enablePcap", "Enable pcap tracing", enablePcap);
  cmd.AddValue("quietLogs", "Disable log components", quietLogs);
  cmd.AddValue("txPower", "TX power in dBm", txPower);
  cmd.AddValue("distance", "Max distance from AP in meters", distance);
  cmd.Parse(argc, argv);

  // Adjust parameters for dense scenarios
  if (nStas > 30) {
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(1000));
    if (packetSize > 512) {
      packetSize = 512; // Smaller packets for very dense networks
    }
  }

  if (quietLogs) {
    LogComponentEnable("WifiPhy", LOG_LEVEL_WARN);
    LogComponentEnable("UdpClient", LOG_LEVEL_WARN);
    LogComponentEnable("UdpServer", LOG_LEVEL_WARN);
  }

  // ---------- Topology ----------
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(nStas);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  // Channel / PHY
  YansWifiChannelHelper channel;
  channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                             "Exponent", DoubleValue(3.0),
                             "ReferenceLoss", DoubleValue(46.6777));

  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  // PHY settings
  phy.Set("TxPowerStart", DoubleValue(txPower));
  phy.Set("TxPowerEnd",   DoubleValue(txPower));
  phy.Set("RxGain", DoubleValue(0.0));
  phy.Set("TxGain", DoubleValue(0.0));
  phy.Set("RxNoiseFigure", DoubleValue(7.0));
  phy.Set("CcaEdThreshold", DoubleValue(-62.0));
  phy.SetErrorRateModel("ns3::YansErrorRateModel");
  
  // For ns-3.40, use ChannelSettings instead of direct ChannelWidth attribute
  // Format: "{channel_number, channel_width, band, primary20_index}"
  std::string channelStr = "{0, " + std::to_string(channelWidth) + ", BAND_5GHZ, 0}";
  phy.Set("ChannelSettings", StringValue(channelStr));

  // MAC/Standard
  WifiHelper wifi;
  if (standard == "ac") {
    wifi.SetStandard(WIFI_STANDARD_80211ac);
  } else if (standard == "ax") {
    wifi.SetStandard(WIFI_STANDARD_80211ax);
  } else {
    NS_ABORT_MSG("Unknown standard: use 'ac' or 'ax'");
  }

  // Use MinstrelHt for all scenarios - it supports HT/VHT/HE rates
  // For very dense scenarios, consider IdealWifiManager for testing
  if (nStas > 50) {
    wifi.SetRemoteStationManager("ns3::IdealWifiManager");
  } else {
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");
  }

  // MAC queue settings (correct for ns-3.40)
  Config::SetDefault("ns3::WifiMacQueue::MaxSize", QueueSizeValue(QueueSize("1000p")));
  Config::SetDefault("ns3::WifiMacQueue::MaxDelay", TimeValue(MilliSeconds(500)));

  WifiMacHelper mac;
  Ssid ssid = Ssid("dense-wifi");

  // STAs
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  // AP
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

  // Mobility
  MobilityHelper mobilitySta;
  mobilitySta.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                   "X", StringValue("ns3::UniformRandomVariable[Min=-" + std::to_string(distance/2) + "|Max=" + std::to_string(distance/2) + "]"),
                                   "Y", StringValue("ns3::UniformRandomVariable[Min=-" + std::to_string(distance/2) + "|Max=" + std::to_string(distance/2) + "]"));
  mobilitySta.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilitySta.Install(wifiStaNodes);

  MobilityHelper mobilityAp;
  Ptr<ListPositionAllocator> apPos = CreateObject<ListPositionAllocator>();
  apPos->Add(Vector(0.0, 0.0, 1.0));
  mobilityAp.SetPositionAllocator(apPos);
  mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityAp.Install(wifiApNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.0.0", "255.255.0.0");
  Ipv4InterfaceContainer staIf = address.Assign(staDevices);
  Ipv4InterfaceContainer apIf  = address.Assign(apDevice);

  // ---------- Applications ----------
  uint16_t basePort = 9000;
  ApplicationContainer serverApps;

  if (useUdp) {
    // One UDP server port per STA (all on the AP)
    for (uint32_t i = 0; i < nStas; ++i) {
      UdpServerHelper server(basePort + i);
      serverApps.Add(server.Install(wifiApNode.Get(0)));
    }
  } else {
    for (uint32_t i = 0; i < nStas; ++i) {
      PacketSinkHelper sink("ns3::TcpSocketFactory",
                            InetSocketAddress(apIf.GetAddress(0), basePort + i));
      serverApps.Add(sink.Install(wifiApNode.Get(0)));
    }
  }

  ApplicationContainer clientApps;
  DataRate dataRate(appRate);

  if (useUdp) {
    for (uint32_t i = 0; i < nStas; ++i) {
      OnOffHelper onoff("ns3::UdpSocketFactory",
                        Address(InetSocketAddress(apIf.GetAddress(0), basePort + i)));
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      onoff.SetAttribute("DataRate", DataRateValue(dataRate));
      onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

      ApplicationContainer appContainer = onoff.Install(wifiStaNodes.Get(i));
      clientApps.Add(appContainer);
      double startTime = 1.5 + (i * 0.01); // 10ms stagger
      appContainer.Get(0)->SetStartTime(Seconds(startTime));
    }
  } else {
    for (uint32_t i = 0; i < nStas; ++i) {
      BulkSendHelper bulk("ns3::TcpSocketFactory",
                          InetSocketAddress(apIf.GetAddress(0), basePort + i));
      bulk.SetAttribute("SendSize", UintegerValue(packetSize));
      bulk.SetAttribute("MaxBytes", UintegerValue(0));

      ApplicationContainer appContainer = bulk.Install(wifiStaNodes.Get(i));
      clientApps.Add(appContainer);
      double startTime = 1.5 + (i * 0.01); // 10ms stagger
      appContainer.Get(0)->SetStartTime(Seconds(startTime));
    }
  }

  serverApps.Start(Seconds(0.5));
  clientApps.Stop(Seconds(simTime - 0.5));
  serverApps.Stop(Seconds(simTime));

  if (enablePcap) {
    phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("wifi-debug", apDevice.Get(0));
  }

  // FlowMonitor
  FlowMonitorHelper fmHelper;
  Ptr<FlowMonitor> monitor = fmHelper.InstallAll();

  std::cout << "Starting simulation with " << nStas << " STAs, "
            << appRate << " per STA, " << standard << " standard..." << std::endl;

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  // ---------- Compute metrics ----------
  monitor->CheckForLostPackets();
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  double aggThroughputMbps = 0.0;
  double totalDelaySec = 0.0;
  uint64_t totalRxBytes = 0;
  uint64_t totalRxPackets = 0;
  uint64_t totalTxPackets = 0;
  uint64_t totalLostPackets = 0;

  std::vector<double> perFlowMbps;
  double effectiveDuration = simTime - 2.0; // account for startup/stagger

  for (const auto& kv : stats) {
    const FlowMonitor::FlowStats& s = kv.second;

    std::cout << "Flow " << kv.first << ": " << s.rxPackets << " received, "
              << s.txPackets << " transmitted, " << s.lostPackets << " lost" << std::endl;

    if (s.rxPackets > 0) {
      double rxMbps = (s.rxBytes * 8.0) / (effectiveDuration * 1e6);
      perFlowMbps.push_back(rxMbps);
      aggThroughputMbps += rxMbps;
    }

    totalDelaySec   += s.delaySum.GetSeconds();
    totalRxBytes    += s.rxBytes;
    totalRxPackets  += s.rxPackets;
    totalTxPackets  += s.txPackets;
    totalLostPackets += s.lostPackets;
  }

  double avgDelayMs = (totalRxPackets > 0)
                        ? (totalDelaySec / totalRxPackets) * 1000.0
                        : 0.0;
  double fairness = ComputeJainFairness(perFlowMbps);
  double lossPct = (totalTxPackets > 0)
                        ? (100.0 * (double)totalLostPackets / (double)totalTxPackets)
                        : 0.0;

  std::cout.setf(std::ios::fixed);
  std::cout.precision(3);

  std::cout << "\n=== Wi-Fi Dense Scenario Summary ===" << std::endl;
  std::cout << "Standard: " << standard << ", STAs: " << nStas
            << ", Channel: " << channelWidth << "MHz" << std::endl;
  std::cout << "AppRate: " << appRate << " per STA, UDP: " << (useUdp ? "yes" : "no")
            << ", Time: " << simTime << "s" << std::endl;
  std::cout << "AggregateThroughput(Mbps): " << aggThroughputMbps << std::endl;
  std::cout << "AvgDelay(ms): " << avgDelayMs << std::endl;
  std::cout << "PacketLoss(%): " << lossPct << std::endl;
  std::cout << "Fairness(Jain): " << fairness << std::endl;
  std::cout << "TotalRxPackets: " << totalRxPackets << std::endl;
  std::cout << "TotalTxPackets: " << totalTxPackets << std::endl;
  std::cout << "SuccessRate(%): " << (totalTxPackets > 0 ? (100.0 * totalRxPackets / totalTxPackets) : 0) << std::endl;

  Simulator::Destroy();
  return 0;
}
