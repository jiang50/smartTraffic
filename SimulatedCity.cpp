#include "SimulatedCity.h"
#include <vector>
#include <iostream>
#include <thread>         // std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds
#include <sstream>
#include <algorithm> 


using namespace std;
using namespace std::chrono; 

/*
Sychronized:

time    0s            1s,2s,3s,4s,                   5s                           6s, 7s, 8s, 9s,                 10s
event  sendCarInfo                     updateCarInfo->get/updateLight->sendcarinfo                      updateCarInfo->get/updateLight->sendcarinfo

*/

void SimulatedCity::sychronizedRun() {
    std::ofstream myfile;
    myfile.open("example.csv");
    myfile << "time,totalArrived,justArrived\n";
    printCurState(myfile);
    
    if (!sendCarInfo()) {
        cout << "Failed to update car info" << endl;
        return;
    }
    while (finishedCars < numCars) {
    //    this_thread::sleep_for (chrono::seconds(1));
        curTime += 5;
        updateCarInfo();
        getTrafficLights();        
        updateTrafficLights();
        printCurState(myfile);
        if (!sendCarInfo()) {
            cout << "Failed to update car info" << endl;
            return;
        }
        
    }
    getFinalStat();
    printFinishInfo();

}

/*
Asychronized:

time        0s       1s    2s    3s    4s           5s           6s     7s     8s      9s        10s
car    sendCarInfo   . . . . . . .       updateCarInfo->sendcarinfo . . . . . . .        updateCarInfo->sendcarinfo . . . . . . . . 
light                          getLight. . . . . updateLight                getLight . . . . . updateLight
*/

void SimulatedCity::asychronizedRun() {
    
    std::ofstream myfile;
    myfile.open("example2.csv");
    printCurState(myfile);
    while (finishedCars < numCars) {
    //    sendCarInfo();
        thread carInfo(&SimulatedCity::sendCarInfo, this);
        this_thread::sleep_for (chrono::seconds(3));
        
        curTime += 3;
//        getTrafficLights();
        thread getLight(&SimulatedCity::getTrafficLights, this);
        this_thread::sleep_for (chrono::seconds(2));
        carInfo.join();
        getLight.join();
        curTime += 2;
        updateCarInfo();
        updateTrafficLights();
        printCurState(myfile);
    }

    getFinalStat();
    printFinishInfo();
}

void SimulatedCity::getFinalStat() {
    
    int totalWait = 0, totalTime = 0;
    for (int i = 0 ; i < cars.size() ; i++) {
        totalTime += cars[i].getTotalTime();
        totalWait += cars[i].getWaitTime();
    }
 //   cout << totalTime << " " << totalWait << endl;
    avgTime = totalTime / cars.size();
    avgWait = totalWait / cars.size();
    percentWait = (double)totalWait / totalTime;
}

void SimulatedCity::printFinishInfo() {
    cout << endl;
    cout << "Total time spent: " << curTime << "s" << endl;
    cout << "Average time each car spent to arrive: " << avgTime << "s" <<endl;
    cout << "Average time each car wait: " << avgWait << "s" <<endl;
    cout << "Total percentage of waitting time: " << percentWait << endl;

}

void SimulatedCity::printCurState(ofstream& myfile) {
    cout << curTime << "s: ";
    cout << waittingCars << " cars are waiting, ";
    cout << runningCars << " cars are running, ";
    cout << finishedCars << " cars arrived.";
    cout << justArrived << " cars just arrived. " << endl;
    myfile << to_string(curTime) + "," + to_string(finishedCars) + "," + to_string(justArrived) + "\n";
}

void SimulatedCity::initializeLights(int ml) {
    cout << "Generating map..." << endl;
    trafficLights.resize(mapSize);
    updatedLights.resize(mapSize);
    for (int i = 0 ; i < mapSize ; i++) {
        for (int j = 0 ; j < mapSize ; j++) {
            trafficLights[i].push_back(Intersection(i, j, ml));
            updatedLights[i].push_back(2); //placeholder
        }
    }
    cout << "Finish generating map" << endl;
}

void SimulatedCity::initializeCars() {
    cout << "Generating cars..." << endl;
    for (int i = 0 ; i < numCars ; i++) {
        cars.push_back(Car(i, len, mapSize));
    }
    cout << "Finish generating cars and routes" << endl;
}

/*
########################################
########### local regular ##############
########################################
*/

SimulatedCityLocalRegular::SimulatedCityLocalRegular(int mapsize, int numcars, int l, int ml) {
    mapSize = mapsize;
    numCars = numcars;
    len = l;
    finishedCars = 0;
    runningCars = numCars;
    waittingCars = 0;
    curTime = 0;
    initializeLights(ml);
    initializeCars();
}


