
maya_useNewAPI = True

def maya_useNewAPI():
	pass

import sys
import maya.api.OpenMaya as om

import math
import maya.cmds as cmds
from collections import namedtuple

kPluginNodeName = "generalIk"
kPluginNodeId = om.MTypeId( 0xDAA1 )

# ChainData = namedtuple("ChainData", ["matrices"])

class generalIk(om.MPxNode):
	# define everything
	id = om.MTypeId( 0xDAA1)

	def __init__(self):
		om.MPxNode.__init__(self)

	def compute(self, pPlug, pData):

		# only compute if output is in out array
		if(pPlug.parent() == generalIk.aOutArray):
			# descend into coordinated cycles
			# inputs

			solver = pData.inputValue(generalIk.aSolver).asInt()
			maxIter = pData.inputValue(generalIk.aMaxIter).asInt()
			tolerance = pData.inputValue(generalIk.aTolerance).asDouble()

			# target
			targetMat = pData.inputValue(generalIk.aTargetMat).asMatrix()

			# end
			endMat = pData.inputValue(generalIk.aEndMat).asMatrix()

			# extract input world matrices from array then leave it alone
			inJntArrayDH = pData.inputArrayValue(generalIk.aJnts)
			inLength = inJntArrayDH.__len__()
			worldInputs = [None] * inLength
			for i in range(inLength):
				inJntArrayDH.jumpToPhysicalElement(i)
				childCompDH = inJntArrayDH.inputValue()
				worldInputs[i] = childCompDH.child(
					generalIk.aJntMat).asMatrix()

			# from world inputs, reconstruct localised chain
			localMatrices = buildChain(worldInputs, length=inLength)

			# main loop
			n = 0
			tol = 100
			# localise end first
			endLocalMat = endMat * worldInputs[-1].inverse()
			targetLocalMat = targetMat * worldInputs[0].inverse()

			results = localMatrices
			while n < maxIter and tol > tolerance:
				# results = iterateChain(results, length=inLength,
				#              targetMat=targetLocalMat, endMat=endLocalMat)
				results = iterateChain(results, length=inLength,
				             targetMat=targetMat, endMat=endLocalMat)
				n += 1


			# convert jntArray of matrices to useful rotation values
			outArrayDH = pData.outputArrayValue(generalIk.aOutArray)

			for i in range(inLength):
				outArrayDH.jumpToPhysicalElement(i)
				outCompDH = outArrayDH.outputValue()

				outRotDH = outCompDH.child(generalIk.aOutRot)
				outRxDH = outRotDH.child(generalIk.aOutRx)
				outRyDH = outRotDH.child(generalIk.aOutRy)
				outRzDH = outRotDH.child(generalIk.aOutRz)

				outRotVals = om.MTransformationMatrix(results[i]).rotation()
				# unitConversions bring SHAME on family
				xAngle = om.MAngle(outRotVals[0])
				yAngle = om.MAngle(outRotVals[1])
				zAngle = om.MAngle(outRotVals[2])
				# xAngle = outRotVals[0]
				# yAngle = outRotVals[1]
				# zAngle = outRotVals[2]
				outRxDH.setMAngle( xAngle )
				outRyDH.setMAngle( yAngle )
				outRzDH.setMAngle( zAngle )

			outArrayDH.setAllClean()

			pData.setClean(pPlug)

def buildChain(worldChain, length=1, endMat=None):
	""" reconstruct a chain of ordered local matrices
	from random world inputs"""

	chain = [None] * length # root to tip
	for i in range(length):
		inMat = worldChain[i]
		if i == 0:
			localMat = inMat
		else:
			localMat = inMat * worldChain[ i-1 ].inverse()
		chain[i] = localMat
	return chain

