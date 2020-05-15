

#ifndef MEMORYSOURCE_H
#define MEMORYSOURCE_H

#include "lib/api.h"

class MemorySource : public MPxNode {
    public:
        MemorySource();
        virtual ~MemorySource();

        virtual MStatus compute(
				const MPlug& plug, MDataBlock& data);

        static void* creator();
        static MStatus initialize();
		MStatus getConnectedSink(MObject &nodeObj);
		MStatus connectionMade(
			const MPlug &plug, const MPlug &otherPlug, bool asSrc);
		MStatus connectionBroken(
			const MPlug &plug, const MPlug &otherPlug, bool asSrc);
		void setSinkObj(MObject &obj);
		void clearSinkObj(MObject &obj);
		MStatus getSinkData(MObject &sinkObj, MObject &sinkData, MObject &sinkFloatData, float &sinkFloatValue, MDataHandle &sinkDH);
		MStatus setOutputSourceData(MArrayDataHandle &sourceArrayDH, MArrayDataHandle &sinkArrayDH);

public:
    static MTypeId kNODE_ID;
    static MString kNODE_NAME;
    
    // attribute MObjects
	static MObject aSinkConnection;
	static MObject aTime;
	static MObject aResetFrame;
	static MObject aData;
	static MObject aFloatData;

	// internal sink objects
	MObject sinkObj;
	std::set<MObject> sinkObjs;
	// check for disconnection
	bool sinkConnected;
    

};

/*
There is some inelegance in using a bool connection, as one source may be connected
to multiple sinks, which is wrong
for now, only the first connected sink will be considered


*/


#endif
	