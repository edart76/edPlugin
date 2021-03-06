

#ifndef TOPO_LIB
#define TOPO_LIB

#include <vector>
#include <set>
#include <algorithm>
#include <numeric>
#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <utility>
//#include <span>

#include <Eigen/Core>
#include <Eigen/Sparse>
#include <Eigen/Dense>
#include <Eigen/SVD>
//#define EIGEN_NO_DEBUG

// the igl include block
#include <igl/adjacency_list.h>
#include <igl/adjacency_matrix.h>
#include <igl/connected_components.h>
#include <igl/cotmatrix.h>
#include <igl/facet_adjacency_matrix.h>
#include <igl/polygon_mesh_to_triangle_mesh.h>
#include <igl/triangle_triangle_adjacency.h>
#include <igl/read_triangle_mesh.h>
#include <igl/vertex_triangle_adjacency.h>
#include <igl/vertex_components.h>

//#define EIGEN_NO_DEBUG 1

#include "containers.h"

#include "macro.h"



// none of this is elegant
// please forgive



/* more complex operations done with eigen types,
to be converted to and from raw buffers at interfaces between software*/

// size_t sucks
#define INT(sizeVar) static_cast<int>(sizeVar)

namespace ed {

	template <typename T>
	inline int arraySize( T arr[]){
		//return sizeof(arr) / sizeof(arr[0]);
		return sizeof(arr) / sizeof(T);
	}

	template <typename T>
	inline int index(const std::vector<T> &vec, const T &element) {
		// returns index in array or -1 if not found
		int result = -1;
		auto it = std::find(vec.begin(), vec.end(), element);
		if (it != vec.end()) {
			result = static_cast<int>(distance(vec.begin(), it));
		}
		return result;
	}


	static std::vector<int> interleaveOffsets(
			std::vector<int> baseOffsets, int maxIndex ){
		// interleave difference between each item, between items themselves
		std::vector<int> newOffsets( baseOffsets.size() * 2, -1);
		for( int i = 0; i < baseOffsets.size(); i++){
			newOffsets[2 * i] = baseOffsets[i];
			if( i == baseOffsets.size() - 1){ // at end
				newOffsets[2 * i + 1] = maxIndex - baseOffsets[i];
			}
			else{
				newOffsets[2 * i + 1] = baseOffsets[i] - baseOffsets[i + 1];
			}
		}
		return newOffsets;

	}

	/* interleaving entries was trash, took up double offset memory space
	* vector of offsets has (nEntries + 1) items, so that entry length can
	be found with ( offset[i+1] - offset[i] )
	on init, final entry is appended to offsets - 
	thus length of offset vector and buffer-> nEntries are no longer in sync
	oh well
	*/

	template <typename T>
	struct OffsetBuffer {
		int nValues;
		int nEntries;

		std::vector<T> values;
		std::vector<int> offsets;

		OffsetBuffer( std::vector<T> initValues, std::vector<int> initOffsets) :
			values(initValues), offsets(initOffsets),
			nValues(static_cast<int>(initValues.size())),
			nEntries(static_cast<int>(initOffsets.size()))
		{
				//offsets = interleaveOffsets(initOffsets, nValues);
			//offsets.push_back(nValues - offsets[nEntries - 1]);
			offsets.push_back(nValues);
		}

		inline int entryLength(int entryIndex){
			/*return offsets[ 2 * entryIndex + 1];*/
			//return offsets[ entryIndex ];
			//if (entryIndex == (nEntries - 1)) {
			//	return nValues - offsets[entryIndex];
			//}
			return offsets[entryIndex + 1] - offsets[entryIndex];
		}

		inline SmallList<T> entry(int entryIndex) {
			// return smallList
			int startIndex = offsets[entryIndex];
			int length = entryLength(entryIndex);
			SmallList<T> result;
			result.reserve(length);
			for (int i = 0; i < length; i++) {
				result.push_back(values[startIndex + i]);
			}
			return result;
		}
		
		inline T* entry(int entryIndex, int &len) {
			// return pointer to first entry, update entryLength 
			T* pointer = values.data() + offsets[entryIndex];
			len = entryLength(entryIndex);
			return pointer;
		}

		inline Span<T> span(int entryIndex) {
			//DEBUGS("buffer span")
			//DEBUGS(entryIndex);
			//DEBUGVI(offsets);
			//DEBUGVI(values);
			//DEBUGS(entryLength(entryIndex));
			Span<T> result{
				entryLength(entryIndex),
				values.data() + offsets[entryIndex]

			};
			/*DEBUGVI(result);*/
			return result;
		}


		SmallList<T>& operator[](int index) {
			return entry(index);
		}

		// constructors
		OffsetBuffer() {
		}
	};


