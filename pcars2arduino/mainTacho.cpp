// Used for memory-mapped functionality
#include <windows.h>
#include "sharedmemory.h"
//#include <iostream>
#include <string>

// Used for this example
#include <stdio.h>
#include <conio.h>
#include "serial.h"

#include <sys/time.h>
#include <math.h>
#define _USE_MATH_DEFINES

// Name of the pCars memory mapped file
#define MAP_OBJECT_NAME "$pcars$"

using namespace std;

void sendData(Serial* SP, char* key, int iValue) {

    char value[16];
    sprintf(value, "%d", iValue);
    char msg[32] = "^";

    strcat(msg,key);
    strcat(msg,",");
    strcat(msg,value);
    strcat(msg,"\n\0");

    //printf(msg);

    //printf(msg);

    SP->WriteData(msg, sizeof(msg));
    char msg2[32] = "";
    msg2[0] = '\0';
    SP->ReadData(msg2, 32);
    //if (msg2[0] != '\0')
        //printf("Antwort %s\n", msg2);
}

int main()
{
	// Open the memory-mapped file
	HANDLE fileHandle = OpenFileMapping( PAGE_READONLY, FALSE, MAP_OBJECT_NAME );
	if (fileHandle == NULL)
	{
		printf( "Could not open file mapping object (%d).\n", GetLastError() );
		return 1;
	}

	// Get the data structure
	const SharedMemory* sharedData = (SharedMemory*)MapViewOfFile( fileHandle, PAGE_READONLY, 0, 0, sizeof(SharedMemory) );
	if (sharedData == NULL)
	{
		printf( "Could not map view of file (%d).\n", GetLastError() );

		CloseHandle( fileHandle );
		return 1;
	}

	// Ensure we're sync'd to the correct data version
	if ( sharedData->mVersion != SHARED_MEMORY_VERSION )
	{
		printf( "Data version mismatch\n");
		return 1;
	}

    printf("Welcome to the serial test app!\n\n");

	Serial* SP = new Serial("\\\\.\\COM3");    // adjust as needed

	if (SP->IsConnected())
		printf("We're connected");

	char incomingData[256] = "";			// don't forget to pre-allocate memory
	//printf("%s\n",incomingData);
	int dataLength = 16;
	int readResult = 0;

    Sleep(500);
    char outData[dataLength];
    //printf("%d", sizeof(test));
    //SP->WriteData(const_cast<char*>(test.c_str()), sizeof(test));
	//------------------------------------------------------------------------------
	// TEST DISPLAY CODE
	//------------------------------------------------------------------------------
	printf( "ESC TO EXIT\n\n");

    timeval oldTime;

	float MAX_FUEL = 100;


	float oldFuel=MAX_FUEL;
	float oldOdo=0;
    gettimeofday(&oldTime, 0);

    int fuelCounter = 0;
    float fuelUpdateInterval = 0.25;
    int SLEEP_LOOP_MS = 50;

	while (true)
	{
	    int maxRpm = floor(sharedData->mMaxRPM+0.5);
	    sendData(SP, "maxrpm", maxRpm);
	    float speedMS = sharedData->mSpeed;
		int speed = floor(speedMS*3.6 + 0.5);
        sendData(SP, "tempo", speed);
		int rpm = floor(sharedData->mRpm+0.5);
        sendData(SP, "rpm", rpm);
        int gear = sharedData->mGear;
        sendData(SP, "gear", gear);
        float fuel = sharedData->mFuelCapacity*sharedData->mFuelLevel;
        int iFuel = floor(fuel + 0.5);
        sendData(SP, "fuelleft", iFuel);

        float currentFuel;
        if (oldFuel > fuel) {
            timeval nowTime;
            gettimeofday(&nowTime, 0);
            float diffFuel = oldFuel - fuel;
            oldFuel = fuel;
            double diffTime = double((nowTime.tv_sec - oldTime.tv_sec) * 1e6 + (nowTime.tv_usec - oldTime.tv_usec)) / 1e6;
            oldTime = nowTime;

            float odo = sharedData->mOdometerKM;
            float diffOdo = odo - oldOdo;
            oldOdo = odo;


            if (speed > 1) {
                currentFuel = diffFuel /  diffOdo * 100 ;
            } else {
                currentFuel = MAX_FUEL;
            }

            sendData(SP, "fuelcur", (int)(floor(currentFuel+0.5)));

            printf("%f %f %f\n", diffFuel, diffTime, currentFuel );
        } else {
            timeval nowTime;
            currentFuel = 0;
            gettimeofday(&nowTime, 0);
            double diffTime = double((nowTime.tv_sec - oldTime.tv_sec) * 1e6 + (nowTime.tv_usec - oldTime.tv_usec)) / 1e6;
            if (diffTime > 0.5)
                sendData(SP, "fuelcur", 0);
        }

        int waterTemp = floor(sharedData->mWaterTempCelsius+0.5);
        sendData(SP, "temp", waterTemp);


        //printf("CurrentFuel: %f\n", currentFuel);


        // maximum sensor distance to side between the center of the cars
        // cars touching side by side will be measured with 1 car width distance
        // Suggestion: 5-15m
        float sensorSideMax = 7;
        // minimum sensor distance to side to avoid cars driving directly behind you trigger sensors.
        // This value should be LESS than the car width, or it could happen that it does not see cars directly on your side.
        // Suggestion: 1-1.5m
        float sensorSideMin = 1;
        // The distance between the center of the cars in direction of the your car, where the sensor should trigger.
        // Consider that a car touching your rear will be measured with 1 car length distance
        // Suggestion: 4-6m
        float sensorCarLength = 5;
        float maxDist = max(sensorCarLength, sensorSideMax);

        int myIdx = sharedData->mViewedParticipantIndex;
        float myAngle = sharedData->mOrientation[1];

        float myX = sharedData->mParticipantInfo[myIdx].mWorldPosition[0];
        float myY = sharedData->mParticipantInfo[myIdx].mWorldPosition[2];

        for (int i=0; i<sharedData->mNumParticipants; ++i) {
            if (i != myIdx) {
                float yourX = sharedData->mParticipantInfo[i].mWorldPosition[0];
                float yourY = sharedData->mParticipantInfo[i].mWorldPosition[2];

                //only do the expensive checks with cars near you to reduce CPU costs
                if (abs(myX-yourX) < maxDist && abs(myY-yourY) < maxDist) {
                    myAngle *= -1;
                    float myNewX = myX*cos(myAngle) + myY*sin(myAngle);
                    float myNewY = -myX*sin(myAngle) + myY*cos(myAngle);

                    float yourNewX = yourX*cos(myAngle) + yourY*sin(myAngle);
                    float yourNewY = -yourX*sin(myAngle) + yourY*cos(myAngle);

                    float xDist = myNewX - yourNewX;
                    float yDist = myNewY - yourNewY;

                    if (abs(xDist) < sensorSideMax && abs(xDist) > sensorSideMin && abs(yDist) < sensorCarLength) {
                        if (xDist < 0) {
                            printf("On your left\n");
                        } else {
                            printf("On your right\n");
                        }
                    }
                    else {
                        //printf("no car on side\n");
                    }
                } else {
                    //printf("no car near \n");
                }
            }
        }
		//printf("Speed: %d Rpm: %d\n", speed, rpm);
		if ( _kbhit() && _getch() == 27 ) // check for escape
		{
			break;
		}
		Sleep(SLEEP_LOOP_MS);
	}
	//------------------------------------------------------------------------------

	// Cleanup
	UnmapViewOfFile( sharedData );
	CloseHandle( fileHandle );

	return 0;
}


