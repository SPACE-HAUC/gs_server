/**
 * @file gss.cpp
 * @author Mit Bailey (mitbailey99@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2021.07.26
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <stdio.h>
#include <string.h>
#include <ifaddrs.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include "network.hpp"
#include "gss.hpp"
#include "meb_debug.hpp"

void *gss_network_rx_thread(void *global_vp)
{
    global_data_t *global = (global_data_t *)global_vp;

    LISTEN_FOR listen_for = LF_ERROR;
    int t_index = -1;
    pthread_t thread_id = pthread_self();

    for (int i = 0; i < NUM_PORTS; i++)
    {
        if (thread_id == global->pid[i])
        {
            listen_for = (LISTEN_FOR)i;
            t_index = i;
        }
    }

    char t_tag[32];

    switch (listen_for)
    {
    case LF_CLIENT:
    {
        strcpy(t_tag, "[RXT_GUICLIENT] ");
        dbprintlf("%sThread (id:%lu) listening for GUI Client.", t_tag, (unsigned long)thread_id);

        break;
    }
    case LF_ROOF_UHF:
    {
        strcpy(t_tag, "[RXT_ROOFUHF] ");
        dbprintlf("%sThread (id:%lu) listening for Roof UHF.", t_tag, (unsigned long)thread_id);

        break;
    }
    case LF_ROOF_XBAND:
    {
        strcpy(t_tag, "[RXT_ROOFXBAND] ");
        dbprintlf("%sThread (id:%lu) listening for Roof X-Band.", t_tag, (unsigned long)thread_id);

        break;
    }
    case LF_HAYSTACK:
    {
        strcpy(t_tag, "[RXT_HAYSTACK] ");
        dbprintlf("%sThread (id:%lu) listening for Haystack.", t_tag, (unsigned long)thread_id);

        break;
    }
    case LF_TRACK:
    {
        strcpy(t_tag, "[RXT_TRACK] ");
        dbprintlf("%sThread (id:%lu) listening for Track.", t_tag, (unsigned long)thread_id);

        break;
    }
    case LF_ERROR:
    default:
    {
        dbprintlf(FATAL "[RXT_ERROR] Thread (id:%lu) not listening for any valid sender (%d).", (unsigned long)thread_id, t_index);
        return NULL;
    }
    }

    // Makes my life easier.
    NetDataServer *network_data = global->network_data[t_index];
    
    while (network_data->Accepting())
    {
        uint8_t netstat = 0x0;
        netstat |= 0x80 * ({NetClient *client = global->network_data[LF_CLIENT]->GetClient(NetVertex::CLIENT); bool res = client == nullptr ? false : client->connection_ready; res;});
        netstat |= 0x40 * ({NetClient *client = global->network_data[LF_ROOF_UHF]->GetClient(NetVertex::ROOFUHF); bool res = client == nullptr ? false : client->connection_ready; res;});
        netstat |= 0x20 * ({NetClient *client = global->network_data[LF_ROOF_XBAND]->GetClient(NetVertex::ROOFXBAND); bool res = client == nullptr ? false : client->connection_ready; res;});
        netstat |= 0x10 * ({NetClient *client = global->network_data[LF_HAYSTACK]->GetClient(NetVertex::HAYSTACK); bool res = client == nullptr ? false : client->connection_ready; res;});
        netstat |= 0x8 * ({NetClient *client = global->network_data[LF_TRACK]->GetClient(NetVertex::TRACK); bool res = client == nullptr ? false : client->connection_ready; res;});

        dbprintlf("%sNETSTAT %d %d %d %d %d (%d)", t_tag,
                                  netstat & 0x80 ? 1 : 0,
                                  netstat & 0x40 ? 1 : 0,
                                  netstat & 0x20 ? 1 : 0,
                                  netstat & 0x10 ? 1 : 0,
                                  netstat & 0x8 ? 1 : 0, netstat);
        bool conn_rdy = false;
        for (int i = 0; i < network_data->GetNumClients(); i++)
        {
            NetClient *client = network_data->GetClient(i);
            if (client->connection_ready) // this client has active connection
            {
                // receive data
                conn_rdy = true;
                NetFrame *netframe = new NetFrame();
                int read_size = netframe->recvFrame(client);

                // parse
                if (read_size >= 0)
                {
                    dbprintlf("Received the following NetFrame:");
                    netframe->print();

                    if (netframe->getDestination() == NetVertex::SERVER)
                    {
                        dbprintlf(BLUE_FG "Received a poll from %d.", (int)netframe->getOrigin());

                        NetFrame *netstat_frame = new NetFrame(NULL, 0, NetType::POLL, (NetVertex)t_index);

                        netstat_frame->setNetstat(netstat);

                        netstat_frame->sendFrame(client);
                        delete netstat_frame;
                    }
                    else
                    {
                        dbprintlf(BLUE_FG "Received a packet from %d addressed to %d.", (int)netframe->getOrigin(), (int)netframe->getDestination());

                        NetClient *destination = global->network_data[(int)netframe->getDestination()]->GetClient(netframe->getDestination());
                        if (destination == nullptr)
                        {
                            dbprintlf(RED_FG "Could not find specified destination.");
                        }
                        else
                        {
                            netframe->sendFrame(destination);
                            delete netframe;
                        }
                    }
                }
                else if (read_size == -404)
                {
                    dbprintlf("Connection to %d is dead", (int)client->self);
                    client->Close(); // connection dead
                }
                else if (errno == EAGAIN)
                {
                    dbprintlf(YELLOW_BG "%sActive connection timed-out (%d).", t_tag, read_size);
                    client->Close();
                }
                // send to destination
                /*
                1. Determine which server to send to
                2. Create destination client: NetClient *dest = global->network_data[LF_DEST_CLIENT]->GetClient(NetVertex for that client type);
                3. sendFrame(dest);
                */
            }
        }
        if (!conn_rdy)
            sleep(1);
    }

    if (!network_data->Accepting())
    {
        dbprintlf(YELLOW_FG "%sNetwork data no longer accepting.", t_tag);
    }

    return NULL;
}