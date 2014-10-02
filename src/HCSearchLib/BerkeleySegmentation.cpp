#include "BerkeleySegmentation.h"
#include "MyLogger.hpp"
#include "MyGraphAlgorithms.hpp"
#include <algorithm>

namespace HCSearch
{
	BerkeleySegmentationUCM::BerkeleySegmentationUCM()
	{
		// NOT IMPLEMENTED
	}

	BerkeleySegmentationUCM::BerkeleySegmentationUCM(ImgLabeling graph, map< Edge_t, double > edgeWeights)
	{
		this->graph = graph;
		this->edgeWeights = edgeWeights;
		this->weightsList = BerkeleySegmentationUtilities::getAllUniqueUCMValues(edgeWeights);
	}

	BerkeleySegmentationUCM::~BerkeleySegmentationUCM()
	{
	}

	vector<double> BerkeleySegmentationUCM::getAllWeights()
	{
		return this->weightsList;
	}

	ImgLabeling BerkeleySegmentationUCM::getThresholdedGraph(double threshold)
	{
		// first pass: determine edges to remove
		vector<Edge_t> edgesToKeep;
		for (map< Edge_t, double >::iterator it = this->edgeWeights.begin(); it != this->edgeWeights.end(); ++it)
		{
			Edge_t edge = it->first;
			double weight = it->second;

			if (weight > threshold)
			{
				edgesToKeep.push_back(edge);
			}
		}

		// second pass: remove edges
		AdjList_t adjList;
		for (vector<Edge_t>::iterator it = edgesToKeep.begin(); it != edgesToKeep.end(); ++it)
		{
			Edge_t edge = *it;
			Node_t node1 = edge.first;
			Node_t node2 = edge.second;
			
			// keep these uncut edges
			if (adjList.count(node1) == 0)
			{
				adjList[node1] = set<int>();
			}
			if (adjList.count(node2) == 0)
			{
				adjList[node2] = set<int>();
			}
			adjList[node1].insert(node2);
			adjList[node2].insert(node1);
		}

		ImgLabeling graphCut;
		graphCut.confidences = this->graph.confidences;
		graphCut.confidencesAvailable = this->graph.confidencesAvailable;
		graphCut.graph = this->graph.graph;
		graphCut.graph.adjList = adjList;

		return graphCut;
	}

	ImgLabeling BerkeleySegmentationUCM::getGraph()
	{
		return this->graph;
	}

	map< Edge_t, double > BerkeleySegmentationUCM::getEdgeWeights()
	{
		return this->edgeWeights;
	}

	vector<double> BerkeleySegmentationUtilities::getAllUniqueUCMValues(map< Edge_t, double > edgeWeights)
	{
		set<double> w;
		for (map< Edge_t, double >::iterator it = edgeWeights.begin(); it != edgeWeights.end(); ++it)
			w.insert(it->second);

		vector<double> weightsList = vector<double>(w.begin(), w.end());
		sort(weightsList.begin(), weightsList.end());

		return weightsList;
	}



	/*** BerkeleySegmentationTree ***/

	BerkeleySegmentationTree::BerkeleySegmentationTree()
	{
		// NOT IMPLEMENTED
	}

	BerkeleySegmentationTree::BerkeleySegmentationTree(ImgLabeling graph, map< Edge_t, double > edgeWeights)
	{
		// get all unique edge weights
		this->weightsList = BerkeleySegmentationUtilities::getAllUniqueUCMValues(edgeWeights);

		// construct tree and record levels
		constructTreeHelper(graph, edgeWeights, this->weightsList);

		// set initial partitioning to whole image
		this->currentPartition.insert(getRoot());
	}

	BerkeleySegmentationTree::~BerkeleySegmentationTree()
	{
		deleteTree(this->root);
	}

	void BerkeleySegmentationTree::splitRegion(BSTNode* node)
	{
		if (getCurrentPartition().count(node) == 0)
		{
			LOG(WARNING) << "region is not in the current partition.";
			return;
		}

		// cannot split superpixels
		if (node->isLeafNode())
		{
			LOG(WARNING) << "region cannot be split because it is a superpixel.";
			return;
		}

		// split region
		this->currentPartition.erase(node);
		this->currentPartition.insert(node->childL);
		this->currentPartition.insert(node->childR);

		node->isActivated = false;
	}

