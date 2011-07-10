/* 
 * File:   main.cpp
 * Author: marrk
 *
 * Created on July 6, 2011, 11:38 PM
 */

#include <cstdlib>
#include <iostream>
#include <cassert>
#include <string>
#include <string.h>
#include <vector>
#include <algorithm>
#include <functional>
#include <limits>
#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "ClientCodes.h"


//using namespace std;

/*
 * 
 */

//#define MAXDATASIZE 100
#define PORT "800"
#define M 1000000
const char* ServerIP = "192.168.15.100";

int sockfd, numbytes;
//char buf[MAXDATASIZE];
struct addrinfo hints, *servinfo, *p;
char s[INET6_ADDRSTRLEN];


void* get_in_addr(struct sockaddr* sa) {
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// enriched recv method ---------------------------------------------------
bool receive(int Socket, char* pBuffer, int BufferSize) {
    char* p = pBuffer;
    char* e = pBuffer + BufferSize;
    int result;
    // loop until p is loaded with full size data
    while (p != e) {
        result = recv(Socket, p, e - p, 0);
        if (result == -1)
            return false;
        p += result;
    }
    return true;
}

bool receive(int Socket, long int & Val) {
    return receive(Socket, (char*)& Val, sizeof(Val));
}

bool receive(int Socket, unsigned long int & Val) {
    return receive(Socket, (char*)& Val, sizeof(Val));
}

bool receive(int Socket, double& Val) {
    return receive(Socket, (char*)& Val, sizeof(Val));
}

//--------------------------------------------------------------

int connectServer();



////////////////
int main(int argc, char** argv) {

    std::cout << "LinuxClient\n";
    if (connectServer() != 0)
        return 1;
    
    // up to here a connection with Vicon RTE is open
    // now begin receiving and process data
    try {
        std::vector<std::string> info;
        const int bufferSize = 2040;
        char buff[bufferSize];
        char* pBuff;
        
        // Get info
        // request channel info
        pBuff = buff;
        // save EInfo into buff thru pBuff
        *((long int*) pBuff) = ClientCodes::EInfo;
        // and move pointer to next available position in buff
        pBuff += sizeof(long int);
        
        *((long int*) pBuff) = ClientCodes::ERequest;
        pBuff += sizeof(long int);
        
        // check packet header
        // pBuff - buff is an int equal to 2*sizeof(long int) in this case
        // send request to server
        if (send(sockfd, buff, pBuff - buff, 0) == -1)
            throw std::string("Error Requesting");
        
        long int packet;
        long int type;
        
        // recv and pass data to packet
        if (!receive(sockfd, packet))
            throw std::string("Error receiving.\n");
        //recv and pass data to type
        if (!receive(sockfd, type))
            throw std::string("Bad receiving.\n");
        if (type != ClientCodes::EReply)
            throw std::string("Bad reply type.\n");
        if (packet != ClientCodes::EInfo)
            throw std::string("Bad packet.\n");
        
        long int size;
        
        // recv and pass data to size, thus get the size of future data?
        if (!receive(sockfd, size))
            throw std::string("Wrong size.\n");   // ????
        
        info.resize(size);
        
        std::vector<std::string>::iterator iInfo;
        //info.
        for (iInfo = info.begin(); iInfo != info.end(); iInfo++) {
            long int s;
            char c[255];
            char* p = c;
            
            if (!receive(sockfd, s))
                throw std::string();
            if (!receive(sockfd, c, s))
                throw std::string();
            p += s;
            *p = 0;
            *iInfo = std::string(c);
        }
        
        
        //----------------------------
        //--------- Parse info --------
        // the info packets contain channel names.
        // identify the channels with DOFs?
        std::vector<MarkerChannel> MarkerChannels;
        std::vector<BodyChannel> BodyChannels;
        int FrameChannel;
        
        for (iInfo = info.begin(); iInfo != info.end(); iInfo++) {
            // Extract the channel type
            // looking for the FIRST '<' in each info string
            
            int openBrace = iInfo->find('<');
            // if no '<' found
            if (openBrace == iInfo->npos)
                throw std::string("Bad channel ID");
            
            int closeBrace = iInfo->find('>');
            if (closeBrace == iInfo->npos)
                throw std::string("Bad channel ID");
            
            closeBrace++;             
            // Type is given the content within the first '<>' of each info
            // which is the channel type
            std::string Type = iInfo->substr(openBrace, closeBrace - openBrace);
            
            // Extract the Name
            // the Name is the substring from the begin to the first '<'
            std::string Name = iInfo->substr(0, openBrace);
            // rfind return the LAST occurrence of the specified char
            int space = Name.rfind(' ');
            if (space != Name.npos)
                Name.resize(space);
            
            std::vector<MarkerChannel>::iterator iMarker;
            std::vector<BodyChannel>::iterator iBody;
            std::vector<std::string>::const_iterator iTypes;
            
            iMarker = std::find(MarkerChannels.begin(),
                                MarkerChannels.end(), 
                                Name);
            iBody = std::find(BodyChannels.begin(),
                              BodyChannels.end(),
                              Name);
            
            if(iMarker != MarkerChannels.end())
            {
                //  The channel is for a marker we already have.
                iTypes = std::find( ClientCodes::MarkerTokens.begin(), ClientCodes::MarkerTokens.end(), Type);
                if(iTypes != ClientCodes::MarkerTokens.end())
                    iMarker->operator[](iTypes - ClientCodes::MarkerTokens.begin()) = iInfo - info.begin();
            }
            else
            if(iBody != BodyChannels.end())
            {
                //  The channel is for a body we already have.
                iTypes = std::find(ClientCodes::BodyTokens.begin(), ClientCodes::BodyTokens.end(), Type);
                if(iTypes != ClientCodes::BodyTokens.end())
                    iBody->operator[](iTypes - ClientCodes::BodyTokens.begin()) = iInfo - info.begin();
            }
            else
            if((iTypes = std::find(ClientCodes::MarkerTokens.begin(), ClientCodes::MarkerTokens.end(), Type))
                    != ClientCodes::MarkerTokens.end())
            {
                //  Its a new marker.
                MarkerChannels.push_back(MarkerChannel(Name));
                MarkerChannels.back()[iTypes - ClientCodes::MarkerTokens.begin()] = iInfo - info.begin();
            }
            else
            if((iTypes = std::find(ClientCodes::BodyTokens.begin(), ClientCodes::BodyTokens.end(), Type))
                    != ClientCodes::BodyTokens.end())
            {
                //  Its a new body.
                BodyChannels.push_back(BodyChannel(Name));
                BodyChannels.back()[iTypes - ClientCodes::BodyTokens.begin()] = iInfo - info.begin();
            }
            else
            if(Type == "<F>")
            {
                FrameChannel = iInfo - info.begin();
            }
            else
            {
                //  It could be a new channel type.
            }
            
        }
        
        //------ Up to now all markersnames are in MarkerChannels
        //------ all bodynames are in BodyChannels
        
        
        //----------- Get Data
        // get the data using request/reply protocol.
        
        int i;
        std::vector<double> data;
        data.resize(info.size());
        double timestamp;
        
        std::vector<MarkerData> markerPositions;
        markerPositions.resize(MarkerChannels.size());
        
        std::vector<BodyData> bodyPositions;
        bodyPositions.resize(BodyChannels.size());
        
        // run for 1000 loops
        for (i = 0; i < 1000; i++) {
            
            // use the same routine as when getting channel info
            
            pBuff = buff;

            * ((long int *) pBuff) = ClientCodes::EData;
            pBuff += sizeof(long int);
            * ((long int *) pBuff) = ClientCodes::ERequest;
            pBuff += sizeof(long int);

            if(send(sockfd, buff, pBuff - buff, 0) == -1)
                throw std::string("Error Requesting");

            long int packet;
            long int type;

            //  Get and check the packet header.

            if(!receive(sockfd, packet))
                throw std::string("Error Recieving");

            if(!receive(sockfd, type))
                throw std::string("Error Recieving");

            if(type != ClientCodes::EReply)
                throw std::string("Bad Packet");

            if(packet != ClientCodes::EData)
                throw std::string("Bad Reply Type");

            if(!receive(sockfd, size))
                throw std::string();

            if(size != info.size())
                throw std::string("Bad Data Packet");
            
            // Actually getting the data and store in "data"
            std::vector<double>::iterator iData;
            for (iData = data.begin(); iData != data.end(); iData++) {
                if (!receive(sockfd, *iData))
                    throw std::string();
            }
            
            //- Look up channels -------------
            // get the timestamp
            timestamp = data[FrameChannel];
            
           
            
          /*
           * Get channels corresponding to markers
           * 
           */
            
            std::vector< MarkerChannel >::iterator iMarker;
            std::vector< MarkerData >::iterator iMarkerData;

            for(    iMarker = MarkerChannels.begin(),
                    iMarkerData = markerPositions.begin();
                    iMarker != MarkerChannels.end(); iMarker++, iMarkerData++)
            {
                iMarkerData->X = data[iMarker->X];
                iMarkerData->Y = data[iMarker->Y];
                iMarkerData->Y = data[iMarker->Z];
                if(data[iMarker->O] > 0.5)
                    iMarkerData->Visible = false;
                else
                    iMarkerData->Visible = true;
            }
            
            /* 
             * Get the channels corresponding to bodies
             * the world is Z-up
             * the translational values are in millimeters
             * the rotational values are in radians
             */
            
            std::vector<BodyChannel>::iterator iBody;
            std::vector<BodyData>::iterator iBodyData;
            
            for (       iBody = BodyChannels.begin(),
                        iBodyData = bodyPositions.begin();
                        iBody != BodyChannels.end(); iBody++, iBodyData++) {
                
                iBodyData->TX = data[iBody->TX];
                iBodyData->TY = data[iBody->TY];
                iBodyData->TZ = data[iBody->TZ];
                
                std::cout << "BodyName: " << iBody->Name        << std::endl
                          << "X: "        << iBodyData->TX      << std::endl
                          << "Y: "        << iBodyData->TY      << std::endl
                          << "Z: "        << iBodyData->TZ      << std::endl;
                
                
                /*
                double len, tmp;
                len = sqrt( data[iBody->RX] * data[iBody->RX] +
                            data[iBody->RY] * data[iBody->RY] +
                            data[iBody->RZ] * data[iBody->RZ]);

                iBodyData->QW = cos(len / 2.0);
                tmp = sin(len / 2.0);
                */
                
                
                std::cout << "--------------Frame: " << timestamp << std::endl;
            }
            
            
            
            
            

        }
        
        
        
        
        
        //-----------------------------
        //------STOPPED HERE----------
        //----------------------------
        
        
    }
    catch (const std::string &rMsg) {
        if (rMsg.empty())
            std::cout << "Error!\n";
        else
            std::cout << rMsg.c_str() << std::endl;
    }
    
    if (close(sockfd) != 0 ) {
        std::cout << "Failed to close socket.\n";
        return 1;
    }
    
    return 0;
}



///////////
int connectServer() {
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family     = AF_UNSPEC;
    hints.ai_socktype   = SOCK_STREAM;
    
    // hard code the server's IP and port 
    if ((getaddrinfo(ServerIP, PORT, &hints, &servinfo)) != 0) {
        std::cout << "Failed to get server info.\n";
        return 1;
    }
    
    for (p = servinfo; p != NULL; p = p->ai_next) {
        // create client's socket
        if ((sockfd = socket(p->ai_family, p->ai_socktype, 
                p->ai_protocol)) == -1) {
            std::cout << "Failed to create client socket.\n";
            continue;
        }
        // connect to server
        std::cout << "Trying to connect to server...";
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            std::cout << "Failed to connect to server.\n";
            continue;
        }
        //usleep(0.01 * M);
        break;
    }
    // if still not connected after looping
    if (p == NULL) {
        std::cout << "Failed to connect to server.\n";
        return 2;
    }
    
    std::cout << "Connected.\n";
    
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *) p->ai_addr),
            s, sizeof s);
    
    freeaddrinfo(servinfo);
    
    return 0;
}