def iterateChain(localChain, tolerance=None, length=1,
                 targetMat=None, endMat=None):
	"""performs one complete iteration of the chain,
	may be possible to keep this solver-agnostic"""
	"""
	welcome to the bone zone
	# reconstruct hierarchy with matrices from previous iteration
	# currently target is known in rootspace, and end isn't known at all
	# backwards to get target and forwards to get end, both in active joint space
	#
	#                  +(target)
	#               .
	#             .
	#           O
	#         /   \       X(end)
	#       /       \   /
	#     /           O
	#   /
	# (root)
	# this works by getting vector from active joint to end, from active joint to target,
	# then aligning one to the other. joints are assumed to have direct parent-child
	# hierarchy, so all rotations are inherited rigidly
	"""
	# HERE we need knowledge of the live end position

	for i in range(length):  # i from TIP TO ROOT
		index = length - 1 - i
		inMat = localChain[ index ]


		orientMat = lookAt(endMat, targetMat) # this does not

		#temp
		orientMat = lookAt(om.MMatrix(), targetMat) # this works
		localChain[index] = orientMat
	return localChain


def lookAt(base, target, up = (0, 1, 0)):
	# axes one by one, code shamelessly copied from somewhere
	# convert to quat someday?

	# x is vector between base and target
	x = om.MVector(target[12 ] -base[12],
	               target[13 ] -base[13],
	               target[14 ] -base[14])

	x.normalize()

	z = x ^ om.MVector(-up[0], -up[1], up[2])
	z.normalize()
	y = x ^ z
	y.normalize()

	aim = om.MMatrix([
		y.x, y.y, y.z, 0,
		x.x, x.y, x.z, 0,
		z.x, z.y, z.z, 0,
		0, 0, 0, 1
	])

	return aim

