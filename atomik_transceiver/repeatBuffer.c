// sudo g++ -std=c++11 -lcurl -ljsoncpp -L/usr/lib -lmysqlcppconn -I/usr/include/cppconn atomik_cypher/atomikCypher.cpp atomik_emulator/atomik_emulator.cpp -o emulator
// Atomik Repeate Command Buffer - Depreiated
// C / C++
// By Rahim Khoja

// Stops duplicate messages during a 1 second period from slowing down the system. Lag Stopper Attempt.

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <sys/time.h>

struct transmissionData {
  int add1;
  int add2;  
  int command;
  int color;
  int bright;
  int prefix;
};

struct transmission{
  struct transmissionData data;
  double timestamp;
};

class repeatBuffer {

private:
 
  std::vector<transmission> trans;



bool compare_trans( const transmissionData & e1, const transmissionData & e2) {
  if( e1.add1 == e2.add1 &&  e1.add2 == e2.add2 && e1.color == e2.color && e1.bright == e2.bright && e1.prefix == e2.prefix)
    return true;
  return false;
};

void removeOldTransmissions() {
    
    timeval tv;
    gettimeofday (&tv, NULL);
    double currentTime = (tv.tv_sec) + 0.0000001 * tv.tv_usec;
    
    auto it = trans.begin();
    while (it != trans.end()) {
      double elapsedTime = currentTime - (*it).timestamp;
      std::cout << std::fixed << (*it).timestamp << std::endl;
      std::cout << std::fixed << currentTime << std::endl;
      std::cout << std::fixed << elapsedTime << std::endl;
      if ( elapsedTime > 0.350000 ) { 
        std::cout << std::fixed << "deleted: " << (*it).timestamp << std::endl;  
        trans.erase(it);
      } else {
        it++;
      }
    }
    std::cout << "Finished Delete Loop" << std::endl;  
} ;
        
public:
  
bool addTransmission(int add1, int add2, int col, int bri, int pf, int command) {
  std::cout << "Adding Tramsission to Buffer!" << std::endl;
  bool returnVal = false;
  transmissionData newTrans;
  newTrans.add1 = add1;
  newTrans.add2 = add2;
  newTrans.bright = bri;
  newTrans.color = col;
  newTrans.prefix = pf;
  newTrans.command = command;
  removeOldTransmissions();
  auto it = trans.begin();
  std::cout << "Check fro trans" << std::endl;
  while (it != trans.end()) {
    if (compare_trans(newTrans, (*it).data)) {
      returnVal = true;
      trans.erase(it);
    } else {
      it++;
    }
  }
  std::cout << "check loop compete!" << std::endl;
  transmission newTra;
  newTra.data = newTrans;
  timeval tv;
  gettimeofday (&tv, NULL);
  newTra.timestamp = (tv.tv_sec) + 0.0000001 * tv.tv_usec;
  std::cout << newTra.timestamp << std::endl;
  trans.push_back(newTra);
  return returnVal;
  
};
};