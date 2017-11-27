#include "packager.h"
#include "bleInterface.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

int packagerInit(uint8_t dataType, uint8_t desiredLength, Packager* currPackager) {
	currPackager->type = dataType;
	currPackager->seq = 0;
	currPackager->firstTimeStamp = 0;
	currPackager->currAmount = 0;
	currPackager->dataLength = desiredLength;
	return 1;
}


int addToPackage(uint16_t currTime, char* dataIn, size_t length, Packager* currPackager) {
	size_t remainingLength = 0;
	NRF_LOG_INFO("The Time is %d", currPackager->firstTimeStamp);
	if (currPackager->firstTimeStamp == 0) {
		currPackager->firstTimeStamp = currTime;
		NRF_LOG_INFO("Set the time to be %d", currPackager->firstTimeStamp);
	}
	if (length + currPackager->currAmount > currPackager->dataLength) {
		remainingLength = length + currPackager->currAmount - currPackager->dataLength;
		NRF_LOG_INFO("Given Length %d Current Amount %d Requested Amount %d", length, currPackager->currAmount, currPackager->type);
		length = currPackager->dataLength - currPackager->currAmount; //write only enough to fill up the buffer
	}
	memcpy(&currPackager->data[currPackager->currAmount], dataIn, length);
	currPackager->currAmount += length;

	if (currPackager->currAmount == currPackager->dataLength) {
		createPackage(currPackager);
	}
	if (remainingLength) {
		addToPackage(currTime, &dataIn[length], remainingLength, currPackager);
		NRF_LOG_INFO("I would have added to package with remainingLength %d", remainingLength);
	}

	return 1;


}

//The packager creates a new package with the following ordering: 
// SOF Byte, Size Byte, Time 2 Bytes, Sequence Byte, Reserved 2 Bytes, Type Byte, Data Bytes, 2 bytes of CRC 
int createPackage(Packager* currPackager) {
	uint16_t reservedData = RES_BYTES;
	uint8_t totalSize = 6 + currPackager->dataLength; //There are 8 header bytes per package (after the size byte)

	char packagedData[currPackager->dataLength + 10]; //10 extra bytes of header data

	packagedData[SOF_LOC] = START_BYTE;

	packagedData[SIZE_LOC] = totalSize;

	memcpy(&packagedData[TIME_LOC], &currPackager->firstTimeStamp, 2);

	packagedData[SEQ_LOC] = ++currPackager->seq;

	memcpy(&packagedData[RESERVED_LOC], &reservedData, 2);

	packagedData[TYPE_LOC] = currPackager->type;

	memcpy(&packagedData[DATA_LOC], currPackager->data, currPackager->dataLength);

	uint16_t crc = crc16(&packagedData[TIME_LOC],  totalSize);

	NRF_LOG_INFO("The crc is 0x%04X", crc);

	memcpy(&packagedData[currPackager->dataLength + 8], &crc, sizeof(crc)); //extra bytes 9 and 10 are the crc


	currPackager->firstTimeStamp = 0; //un set it so that next data byte will reset it
	currPackager->currAmount = 0;

	pendingMessagesPush(sizeof(packagedData), (char*)packagedData, &globalQ);

	NRF_LOG_INFO("Completed Full Packet");


	return 1; //currently we always return success
}