	void BerkeleySegmentationTree::mergeRegion(BSTNode* node)
	{
		if (getCurrentPartition().count(node) == 0)
		{
			LOG(WARNING) << "region is not in the current partition";
			return;
		}

		// cannot merge top most level
		if (node->isRootNode())
		{
			LOG(WARNING) << "region cannot be merged because it is the entire image.";
			return;
		}

		// get the sibiling of the node
		BSTNode* parent = node->parent;
		BSTNode* sibling = NULL;
		if (parent->childL == node)
		{
			sibling = parent->childR;
		}
		else if (parent->childR == node)
		{
			sibling = parent->childL;
		}
		else
		{
			LOG(ERROR) << "no child of parent belongs to node, impossible case";
			exit(1);
		}

		if (sibling == NULL)
		{
			LOG(ERROR) << "sibiling is NULL";
			exit(1);
		}

		// check that the sibiling is in the current partition
		if (getCurrentPartition().count(sibling) == 0)
		{
			LOG(WARNING) << "sibiling of region to merge is not in the current partition";
			return;
		}

		// merge region
		this->currentPartition.erase(node);
		this->currentPartition.erase(sibling);
		this->currentPartition.insert(parent);

		parent->isActivated = true;
	}

	bool BerkeleySegmentationTree::isSiblingInPartition(BSTNode* node)
	{
		// top level does not have sibiling
		if (node->isRootNode())
		{
			return false;
		}

		// get the sibiling of the node
		BSTNode* parent = node->parent;
		BSTNode* sibling = NULL;
		if (parent->childL == node)
		{
			sibling = parent->childR;
		}
		else if (parent->childR == node)
		{
			sibling = parent->childL;
		}
		else
		{
			LOG(ERROR) << "no child of parent belongs to node, impossible case";
			exit(1);
		}

		if (sibling == NULL)
		{
			LOG(ERROR) << "sibiling is NULL";
			exit(1);
		}

		// check that the sibiling is in the current partition
		return getCurrentPartition().count(sibling) == 0;
	}

	set<BSTNode*> BerkeleySegmentationTree::getCurrentPartition()
	{
		return this->currentPartition;
	}

	BSTNode* BerkeleySegmentationTree::getRoot()
	{
		return this->root;
	}

