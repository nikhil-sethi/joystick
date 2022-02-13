// ================= joystick_to_socket.cpp ================= //
// This file takes in joystick input from a device at /dev/input/js0 and outputs the appropriate map to udp:127.0.0.1:9004

#include "joystick.hh"	// github.com/drenoakes/joystick.git Thank you

// socket stuffs
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <cstring>

#include <iostream>
#include <unistd.h>

static char IP[32] =  "127.0.0.1";
static uint16_t PORT = 9004;
// port at which betaflight SITL listens to a client and accepts RC commands

static const uint8_t numChannels = 8; 
// Change this size accordingly. But don't forget to change it in betaflight target as well

static char joy_map[numChannels] = {0,1,2,3,4,5,6,7}; 
// this map converts the joystick axis to rc axis at the i'th location . It might be different for you depending on your joystick company.

struct rcPacket{ // RC packet compatible with betaflight SITL: target.h and airim: BetaflightApi.hpp
	double timestamp;	// 8 bits
	uint16_t channel[numChannels]; //16 bits
};

struct udpLink{
    int fd;
    struct sockaddr_in si;
    struct sockaddr_in recv;
    int port;
    char* addr;
} ;
/*
@param curr_min = the joystick manufacturer's minimum axis value
@param curr_max = the joystick manufacturer's maximum axis value
@param targ_min = Your remapped minimum axis value
@param targ_max = Your remapped maximum axis value
*/
int normalise(int val, int targ_min, int targ_max, int curr_min = -32767, int curr_max = 32767){
	
	return targ_min + (targ_max-targ_min)*(val-curr_min)/(curr_max-curr_min);
}

void joystick_read(Joystick* joystick, rcPacket* pkt){
	JoystickEvent event;
    	if (joystick->sample(&event)){
    		pkt->timestamp = event.time;
    		// (-32767, 32767) --> (1000,2000)
		pkt->channel[joy_map[event.number]] = normalise(event.value, 1000, 2000);
	}				
}
// taken from betaflight sitl
int udpInit(udpLink* link, const char* addr, int port){
	int one = 1;

	if ((link->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		return -2;
	}

	setsockopt(link->fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)); // can multi-bind
	fcntl(link->fd, F_SETFL, fcntl(link->fd, F_GETFL, 0) | O_NONBLOCK); // nonblock

	memset(&link->si, 0, sizeof(link->si));
	link->si.sin_family = AF_INET;
	link->si.sin_port = htons(port);
	link->port = port;
	link->si.sin_addr.s_addr = inet_addr(addr);
	
	return 0;

}
void socket_send(udpLink* link, rcPacket* pkt){
	sendto(link->fd, pkt, sizeof(rcPacket), 0, (struct sockaddr *)&link->si, sizeof(link->si));
}

int main(int argc, char* argv[]){

	// create message
	rcPacket pkt;
	pkt.timestamp = 100; // just some random init values
	for (int i=0;i <numChannels; i++){
		pkt.channel[i] = 1000; // just some random init values
	}

	// create message reader aka joystick
	Joystick joystick("/dev/input/js0"); // change this accordingly 
	  if (!joystick.isFound()){
	    std::cout << "Cannot open joystick. Terminating\n";
	    exit(1);
	  }

	// create message sender aka udp output
	udpLink sock;
	int ret = udpInit(&sock, IP, PORT);
	if (ret!=0) {
		std::cout<<"Could not create udp port. Terminating"<<std::endl;
		exit(1);
	}
	while (true){
		// read message
		joystick_read(&joystick, &pkt);	// could make this function threaded. Need to experiment
		
		// send message
		socket_send(&sock, &pkt);
		
		// comment out the line below for debugging
		//std::cout<<sizeof(pkt)<<" "<<pkt.channel[0]<<" "<<pkt.channel[1]<<" "<<pkt.channel[2]<<" "<<pkt.channel[3]<<" "<<std::endl;
		usleep(100); // need to experiment with this value.
	}
	
}
