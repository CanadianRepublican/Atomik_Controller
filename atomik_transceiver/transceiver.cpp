// sudo g++ -std=c++11 -lrf24-bcm -lcurl -pthread -ljsoncpp -L/usr/lib -lmysqlcppconn -I/usr/include/cppconn atomik_transceiver/PL1167_nRF24.cpp atomik_transceiver/MiLightRadio.cpp atomik_transceiver/transceiver.cpp atomik_cypher/atomikCypher.cpp -o transceiver
// Atomik Transciever
// C / C++
// By Rahim Khoja

// Listens for and Transmits to Mi-Light Bulbs. Listens on the RGB settings, Transmits with both RGB and CCT

#include <cstdlib>
#include <iostream>
#include <string>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <RF24/RF24.h>
#include <list>
#include <thread>
#include <vector>
#include <pthread.h>
#include <atomic>
#include <algorithm> 
#include <sstream>
#include <fstream>
#include <iterator>
#include <curl/curl.h>

#include <mutex>

#include "PL1167_nRF24.h"
#include "MiLightRadio.h"
#include "../atomik_cypher/atomikCypher.h"

#include "mysql_connection.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/writer.h>
#include <jsoncpp/json/value.h>


RF24 radio(RPI_V2_GPIO_P1_22, RPI_V2_GPIO_P1_24, BCM2835_SPI_SPEED_8MHZ);

PL1167_nRF24 prf(radio);
MiLightRadio mlr(prf);

static int debug = 0;
static int dupesPrinted = 0;
std::list<std::string> commandList;
atomikCypher MiLightCypher;

std::mutex commandListMutex;
std::mutex JSONfileMutex;
std::mutex consoleMutex;

std::atomic<bool> disableSocket;

int do_receive = 0;
int do_command = 0;
int do_server = 0;
int do_sync = 0;
int do_desync = 0;
int alreadyRunning = 0;

uint8_t prefix   = 0xB8;
uint8_t rem_p    = 0x00;
uint8_t remote   = 0x01;
uint8_t color    = 0x00;
uint8_t bright   = 0x00;
uint8_t key      = 0x01;
uint8_t seq      = 0x00;
uint8_t resends  =   60;
uint64_t command = 0x00;

int radiomode = 1;
int default_radiomode = 1;

const char *options = "hdsut:n:p:q:r:c:b:k:v:w:";
  
std::thread socketServerThread;
  
int socketPort = 5000;

std::vector<std::string> all_args;
std::vector<std::string> socket_args;

sql::Driver *driver;
sql::Connection *con;
sql::ConnectOptionsMap connection_properties;

enum option_code {
    h,
    d,
    t,
    n,
    p,
    q,
    r,
    s,
    u,
    c,
    b,
    k,
    v,
    w
};

option_code hashit (std::string inString) {
    if (inString == "-h") return h;
    if (inString == "-d") return d;
    if (inString == "-t") return t;
    if (inString == "-n") return n;
    if (inString == "-p") return p;
    if (inString == "-q") return q;
    if (inString == "-r") return r;
    if (inString == "-u") return u;
    if (inString == "-s") return s;
    if (inString == "-c") return c;
    if (inString == "-b") return b;
    if (inString == "-k") return k;
    if (inString == "-v") return v;
    if (inString == "-w") return w;
}

int getCommandListSize()
{
    int z;
    commandListMutex.lock();
    z = commandList.size();
    commandListMutex.unlock();
    return z; 
}

std::list<std::string> split(const char *str, char c = '!')
{
    std::list<std::string> result;

    do
    {
        const char *begin = str;

        while(*str != c && *str)
            str++;

        result.push_back(std::string(begin, str));
    } while (0 != *str++);

    return result;
}

void addCommand(std::string str)
{
        
        std::list<std::string> temp;
     std::string delim = "!";
    if (str.find(delim) != std::string::npos) {
        temp = split(str.c_str());
        commandListMutex.lock();
        commandList.insert(commandList.end(), temp.begin(), temp.end());
        
    commandListMutex.unlock();
    } else {
    
    
    // add command string to bottom element of list
    commandListMutex.lock();
    commandList.push_back (str);
    commandListMutex.unlock();
    }
    return; 
}



std::string getCommand()
{
    // returns the first command sting element in the list
    
    std::string str;
    commandListMutex.lock();
    str = commandList.front();
    commandListMutex.unlock();
    return str;
}


void removeCommand()
{
    // removes the first command string element from the listlong long c;
    commandListMutex.lock();
    commandList.pop_front();
    commandListMutex.unlock();
	return;     
}

std::string Vector2String(std::vector<std::string> vec)
{
    std::stringstream ss;
    for(size_t i = 0; i < vec.size(); ++i)
    {
        if(i != 0)
            ss << ",";
        ss << vec[i];
    }
    return ss.str();
}

std::string int2hex(int x)
{
    char out[2];
    sprintf(out, "%02X", x);
    return std::string(out);
}

std::string int2int(int x)
{
    std::stringstream ss;
    ss << x;
    return ss.str();
}

int hex2int(std::string hexnum) {

        int x = std::strtol(hexnum.c_str(), NULL, 16);
        return x;
}


void runCommand(std::string command) {

    std::string output = command;
    std::replace( output.begin(), output.end(), ' ', ','); // replace all ' ' to ','
    
    addCommand(output);
    //FILE *in;
	//char buff[512];

	//if(!(in = popen(command.c_str(), "r"))){
	//	return;
	//}
    
	//pclose(in);
}


int getAtomikJSONValue(std::string name, std::string atomikJSON) {

        Json::Value root;
        Json::Value conf;
        std::stringstream buffer;
        Json::Reader reader;

        bool parsingSuccessful = reader.parse( atomikJSON.c_str(), root );     //parse process
        if ( !parsingSuccessful )
        {
                std::cout  << "Failed to parse"
                << reader.getFormattedErrorMessages();
                return -1;
        }

        buffer << root.get("Configuration", "error" ) << std::endl;

        parsingSuccessful = reader.parse( buffer, conf );     //parse process
        if ( !parsingSuccessful )
        {
                std::cout  << "Failed to parse"
               << reader.getFormattedErrorMessages();
                return -1;
        }

        if (name == "Address1") {
                std::string output = root.get("Address1", "error" ).asString();

                if (  output == "error" ) {
                        return -1;
                } else {
                        return hex2int(output);
                }
        } else if (name == "Address2") {
                std::string output = root.get("Address2", "error" ).asString();

                if (  output == "error" ) {
                        return -1;
                } else {
                        return hex2int(output);
                }
        } else if (name == "ColorMode") {
                std::string output = conf.get("ColorMode", "error" ).asString();

                if (  output == "error" ) {
                        return -1;
                } else if (output == "RGB256") {
                        return 0;
                } else if (output == "White") {
                        return 1;
                }
        } else if (name == "Color") {
                std::string output = conf.get("Color", "error" ).asString();

                if (  output == "error" ) {
                        return -1;
                } else {
                        return std::stoi(output);
                }
        } else if (name == "Brightness") {
                std::string output = conf.get("Brightness", "error" ).asString();

                if (  output == "error" ) {
                        return -1;
                } else if ( output == "Max" ) {
                        return 100;
                } else if ( output == "Nightlight" ) {
                        return 4;
                } else {
                        return std::stoi(output);
                }
        } else if ( name == "Status") {
                std::string output = conf.get("Status", "error" ).asString();

                if (  output == "error" ) {
                        return -1;
                } else if (output == "On") {
                        return 1;
                } else if (output == "Off") {
                        return 0;
                }
        } else if ( name == "Channel") {
                std::string output = conf.get("Channel", "error" ).asString();

                if (  output == "error" ) {
                        return -1;
                } else {
                        return std::stoi(output);
                }
        } else if ( name == "WhiteTemp") {
                std::string output = conf.get("WhiteTemp", "error" ).asString();

                if (  output == "error" ) {
                        return -1;
                } else {
                        return std::stoi(output);
                }
        }

}