	// TODO: figure out a sanity check to make sure edge weights are good
	void BerkeleySegmentationTree::constructTreeHelper(ImgLabeling graph, 
		map< Edge_t, double > edgeWeights, vector<double> weightsList)
	{
		using namespace MyGraphAlgorithms;

		// first pass: order by weights
		map< double, set<Edge_t> > edgesOrdered;
		for (map< Edge_t, double >::iterator it = edgeWeights.begin(); it != edgeWeights.end(); ++it)
		{
			Edge_t edge = it->first;
			double weight = it->second;

			if (edgesOrdered.count(weight) == 0)
				edgesOrdered[weight] = set<Edge_t>();
			edgesOrdered[weight].insert(edge);
		}

		// data structures to help construct tree
		DisjointSet ds = DisjointSet(graph.getNumNodes()); // for keeping track of merging
		VectorXi nodeIDs = VectorXi::Zero(graph.getNumNodes()); // for keeping track of new IDs for intermediate nodes
		int idCounter = 0; // for creating new IDs for intermediate nodes

		// construct BSTNodes for all superpixels
		vector<BSTNode*> BSTNodeList; // for keeping track of nodes
		for (int nodeID = 0; nodeID < graph.getNumNodes(); nodeID++)
		{
			// initialize node IDs
			nodeIDs(nodeID) = nodeID;

			// construct node for tree
			BSTNode* superpixelNode = new BSTNode();
			superpixelNode->nodeID = nodeID;
			superpixelNode->descendentSuperpixels.insert(nodeID);
			superpixelNode->isActivated = true;
			BSTNodeList.push_back(superpixelNode);

			// update ID counter
			idCounter++;
		}

		// second pass: construct tree by merging into intermediate nodes
		for (vector<double>::iterator it = weightsList.begin(); it != weightsList.end(); ++it)
		{
			double threshold = *it;
			if (edgesOrdered.count(threshold) == 0)
			{
				LOG(ERROR) << "failed sanity check that threshold must exist for some edge";
				throw 1;
			}
			set<Edge_t> edgeSet = edgesOrdered[threshold];

			// repeat until cannot select any more contours:
			// select contour of weight equal to the threshold
			// create intermediate node that links the two regions
			for (set<Edge_t>::iterator it2 = edgeSet.begin(); it2 != edgeSet.end(); ++it2)
			{
				Edge_t edge = *it2;
				Node_t node1 = edge.first;
				Node_t node2 = edge.second;

				if (ds.FindSet(node1) == ds.FindSet(node2))
				{
					// already merged? duplicate case handled?
					continue;
				}

				// construct intermediate node
				int nodeID = idCounter;
				BSTNode* leftChildNode = BSTNodeList[nodeIDs(node1)];
				BSTNode* rightChildNode = BSTNodeList[nodeIDs(node2)];
				if (leftChildNode == NULL)
				{
					LOG(ERROR) << "failed sanity check that left child is not null";
					throw 1;
				}
				if (rightChildNode == NULL)
				{
					LOG(ERROR) << "failed sanity check that right child is not null";
					throw 1;
				}

				BSTNode* intermediateNode = new BSTNode();
				intermediateNode->nodeID = nodeID;
				intermediateNode->childL = leftChildNode;
				intermediateNode->childR = rightChildNode;
				intermediateNode->ucmValue = threshold;
				set<Node_t> leftChildDescendents = leftChildNode->getAllDescendentSuperpixels();
				intermediateNode->descendentSuperpixels.insert(leftChildDescendents.begin(), leftChildDescendents.end());
				set<Node_t> rightChildDescendents = rightChildNode->getAllDescendentSuperpixels();
				intermediateNode->descendentSuperpixels.insert(rightChildDescendents.begin(), rightChildDescendents.end());
				intermediateNode->isActivated = true;
				BSTNodeList.push_back(intermediateNode);

				// update children to parent pointers
				leftChildNode->parent = intermediateNode;
				rightChildNode->parent = intermediateNode;

				// update union
				ds.Union(node1, node2);
				Node_t node1Top = nodeIDs(node1);
				Node_t node2Top = nodeIDs(node2);

				// TODO: more efficient way to update...
				for (int i = 0; i < graph.getNumNodes(); i++)
				{
					if (nodeIDs(i) == node1Top || nodeIDs(i) == node2Top)
						nodeIDs(i) = nodeID;
				}

				// increment counter
				idCounter++;
			}

			// set level
			int nodeID = idCounter-1;
			this->levels[threshold] = BSTNodeList[nodeID];
		}

		// SANITY CHECKS BEFORE FINISHING

		int checkAllSameID = nodeIDs[0];
		for (int i = 1; i < nodeIDs.size(); i++)
		{
			if (nodeIDs[i] != checkAllSameID)
			{
				LOG(ERROR) << "failed sanity check that all superpixels ultimately lead to root";
				throw 1;
			}
		}
		// sanity check: check last node is the parent
		BSTNode* root = BSTNodeList.back();
		if (root->nodeID != checkAllSameID)
		{
			LOG(ERROR) << "failed sanity check that latest node generated is root";
			throw 1;
		}

		this->root = root;
	}

	void BerkeleySegmentationTree::deleteTree(BSTNode* node)
	{
		// delete left subtree
		if (node->childL != NULL && node->childL->isLeafNode())
		{
			delete node->childL;
		}
		else
		{
			deleteTree(node->childL);
		}
		node->childL = NULL;

		// delete right subtree
		if (node->childR != NULL && node->childR->isLeafNode())
		{
			delete node->childR;
		}
		else
		{
			deleteTree(node->childR);
		}
		node->childR = NULL;

		// delete node
		delete node;
	}



	/*** BSTNode ***/

	BSTNode::BSTNode()
	{
		this->parent = NULL;
		this->childL = NULL;
		this->childR = NULL;
		this->nodeID = -1;
		this->ucmValue = 0;
		this->descendentSuperpixels = set<Node_t>();
		this->isActivated = false;
	}

	BSTNode::~BSTNode()
	{
	}

	set<Node_t> BSTNode::getAllDescendentSuperpixels()
	{
		return this->descendentSuperpixels;
	}
		
	bool BSTNode::isLeafNode()
	{
		return this->childL == NULL && this->childR == NULL;
	}

	bool BSTNode::isIntermediateNode()
	{
		return !isLeafNode();
	}

	bool BSTNode::isRootNode()
	{
		return this->parent == NULL;
	}
}