// g++ -std=c++11 atomikCypher.cpp atomik_emulator.cpp -o emulator

#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <linux/sockios.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <utility>

#include "../atomik_cypher/atomikCypher.h"

using namespace std;

float ver = 0.8;
static int debug = 0;
static int dupesPrinted = 0;
static atomikCypher MiLightCypher;


// this is new arp + mac
static char *ethernet_mactoa(struct sockaddr *addr)
{
    static char buff[256];

    unsigned char *ptr = (unsigned char *) addr->sa_data;

    sprintf(buff, "%02X:%02X:%02X:%02X:%02X:%02X",
            (ptr[0] & 0xff), (ptr[1] & 0xff), (ptr[2] & 0xff),
            (ptr[3] & 0xff), (ptr[4] & 0xff), (ptr[5] & 0xff));

    return (buff);
}

std::string getTime()
{
    timeval curTime;
    time_t now;
    time(&now);
    
    gettimeofday(&curTime, NULL);
    int milli = curTime.tv_usec / 1000;

    char buf[sizeof "2011-10-08T07:07:09"];
    strftime(buf, sizeof buf, "%FT%T", gmtime(&now));
    
    sprintf(buf, "%s.%dZ", buf, milli);
    return buf;
}

std::string createJSON(std::string mac, std::string ip, std::string data, std::string config)
{                                    
    std::string output;
    output = "{\n \"Command\": \"Issue\",\n \"DateTime\": \"" + getTime() + "\",\n ";
    output = output + "\"MAC\": \"" + mac + "\",\n ";
    output = output + "\"IP\": \"" + ip + "\",\n ";
    output = output + "\"Data\": \"" + data + "\",\n ";
    output = output + "\"Configuration\": {\n " + config + " }\n}";
    return output;
}
   