	// corresponding wrapper for buffers of constant entry length
	template <typename T>
	struct UniformBuffer{
		int nValues;
		int nEntries;
		int strideLength;
		std::vector<T> values;

		UniformBuffer() {}


		UniformBuffer( std::vector<T> initValues, int initStrideLength) :
			values(initValues), strideLength(initStrideLength),
			nValues(static_cast<int>(initValues.size())),
			nEntries(static_cast<int>(initValues.size()) / initStrideLength){}

		void setVector(std::vector<T> initValues, int initStrideLength) {
			std::vector<T> p = initValues;
			values = p;

			nValues = static_cast<int>(initValues.size());
			nEntries = (static_cast<int>(initValues.size()) / initStrideLength);
		}

		 SmallList<T> entry(int entryIndex) {
		 	SmallList<T> result;
		 	result.reserve(strideLength);
		 	for (int i = 0; i < strideLength; i++) {
		 		result.push_back(values[entryIndex * strideLength + i]);
				//result[i] = values[entryIndex * strideLength + i];
		 	}
		 	return result;
		 }

		 inline const T* rawEntry(int entryIndex){
			 // return raw pointer to values starting at entryIndex
			 return values.data() + entryIndex * strideLength;
		 }
		 inline Span<T> span(int entryIndex) {
			 return Span<T>{
				 values.data() + entryIndex * strideLength,
					 strideLength
			 };
		 }

		 void setEntry(int index, T* data){
			 for(int i = 0; i < strideLength; i++){
				 values[index * strideLength + i] = data[i];
			 }
		 }

		 template< typename V>
		 void setEntry(int index, V obj){
			 for(int i = 0; i < strideLength; i++){
				 values[index * strideLength + i] = static_cast<T>(obj[i]);
			 }
		 }
	};


	// --- building buffers ---

	template <typename T>
	inline SmallList <T> entryFromBuffer(
		const std::vector<T> &values,
		const std::vector<int> &offsets,
		int entryIndex) {
		// use buffer indices to retrieve main values in entry
		SmallList<T> result;
		int startIndex = offsets[entryIndex];
		int endIndex;

		// check if entry is last
		if (entryIndex == offsets.size() - 1) {
			endIndex = static_cast<int>(values.size());
		}
		else {
			endIndex = offsets[entryIndex + 1];
		}
		if (startIndex == endIndex) {
			endIndex++;
		}
		for (int i = startIndex; i < endIndex; i++) {
			result.push_back(values[i]);
		}
		return result;
	}

	template <typename T>
	inline SmallList <T> entryFromBuffer(
		const std::vector<T> &values,
		int strideLength,
		int entryIndex) { // for uniform buffers

		SmallList<T> result;
		result.reserve(strideLength);

		for (int i = 0; i < strideLength; i++) {
			result[i] = values[entryIndex*strideLength + i];
		}
		return result;
	}


	static OffsetBuffer<int> pointBufferFromFaceBuffer(OffsetBuffer<int> &faceBuffer)
	{
		/*
		for now gives points in unordered entries, with no consistent winding order
		buffer entries are points connected to that index
		buffer_entry[i] = iA, iB, iC, iD

		*/

		// maximum point is number of points
		int nPoints = *max_element(faceBuffer.values.begin(), faceBuffer.values.end()) + 1;
		int nFaces = faceBuffer.nEntries;

		std::vector<int> pointConnects;
		std::vector<int> pointOffsets(nPoints, -1);

		//DEBUGVI(pointOffsets);

		// build set representing each point
		std::vector< std::set<int> > pointSets(nPoints);

		// iterate over faces in buffer
		for (int i = 0; i < nFaces; i++) {
			// iterate over points in face entry

			Span<int> facePoints = faceBuffer.span(i);
			//DEBUGS("found span");
			//DEBUGS(facePoints.length)
			//DEBUGVI(facePoints);


			// if triangle, add all other points
			int entryLength = facePoints.length;
			//DEBUGS("entryLength" << entryLength);

			// gather connected points
			for (int n = 0; n < entryLength; n++) {
				int pointIndex = facePoints[n];

				int left = (n - 1 + entryLength) % entryLength;
				int right = (n + 1) % entryLength;
				//DEBUGS("modulo wrapping : n " << n << " left " << left << " right " << right)

				pointSets[pointIndex].insert(facePoints[left]);
				pointSets[pointIndex].insert(facePoints[right]);
			}
		}
		/*DEBUGS("point neighbour sets built");*/

		// flatten pointSets to vector
		std::set<int>::iterator it;
		int n = 0;
		for (int i = 0; i < nPoints; i++)
		{
			// set offset index
			pointOffsets[i] = n;

			// iterate over connected point indices in set
			for (it = pointSets[i].begin(); it != pointSets[i].end(); ++it) {
				pointConnects.push_back(*it);
				n++;
			}
		}
		//DEBUGVI(pointConnects);
		//DEBUGVI(pointOffsets);

		OffsetBuffer<int> result(pointConnects, pointOffsets);
		return result;
	}