def nodeInitializer():
	# create attributes

	# pick your pointy poison
	solverAttrFn = om.MFnEnumAttribute()
	generalIk.aSolver = solverAttrFn.create("solver", "sol", 0)
	solverAttrFn.addField("CCD", 0)
	solverAttrFn.addField("FABRIK (not yet implemented)", 1)
	solverAttrFn.storable = True
	solverAttrFn.keyable = True
	solverAttrFn.readable = False
	solverAttrFn.writable = True
	om.MPxNode.addAttribute(generalIk.aSolver)

	iterAttrFn = om.MFnNumericAttribute()
	generalIk.aMaxIter = iterAttrFn.create("maxIterations", "mi",
	                                       om.MFnNumericData.kLong, 30)
	iterAttrFn.storable = True
	iterAttrFn.keyable = True
	iterAttrFn.readable = False
	iterAttrFn.writable = True
	iterAttrFn.setMin(0)
	om.MPxNode.addAttribute(generalIk.aMaxIter)

	# how far will you go for perfection
	toleranceAttrFn = om.MFnNumericAttribute()
	generalIk.aTolerance = toleranceAttrFn.create("tolerance", "tol",
	                                              om.MFnNumericData.kDouble, 0.01)
	toleranceAttrFn.storable = True
	toleranceAttrFn.keyable = True
	toleranceAttrFn.readable = False
	toleranceAttrFn.writable = True
	toleranceAttrFn.setMin(0)
	om.MPxNode.addAttribute(generalIk.aTolerance)

	# what are your goals in life
	targetMatAttrFn = om.MFnMatrixAttribute()
	generalIk.aTargetMat = targetMatAttrFn.create("targetMatrix",
	                                              "targetMat", 1)
	targetMatAttrFn.storable = True
	targetMatAttrFn.readable = False
	targetMatAttrFn.writable = True
	targetMatAttrFn.cached = True
	om.MPxNode.addAttribute(generalIk.aTargetMat)

	# compare and contrast
	endMatAttrFn = om.MFnMatrixAttribute()
	generalIk.aEndMat = endMatAttrFn.create("endMatrix", "endMat", 1)
	endMatAttrFn.storable = True
	endMatAttrFn.readable = False
	endMatAttrFn.writable = True
	endMatAttrFn.cached = True
	om.MPxNode.addAttribute(generalIk.aEndMat)


	# once i built a tower
	jntMatAttrFn = om.MFnMatrixAttribute()
	generalIk.aJntMat = jntMatAttrFn.create("matrix",
	                                        "jntMat", 1)
	jntMatAttrFn.storable = False
	jntMatAttrFn.writable = True
	jntMatAttrFn.cached = False # prevent ghost influences from staying
	# om.MPxNode.addAttribute(generalIk.aJntMat)

	# eye on the sky
	jntUpMatAttrFn = om.MFnMatrixAttribute()
	generalIk.aJntUpMat = jntUpMatAttrFn.create("upMatrix",
	                                            "jntUpMat", 1)
	jntUpMatAttrFn.storable = True
	jntUpMatAttrFn.writable = True
	jntUpMatAttrFn.cached = True
	# om.MPxNode.addAttribute(generalIk.aJntUpMat)

	# who is the heftiest boi
	jntWeightAttrFn = om.MFnNumericAttribute()
	generalIk.aJntWeight = jntWeightAttrFn.create("weight", "jntWeight",
	                                              om.MFnNumericData.kFloat, 1)
	jntWeightAttrFn.storable = True
	jntWeightAttrFn.keyable = True
	jntWeightAttrFn.writable = True
	jntWeightAttrFn.setMin(0)
	# om.MPxNode.addAttribute(generalIk.aJntWeight)

	# like really know them
	rxMaxAttrFn = om.MFnNumericAttribute()
	generalIk.aRxMax = rxMaxAttrFn.create("maxRotateX", "maxRx",
	                                      om.MFnNumericData.kFloat, 0)
	rxMaxAttrFn.storable = True
	rxMaxAttrFn.keyable = True
	#om.MPxNode.addAttribute(generalIk.aRxMax)

	# how low can you go
	rxMinAttrFn = om.MFnNumericAttribute()
	generalIk.aRxMin = rxMinAttrFn.create("minRotateX", "minRx",
	                                      om.MFnNumericData.kFloat, 0)
	rxMinAttrFn.storable = True
	rxMinAttrFn.keyable = True
	#om.MPxNode.addAttribute(generalIk.aRxMin)

	## there is more to be done here

	# you will never break the chain
	jntArrayAttrFn = om.MFnCompoundAttribute()
	generalIk.aJnts = jntArrayAttrFn.create("joints", "joints")
	jntArrayAttrFn.array = True
	jntArrayAttrFn.usesArrayDataBuilder = True
	jntArrayAttrFn.addChild(generalIk.aJntMat)
	jntArrayAttrFn.addChild(generalIk.aJntUpMat)
	jntArrayAttrFn.addChild(generalIk.aJntWeight)
	# add limits later
	om.MPxNode.addAttribute(generalIk.aJnts)

	# fruits of labour
	outRxAttrFn = om.MFnUnitAttribute()
	generalIk.aOutRx = outRxAttrFn.create("outputRotateX", "outRx", 1, 0.0)
	outRxAttrFn.writable = False
	outRxAttrFn.keyable = False
	# om.MPxNode.addAttribute(generalIk.aOutRx)


	outRyAttrFn = om.MFnUnitAttribute()
	generalIk.aOutRy = outRyAttrFn.create("outputRotateY", "outRy", 1, 0.0)
	outRyAttrFn.writable = False
	outRyAttrFn.keyable = False
	# om.MPxNode.addAttribute(generalIk.aOutRy)

	outRzAttrFn = om.MFnUnitAttribute()
	generalIk.aOutRz = outRzAttrFn.create("outputRotateZ", "outRz", 1, 0.0)
	outRzAttrFn.writable = False
	outRzAttrFn.keyable = False
	# om.MPxNode.addAttribute(generalIk.aOutRz)

	outRotAttrFn = om.MFnCompoundAttribute()
	# generalIk.aOutRot = outRotAttrFn.create("outputRotate", "outRot",
	#     om.MFnNumericData.k3Double)
	generalIk.aOutRot = outRotAttrFn.create("outputRotate", "outRot")
	outRotAttrFn.storable = False
	outRotAttrFn.writable = False
	outRotAttrFn.keyable = False
	outRotAttrFn.addChild(generalIk.aOutRx)
	outRotAttrFn.addChild(generalIk.aOutRy)
	outRotAttrFn.addChild(generalIk.aOutRz)
	om.MPxNode.addAttribute(generalIk.aOutRot)


	# # add smooth jazz

	outTransAttrFn = om.MFnNumericAttribute()
	generalIk.aOutTrans = outTransAttrFn.create("outputTranslate", "outTrans",
	                                            om.MFnNumericData.k3Double)
	outTransAttrFn.storable = False
	outTransAttrFn.writable = False
	outTransAttrFn.keyable = False
	om.MPxNode.addAttribute(generalIk.aOutTrans)


	# all that the sun touches
	outArrayAttrFn = om.MFnCompoundAttribute()
	generalIk.aOutArray = outArrayAttrFn.create("outputArray", "out")
	outArrayAttrFn.array = True
	outArrayAttrFn.usesArrayDataBuilder = True
	outArrayAttrFn.storable = False
	outArrayAttrFn.writable = False
	outArrayAttrFn.keyable = False
	outArrayAttrFn.addChild(generalIk.aOutRot)
	outArrayAttrFn.addChild(generalIk.aOutTrans)
	om.MPxNode.addAttribute(generalIk.aOutArray)
	# investigate rolling this into the input hierarchy

	# everyone's counting on you
	generalIk.attributeAffects(generalIk.aTargetMat, generalIk.aOutArray)
	generalIk.attributeAffects(generalIk.aMaxIter, generalIk.aOutArray)
	generalIk.attributeAffects(generalIk.aTolerance, generalIk.aOutArray)


	# following are reference chain - trigger rebuild only when these change
	# generalIk.attributeAffects(generalIk.aRootMat, generalIk.aOutArray)
	generalIk.attributeAffects(generalIk.aEndMat, generalIk.aOutArray)
	generalIk.attributeAffects(generalIk.aJnts, generalIk.aOutArray)