std::string getLocalTimezone() {

    FILE *in;
	char buff[512];

	if(!(in = popen("cat /etc/timezone", "r"))){
		return "";
	}

	while(fgets(buff, sizeof(buff), in)!=NULL){
		std::cout << buff << std::endl;
	}
	pclose(in);
    std::string str(buff);
    str.erase(str.find_last_not_of(" \n\r\t")+1);
    return str;
}

void log2db (sql::Connection *con, int channel, std::string rec_data, int status, int colormode, int color, int whitetemp, int bright, std::string add1, std::string add2, int processed, std::string source)  {
	
	std::string sql_update;
	
	sql_update = "INSERT INTO atomik_commands_received (command_received_source_type, command_received_channel_id, command_received_date, command_received_data, command_received_status, command_received_color_mode, command_received_rgb256, command_received_white_temprature, command_received_brightness, command_received_processed, command_received_ADD1, command_received_ADD2) VALUES (\""+source+"\", "+int2int(channel)+", UTC_TIMESTAMP(6), \""+rec_data+"\", "+int2int(status)+", "+int2int(colormode)+", "+int2int(color)+", "+int2int(whitetemp)+", "+int2int(bright)+", "+int2int(processed)+", \""+add1+"\", \""+add2+"\")";

    try {
        
        sql::Statement *stmt;
        int recordsUpdated;

        stmt = con->createStatement();
        recordsUpdated = stmt->executeUpdate(sql_update);
  
        std::cout << " log2db: " << recordsUpdated << " Records Updated!" << std::endl;
    
        delete stmt;

    } catch (sql::SQLException &e) {
        std::cout << "# ERR: SQLException in " << __FILE__;
        std::cout << "# ERR: " << e.what();
        std::cout << " (MySQL error code: " << e.getErrorCode();
        std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
    }
}

void updateZoneDevicesProperties(sql::Connection *con, int zone, std::string timezone) {

    std::string sql_update;
	
	sql_update = "UPDATE atomik_zone_devices SET zone_device_last_update=UTC_TIMESTAMP(6) WHERE zone_device_zone_id="+int2int(zone)+";";
    try {
        
        sql::Statement *stmt;
        int recordsUpdated;

        stmt = con->createStatement();
        recordsUpdated = stmt->executeUpdate(sql_update);
  
        std::cout << " updateZoneDevicesProperties: ( Zone: " << zone << " ) " << recordsUpdated << " Records Updated!" << std::endl;
    
        delete stmt;

    } catch (sql::SQLException &e) {
        std::cout << "# ERR: SQLException in " << __FILE__;
        std::cout << "# ERR: " << e.what();
        std::cout << " (MySQL error code: " << e.getErrorCode();
        std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
    }
}

void updateCurrentChannel(sql::Connection *con, int channel, int add1, int add2) {

    std::string sql_update;
	
	sql_update = "UPDATE atomik_remotes SET remote_current_channel="+int2int(channel)+" WHERE remote_address1="+int2int(add1)+" && remote_address2="+int2int(add2)+";"; 
    
    try {
        
        sql::Statement *stmt;
        int recordsUpdated;

        stmt = con->createStatement();
        recordsUpdated = stmt->executeUpdate(sql_update);
  
        std::cout << " updateCurrentChannel: ( Remote: " << remote << " ) " << std::endl;
    
        delete stmt;

    } catch (sql::SQLException &e) {
        std::cout << "# ERR: SQLException in " << __FILE__;
        std::cout << "# ERR: " << e.what();
        std::cout << " (MySQL error code: " << e.getErrorCode();
        std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
    }
}



bool validRFAddressCheck(sql::Connection *con, int add1, int add2 ) {
    
    bool success;
    
	std::string sql_select;
	
	sql_select = "SELECT atomik_zones.zone_id, atomik_zone_remotes.zone_remote_remote_id FROM atomik_zones, atomik_remotes, atomik_zone_remotes WHERE atomik_zones.zone_id=atomik_zone_remotes.zone_remote_zone_id && atomik_zone_remotes.zone_remote_remote_id=atomik_remotes.remote_id && atomik_remotes.remote_address1="+int2int(add1)+" && atomik_remotes.remote_address2="+int2int(add2)+";"; 
    
    try {
        
        sql::Statement *stmt;
        int records;
        sql::ResultSet *res;

        stmt = con->createStatement();
        res = stmt->executeQuery(sql_select);
        
        records = res -> rowsCount();
        
        std::cout << " validRFAddressCheck: ( RF: " << add1 << " , " << add2 << " ) "; 
        
        if ( records > 0 ) {
            success = true;
            std::cout << " VALID" << std::endl;
        } else {
            success = false;
            std::cout << " NOT VALID" << std::endl;
        }
        
        delete res;
        delete stmt;

    } catch (sql::SQLException &e) {
        success = false;
        std::cout << "# ERR: SQLException in " << __FILE__;
        std::cout << "# ERR: " << e.what();
        std::cout << " (MySQL error code: " << e.getErrorCode();
        std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
    }
    return success;
}
	

void updateDeviceProperties(sql::Connection *con, int status, int colormode, int brightness, int rgb256, int whitetemp, int transnum, int device) {

    std::string sql_update;
	
	sql_update = "UPDATE atomik_devices SET device_status="+int2int(status)+", device_colormode="+int2int(colormode)+", device_brightness="+int2int(brightness)+", device_rgb256="+int2int(rgb256)+", device_white_temprature="+int2int(whitetemp)+", device_transmission="+int2int(transnum)+" WHERE device_id="+int2int(device)+";";
	
    try {
        
        sql::Statement *stmt;
        int recordsUpdated;

        stmt = con->createStatement();
        recordsUpdated = stmt->executeUpdate(sql_update);
  
        std::cout << " updateDeviceProperties: ( Device: " << device << " ) " << std::endl;
    
        delete stmt;

    } catch (sql::SQLException &e) {
        std::cout << "# ERR: SQLException in " << __FILE__;
        std::cout << "# ERR: " << e.what();
        std::cout << " (MySQL error code: " << e.getErrorCode();
        std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
    }
}

