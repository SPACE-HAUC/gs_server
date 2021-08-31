/**
 * @file main.cpp
 * @author Mit Bailey (mitbailey99@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2021.07.26
 * 
 * This program is the Ground Station Network Server (GSNS), a central throughway-hub for handling communications between the three radios and GUI Client. It includes an RX thread for each potential client, and methods of transmission, parsing, and error handling. Communication along the network is done via NetworkFrames.
 * 
 * @copyright Copyright (c) 2021
 * 
 */

// Now sets Netstat.
// Receives and then sends status-filled null packets.
// NOTE: Leave packets destined for the server alone - no configs yet.
//       Frames destined for the server and are NULL are polling for status.
//       Non-NULL frames for the server must be a config (NYI), or an error.

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include "network.hpp"
#include "gss.hpp"
#include "meb_debug.hpp"

int main(int argc, char *argv[])
{
    // Ignores broken pipe signal, which is sent to the calling process when writing to a nonexistent socket (
    // see: https://www.linuxquestions.org/questions/programming-9/how-to-detect-broken-pipe-in-c-linux-292898/
    // and 
    // https://github.com/sunipkmukherjee/example_imgui_server_client/blob/master/guimain.cpp
    // Allows manual handling of a broken pipe signal using 'if (errno == EPIPE) {...}'.
    // Broken pipe signal will crash the process, and it caused by sending data to a closed socket.
    signal(SIGPIPE, SIG_IGN);

    // Create global.
    global_data_t global[1] = {0};

    // Create NUM_PORTS network_data objects with their corresponding ports.
    for (int i = 0; i < NUM_PORTS; i++)
    {
        global->network_data[i] = new NetDataServer((NetPort)((int)NetPort::CLIENT + (10 * i)));
    }

    // Activate each thread's receive ability.
    for (int i = 0; i < NUM_PORTS; i++)
    {
        global->network_data[i]->recv_active = true;
    }

    // Begin receiver threads.
    // 0:Client, 1:RoofUHF, 2: RoofXB, 3: Haystack, 4: Track
    for (int i = 0; i < NUM_PORTS; i++)
    {
        if (pthread_create(&global->pid[i], NULL, gss_network_rx_thread, global) != 0)
        {
            dbprintlf(FATAL "Thread %d failed to start.", i);
            return -1;
        }
        else
        {
            dbprintlf(GREEN_FG "Thread %d started.", i);
        }
    }

    for (int i = 0; i < NUM_PORTS; i++)
    {
        void *status;
        if (pthread_join(global->pid[i], &status) != 0)
        {
            dbprintlf(RED_FG "Thread %d failed to join.", i);
        }
        else
        {
            dbprintlf(GREEN_FG "Thread %d joined.", i);
        }
    }

    // On receive:
    // (If endpoint != Here)
    // - Forward to endpoint.
    // (If endpoint == Here)
    // - Accept, perform relevant actions, and respond.

    // Finished.
    for (int i = 0; i < NUM_PORTS; i++)
    {
        delete global->network_data[i];
    }

    return 1;
}