#     # TRY THIS OUT LATER:
#     refMatArrayFn = om.MFnMatrixAttribute()
#     generalIk.aRefArray = refMatArrayFn.create("refMatArray")
#     refMatArrayFn.array = True
#     refMatArrayFn.internal = True
#     refMatArrayFn.cached = True
#     refMatArrayFn.storable = True
#     # do we need arrayDataBuilder?
#     om.MPxNode.addAttribute(generalIk.aRefArray)
# this would be constructed of the joints' relative matrices, and then store the outputs
# of the node across iterations - basically its memory
# if the reference skeleton changes RELATIVE TO THE ROOT, this would need to be recalculated
# a battle for another day

def nodeCreator():
	# creates node, returns to maya as pointer
	return generalIk()

def initializePlugin(mobject):
	mplugin = om.MFnPlugin(mobject)
	try:
		mplugin.registerNode( kPluginNodeName, kPluginNodeId,
		                      nodeCreator, nodeInitializer)
	except:
		sys.stderr.write("Failed to register node:" + kPluginNodeName)
		raise

def uninitializePlugin( mobject ):
	mPlugin = om.MFnPlugin(mobject)
	# try:
	# 	mPlugin.deregisterNode(kPluginNodeId)
	#
	# except:
	# 	sys.stderr.write("failed to unregister node, you're stuck with generalIk forever lol")
	# 	raise
	mPlugin.deregisterNode(kPluginNodeId)

# roadmap:
# get it working
# get it working with constraints
# find way to cache matrix chain unless reference chain rebuilds
# get different solvers working - maybe fabrik, but i want to try the quat splice
# rebuild in c++ if it really needs it?
# make cmd to attach joints automatically