void updateZoneProperties(sql::Connection *con, int zone, int bright, int status, int color, int whitetemp, int colormode, std::string timezone) {
	
	std::string sql_update;
	
	sql_update = "UPDATE atomik_zones SET zone_status="+int2int(status)+", zone_colormode="+int2int(colormode)+", zone_brightness="+int2int(bright)+", zone_rgb256="+int2int(color)+", zone_white_temprature="+int2int(whitetemp)+",zone_last_update = UTC_TIMESTAMP(6) WHERE zone_id="+int2int(zone)+";";
	
    try {
        
        sql::Statement *stmt;
        int recordsUpdated;

        stmt = con->createStatement();
        recordsUpdated = stmt->executeUpdate(sql_update);
  
        std::cout << " Updated " << recordsUpdated << " Records in Zone Table" << std::endl;
    
        delete stmt;

    } catch (sql::SQLException &e) {
        std::cout << "# ERR: SQLException in " << __FILE__;
        std::cout << "# ERR: " << e.what();
        std::cout << " (MySQL error code: " << e.getErrorCode();
        std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
    }
}

// Set Color Brightness to Valid Value
int colorBright ( int bri ) {
	int bright = bri;
	
  	if (bright <= 4) {
    	bright = 4;
	} else if (bright <= 8) {
    	bright = 8;
	} else if (bright <= 12) {
    	bright = 12;
	} else if (bright <= 15) {
    	bright = 15;
	} else if (bright <= 19) {
    	bright = 19;
	} else if (bright <= 23) {
    	bright = 23;
	} else if (bright <= 27) {
    	bright = 27;
	} else if (bright <= 31) {
    	bright = 31;
	} else if (bright <= 35) {
    	bright = 35;
	} else if (bright <= 39) {
    	bright = 39;
	} else if (bright <= 42) {
    	bright = 42;
	} else if (bright <= 46) {
    	bright = 46;
	} else if (bright <= 50) {
    	bright = 50;
	} else if (bright <= 54) {
    	bright = 54;
	} else if (bright <= 58) {
    	bright = 58;
	} else if (bright <= 62) {
    	bright = 62;
	} else if (bright <= 65) {
    	bright = 65;
	} else if (bright <= 69) {
    	bright = 69;
	} else if (bright <= 73) {
    	bright = 73;
	} else if (bright <= 77) {
    	bright = 77;
	} else if (bright <= 81) {
    	bright = 81;
	} else if (bright <= 85) {
    	bright = 85;
	} else if (bright <= 88) {
    	bright = 88;
	} else if (bright <= 92) {
    	bright = 92;
	} else if (bright <= 96) {
    	bright = 96;
	} else if (bright <= 100) {
	    bright = 100;
	} else {
		bright = 100;
	}
  
	return bright;
}

// Set White Brightness To Valid Value
int whiteBright( int bri ) {
	int bright = bri;
	if (bright <= 9) {
	    bright = 9;
	} else if (bright <= 18) {
    	bright = 18;
  	} else if (bright <= 27) {
   	 	bright = 27;
  	} else if (bright <= 36) {
    	bright = 36;
  	} else if (bright <= 45) {
    	bright = 45;
  	} else if (bright <= 54) {
    	bright = 54;
  	} else if (bright <= 63) {
    	bright = 63;
  	} else if (bright <= 72) {
    	bright = 72;
  	} else if (bright <= 81) {
    	bright = 81;
  	} else if (bright <= 90) {
    	bright = 90;
  	} else if (bright <= 100) {
    	bright = 100;
  	} else {
		bright = 100;
  	}
	return bright;
}

// Increment Transmission Number Between 0 and 255
int IncrementTransmissionNum( int number ){
	int trans = number + 1;

	if (trans >= 256) {
		trans = trans - 256;
	}
	return trans;
}

// Transmits Commands To Bulbs
int transmit(int new_b, int old_b, int new_s, int old_s, int new_c, int old_c, int new_wt, int old_wt, int new_cm, int old_cm, int add1, int add2, int tra, int rgb, int cw, int ww) {

    std::string sendcommandbase;
	std::string sendcom;
    std::string initcom;
	int trans = tra;
    int move;
	
	if (cw == 1 && ww == 1 && rgb != 1) {
    sendcommandbase = "/usr/bin/transceiver -t 2 -q " + int2hex(add1) + " -r " + int2hex(add2) + " -c 01";

    // White Bulb Details

	int Brightness[] = {9, 18, 27, 36, 45, 54, 63, 72, 81, 90, 100};  // 11 Elements
    int WhiteTemp[] = {2700, 3080, 3460, 3840, 4220, 4600, 4980, 5360, 5740, 6120, 6500}; // 11 Elements 
	
    if (new_s != old_s) {
      // Status Changed

      trans = IncrementTransmissionNum(trans);

      if (new_s == 1) {
        sendcom = sendcommandbase + " -k " + int2hex((255 - trans)) + " -v " + int2hex(trans) + " -b 08";
		old_b = 9;
      } else {
        sendcom = sendcommandbase + " -k " + int2hex((255 - trans)) + " -v " + int2hex(trans) + " -b 0B";
      }

      runCommand(sendcom);
    } // End Status Change
    
	if (new_s == 1) {
      // Status On

      if (old_cm != new_cm) {
        trans = IncrementTransmissionNum(trans);
	  
        // Color Mode Change
		old_b = 9;
        if (new_cm == 1) {
          sendcom = sendcommandbase + " -k " + int2hex((255 - trans)) + " -v " + int2hex(trans) + " -b 18";
		  
        } else {
          sendcom = sendcommandbase + " -k " + int2hex((255 - trans)) + " -v " + int2hex(trans) + " -b 08";
        }

        runCommand(sendcom);
	  }

	if (new_b != old_b) {
		// Brightness Change
		new_b = whiteBright( new_b );
		int old_pos = std::distance(Brightness, std::find(Brightness, Brightness + 11, old_b));    //   array_search(old_b, Brightness);
		int new_pos = std::distance(Brightness, std::find(Brightness, Brightness + 11, new_b));    //   array_search(new_b, Brightness);
		
		if (new_pos > old_pos) {
			if (new_pos == std::distance(Brightness, std::find(Brightness, Brightness + 11, 100)) ) {
				trans = IncrementTransmissionNum(trans);
				sendcom = sendcommandbase + " -k " + int2hex((255 - trans)) + " -v " + int2hex(trans) + " -b 18";
                runCommand(sendcom);
            } else {
				move = new_pos - old_pos;
				for (int x = 0; x <= move; x++) {
					trans = IncrementTransmissionNum(trans);
					sendcom = sendcommandbase + " -k " + int2hex((255 - trans)) + " -v " + int2hex(trans) + " -b 0C";
                    runCommand(sendcom);
				}
			}
		} else {
			move = old_pos - new_pos;
			for (int x = 0; x <= move; x++) {
				trans = IncrementTransmissionNum(trans);
				sendcom = sendcommandbase + " -k " + int2hex((255 - trans)) + " -v " + int2hex(trans) + " -b 04";
                runCommand(sendcom);
			}
		}
	}

	if (new_wt != old_wt) {
		// White Temp Change

		int old_pos = std::distance(WhiteTemp, std::find(WhiteTemp, WhiteTemp + 11, old_wt));    //   array_search(old_wt, WhiteTemp);
        
		int new_pos = std::distance(WhiteTemp, std::find(WhiteTemp, WhiteTemp + 11, new_wt));    //   array_search(new_wt, WhiteTemp);
		
		if (new_pos > old_pos) {
			move = new_pos - old_pos;
			
			for (int x = 0; x <= move; x++) {
				trans = IncrementTransmissionNum(trans);
				sendcom = sendcommandbase + " -k " + int2hex((255 - trans)) + " -v " + int2hex(trans) + " -b 0f";
                runCommand(sendcom);
			}
		} else {
			move = old_pos - new_pos;
            for (int x = 0; x <= move; x++) {
              trans = IncrementTransmissionNum(trans);
              sendcom = sendcommandbase + " -k " + int2hex((255 - trans)) + " -v " + int2hex(trans) + " -b 0e";
              runCommand(sendcom);
            }
          }
        }
      }
	  
  } else if (cw == 1 && rgb == 1 || ww == 1 && rgb == 1) {
      sendcommandbase = "/usr/bin/transceiver -t 1 -q " + int2hex(add1) + " -r " + int2hex(add2);
      // RGBWW and RGBCW

      if (new_s != old_s) {

        // Status Changed
        trans = IncrementTransmissionNum(trans);

        if (new_s == 1) {
          sendcom = sendcommandbase + " -k 03 -v " + int2hex(trans);
		  old_cm = -1;
		  old_b = 0;
		  
        }        else {
          sendcom = sendcommandbase + " -k 04 -v " + int2hex(trans);
        }
        runCommand(sendcom);
      }
      // End Status Change

      if (new_s == 1) {

        // Status On

        if (old_cm != new_cm) {

          // Color Mode Change

          trans = IncrementTransmissionNum(trans);

          if (new_cm == 1) {
            sendcom = sendcommandbase + " -k 13 -v " + int2hex(trans) + " -c " + int2hex(old_c);
			
          }          else {
            sendcom = sendcommandbase + " -k 03 -v " + int2hex(trans) + " -c " + int2hex(old_c);
			old_b = 0;
          }
          runCommand(sendcom);
        }

        // End Color Mode Change > /dev/null &

        if (new_cm == 0) {
          // Color Mode Color

          if (new_c != old_c ) {
            // Color Change
            trans = IncrementTransmissionNum(trans);

            initcom = sendcommandbase + " -c " + int2hex(new_c) + " -k 03 -v " + int2hex(trans);
            runCommand(initcom);
            trans = IncrementTransmissionNum(trans);

            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -k 0f -v " + int2hex(trans);
            runCommand(sendcom);
          }
          // End Color Change

        }
        // End Color Mode Color

        if (new_b != old_b) {
          // Brightness Change

          trans = IncrementTransmissionNum(trans);

          if (new_b == 4) {
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(129) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 8) {
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(121) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 12) {
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(113) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 15) {
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(105) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 19) {
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(97) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 23) {
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(89) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 27) {
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(81) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 31) {
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(73) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 35) {
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(65) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 39) { //10
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(57) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 42) { //11
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(49) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 46) { //12
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(41) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 50) { //13
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(33) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 54) { //14
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(25) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 58) { //15
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(17) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 62) { //16
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(9) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 65) { //17
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(1) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 69) { //18
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(249) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 73) { //19
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(241) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 77) { // 20
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(233) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 81) { // 21
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(225) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 85) { // 22
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(217) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 88) { // 23
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(209) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 92) { // 24
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(201) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 96) { // 25
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(193) + " -k 0e -v " + int2hex(trans);
           } else if ( new_b == 100) { // 26
            sendcom = sendcommandbase + " -c " + int2hex(new_c) + " -b " + int2hex(185) + " -k 0e -v " + int2hex(trans);
          }
          runCommand(sendcom);
        }

        // End Brightness Change
      }

      // End Status On
    }
    return trans;
  }