void listen()
{
    fd_set socks;
    int discover_fd, data_fd;
    struct sockaddr_in discover_addr, data_addr, cliaddr;
    char mesg[42];
    char reply[30];

    discover_fd = socket(AF_INET, SOCK_DGRAM, 0);
    bzero(&discover_addr, sizeof(discover_addr));
    discover_addr.sin_family = AF_INET;
    discover_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    discover_addr.sin_port = htons(48899);
    bind(discover_fd, (struct sockaddr *)&discover_addr, sizeof(discover_addr));

    data_fd = socket(AF_INET, SOCK_DGRAM, 0);
    bzero(&data_addr, sizeof(data_addr));
    data_addr.sin_family = AF_INET;
    data_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    data_addr.sin_port = htons(8899);
    bind(data_fd, (struct sockaddr *)&data_addr, sizeof(data_addr));

    if (1)
    {
        FILE *fd;
        size_t s1, s2;
        fd = popen("ifconfig | grep \"inet addr\" | cut -d ':' -f 2 | cut -d ' ' -f 1 | grep -v \"127.0.0.1\" | head -n 1 | tr -d [:space:]", "r");
        s1 = fread(reply, 1, 15, fd);
        reply[s1] = ',';
        s1++;
        fd = popen("ifconfig | grep \"HWaddr\" | cut -d ' ' -f 11 | tr -d [:space:] | tr -d ':' | tr [:lower:] [:upper:]", "r");
        s2 = fread(reply + s1, 1, 12 ,fd);
        reply[s1 + s2] = ',';
    }

    if (debug)
    {
        printf("Reply String: %s\n", reply);
        fflush(stdout);
    }

    while (1)
    {
        socklen_t len = sizeof(cliaddr);

        FD_ZERO(&socks);
        FD_SET(discover_fd, &socks);
        FD_SET(data_fd, &socks);
        if (select(FD_SETSIZE, &socks, NULL, NULL, NULL) >= 0)
        {
            if (FD_ISSET(discover_fd, &socks))
            {                       

			    int n = recvfrom(discover_fd, mesg, 41, 0, (struct sockaddr *)&cliaddr, &len);
                mesg[n] = '\0';

                if (debug)
                {
                    char stri[INET_ADDRSTRLEN];
                    long ipv4 = cliaddr.sin_addr.s_addr;
                    inet_ntop(AF_INET, &ipv4, stri, INET_ADDRSTRLEN);
                    printf("UDP --> Received discovery request (%s) from %s\n", mesg, stri);
                }

                //if (!strncmp(mesg, "Link_Wi-Fi", 41))
                if (!strncmp(mesg, "Link Wifi0", 41))
                {
                    sendto(discover_fd, reply, strlen(reply), 0, (struct sockaddr*)&cliaddr, len);
                }
            }
            
            if (FD_ISSET(data_fd, &socks))
            {
                int n = recvfrom(data_fd, mesg, 41, 0, (struct sockaddr *)&cliaddr, &len);
                mesg[n] = '\0';

                if (n == 2 || n == 3)
                {
                    char str[INET_ADDRSTRLEN];
                    long ip = cliaddr.sin_addr.s_addr;
                    inet_ntop(AF_INET, &ip, str, INET_ADDRSTRLEN);
                                    
                    if (debug)
                    {
                        printf("Connection from %s\n", str);
                        fflush(stdout);
                    }

                    struct arpreq       areq;
                    struct sockaddr_in *sin;
                    struct in_addr      ipaddr;

                    string mac_address;
                    int messagedata;
                    char message [50];
                                    
                    /* Make the ARP request.
                                                        */
                    memset(&areq, 0, sizeof(areq));
                    sin = (struct sockaddr_in *) &areq.arp_pa;
                    sin->sin_family = AF_INET;

                    if (inet_aton(str, &ipaddr) == 0)
                    {
                        fprintf(stderr, "-- Error: bad dotted-decimal IP '%s'.\n", str);
                    }

                    sin->sin_addr = ipaddr;
                    sin = (struct sockaddr_in *) &areq.arp_ha;
                    sin->sin_family = ARPHRD_ETHER;

                    strncpy(areq.arp_dev, "wlan0", 15);

                    if (ioctl(data_fd, SIOCGARP, (caddr_t) &areq) == -1)
                    {
                        perror("-- Error: unable to make ARP request, error");
                    }
                    
                    if (debug)
                    {
                        printf("%s (%s) -> %s\n", str,
                                    inet_ntoa(((struct sockaddr_in *) &areq.arp_pa)->sin_addr), ethernet_mactoa(&areq.arp_ha));
                        printf("UDP --> Received hex value (%02x, %02x, %02x)\n", mesg[0], mesg[1], mesg[2]);
                        fflush(stdout);
                    }
                                        
                    mac_address = ethernet_mactoa(&areq.arp_ha);
                                  
                    messagedata = sprintf (message, "%02x %02x %02x", mesg[0], mesg[1], mesg[2]);
                                    
                    printf("\n");
                                    
                    printf(createJSON(mac_address.c_str(), str, message,  MiLightCypher.getSmartPhoneAtomikJSON(mesg[0], mesg[1], mesg[2])).c_str());

                    printf("\n");
                    fflush(stdout);
                    
                } else {
                   
                    printf("failed! Message Error!");
                    fprintf(stderr, "Message has invalid size %d (expecting 2 or 3)!\n", n);
                    
                } /* End message size check */

            } /* End handling data */

        } /* End select */

    } /* While (1) */

}

void usage(const char *arg, const char *options)
{
    printf("\n");
    printf("Usage: %s [%s]\n", arg, options);
    printf("\n");
    printf("   -h                       Show this help\n");
    printf("   -d                       Show debug output\n");
    printf("\n");
}

int main(int argc, char** argv)
{
   
    const char *options = "hd";
    int c;

    while ((c = getopt(argc, argv, options)) != -1)
    {
        switch (c)
        {
            case 'h':
                usage(argv[0], options);
                exit(0);
                break;
            case 'd':
                debug = 1;
                break;
            default:
                fprintf(stderr, "Error parsing options");
                return -1;
        }
    }
    
    printf("Atomik MiLight Emulator - Version %d\n%.*f", 2, ver);
    printf("Listening to Mi-Light RGBW Band: ( press ctrl-c to end )\n");
    listen();

    return 0;
}