bool SimulatedCityLocalRegular::sendCarInfo() {
    return true;
}

void SimulatedCityLocalRegular::updateCarInfo() {
    int x,y;
    int state;
    int oldFinished = finishedCars;
    finishedCars = 0;
    runningCars = 0;
    waittingCars = 0;
    
    for (int i = 0 ; i < cars.size() ; i++) {
        x = cars[i].getNextx();
        y = cars[i].getNexty();
        cars[i].update(trafficLights[x][y].getLight());
        state = cars[i].getState();
        if (state == 0) {
            finishedCars++;
        }
        else if (state == 1) {
            runningCars++;
        }
        else {
            waittingCars++;
        }

    }
    justArrived = finishedCars - oldFinished;
}


bool SimulatedCityLocalRegular::getTrafficLights() {
    return true;
}


void SimulatedCityLocalRegular::updateTrafficLights() {
    for (int i = 0 ; i < mapSize ; i++) {
        for (int j = 0 ; j < mapSize ; j++) {
            trafficLights[i][j].checkAndSwitch();
        }
    }
}

void SimulatedCityLocalRegular::run() {
    sychronizedRun();
}

/*
################################
#######local smart##############
################################
*/

SimulatedCityLocalSmart::SimulatedCityLocalSmart(int mapsize, int numcars, int l, int ml) {
    mapSize = mapsize;
    numCars = numcars;
    len = l;
    finishedCars = 0;
    runningCars = numCars;
    waittingCars = 0;
    curTime = 0;
    initializeLights(ml);
    initializeCars();
    smartServer = new SmartServer(mapSize);
}

bool SimulatedCityLocalSmart::sendCarInfo() {
    for (int i = 0 ; i < cars.size() ; i++) {
        smartServer->receiveCarInfo(i, cars[i].getDir(), cars[i].getNextx(), cars[i].getNexty(), cars[i].getTime(), cars[i].isArrived());
    }
 //   cout << "finish sending car info" << endl;
    return true;
}

void SimulatedCityLocalSmart::updateCarInfo() {
    int x,y;
    int state;
    int oldFinished = finishedCars;
    finishedCars = 0;
    runningCars = 0;
    waittingCars = 0;
    
    for (int i = 0 ; i < cars.size() ; i++) {
        x = cars[i].getNextx();
        y = cars[i].getNexty();
        cars[i].update(trafficLights[x][y].getLight());
        state = cars[i].getState();
        if (state == 0) {
            finishedCars++;
        }
        else if (state == 1) {
            runningCars++;
        }
        else {
            waittingCars++;
        }

    }
    justArrived = finishedCars - oldFinished;

}

bool SimulatedCityLocalSmart::getTrafficLights() {
    updatedLights = smartServer->getTtrafficLights();
    return true;
}

void SimulatedCityLocalSmart::updateTrafficLights() {
    for (int i = 0 ; i < mapSize ; i++) {
        for (int j = 0 ; j < mapSize ; j++) {
            trafficLights[i][j].update(updatedLights[i][j]);
        }
    }
}

void SimulatedCityLocalSmart::run() {
    sychronizedRun();
}

/*
###################################
########## remote ################
#################################
*/

SimulatedCityRemote::SimulatedCityRemote(int mapsize, int numcars, int l, int ml, int bz) {
    mapSize = mapsize;
    numCars = numcars;
    len = l;
    finishedCars = 0;
    runningCars = numCars;
    waittingCars = 0;
    curTime = 0;
    batch = bz;
    updatedLights = vector<vector<int>>(mapSize, vector<int>(mapSize, 2));
    initializeLights(ml);
    initializeCars();
}

bool SimulatedCityRemote::sendCarInfo() {
    char *host = (char*)"localhost";
    Client *c = new Client(53044, host);
    string message;
    for (int i = 0 ; i < cars.size() ; i++) {
    //    cout << cars[i].getId() << endl;
        message += cars[i].serialize();
    }
 //   cout << message << endl;
    c->connectToServer();
    c->sendCarInfo(message);
    cout << "finish sending car info" << endl;
    return true;

}

void SimulatedCityRemote::updateCarInfo() {
    int x,y;
    int state;
    finishedCars = 0;
    runningCars = 0;
    waittingCars = 0;
    justArrived = 0;
    for (int i = 0 ; i < cars.size() ; i++) {
        x = cars[i].getNextx();
        y = cars[i].getNexty();
        cars[i].update(trafficLights[x][y].getLight());
        state = cars[i].getState();
        if (state == 0) {
            finishedCars++;
            justArrived++;
        }
        else if (state == 1) {
            runningCars++;
        }
        else {
            waittingCars++;
        }

    }
}