bool updateZone(sql::Connection *con, int status, int colormode, int brightness, int rgb256, int whitetemp, int zone, std::string timezone) {
    
    bool success = true;
    int new_s = status;
    int new_cm = colormode;
    int new_c = rgb256;
    int new_b = brightness;
    int new_wt = whitetemp;
    
    std::string sql_select;
    
    sql_select = "SELECT atomik_zones.zone_status, atomik_zones.zone_colormode, atomik_zones.zone_brightness, atomik_zones.zone_rgb256, atomik_zones.zone_white_temprature FROM atomik_zones WHERE atomik_zones.zone_id="+int2int(zone)+";";
    std::cout << sql_select << std::endl;
    try {
        
        sql::Statement *stmt;
        sql::ResultSet *res;

        stmt = con->createStatement();
        res = stmt->executeQuery(sql_select);
        res->next();
        
            std::cout << "Loading Current Zone Data ... " << std::endl;
            /* Access column data by alias or column name */
            
            if (new_s == -1 ) {
                    new_s        = res->getInt("zone_status");
            }
            if (new_cm == -1 ) {
                    new_cm     = res->getInt("zone_colormode");
            }
            if (new_b == -1 ) {
                    new_b    = res->getInt("zone_brightness");
            }
            if (new_c == -1 ) {
                    new_c        = res->getInt("zone_rgb256"); 
            }
            if (new_wt == -1 ) {
                    new_wt     = res->getInt("zone_white_temprature"); 
            }
        
        std::cout << "Status: " << new_s << " - ColorMode: " << new_cm << " - Brightness: " << new_b << " - Color: " << new_c << " - WhiteTemp: " << new_wt << std::endl;
        delete stmt;
        delete res;

    } catch (sql::SQLException &e) {
        success = false;
        std::cout << "# ERR: SQLException in " << __FILE__;
        std::cout << "# ERR: " << e.what();
        std::cout << " (MySQL error code: " << e.getErrorCode();
        std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
    }
    
    
    sql_select = "SELECT atomik_devices.device_id, atomik_devices.device_status, atomik_devices.device_colormode, atomik_devices.device_brightness, atomik_devices.device_rgb256, atomik_devices.device_white_temprature, atomik_devices.device_address1, atomik_devices.device_address2, atomik_device_types.device_type_rgb256, atomik_device_types.device_type_warm_white, atomik_device_types.device_type_cold_white, atomik_devices.device_transmission FROM atomik_zone_devices, atomik_device_types, atomik_devices WHERE atomik_zone_devices.zone_device_zone_id="+int2int(zone)+" && atomik_zone_devices.zone_device_device_id=atomik_devices.device_id && atomik_devices.device_type=atomik_device_types.device_type_id && atomik_device_types.device_type_brightness=1 ORDER BY atomik_devices.device_type ASC;";
       std::cout << sql_select << std::endl;
    try {
        
        sql::Statement *stmt;
        sql::ResultSet *res;

        stmt = con->createStatement();
        res = stmt->executeQuery(sql_select);
          
        while (res->next()) {
            std::cout << "\tUpdating Devices... " << std::endl;
            /* Access column data by alias or column name */
            std::cout << "Device ID: " << res->getInt("device_id") << std::endl;
            
            int old_status        = res->getInt("device_status");
            int old_colormode     = res->getInt("device_colormode");
            int old_brightness    = res->getInt("device_brightness");
            int old_color         = res->getInt("device_rgb256"); 
            int old_whitetemp     = res->getInt("device_white_temprature"); 
            int old_trans_id      = res->getInt("device_transmission");
            int address1          = res->getInt("device_address1");
            int address2          = res->getInt("device_address2"); 
            int type_rgb          = res->getInt("device_type_rgb256");
            int type_ww           = res->getInt("device_type_warm_white");
            int type_cw           = res->getInt("device_type_cold_white");
            int dev_id            = res->getInt("device_id"); 
            
            if ( type_cw == 1 & type_ww == 1 ) {
                new_b = whiteBright(new_b);
            } else {
                    new_b = colorBright(new_b);
            }
            
            int trans_id = transmit(new_b, old_brightness, new_s, old_status, new_c, old_color, new_wt, old_whitetemp, new_cm, old_colormode, address1, address2, old_trans_id, type_rgb, type_cw, type_ww);
            updateDeviceProperties(con, new_s, new_cm, new_b, new_c, new_wt, trans_id, dev_id);
        }
        
        updateZoneProperties(con, zone, new_b, new_s, new_c, new_wt, new_cm, timezone);
        updateZoneDevicesProperties(con, zone, timezone);
        
        std::cout << " updateZone: ( Zone: " << zone << " ) " << std::endl;
    
        delete stmt;
        delete res;

    } catch (sql::SQLException &e) {
        success = false;
        std::cout << "# ERR: SQLException in " << __FILE__;
        std::cout << "# ERR: " << e.what();
        std::cout << " (MySQL error code: " << e.getErrorCode();
        std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
    }
    
    return success;
    
}

