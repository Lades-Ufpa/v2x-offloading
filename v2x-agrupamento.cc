/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Joahannes Costa <joahannes@gmail.com>
 * 
 */

#include "ns3/core-module.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/network-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/v2v-control-client-helper.h"
#include "ns3/v2v-mobility-model.h"
#include "ns3/netanim-module.h"
#include "ns3/evalvid-client-server-helper.h"


#define SIMULATION_TIME_FORMAT(s) Seconds(s)


using namespace ns3;
NS_LOG_COMPONENT_DEFINE("V2vClusteringExample");


int main(int argc, char *argv[]) {

    /*--------------------- Logging System Configuration -------------------*/
    LogLevel logLevel = (LogLevel) (LOG_PREFIX_ALL | LOG_LEVEL_WARN);
    LogComponentEnable("V2vClusteringExample", logLevel);
    LogComponentEnable("V2vControlClient", logLevel);

    NS_LOG_INFO("/------------------------------------------------\\");
    NS_LOG_INFO(" - V2vClusteringExample [Example] -> Cluster vehicles communication");
    NS_LOG_INFO("\\------------------------------------------------/");
    /*----------------------------------------------------------------------*/

    /*---------------------- Simulation Default Values ---------------------*/
    std::string phyMode ("OfdmRate6MbpsBW10MHz");

    uint16_t numberOfUes = 10; //VEÍCULOS

    uint16_t numberOfRsus = 1; //RSU

    double minimumTdmaSlot = 0.001;         /// Time difference between 2 transmissions
    double clusterTimeMetric = 3.0;         /// Clustering Time Metric for Waiting Time calculation
    double speedVariation = 5.0;
    double incidentWindow = 30.0;

    double simTime = 40.0;
    /*----------------------------------------------------------------------*/


    /*-------------------- Set explicitly default values -------------------*/
    Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold",
                            StringValue ("2200"));
    // turn off RTS/CTS for frames below 2200 bytes
    Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",
                            StringValue ("2200"));
    // Fix non-unicast data rate to be the same as that of unicast
    Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
                            StringValue (phyMode));
    /*----------------------------------------------------------------------*/


    /*-------------------- Command Line Argument Values --------------------*/
    CommandLine cmd;
    cmd.AddValue("ueNumber", "Number of UE", numberOfUes);
    cmd.AddValue("simTime", "Simulation Time in Seconds", simTime);

    NS_LOG_INFO("");
    NS_LOG_INFO("|---"<< " SimTime -> " << simTime <<" ---|\n");
    NS_LOG_INFO("|---"<< " Number of UE -> " << numberOfUes <<" ---|\n");
    /*----------------------------------------------------------------------*/


    /*------------------------- Create UEs-RSUs ----------------------------*/
    NodeContainer ueNodes;
    ueNodes.Create(numberOfUes);

    //-RSU
    NodeContainer rsuNodes;
    rsuNodes.Create(numberOfRsus);

    //Instala pilha de Internet
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(rsuNodes);
    /*----------------------------------------------------------------------*/


    /*-------------------- Install Mobility Model in Ue --------------------*/
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel ("ns3::V2vMobilityModel",
         "Mode", StringValue ("Time"),
         "Time", StringValue ("40s"),
         "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=30.0]"),
         "Bounds", RectangleValue (Rectangle (0, 10000, -1000, 1000))); //Valores padrão: 0, 10000, -1000, 1000
    ueMobility.Install(ueNodes);

    /// Create a 3 line grid of vehicles
    for (uint16_t i = 0; i < numberOfUes; i++)
    {
        if(i % 3 == 0){
            ueNodes.Get (i)->GetObject<MobilityModel> ()->SetPosition (Vector (i*5, 0, 0));
        }
        else if(i % 3 == 1){
            ueNodes.Get (i)->GetObject<MobilityModel> ()->SetPosition (Vector (i*5, 3, 0));
        }
        else{
            ueNodes.Get (i)->GetObject<MobilityModel> ()->SetPosition (Vector (i*5, 6, 0));
        }

    }
    /*----------------------------------------------------------------------*/

    //Mobilidade para RSU
    /*
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    for (uint16_t i = 1; i <= rsuNodes.GetN(); i++){
     positionAlloc->Add (Vector(150 * i, 15, 0)); //DISTANCIA ENTRE RSUs [m] 
    }
    */

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector(300, 15, 0)); //DISTANCIA ENTRE RSUs [m] }

    MobilityHelper mobilityRsu;
    mobilityRsu.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityRsu.SetPositionAllocator(positionAlloc);
    mobilityRsu.Install(rsuNodes);

    /*-------------------------- Setup Wifi nodes --------------------------*/
    // The below set of helpers will help us to put together the wifi NICs we want
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    Ptr<YansWifiChannel> channel = wifiChannel.Create ();

    YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
    wifiPhy.SetChannel (channel);
    wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11);
    wifiPhy.Set ("TxPowerStart", DoubleValue(32));
    wifiPhy.Set ("TxPowerEnd", DoubleValue(32));
    wifiPhy.Set ("TxGain", DoubleValue(12));
    wifiPhy.Set ("RxGain", DoubleValue(12));
    wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue(-61.8));
    wifiPhy.Set ("CcaMode1Threshold", DoubleValue(-64.8));

    NqosWaveMacHelper wifi80211pMac = NqosWaveMacHelper::Default ();
    Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
    //wifi80211p.EnableLogComponents ();

    wifi80211p.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode",StringValue (phyMode),
                                      "ControlMode",StringValue (phyMode));
    NetDeviceContainer wifiDevices1 = wifi80211p.Install (wifiPhy, wifi80211pMac, ueNodes);

    //RSU
    NetDeviceContainer wifiDevices2 = wifi80211p.Install (wifiPhy, wifi80211pMac, rsuNodes); //RSU

    NS_LOG_INFO ("Assign IP Addresses.");
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i1 = ipv4h.Assign (wifiDevices1);

    //Configuração de IP da RSU
    Ipv4InterfaceContainer i2 = ipv4h.Assign (wifiDevices2); //RSU

    uint16_t controlPort = 3999;
    ApplicationContainer controlApps; //Para troca de mensagens

    /**
     * Setting Control Channel
     */
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {

        //!< Initial TDMA UE synchronization Function
        double tdmaStart = (u+1)*minimumTdmaSlot;

        Ptr<V2vMobilityModel> mobilityModel = ueNodes.Get(u)->GetObject<V2vMobilityModel>();
        mobilityModel->SetSpeedVariation(speedVariation);
        V2vControlClientHelper ueClient("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetBroadcast(), controlPort)),
                "ns3::UdpSocketFactory",InetSocketAddress(Ipv4Address::GetAny(), controlPort),
                mobilityModel, tdmaStart, numberOfUes, minimumTdmaSlot, clusterTimeMetric);
        ueClient.SetAttribute ("IncidentWindow", DoubleValue(incidentWindow));
        controlApps.Add(ueClient.Install(ueNodes.Get(u)));
    }