	static OffsetBuffer<int> pointBufferFromFaceVectors(
		std::vector<int> &faceConnects, std::vector<int> &faceOffsets) {
		DEBUGS("pointBufferFromFaceVectors");
		// create offsetBuffer from input vectors if not supplied
		OffsetBuffer<int> faceBuffer(faceConnects, faceOffsets);
		//DEBUGVI(faceConnects);
		//DEBUGVI(faceOffsets);
		return pointBufferFromFaceBuffer(faceBuffer);
	}

	static OffsetBuffer<int> faceVertexConnectsFromFacePointConnects(
		OffsetBuffer<int>& facePointConnects
	) {

	}

	// static UniformBuffer<int> edgeBufferFromPointBuffer(
	// 	OffsetBuffer<int> &pointBuffer){
	// 		// maybe?
	// 	}

	static void pointEdgeBuffersFromFaceBuffer(
		OffsetBuffer<int> &pointBuffer, UniformBuffer<int> &edgeBuffer
	){
		// I am in great pain, please help me
	}

	struct IndexedComponent {
		// base class for index comparison
		int index;
		bool operator==(const IndexedComponent& other) {
			// common sense rules - don't compare face with edge
			return (index == other.index);
		}
		bool operator==(const int& other) {
			// common sense rules - don't compare face with edge
			return (index == other);
		}
	};

	// topology types, no spatial info
	struct Point; // point in space
	struct Vertex; // unique vertex, split by face
	struct Face; // polygonal face
	struct Edge; // undirected shared edge
	struct HalfEdge; // directed unique edge


	struct Point : IndexedComponent {
		int* faces;
		int* points;
	};

	struct Edge : IndexedComponent {
		Point* points[2];
	};

	struct Vertex : IndexedComponent {
		Point* point; // point to which this vertex belongs
		Face* face; // face to which this vertex belongs
		Edge* edges[2]; // previous and next edges
		HalfEdge* hedges[2]; // previous and next halfEdges
	};

	struct Face : IndexedComponent {
		std::vector<Point*> points; // points in this face
		std::vector<Edge*> edges; // edges shared by this face
		std::vector<HalfEdge*> hedges; // ordered half edges
		std::vector<Vertex*> vertices; // ordered vertices
	};



	struct HalfEdgeMesh {
		// ignore the name, this is nothing like a real half-edge mesh


		int hasBuilt = 0;

		int nPoints;
		int nFaces;
		int nVertices;
		int nEdges;
		int nHalfEdges;


		//std::vector<int> halfEdgeConnects; // uniform info : [prevVertex, nextVertex, prevHedge, nextHedge, face]

		// offsetBuffer structs for easier manipulation
		OffsetBuffer<int> pointConnects; // points to points
		OffsetBuffer<int> facePointConnects; // faces to points
		OffsetBuffer<int> faceVertexConnects; // faces to vertices


		// spatial information
		UniformBuffer<float> pointPositions;
		UniformBuffer<float> pointNormals;
		UniformBuffer<float> faceNormals;
		// positions at least should be put to double
		// will also allow direct pointer processing between maya types and buffers

		// separate buffers allowing threadsafe read-writing
		UniformBuffer<float> deltaPointPositions;
		UniformBuffer<float> deltaPointNormals;


		// uv system
		//std::unordered_map<std::string, int> uvSetNames; // set names to vector index
		// std::vector< UniformBuffer<float> > vertexUvPositions;
		// /*  vertexUvPositions:
		// face vertices mapped directly to uvs - this requires 2 floats for each vertex,
		// more tools to find uv connectivity, but is the simplest option for storage
		// */
		// std::vector< UniformBuffer<int> > uvConnects;
		// /* uvConnects:
		// regenerated once
		// how???
		// */

		// take houdini approach: uvs stored as per-vertex attributes
		// point attributes
		// std::unordered_map< *char,


		// buffer to check areas of mesh that have changed
		std::vector<float> cachePositions;


		void build(
			std::vector<int> initPointConnects,
			std::vector<int> initPointOffsets,
			std::vector<int> initFacePointConnects,
			std::vector<int> initFacePointOffsets
		) {
			// main method to build half-edge representation from raw buffers
			DEBUGS("HalfEdgeMesh::build")
				nPoints = static_cast<int>(initPointOffsets.size());
			nFaces = static_cast<int>(initFacePointOffsets.size());
			nVertices = static_cast<int>(initFacePointConnects.size()); // 1 per face per point

			// pointConnects = new OffsetBuffer<int>(initPointConnects, initPointOffsets);
			// facePointConnects = new OffsetBuffer<int>(initFacePointConnects, initFacePointOffsets);

			OffsetBuffer<int> pointConnects(initPointConnects, initPointOffsets);
			OffsetBuffer<int> facePointConnects(initFacePointConnects, initFacePointOffsets);

			int n = 0;
			// loop over faces
			for (int i = 0; i < nFaces; i++) {
			}
			// for half edges a continuous ring of points around face is needed
			hasBuilt = 1;
			DEBUGS("HalfEdgeMesh built")
		}