int validateRFChannel(sql::Connection *con, int add1, int add2, int chan) {
    int success = -1;
    std::string sql_select;
    int records;
	
    if ( chan == -1 ) {
	    sql_select = "SELECT atomik_zones.zone_id FROM atomik_zones, atomik_remotes, atomik_zone_remotes WHERE atomik_zones.zone_id = atomik_zone_remotes.zone_remote_zone_id && atomik_zone_remotes.zone_remote_remote_id = atomik_remotes.remote_id && atomik_remotes.remote_address1="+int2int(add1)+" && atomik_remotes.remote_address2="+int2int(add2)+" && atomik_remotes.remote_current_channel=atomik_zone_remotes.zone_remote_channel_number;";
    } else {
        sql_select = "SELECT atomik_zones.zone_id, atomik_zone_remotes.zone_remote_remote_id FROM atomik_zones, atomik_remotes, atomik_zone_remotes WHERE atomik_zones.zone_id=atomik_zone_remotes.zone_remote_zone_id && atomik_zone_remotes.zone_remote_remote_id=atomik_remotes.remote_id && atomik_remotes.remote_address1="+int2int(add1)+" && atomik_remotes.remote_address2="+int2int(add2)+" && atomik_zone_remotes.zone_remote_channel_number="+int2int(chan)+";"; 
    }
    
    std::cout << sql_select << std::endl;
    try {
    
        sql::Statement *stmt;
        sql::ResultSet *res;
        stmt = con->createStatement();
        res = stmt->executeQuery(sql_select);
        
        records = res -> rowsCount();
        res -> next();
                        
        std::cout << " validateRFChannel: "; 
        
        if ( records > 0 ) {
            std::cout << " VALID" << std::endl;
            success = res->getInt("zone_id");
        } else {
            std::cout << " NOT VALID" << std::endl;
        }
        
        if ( chan != -1 ) {
            updateCurrentChannel(con, chan, add1, add2); 
        }
        
        delete res;
        delete stmt;
        return success;
    } catch (sql::SQLException &e) {
        return -1;
        std::cout << "# ERR: SQLException in " << __FILE__;
        std::cout << "# ERR: " << e.what();
        std::cout << " (MySQL error code: " << e.getErrorCode();
        std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
    }
    
}
    
int validateJSON (std::string JSONstr) {
    
    int channel = getAtomikJSONValue("Channel", JSONstr);
    int add1 = getAtomikJSONValue("Address1", JSONstr);
    int add2 = getAtomikJSONValue("Address2", JSONstr);
    int valid = validateRFChannel(con, add1 , add2, channel );
    
    return valid;
}

void sendJSON(std::string jsonstr)
{
  
  CURLcode ret;
  CURL *hnd;
  struct curl_slist *slist1;

  slist1 = NULL;
  slist1 = curl_slist_append(slist1, "Content-Type: application/json");

  hnd = curl_easy_init();
  curl_easy_setopt(hnd, CURLOPT_URL, "http://localhost:4200/transceiver");
  curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, jsonstr.c_str());
  curl_easy_setopt(hnd, CURLOPT_USERAGENT, "Atomik Controller");
  curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
  curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
  curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
  curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);

  ret = curl_easy_perform(hnd);

  curl_easy_cleanup(hnd);
  hnd = NULL;
  curl_slist_free_all(slist1);
  slist1 = NULL;
return;

 }


std::string strConcat(std::string a, std::string b)
{
  std::stringstream ss;
  ss << a << b;
  std::string s = ss.str();
  return s;
}

void JSONfilewrite (std::string textjson) 
{
  JSONfileMutex.lock();
  
  char filename[] = "/var/log/atomik/AtomikRadioJSON.log";
  std::ofstream json;

  json.open(filename, std::ios_base::app);
  if (!json ) 
  {
        json.open(filename, std::ios_base::app);
        json << textjson.c_str() << std::endl;
        json.close();
  } else {
  
        json << textjson.c_str() << std::endl;
        json.close();
  }
       
  JSONfileMutex.unlock();
  return;
}

void consoleWrite(std::string input)
{
  consoleMutex.lock();
  printf(input.c_str());
  printf("\n");
  fflush(stdout);
  consoleMutex.unlock();
  return;
}

void usage(const char *arg, const char *options){
  printf("\n");
  printf("Usage: sudo %s [%s]\n", arg, options);
  printf("\n");
  printf("   -h                       Show this help\n");
  printf("   -d                       Show debug output\n");
  printf("   -n NN<dec>               Resends of the same message\n");
  printf("   -p PP<hex>               Prefix value (Disco Mode)\n");
  printf("   -q RR<hex>               First byte of the remote\n");
  printf("   -r RR<hex>               Second byte of the remote\n");
  printf("   -c CC<hex>               Color byte\n");
  printf("   -b BB<hex>               Brightness byte\n");
  printf("   -k KK<hex>               Key byte\n");
  printf("   -v SS<hex>               Sequence byte\n");
  printf("   -t T<int>                Radio Mode ( RGB=1 White=2 New_Bulb_Test=3 )\n");
  printf("   -s                       Sync Bulb ( Requires options -q and -r )\n");
  printf("   -u                       De-Sync Bulb ( Requires options -q and -r )\n");
  printf("   -w SSPPRRRRCCBBKKNN<hex> Complete message to send\n");
  printf("\n");
}

