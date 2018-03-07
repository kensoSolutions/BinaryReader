/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2018 Open Ephys

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/
#include <H5Cpp.h>
#include "BinaryFileSource.h"
#include <CoreServicesHeader.h>
#include <RecordingLib.h>
#include <fstream>      // std::ifstream
#include <iostream>
#include <iomanip>

using namespace H5;


BinaryFileSource::BinaryFileSource() : samplePos(0), skipRecordEngineCheck(false)
{
}

BinaryFileSource::~BinaryFileSource()
{
}

void  BinaryFileSource::readJSON(File& file)
{
	var s[4];
	//std::cout << "ALVAROEJECUTADO\n";
	//const File& file = File("Full.txt");
	String text = file.loadFileAsString();
	//std::cout << "____________________SALIDA2: " << (String)text << std::endl;
	var result;
	Result re = JSON::parse(text, result);
	s[0] = result;
	//std::cout << "____________________SALIDA: " << (String)result["GUI version"] << std::endl;
	re = JSON::parse(text, result);
	continuous = result;
	//std::cout << "____________________SALIDA: " << (String)result["continuous"][0]["channels"][0]["units"] << std::endl;
	//std::cout << "____________________SALIDA: " << (String)result["continuous"][0]["num_channels"] << std::endl;
	re = JSON::parse(text, result);
	events = result;
	//std::cout << "____________________SALIDA1: " << (String)result["folder_name"] << std::endl;
	re = JSON::parse(text, result);
	spikes = result;
}

bool BinaryFileSource::Open(File file)
{
	//DynamicObject::Ptr jsonSettingsFile = new DynamicObject();
	var dict[4];
	readJSON(file);
	/*jsonSettingsFile->readJSON(file);
	var cont = jsonSettingsFile->getContinuousData();*/
	var cont = continuous;
	String fNameC = (String)cont["continuous"][0]["folder_name"];
	//String fNameS = (String)cont["spikes"][0]["folder_name"];
	//std::cout << "____________________SALIDA3: " << (String)cont["continuous"][0]["folder_name"] << std::endl;

#ifdef _WIN32
	fNameC = fNameC.replace("/", "\\", false);
#endif
	binaryPath = file.getParentDirectory().getFullPathName() + "\\continuous\\" + fNameC + "continuous.dat";
	if (fNameC.compare("") == 0){
		fNameC = (String)cont["continuous"][0]["name"];
		binaryPath = file.getParentDirectory().getFullPathName() + "\\continuous\\" + fNameC + ".dat";
	}
	File binFile(binaryPath);

	int64 fileSize = binFile.getSize(); 

	is.open(binaryPath.toStdString(), std::ifstream::binary);

	RecordInfo info;
	/*if (fNameC.compare("") == 0){
		CoreServices::sendStatusMessage("Error loading file. Upgrade GUI version");
		info.name = "Error: Old version";
	}
	else
	{*/
		info.name = fNameC + "continuous"; // cont["name"];
		//std::cout << "NUM_CHANNELS: " << (String)cont["continuous"][0]["num_channels"] << " : " << (String)(int)cont["continuous"][0]["num_channels"] << " processo: " << std::to_string((int)cont["continuous"][0]["recorded_processor_id"]) << std::endl;
		info.numSamples = fileSize / 2 / (int)cont["continuous"][0]["num_channels"];
		info.sampleRate = cont["continuous"][0]["sample_rate"];
		//std::cout << "------------ Num samples: " << info.numSamples << std::endl;
		for (int j = 0; j < (int)cont["continuous"][0]["num_channels"]; j++)
		{
			RecordedChannelInfo c;
			c.name = cont["continuous"][0]["channels"][j]["channel_name"];
			c.bitVolts = cont["continuous"][0]["channels"][j]["bit_volts"];
			//std::cout << "------------Opening file _bit_volts: " << (String)cont["continuous"][0]["channels"][j]["bit_volts"] << std::endl;
			info.channels.add(c);
		}
//	}
	
	infoArray.add(info);
	availableDataSets.add(0);
	numRecords++;

	charBuffer = new char[(int)((float)cont["continuous"][0]["sample_rate"] * 0.1) * 2 * 16 * 16];
	return true;
	
}