bool SimulatedCityRemote::getTrafficLights() {
    char *host = (char*)"localhost";
    Client *c = new Client(53044, host);
    cout << "get traffic lights...";
    c->connectToServer();
    cout << "connected!" << endl;
    string tl = c->getTrafficLights();
 //   cout << tl << endl;
    istringstream iss(tl);
    int light;
    for (int i = 0 ; i < mapSize ; i++) {
        for (int j = 0 ; j < mapSize ; j++) {
            iss >> light;
            updatedLights[i][j] = light;
        }
    }
    return true;

}

void SimulatedCityRemote::updateTrafficLights() {
    for (int i = 0 ; i < mapSize ; i++) {
        for (int j = 0 ; j < mapSize ; j++) {
            trafficLights[i][j].update(updatedLights[i][j]);
        }
    }
}

void SimulatedCityRemote::run() {
    asychronizedRun();

}

/*
########################################
############# Cloud ###################
#######################################
*/

SimulatedCityCloud::SimulatedCityCloud(int mapsize, int numcars, int l, int ml, string h) {
    mapSize = mapsize;
    numCars = numcars;
    len = l;
    finishedCars = 0;
    runningCars = numCars;
    waittingCars = 0;
    curTime = 0;
    host = h;
   // updatedLights = vector<vector<int>>(mapSize, vector<int>(mapSize, 2));
    initializeLights(ml);
    initializeCars();
}

bool SimulatedCityCloud::sendCarInfo() {
    const char *h = host.c_str();
    auto start = high_resolution_clock::now(); 
    Client *c = new Client(80, h);
    string message;
    string header = "POST /api/UpdateLocation HTTP/1.1\r\nHost: smart-traffic-node.azurewebsites.net\r\nContent-Type: text/plain\r\nContent-Length: ";
    for (int i = 0 ; i < cars.size() ; i++) {
    //    cout << cars[i].getId() << endl;
        message += cars[i].serialize();
    }
    header += to_string(message.size());
    header += "\r\n\r\n";
    message = header + message;
 //   cout << message << endl;
    c->connectToServer();
    c->sendCarInfo(message);

    auto stop = high_resolution_clock::now(); 
    auto duration = duration_cast<microseconds>(stop - start); 
  
    cout << "Send car info take: "<< duration.count() << " microseconds" << endl; 
    return true;
}

void SimulatedCityCloud::updateCarInfo() {
    int x,y;
    int state;
    int oldFinished = finishedCars;
    finishedCars = 0;
    runningCars = 0;
    waittingCars = 0;
    
    for (int i = 0 ; i < cars.size() ; i++) {
        x = cars[i].getNextx();
        y = cars[i].getNexty();
        cars[i].update(trafficLights[x][y].getLight());
        state = cars[i].getState();
        if (state == 0) {
            finishedCars++;
        }
        else if (state == 1) {
            runningCars++;
        }
        else {
            waittingCars++;
        }

    }
    justArrived = finishedCars - oldFinished;
}

bool SimulatedCityCloud::getTrafficLights() {
    const char *h = host.c_str();
    Client *c = new Client(80, h);
    cout << "get traffic lights...";
    auto start = high_resolution_clock::now(); 
    c->connectToServer();
    cout << "connected!" << endl;
    string tl = c->getTrafficLights();
    auto stop = high_resolution_clock::now(); 
    string delimiter = "lights:";
    size_t pos = 0;

    pos = tl.find(delimiter);
    tl.erase(0, pos + delimiter.length());
   // cout << "lights: " << tl << endl;
    istringstream iss(tl);
    int light;
    for (int i = 0 ; i < mapSize ; i++) {
        for (int j = 0 ; j < mapSize ; j++) {
            iss >> light;
            
            updatedLights[i][j] = light;
        }
    }
   
    auto duration = duration_cast<microseconds>(stop - start); 
  
    cout << "Get traffic lights take: "<< duration.count() << " microseconds" << endl; 
  
    return true;
}

void SimulatedCityCloud::updateTrafficLights() {
    for (int i = 0 ; i < mapSize ; i++) {
        for (int j = 0 ; j < mapSize ; j++) {
           // trafficLights[i][j].update(updatedLights[i][j]);
          //  cout << updatedLights[i][j] << " " << trafficLights[i][j].getLight() << " ";
          //  trafficLights[i][j].checkAndSwitch();
            trafficLights[i][j].update(updatedLights[i][j]);
            cout << trafficLights[i][j].getLight() << " ";
        }
        cout << endl;
    }
    cout << endl;
}

void SimulatedCityCloud::run() {
    sychronizedRun();
}