void getOptions(std::vector<std::string> args, int type)
{
    int cint;
    uint64_t tmp;
    std::vector<std::string> arguments = args;
    
    if (type > 0 ) 
    {
        for(int i=0; i < arguments.size(); i++)
        {
            switch (hashit(arguments[i])) 
            {
                case n:
                 tmp = strtoll(arguments[i+1].c_str(), NULL, 10);
                 resends = (uint8_t)tmp;
                 break;
                case p:
                 tmp = strtoll(arguments[i+1].c_str(), NULL, 16);
                 prefix = (uint8_t)tmp;
                 break;
                case q:
                 tmp = strtoll(arguments[i+1].c_str(), NULL, 16);
                 rem_p = (uint8_t)tmp;       
                 break;
                case r:
                 tmp = strtoll(arguments[i+1].c_str(), NULL, 16);
                 remote = (uint8_t)tmp;   
                 break;
                case c:
                 tmp = strtoll(arguments[i+1].c_str(), NULL, 16);
                 color = (uint8_t)tmp;    
                 break;
                case b:
                 tmp = strtoll(arguments[i+1].c_str(), NULL, 16);
                 bright = (uint8_t)tmp;
                 break;
                case k:
                 tmp = strtoll(arguments[i+1].c_str(), NULL, 16);
                 key = (uint8_t)tmp;
                 break;
                case v:
                 tmp = strtoll(arguments[i+1].c_str(), NULL, 16);
                 seq = (uint8_t)tmp;
                 break;
                case t:
                 tmp = strtoll(arguments[i+1].c_str(), NULL, 10);
                 radiomode = (uint8_t)tmp;
                 break;
                case s:
                 do_sync = 1;
                 do_desync = 0;
                 break;
                case u:
                 do_desync = 1;
                 do_sync = 0;
                 break;

            }
        }
    
    } else {
    
        std::vector<const char *> argv(args.size());
        std::transform(args.begin(), args.end(), argv.begin(), [](std::string& str){
            return str.c_str();});
          
        while((cint = getopt(argv.size(), const_cast<char**>(argv.data()), options)) != -1)
	    {
		    switch(cint){
              case 'h':
                usage(argv[0], options);
                exit(0);
                break;
              case 'd':
                if ( type == 0 )
          {
            debug = 1;
          }
        break;
      case 'n':
         if ( type == 0 )
         {
            do_receive = 0;
            do_server = 0;
            do_command = 1;
            tmp = strtoll(optarg, NULL, 10);
            resends = (uint8_t)tmp;
         }
         break;
      case 'p':
          if ( type == 0 )
          {
            do_receive = 0;
            do_server = 0;      
            do_command = 1;
          }
		  tmp = strtoll(optarg, NULL, 16);
            prefix = (uint8_t)tmp;          
          
        break;
      case 'q':
        if ( type == 0 )
        {
            do_receive = 0;
            do_server = 0;      
            do_command = 1;
        } 
        tmp = strtoll(optarg, NULL, 16);
        rem_p = (uint8_t)tmp;       
        break;
      case 'r':
        if ( type == 0 )
        {
            do_receive = 0;
            do_server = 0;      
            do_command = 1;
        }
		tmp = strtoll(optarg, NULL, 16);
        remote = (uint8_t)tmp;   
        break;
      case 'c':
        if ( type == 0 )
        {
            do_receive = 0;
            do_server = 0;      
            do_command = 1;
        }
            tmp = strtoll(optarg, NULL, 16);
            color = (uint8_t)tmp;
        
        break;
      case 'b':
        if ( type == 0 )
        {
            do_receive = 0;
            do_server = 0;      
            do_command = 1;
        }
            tmp = strtoll(optarg, NULL, 16);
            bright = (uint8_t)tmp;
                
        break;
      case 'k':
        if ( type == 0 )
        {
            do_receive = 0;
            do_server = 0;      
            do_command = 1;
        }
            
            tmp = strtoll(optarg, NULL, 16);
            key = (uint8_t)tmp;
        
        break;
      case 'v':
        if ( type == 0 )
        {
            do_receive = 0;
            do_server = 0;      
            do_command = 1;
         }
            tmp = strtoll(optarg, NULL, 16);
            seq = (uint8_t)tmp;
        
        break;
      case 'w':
      if ( type == 0 )
        {
        do_receive = 0;
        do_server = 0;
        do_command = 2;
        command = strtoll(optarg, NULL, 16);
        }
        break;
      case 't':
        tmp = strtoll(optarg, NULL, 10);
        radiomode = (uint8_t)tmp;
        default_radiomode = (uint8_t)tmp;
        break;
      case 's':
        do_sync = 1;
        do_desync = 0;
        do_command = 1;
        break;
      case 'u':
        do_desync = 1;
        do_sync = 0;
        do_command = 1;
        break;
      case '?':
        if (type == 0 ) {
        if(optopt == 'n' || optopt == 'p' || optopt == 'q' || 
           optopt == 'r' || optopt == 'c' || optopt == 'b' ||
           optopt == 'k' || optopt == 'w'|| optopt == 't'){
          fprintf(stderr, "Option -%c requires an argument.\n", optopt);
        }
        else if(isprint(optopt)){
          fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        }
        else{
          fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
        }
        exit(1);
        }
      default:
        fprintf(stderr, "Error parsing options");
        exit(-1);
    }
  }     
}
 if ( radiomode==1 )
  {
     prefix   = 0xB8;
  } else if ( radiomode==2 ) {
     prefix   = 0x5A;
  }

}

void resetVars()
{
  do_receive = 0;
  do_command = 0;
  do_sync    = 0;
  do_desync  = 0;
  if ( radiomode==1 )  
  {
     prefix   = 0xB8;
  } else if ( radiomode==2 ) {
     prefix   = 0x5A;
  }
  rem_p    = 0x00;
  remote   = 0x01;
  color    = 0x00;
  bright   = 0x00;
  key      = 0x01;
  seq      = 0x00;
  resends  =   30;

  command = 0x00;
  
}