void BinaryFileSource::fillRecordInfo()
{
    //Group recordings;
	//std::cout << "++++++++ fillRecordInfo" << std::endl;

}

void BinaryFileSource::updateActiveRecord()
{
	//std::cout << "++++++++ update ActiveRecord" << std::endl;
    samplePos=0;

}

void BinaryFileSource::seekTo(int64 sample)
{
	//std::cout << "++++++++ seekTo:" << sample << std::endl;
	samplePos = sample % getActiveNumSamples();
	//std::cout << "++++++++ :" << getActiveNumSamples() << std::endl;
}

int BinaryFileSource::readData(int16* buffer, int nSamples)
{
	//std::cout << "___ readData" << std::endl;
    //DataSpace fSpace,mSpace;
    int samplesToRead;
    int nChannels = getActiveNumChannels();
    //hsize_t dim[3],offset[3];

    if (samplePos + nSamples > getActiveNumSamples())
    {
        samplesToRead = (int) getActiveNumSamples() - (int) samplePos;
    }
    else
    {
        samplesToRead = nSamples;
    }
	//std::cout << "nSamples:" << nSamples << " samplesToRead:" << samplesToRead << " SamplePOS:" << samplePos << std::endl;

		if (is.is_open()) {
			// get length of file:
			//is.seekg(0, samplePos * 2 * nChannels );
			is.seekg(samplePos * 2 * nChannels, 0);
			//std::cout << "POSICION: " << (samplePos * 2 * nChannels) << std::endl;
			//int length = is.tellg();
			//is.seekg(0, is.beg);

			// allocate memory:

			// read data as a block:
			is.read(charBuffer, samplesToRead * 2 * nChannels);

			//is.close();
			for (int i = 0; i < samplesToRead * nChannels; i++)
			{
				buffer[i] = ((static_cast<int16>(charBuffer[i * 2 + 1]) << 8) & 0xFF00) | (charBuffer[i * 2] & 0x00FF);
				
				//buffer[i] *= 0.05000000074505806;
				//std::cout << "Value:" << buffer[i] << std::endl;
				//std::cout << "Value: " << std::hex << std::setw(4) << (uint16)buffer[i] << std::endl;//" " << std::hex << std::setw(4) << (uint32)charBuffer[i + 1] << std::endl;
				//std::cout << "Value: " << buffer[i] << std::endl;
			}

		}
		else
		{
			std::cout << "alvaro.dat: ERROR OPEN" << std::endl;
		}
	
        samplePos += samplesToRead;
        return samplesToRead;

}

void BinaryFileSource::processChannelData(int16* inBuffer, float* outBuffer, int channel, int64 numSamples)
{
	//std::cout << "++++++++ processChannelData" << std::endl;
    int n = getActiveNumChannels();
    float bitVolts = getChannelInfo(channel).bitVolts;

    for (int i=0; i < numSamples; i++)
    {
		*(outBuffer + i) = *(inBuffer + (n*i) + channel) * bitVolts;
    }
	//std::cout << "End processChannel" << std::endl;

}

bool BinaryFileSource::isReady()
{
	//std::cout << "++++++++ isReady" << std::endl;
	//HDF5 is by default not thread-safe, so we must warn the user.
	if ((!skipRecordEngineCheck) && (CoreServices::getSelectedRecordEngineId() == "KWIK"))
	{
		int res = AlertWindow::showYesNoCancelBox(AlertWindow::WarningIcon, "Record format conflict",
			"Both the selected input file for the File Reader and the output file format for recording use the HDF5 library.\n"
			"This library is, by default, not thread safe, so running both at the same time might cause unexpected crashes (chances increase with signal complexity and number of recorded channels).\n\n"
			"If you have a custom-built hdf5 library with the thread safe features turned on, you can safely continue, but performance will be reduced.\n"
			"More information on:\n"
			"https://www.hdfgroup.org/HDF5/doc/TechNotes/ThreadSafeLibrary.html\n"
			"https://www.hdfgroup.org/hdf5-quest.html\n\n"
			"Do you want to continue acquisition?", "Yes", "Yes and don't ask again", "No");
		switch (res)
		{
		case 2:
			skipRecordEngineCheck = true;
		case 1:
			return true;
			break;
		default:
			return false;
		}
	}
	else
		return true;
}