//TESTE DE TRANSMISSÃO DE VÍDEO

//-------------Rodar aplicação EvalVid
  for (uint32_t i = 0; i < ueNodes.GetN(); ++i){
    //Gera SD e RD para cada Veículo
    std::stringstream sdTrace;
    std::stringstream rdTrace;
    sdTrace << "resultados/agrup_sd_a01_" << (int)i;
    rdTrace << "resultados/agrup_rd_a01_" << (int)i;
 
    double start = 7.0;
    double stop = simTime; 
    
    uint16_t port = 2000;
    uint16_t m_port = port * i + 2000; //Para alcançar o nó ZERO quando i = 0

    EvalvidServerHelper server (m_port);
     server.SetAttribute ("SenderTraceFilename", StringValue("video/st_a03"));
     server.SetAttribute ("SenderDumpFilename", StringValue(sdTrace.str()));
     server.SetAttribute ("PacketPayload",UintegerValue(1014));
     ApplicationContainer apps = server.Install(rsuNodes.Get(0));
     apps.Start (Seconds (start));
     apps.Stop (Seconds (stop));
  
    EvalvidClientHelper client (i2.GetAddress (0),m_port);
     client.SetAttribute ("ReceiverDumpFilename", StringValue(rdTrace.str()));
     apps = client.Install (ueNodes.Get(i));
     apps.Start (Seconds (start + 1.0));
     apps.Stop (Seconds (stop));
   }

//FIM TESTE DE TRANSMISSÃO DE VÍDEO

    controlApps.Start (Seconds(0.1));
    controlApps.Stop (Seconds(simTime-0.1));

    //AsciiTraceHelper ascii;
    //wifiPhy.EnableAsciiAll(ascii.CreateFileStream ("resultados/socket-options-ipv4.txt"));
    //wifiPhy.EnablePcapAll ("resultados/socket.pcap", false);

    /*----------------------------------------------------------------------*/
       
    AnimationInterface anim ("resultados/agrup_v2x.xml");
    //Cor e Descrição para RSU
    for (uint32_t i = 0; i < rsuNodes.GetN (); ++i){
        anim.UpdateNodeDescription (rsuNodes.Get (i), "RSU");
        anim.UpdateNodeColor (rsuNodes.Get (i), 0, 255, 0);
    }

    /*---------------------- Simulation Stopping Time ----------------------*/
    Simulator::Stop(SIMULATION_TIME_FORMAT(simTime));
    /*----------------------------------------------------------------------*/

    /*--------------------------- Simulation Run ---------------------------*/
    Simulator::Run();
    Simulator::Destroy();
    /*----------------------------------------------------------------------*/

    return EXIT_SUCCESS;
}