std::vector<std::string> String2Vector (std::string vecstring)
{
    std::string str = vecstring;
    std::string delimiter = ",";
    std::vector<std::string> vec;
    size_t pos = 0;
    std::string token;

    while ((pos = str.find(delimiter)) != std::string::npos) {
        token = str.substr(0, pos);
        vec.push_back( token );
        str.erase(0, pos + delimiter.length());
    }
    vec.push_back( str );
    return vec;    
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


std::string createJSON(std::string add1, std::string add2, std::string data, std::string config)
{                                    
    std::string output;
    output = "{\"Command\":\"Issue\",\"DateTime\":\"" + getTime() + "\",";
    output = output + "\"Address1\":\"" + add1 + "\",";
    output = output + "\"Address2\":\"" + add2 + "\",";
    output = output + "\"Data\":\"" + data + "\",";
    output = output + "\"Configuration\":{" + config + "}}";
    return output;
}

void send(uint8_t data[8])
{
  int ret = mlr.setRadioMode(radiomode);
  
  if(ret < 0)
  {
      fprintf(stderr, "Failed to open connection to the 2.4GHz module.\n");
      fprintf(stderr, "Make sure to run this program as root (sudo)\n\n");
      usage(all_args.front().c_str(), options);
      exit(-1);
  }
  
  static uint8_t seq = 1;
  char sendDATA[50];
  char tdata[50];

  uint8_t resends = data[7];
  if(data[6] == 0x00){
    data[6] = seq;
    seq++;
  }

  sprintf(sendDATA, "%02X %02X %02X %02X %02X %02X %02X ", data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
  consoleWrite(strConcat("2.4GHz --> Sending: ", sendDATA));
  JSONfilewrite(strConcat("2.4GHz --> Sending: ", sendDATA));
  if(radiomode == 1) {
	sprintf(tdata, "Color Mode: Color - Resends: %d\n", resends);
  } else {
  sprintf(tdata, "Color Mode: White - Resends: %d\n", resends);
  }
  
  consoleWrite(tdata);
  JSONfilewrite(tdata);
  
  mlr.write(data, 7);
    
  for(int i = 0; i < resends; i++){
    mlr.resend();
  }
}

void send(uint64_t v)
{
  uint8_t data[8];
  data[7] = (v >> (7*8)) & 0xFF;
  data[0] = (v >> (6*8)) & 0xFF;
  data[1] = (v >> (5*8)) & 0xFF;
  data[2] = (v >> (4*8)) & 0xFF;
  data[3] = (v >> (3*8)) & 0xFF;
  data[4] = (v >> (2*8)) & 0xFF;
  data[5] = (v >> (1*8)) & 0xFF;
  data[6] = (v >> (0*8)) & 0xFF;

  send(data);
}

void send(uint8_t color, uint8_t bright, uint8_t key,
          uint8_t remote = 0x01, uint8_t remote_prefix = 0x00,
	  uint8_t prefix = 0xB8, uint8_t seq = 0x00, uint8_t resends = 10)
{
  
  uint8_t data[8];
  data[0] = prefix;
  data[1] = remote_prefix;
  data[2] = remote;
  data[3] = color;
  data[4] = bright;
  data[5] = key;
  data[6] = seq;
  data[7] = resends;

  send(data);
}


void receive()
{ // 1
    	printf("Receiving mode, press Ctrl-C to end\n");
        int ret = ret = mlr.setRadioMode(default_radiomode);
    	while(1)
	{
        	// check if there are any new messages to send! 
        	if(getCommandListSize() == 0)
		{
			char data[50];
            if( default_radiomode != radiomode ) {
			  ret = mlr.setRadioMode(default_radiomode);
            }
 
			if(ret < 0)
			{
				fprintf(stderr, "Failed to open connection to the 2.4GHz module.\n");
				fprintf(stderr, "Make sure to run this program as root (sudo)\n\n");
				usage(all_args.front().c_str(), options);
				exit(-1);
			}
            
			if(mlr.available()) 
			{
               			uint8_t packet[7];
               			size_t packet_length = sizeof(packet);
               			mlr.read(packet, packet_length);
               			if (packet[5] > 0) 
               			{
                 			sprintf(data, "%02X %02X %02X %02X %02X %02X %02X", packet[0], packet[1], packet[2], packet[3], packet[4], packet[5], packet[6]);
                    			std::string output = createJSON(int2hex(packet[1]), int2hex(packet[2]), data, MiLightCypher.getRadioAtomikJSON(packet[5], packet[3], packet[4]));
                                int zone = validateJSON (output);
                                if (zone == -1 ) {
                                        log2db (con, 0, data, 0, 0, 0, 0, 0, int2hex(packet[1]), int2hex(packet[2]), 0, "Radio");
                                } else {
                                    if ( updateZone(con, getAtomikJSONValue("Status", output), getAtomikJSONValue("ColorMode", output), getAtomikJSONValue("Brightness", output), getAtomikJSONValue("Color", output), getAtomikJSONValue("WhiteTemp", output), zone, getLocalTimezone()) ) { 
                                            std::cout << "Zone Updated!" << std::endl;
                                    } else {
                                            std::cout << "Error! Zone Not Updated!" << std::endl;
                                    }
                                } 
                                // sendJSON(output);
                                JSONfilewrite(output);
                    			consoleWrite(output);
                		}
           		}
           
            		int dupesReceived = mlr.dupesReceived();
			usleep(50000);
 
	      	} else {
                       
        		std::string comandSTR = getCommand();     
			
       		     	if (debug) 
			{			
				consoleWrite(strConcat("Command Processed: ", comandSTR));
			}
            
            		getOptions(String2Vector(comandSTR), 1);
           
			if ( do_sync == 1 && radiomode == 1 ) {
               			for(int i=0; i<8; i++) {
               				send(0xd3, 0xe1, 0x03, remote, rem_p, prefix, seq, 30);
               				usleep(350);
               			}
            		} else if ( do_desync == 1 && radiomode == 1 ) {
               			send(0xd3, 0xe1, 0x03, remote, rem_p, prefix, seq, 30);

               			for(int i=0; i<10; i++) {
               				send(0xd3, 0xe1, 0x13, remote, rem_p, prefix, seq, 30);
               				usleep(5000);
               			}

            		} else if ( do_sync == 1 && radiomode == 2 ) {
               			for(int i=0; i<5; i++) {
               				send(0x01, 0x08, 0x03, remote, rem_p, 0x5a, seq, 30);
               				usleep(350);
               			}

          	  	}  else if ( do_desync == 1 && radiomode == 2 ) {
               			uint8_t bseq = 0xf9;
               			for(int i=0; i<5; i++) {
               				send(0x01, 0x08, bseq, remote, rem_p, 0x5a, seq, 10);
               				bseq = bseq - 1;
                      			usleep(155000);
               			}

            		} else {
               			send(color, bright, key, remote, rem_p, prefix, seq, resends);
            		}
                     
            		removeCommand();
            		resetVars();
       		}
	}
}

void socketConnect(int type , std::string data)
{
    int sock;
    struct sockaddr_in server;
    char serverData[256];
    
    int ty = type;
    if (debug) 
	{			
		consoleWrite(strConcat("Socket Connect Data: ", data));
	}
    
    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1)
    {
        
    }
	if (debug) 
	{
		consoleWrite("Socket Created"); 
    }
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons( socketPort );
 
    //Connect to remote server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("connect failed. Error");
        alreadyRunning = 0;
        return;
    }
    if (debug) 
	{
		consoleWrite("Connected"); 
	}
     
    //keep communicating with server
    while(1)
    {
        bzero(serverData, 256);
        
        //Receive a reply from the server
        if( recv(sock , serverData , 256 , 0) < 0)
        {
            consoleWrite("Receive Failed"); 
            break;
        }
               
        std::string sData(serverData);
                                 
        if (ty == 1)
        {	
			if (debug) 
			{
				consoleWrite(strConcat("Sending Arg String: ", Vector2String(all_args)));
			}
            if( send(sock , Vector2String(all_args).c_str() , strlen(Vector2String(all_args).c_str()) , 0) < 0)
            {
                perror("Send to Atomik Transceiver Failed.");
                exit(1);
            }
        } 
        
        if (debug) 
		{
			consoleWrite(sData);
        }
        
        if( sData.find("Atomik") != std::string::npos )
        {
            if (debug) 
			{
				consoleWrite("Atomik Server already Running!");
            }
			alreadyRunning = 1;
        } else {
            perror("Atomik Transceiver Socket Busy.");
            exit(1);
        }
        
        break;
    }
     
    close(sock);
}