		void setPositions(
			std::vector<float> initPointPositions
		) {
			// sets point positions externally, to be called after build
			UniformBuffer<float> newBuffer(initPointPositions, 3);
			pointPositions = newBuffer;
		}

		void swapDeltaBuffers() {
			// swaps pointPositions and deltaPointPositions
			// used for parallel modification
			pointPositions.values.swap(deltaPointPositions.values);
			pointNormals.values.swap(deltaPointNormals.values);
		}


		// check for "dirty" points to rebuild partitioning
		std::vector<int> checkDirtyPoints(
			const std::vector<float>& prevPositions,
			const std::vector<float>& newPositions
		) {
			std::vector<int> result;
			// loop through and compare "checksum" of positions
			for (int i = 0; i < nPoints; i++) {
				float prevHash = prevPositions[3 * i] + prevPositions[3 * i + 1] + prevPositions[3 * i + 2];
				float newHash = newPositions[3 * i] + newPositions[3 * i + 1] + newPositions[3 * i + 2];
				if (abs(prevHash - newHash) > 0.0001) {
					result.push_back(i);
				}
			}
			return result;
		}

		// actual topological operations
		// object-oriented since these depend on this specific mesh type
		inline SmallList<int> adjacentFaces(int face) {
			// return all faces topologically connected to the given

			
		}



		HalfEdgeMesh() {}

		~HalfEdgeMesh() {
			//delete &pointConnects;
			//delete &facePointConnects;
			//delete &faceVertexConnects;
			//delete &pointPositions;
		}

	};


	// if two faces are connected by an edge, and share a pair of indices in point order,
	// those faces have consistent winding order


	// ---- EIGEN STUFF -----
	typedef Eigen::SparseMatrix<double> Sparse;
	typedef Eigen::Matrix<double, 4, 4> Mat4;
	typedef Eigen::Matrix<double, 4, 1> Vec4;
	typedef Eigen::Matrix<double, 3, 3> Mat3;
	typedef Eigen::Matrix<double, 3, 1> Vec3;
	typedef Eigen::Matrix<double, -1, -1> MatX;


	static Vec4 toVec4(const Vec3& v) {
		return Vec4(v[0], v[1], v[2], 1);
	}

	static Vec3 toVec3(const Vec4& v) {
		return Vec3(v[0], v[1], v[2]);
	}

	// --- LAPLACIAN AND CHILL ---

	inline Eigen::SparseMatrix<int> buildValenceMatrix(std::vector<int> &pointOffsets) {
		// aka degree matrix
		DEBUGS("topo.h buildValenceMatrix");

		int n = static_cast<int>(pointOffsets.size());
		Eigen::SparseMatrix<int> output(n, n);
		for (int i = 0; i < n; i++) {
			output.insert(i, i) = pointOffsets[i];
		}
		return output;
	}

	inline Eigen::SparseMatrix<int> buildAdjacencyMatrix(std::vector<int> &pointBuffer) {
		DEBUGS("topo.h buildAdjacencyMatrix");

		int n = static_cast<int>(pointBuffer.size());
		Eigen::SparseMatrix<int> output(n, n);

		for (int i = 0; i < n; i++) {
			for (int j = 0; j < 4; j++) {
				int foundIndex = pointBuffer[i * 4 + j];
				// check if real
				if (foundIndex < 0) {
					continue;
				}
				// insert value of one at row i, column foundIndex
				output.insert(i, foundIndex) = 1;
			}
		}
		return output;
	}


	inline Eigen::SparseMatrix<int> buildLaplaceMatrix(
		Eigen::SparseMatrix<int> &adjacencyMatrix,
		std::vector<int> &pointValences,
		int nPoints)
	{
		DEBUGS("topo.h buildLaplaceMatrix");

		Eigen::SparseMatrix<int> output(nPoints, nPoints);

		output = adjacencyMatrix;
		// negate adjacency
		output *= -1;
		// add valences in diagonal
		for (int i = 0; i < nPoints; i++) {
			output.insert(i, i) = pointValences[i];
		}
		return output;
	}

	// struct Topo to represent mesh
	// either builds buffers or accepts prebuilt
	// assumes max vertex valence of 4


} //ed
#endif