void socketCommand ( std::atomic<bool> & quit )
{

    int opt = 1;
    int master_socket , addrlen , new_socket , client_socket[5] , max_clients = 5 , activity, i , valread , sd;
    int max_sd;
    struct sockaddr_in address;
    struct timeval tv;
      
    char buffer[1025];  //data buffer of 1K
      
    //set of socket descriptors
    fd_set readfds;
      
    std::string welcomMessage = "Atomik Tranceiver V0.5 alpha";
  
    //initialise all client_socket[] to 0 so not checked
    for (i = 0; i < max_clients; i++) 
    {
        client_socket[i] = 0;
    }
      
    //create a master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) 
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
  
    //set master socket to allow multiple connections , this is just a good habit, it will work without this
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
  
    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( socketPort );
      
    //bind the socket to localhost 
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) 
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
	if (debug) {
		char listninSTR[50];
		sprintf(listninSTR, "Listener on port %d \n", socketPort);
		consoleWrite(listninSTR);
    }
    
     
    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
      
    //accept the incoming connection
    addrlen = sizeof(address);
    if (debug) {
		consoleWrite("Waiting for connections ...");
    }
	
    while(!quit) 
    {
        // reset the time value for select timeout
        tv.tv_sec = 0;
        tv.tv_usec = 1000 * 1000;
   
        //clear the socket set
        FD_ZERO(&readfds);
  
        //add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;
         
        //add child sockets to set
        for ( i = 0 ; i < max_clients ; i++) 
        {
            //socket descriptor
            sd = client_socket[i];
             
            //if valid socket descriptor then add to read list
            if(sd > 0)
                FD_SET( sd , &readfds);
             
            //highest file descriptor number, need it for the select function
            if(sd > max_sd)
                max_sd = sd;
        }
  
        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select( max_sd + 1 , &readfds , NULL , NULL , &tv);
    
        if ((activity < 0) && (errno!=EINTR)) 
        {
            consoleWrite("Select Error");
        }
          
        //If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)) 
        {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            
            char outputchar[100];
            //inform user of socket number - used in send and receive commands
            sprintf(outputchar, "New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
            
           //send new connection greeting message
            if( send(new_socket, welcomMessage.c_str(), strlen(welcomMessage.c_str()), 0) != strlen(welcomMessage.c_str()) ) 
            {
                perror("send");
            }
              if (debug) {
				consoleWrite("Welcome message sent successfully");
			  }
             
            //add new socket to array of sockets
            for (i = 0; i < max_clients; i++) 
            {
                //if position is empty
                if( client_socket[i] == 0 )
                {
                    client_socket[i] = new_socket;
                    char txtsocket[50];
                    sprintf(txtsocket, "Adding to list of sockets as %d\n" , i);
					if (debug) {                    
						consoleWrite(txtsocket);
					}
					break;
                }
            }
        }
          
        //else its some IO operation on some other socket :)
        for (i = 0; i < max_clients; i++) 
        {
            sd = client_socket[i];
              
            if (FD_ISSET( sd , &readfds)) 
            {
                //Check if it was for closing , and also read the incoming message
                if ((valread = read( sd , buffer, 1024)) == 0)
                {
                    //Somebody disconnected , get his details and print
                    getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                    if (debug) 
                    {
                        char debugtxt[100];
                         sprintf(debugtxt, "Host disconnected , ip %s , port %d " , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
                         consoleWrite(debugtxt);
                    } 
                    
                    //Close the socket and mark as 0 in list for reuse
                    close( sd );
                    client_socket[i] = 0;
                }
                  
                //Echo back the message that came in
                else
                {
                    //set the string terminating NULL byte on the end of the data read
                    buffer[valread] = '\0';
                    send(sd , buffer , strlen(buffer) , 0 );
                    std::string commandSTR (buffer);
                    addCommand(commandSTR);
                    if (debug) 
                    {
                        consoleWrite(strConcat("Socket Server Receiving Command: ", commandSTR));
                    }
                    
                    if(commandSTR.find ("\n"))
                    {
                      getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                      if (debug) {
                          char debugtxt[100];
                          sprintf(debugtxt, "Host disconnected , ip %s , port %d " , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
                          consoleWrite(debugtxt);
                      }
                      close( sd );
                      client_socket[i] = 0;
                    }
                    
                }
            }
        }
    }
      
}

int main(int argc, char** argv)
{
        
        
    try {
        
        sql::Statement *stmt;
        int recordsUpdated;
        
        
        connection_properties ["hostName"] = std::string("localhost");      
        connection_properties ["userName"] = std::string("root");
        connection_properties ["password"] = std::string("raspberry");
        connection_properties ["schema"] = std::string("atomik_controller");
        connection_properties ["OPT_RECONNECT"] = true;
        

        /* Create a connection */
        driver = get_driver_instance();
        con = driver->connect(connection_properties);
    
     } catch (sql::SQLException &e) {
        std::cout << "# ERR: SQLException in " << __FILE__;
        std::cout << "# ERR: " << e.what();
        std::cout << " (MySQL error code: " << e.getErrorCode();
        std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
    }
    
    all_args = std::vector<std::string>(argv, argv + argc);
    
    do_receive = 1;
    do_server = 1;
    
    getOptions(all_args, 0);
    
    if (debug) 
    {
        char argstrout[100];
        sprintf(argstrout, "Arg String: %s", Vector2String(all_args).c_str());
        consoleWrite(argstrout);
    }
 
    if(do_server) 
    {
        socketConnect(0,"");
        if(alreadyRunning) 
        { 
           perror("Atomik Transceiver Already Running!");
           exit(1);
        }
       
        disableSocket = false;
        socketServerThread = std::thread(socketCommand, std::ref(disableSocket));
    }
  
    if(do_receive)
    {
        
        receive();
    }
 
    if(do_command>0)
    {
        socketConnect(0, "");
        if(alreadyRunning) { 
            socketConnect(1, Vector2String(all_args));
            exit(1);
        } 
        
        if (do_command==2) 
        {
            send(command);
        } else {
            if ( do_sync == 1 && radiomode == 1 ) {
                for(int i=0; i<8; i++) {
                     send(0xd3, 0xe1, 0x03, remote, rem_p, prefix, seq, 30);
                     usleep(350);
                } 
            } else if ( do_desync == 1 && radiomode == 1 ) {
                send(0xd3, 0xe1, 0x03, remote, rem_p, prefix, seq, 30);
                for(int i=0; i<10; i++) {
                     send(0xd3, 0xe1, 0x13, remote, rem_p, prefix, seq, 30);
                     usleep(5000);
                }
    
            } else if ( do_sync == 1 && radiomode == 2 ) {
                for(int i=0; i<5; i++) {
                     send(0x01, 0x08, 0x03, remote, rem_p, prefix, seq, 30);
                     usleep(350);
                }

            }  else if ( do_desync == 1 && radiomode == 2 ) {
                uint8_t bseq = 0xf9; 
		for(int i=0; i<5; i++) {
                     send(0x01, 0x08, bseq, remote, rem_p, prefix, seq, 30);
                     bseq = bseq - 1; 
			usleep(155000);
                }

            } else {
                send(color, bright, key, remote, rem_p, prefix, seq, resends);
            }        
        }
       
    }
   
    if(do_server) 
    {
        disableSocket = true;
        socketServerThread.join();
    }
    
    delete con;
    return 0;
}

