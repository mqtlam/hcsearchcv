#include "FeatureFunction.hpp"
#include "Globals.hpp"

namespace HCSearch
{
	/**************** Search Space Functions Abstract Definitions ****************/

	int IFeatureFunction::featureSize(ImgFeatures& X, ImgLabeling& Y, set<int> action)
	{
		// This is inefficient but does the job
		// Better to define more efficient functions for extended classes
		return computeFeatures(X, Y, action).data.size();
	}

	/**************** Feature Functions ****************/

	/**************** Standard Features ****************/

	StandardFeatures::StandardFeatures()
	{
	}

	StandardFeatures::~StandardFeatures()
	{
	}

	RankFeatures StandardFeatures::computeFeatures(ImgFeatures& X, ImgLabeling& Y, set<int> action)
	{
		int numNodes = X.getNumNodes();
		int featureDim = X.getFeatureDim();
		int numClasses = Global::settings->CLASSES.numClasses();

		int unaryFeatDim = 1+featureDim;
		int pairwiseFeatDim = featureDim;
		int numPairs = (numClasses*(numClasses+1))/2;

		VectorXd phi = VectorXd::Zero(featureSize(X, Y, action));
		
		VectorXd unaryTerm = computeUnaryTerm(X, Y);
		VectorXd pairwiseTerm = computePairwiseTerm(X, Y);

		phi.segment(0, numClasses*unaryFeatDim) = unaryTerm;
		phi.segment(numClasses*unaryFeatDim, numPairs*pairwiseFeatDim) = pairwiseTerm;

		return RankFeatures(phi);
	}

	int StandardFeatures::featureSize(ImgFeatures& X, ImgLabeling& Y, set<int> action)
	{
		int numNodes = X.getNumNodes();
		int featureDim = X.getFeatureDim();
		int unaryFeatDim = 1+featureDim;
		int pairwiseFeatDim = featureDim;
		int numClasses = Global::settings->CLASSES.numClasses();
		int numPairs = (numClasses*(numClasses+1))/2;

		return numClasses*unaryFeatDim + numPairs*pairwiseFeatDim;
	}

	VectorXd StandardFeatures::computeUnaryTerm(ImgFeatures& X, ImgLabeling& Y)
	{
		const int numNodes = X.getNumNodes();
		const int numClasses = Global::settings->CLASSES.numClasses();
		const int featureDim = X.getFeatureDim();
		const int unaryFeatDim = 1+featureDim;
		
		VectorXd phi = VectorXd::Zero(numClasses*unaryFeatDim);

		// unary potential
		for (int node = 0; node < numNodes; node++)
		{
			// get node features and label
			VectorXd nodeFeatures = X.graph.nodesData.row(node);
			int nodeLabel = Y.getLabel(node);

			// map node label to indexing value in phi vector
			int classIndex = Global::settings->CLASSES.getClassIndex(nodeLabel);

			// assignment: bias and unary feature
			phi(classIndex*unaryFeatDim) += 1;
			phi.segment(classIndex*unaryFeatDim+1, featureDim) += nodeFeatures;
		}

		phi = 1.0/X.getNumNodes() * phi;

		return phi;
	}
	
	VectorXd StandardFeatures::computePairwiseTerm(ImgFeatures& X, ImgLabeling& Y)
	{
		const int numNodes = X.getNumNodes();
		const int numClasses = Global::settings->CLASSES.numClasses();
		const int featureDim = X.getFeatureDim();
		const int pairwiseFeatDim = featureDim;
		int numPairs = (numClasses*(numClasses+1))/2;
		
		VectorXd phi = VectorXd::Zero(numPairs*pairwiseFeatDim);

		int numEdges = 0;
		for (int node1 = 0; node1 < numNodes; node1++)
		{
			if (X.graph.adjList.count(node1) == 0)
				continue;

			// get neighbors (ending nodes) of starting node
			NeighborSet_t neighbors = X.graph.adjList[node1];
			const int numNeighbors = neighbors.size();
			for (NeighborSet_t::iterator it = neighbors.begin(); it != neighbors.end(); ++it)
			{
				int node2 = *it;
				numEdges++;

				// get node features and label
				VectorXd nodeFeatures1 = X.graph.nodesData.row(node1);
				int nodeLabel1 = Y.getLabel(node1);

				VectorXd nodeFeatures2 = X.graph.nodesData.row(node2);
				int nodeLabel2 = Y.getLabel(node2);

				int classIndex = -1;
				VectorXd edgeFeatureVector = computePairwiseFeatures(nodeFeatures1, nodeFeatures2, nodeLabel1, nodeLabel2, classIndex);
				phi.segment(classIndex*pairwiseFeatDim, pairwiseFeatDim) += edgeFeatureVector; // contrast sensitive pairwise potential
			}
		}

		phi = 1.0/numEdges * phi;

		return phi;
	}

	VectorXd StandardFeatures::computePairwiseFeatures(VectorXd& nodeFeatures1, VectorXd& nodeFeatures2, 
		int nodeLabel1, int nodeLabel2, int& classIndex)
	{
		int node1ClassIndex = Global::settings->CLASSES.getClassIndex(nodeLabel1);
		int node2ClassIndex = Global::settings->CLASSES.getClassIndex(nodeLabel2);
		int numClasses = Global::settings->CLASSES.numClasses();

		int i = min(node1ClassIndex, node2ClassIndex);
		int j = max(node1ClassIndex, node2ClassIndex);

		classIndex = (numClasses*(numClasses+1)-(numClasses-i)*(numClasses-i+1))/2+(numClasses-1-j);

		// phi features depend on labels
		if (nodeLabel1 != nodeLabel2)
		{
			VectorXd diff = nodeFeatures1 - nodeFeatures2;
			VectorXd negdiffabs2 = -diff.cwiseAbs2();
			VectorXd expnegdiffabs2 = negdiffabs2.array().exp();

			// assignment
			return expnegdiffabs2;
		}
		else
		{
			VectorXd diff = nodeFeatures1 - nodeFeatures2;
			VectorXd negdiffabs2 = -diff.cwiseAbs2();
			VectorXd expnegdiffabs2 = 1 - negdiffabs2.array().exp();

			// assignment
			return expnegdiffabs2;
		}
	}

	/**************** Standard Features Alternative Formulation ****************/

	StandardAltFeatures::StandardAltFeatures()
	{
	}

	StandardAltFeatures::~StandardAltFeatures()
	{
	}

	RankFeatures StandardAltFeatures::computeFeatures(ImgFeatures& X, ImgLabeling& Y, set<int> action)
	{
		int numNodes = X.getNumNodes();
		int featureDim = X.getFeatureDim();
		int numClasses = Global::settings->CLASSES.numClasses();

		int unaryFeatDim = 1+featureDim;
		int pairwiseFeatDim = featureDim;

		VectorXd phi = VectorXd::Zero(featureSize(X, Y, action));
		
		VectorXd unaryTerm = computeUnaryTerm(X, Y);
		VectorXd pairwiseTerm = computePairwiseTerm(X, Y);

		phi.segment(0, numClasses*unaryFeatDim) = unaryTerm;
		phi.segment(numClasses*unaryFeatDim, (numClasses+1)*pairwiseFeatDim) = pairwiseTerm;

		return RankFeatures(phi);
	}

	int StandardAltFeatures::featureSize(ImgFeatures& X, ImgLabeling& Y, set<int> action)
	{
		int numNodes = X.getNumNodes();
		int featureDim = X.getFeatureDim();
		int unaryFeatDim = 1+featureDim;
		int pairwiseFeatDim = featureDim;
		int numClasses = Global::settings->CLASSES.numClasses();

		return numClasses*unaryFeatDim + (numClasses+1)*pairwiseFeatDim;
	}

	VectorXd StandardAltFeatures::computeUnaryTerm(ImgFeatures& X, ImgLabeling& Y)
	{
		const int numNodes = X.getNumNodes();
		const int numClasses = Global::settings->CLASSES.numClasses();
		const int featureDim = X.getFeatureDim();
		const int unaryFeatDim = 1+featureDim;
		
		VectorXd phi = VectorXd::Zero(numClasses*unaryFeatDim);

		// unary potential
		for (int node = 0; node < numNodes; node++)
		{
			// get node features and label
			VectorXd nodeFeatures = X.graph.nodesData.row(node);
			int nodeLabel = Y.getLabel(node);

			// map node label to indexing value in phi vector
			int classIndex = Global::settings->CLASSES.getClassIndex(nodeLabel);

			// assignment: bias and unary feature
			phi(classIndex*unaryFeatDim) += 1;
			phi.segment(classIndex*unaryFeatDim+1, featureDim) += nodeFeatures;
		}

		return phi;
	}
	
	VectorXd StandardAltFeatures::computePairwiseTerm(ImgFeatures& X, ImgLabeling& Y)
	{
		const int numNodes = X.getNumNodes();
		const int numClasses = Global::settings->CLASSES.numClasses();
		const int featureDim = X.getFeatureDim();
		const int pairwiseFeatDim = featureDim;
		
		VectorXd phi = VectorXd::Zero((numClasses+1)*pairwiseFeatDim);

		for (int node1 = 0; node1 < numNodes; node1++)
		{
			if (X.graph.adjList.count(node1) == 0)
				continue;

			// get neighbors (ending nodes) of starting node
			NeighborSet_t neighbors = X.graph.adjList[node1];
			const int numNeighbors = neighbors.size();
			for (NeighborSet_t::iterator it = neighbors.begin(); it != neighbors.end(); ++it)
			{
				int node2 = *it;

				// get node features and label
				VectorXd nodeFeatures1 = X.graph.nodesData.row(node1);
				int nodeLabel1 = Y.getLabel(node1);

				VectorXd nodeFeatures2 = X.graph.nodesData.row(node2);
				int nodeLabel2 = Y.getLabel(node2);

				int classIndex = -1;
				VectorXd edgeFeatureVector = computePairwiseFeatures(nodeFeatures1, nodeFeatures2, nodeLabel1, nodeLabel2, classIndex);
				phi.segment(classIndex*pairwiseFeatDim, pairwiseFeatDim) += edgeFeatureVector; // contrast sensitive pairwise potential
			}
		}

		return 0.5*phi;
	}

	VectorXd StandardAltFeatures::computePairwiseFeatures(VectorXd& nodeFeatures1, VectorXd& nodeFeatures2, 
		int nodeLabel1, int nodeLabel2, int& classIndex)
	{
		// phi features depend on labels
		if (nodeLabel1 != nodeLabel2)
		{
			VectorXd diff = nodeFeatures1 - nodeFeatures2;
			VectorXd negdiffabs2 = -diff.cwiseAbs2();
			VectorXd expnegdiffabs2 = negdiffabs2.array().exp();
			classIndex = Global::settings->CLASSES.numClasses(); // numClasses

			// assignment
			return expnegdiffabs2;
		}
		else
		{
			VectorXd diff = nodeFeatures1 - nodeFeatures2;
			VectorXd negdiffabs2 = -diff.cwiseAbs2();
			VectorXd expnegdiffabs2 = 1 - negdiffabs2.array().exp();

			// map node label to indexing value in phi vector
			classIndex = Global::settings->CLASSES.getClassIndex(nodeLabel1);

			// assignment
			return expnegdiffabs2;
		}
	}

	/**************** Standard Features With Unary Confidences and Raw Pairwise ****************/

	StandardConfFeatures::StandardConfFeatures()
	{
	}

	StandardConfFeatures::~StandardConfFeatures()
	{
	}

	RankFeatures StandardConfFeatures::computeFeatures(ImgFeatures& X, ImgLabeling& Y, set<int> action)
	{
		int numNodes = X.getNumNodes();
		int featureDim = X.getFeatureDim();
		int numClasses = Global::settings->CLASSES.numClasses();

		int unaryFeatDim = 1;
		int pairwiseFeatDim = featureDim;
		int numPairs = (numClasses*(numClasses+1))/2;

		VectorXd phi = VectorXd::Zero(featureSize(X, Y, action));
		
		VectorXd unaryTerm = computeUnaryTerm(X, Y);
		VectorXd pairwiseTerm = computePairwiseTerm(X, Y);

		phi.segment(0, numClasses*unaryFeatDim) = unaryTerm;
		phi.segment(numClasses*unaryFeatDim, numPairs*pairwiseFeatDim) = pairwiseTerm;

		return RankFeatures(phi);
	}

	int StandardConfFeatures::featureSize(ImgFeatures& X, ImgLabeling& Y, set<int> action)
	{
		int numNodes = X.getNumNodes();
		int featureDim = X.getFeatureDim();
		int unaryFeatDim = 1;
		int pairwiseFeatDim = featureDim;
		int numClasses = Global::settings->CLASSES.numClasses();
		int numPairs = (numClasses*(numClasses+1))/2;

		return numClasses*unaryFeatDim + numPairs*pairwiseFeatDim;
	}

	VectorXd StandardConfFeatures::computeUnaryTerm(ImgFeatures& X, ImgLabeling& Y)
	{
		if (!Y.confidencesAvailable)
		{
			LOG(ERROR) << "confidences not available for unary potential.";
			abort();
		}

		const int numNodes = X.getNumNodes();
		const int numClasses = Global::settings->CLASSES.numClasses();
		const int featureDim = X.getFeatureDim();
		const int unaryFeatDim = 1;
		
		VectorXd phi = VectorXd::Zero(numClasses*unaryFeatDim);

		// unary potential
		for (int node = 0; node < numNodes; node++)
		{
			// get node features and label
			VectorXd nodeFeatures = X.graph.nodesData.row(node);
			int nodeLabel = Y.getLabel(node);

			// map node label to indexing value in phi vector
			int classIndex = Global::settings->CLASSES.getClassIndex(nodeLabel);

			// assignment
			phi(classIndex*unaryFeatDim) += 1-Y.confidences(node, classIndex);
		}

		phi = 1.0/X.getNumNodes() * phi;

		return phi;
	}
	
	VectorXd StandardConfFeatures::computePairwiseTerm(ImgFeatures& X, ImgLabeling& Y)
	{
		const int numNodes = X.getNumNodes();
		const int numClasses = Global::settings->CLASSES.numClasses();
		const int featureDim = X.getFeatureDim();
		const int pairwiseFeatDim = featureDim;
		int numPairs = (numClasses*(numClasses+1))/2;
		
		VectorXd phi = VectorXd::Zero(numPairs*pairwiseFeatDim);

		int numEdges = 0;
		for (int node1 = 0; node1 < numNodes; node1++)
		{
			if (X.graph.adjList.count(node1) == 0)
				continue;

			// get neighbors (ending nodes) of starting node
			NeighborSet_t neighbors = X.graph.adjList[node1];
			const int numNeighbors = neighbors.size();
			for (NeighborSet_t::iterator it = neighbors.begin(); it != neighbors.end(); ++it)
			{
				int node2 = *it;
				numEdges++;

				// get node features and label
				VectorXd nodeFeatures1 = X.graph.nodesData.row(node1);
				int nodeLabel1 = Y.getLabel(node1);

				VectorXd nodeFeatures2 = X.graph.nodesData.row(node2);
				int nodeLabel2 = Y.getLabel(node2);

				int classIndex = -1;
				VectorXd edgeFeatureVector = computePairwiseFeatures(nodeFeatures1, nodeFeatures2, nodeLabel1, nodeLabel2, classIndex);
				phi.segment(classIndex*pairwiseFeatDim, pairwiseFeatDim) += edgeFeatureVector; // contrast sensitive pairwise potential
			}
		}

		phi = 1.0/numEdges * phi;

		return phi;
	}

	VectorXd StandardConfFeatures::computePairwiseFeatures(VectorXd& nodeFeatures1, VectorXd& nodeFeatures2, 
		int nodeLabel1, int nodeLabel2, int& classIndex)
	{
		int node1ClassIndex = Global::settings->CLASSES.getClassIndex(nodeLabel1);
		int node2ClassIndex = Global::settings->CLASSES.getClassIndex(nodeLabel2);
		int numClasses = Global::settings->CLASSES.numClasses();

		int i = min(node1ClassIndex, node2ClassIndex);
		int j = max(node1ClassIndex, node2ClassIndex);

		classIndex = (numClasses*(numClasses+1)-(numClasses-i)*(numClasses-i+1))/2+(numClasses-1-j);

		// phi features depend on labels
		if (nodeLabel1 != nodeLabel2)
		{
			VectorXd diff = nodeFeatures1 - nodeFeatures2;
			VectorXd negdiffabs2 = -diff.cwiseAbs2();
			VectorXd expnegdiffabs2 = negdiffabs2.array().exp();

			// assignment
			return expnegdiffabs2;
		}
		else
		{
			VectorXd diff = nodeFeatures1 - nodeFeatures2;
			VectorXd negdiffabs2 = -diff.cwiseAbs2();
			VectorXd expnegdiffabs2 = 1 - negdiffabs2.array().exp();

			// assignment
			return expnegdiffabs2;
		}
	}

	/**************** Unary Only Raw Features ****************/

	UnaryFeatures::UnaryFeatures()
	{
	}

	UnaryFeatures::~UnaryFeatures()
	{
	}

	RankFeatures UnaryFeatures::computeFeatures(ImgFeatures& X, ImgLabeling& Y, set<int> action)
	{
		int numNodes = X.getNumNodes();
		int featureDim = X.getFeatureDim();
		int numClasses = Global::settings->CLASSES.numClasses();

		int unaryFeatDim = 1+featureDim;
		int pairwiseFeatDim = featureDim;

		VectorXd phi = VectorXd::Zero(featureSize(X, Y, action));
		
		VectorXd unaryTerm = computeUnaryTerm(X, Y);

		phi.segment(0, numClasses*unaryFeatDim) = unaryTerm;

		return RankFeatures(phi);
	}

	int UnaryFeatures::featureSize(ImgFeatures& X, ImgLabeling& Y, set<int> action)
	{
		int numNodes = X.getNumNodes();
		int featureDim = X.getFeatureDim();
		int unaryFeatDim = 1+featureDim;
		int pairwiseFeatDim = featureDim;
		int numClasses = Global::settings->CLASSES.numClasses();

		return numClasses*unaryFeatDim;
	}

	/**************** Unary Only Confidences Features ****************/

	UnaryConfFeatures::UnaryConfFeatures()
	{
	}

	UnaryConfFeatures::~UnaryConfFeatures()
	{
	}

	RankFeatures UnaryConfFeatures::computeFeatures(ImgFeatures& X, ImgLabeling& Y, set<int> action)
	{
		int numNodes = X.getNumNodes();
		int featureDim = X.getFeatureDim();
		int numClasses = Global::settings->CLASSES.numClasses();

		int unaryFeatDim = 1;
		int pairwiseFeatDim = featureDim;
		int numPairs = (numClasses*(numClasses+1))/2;

		VectorXd phi = VectorXd::Zero(featureSize(X, Y, action));
		
		VectorXd unaryTerm = computeUnaryTerm(X, Y);

		phi.segment(0, numClasses*unaryFeatDim) = unaryTerm;

		return RankFeatures(phi);
	}

	int UnaryConfFeatures::featureSize(ImgFeatures& X, ImgLabeling& Y, set<int> action)
	{
		int numNodes = X.getNumNodes();
		int featureDim = X.getFeatureDim();
		int unaryFeatDim = 1;
		int pairwiseFeatDim = featureDim;
		int numClasses = Global::settings->CLASSES.numClasses();
		int numPairs = (numClasses*(numClasses+1))/2;

		return numClasses*unaryFeatDim;
	}

	/**************** Standard Raw Unary and Co-occurence Counts Pairwise Features ****************/

	StandardPairwiseCountsFeatures::StandardPairwiseCountsFeatures()
	{
	}

	StandardPairwiseCountsFeatures::~StandardPairwiseCountsFeatures()
	{
	}

	RankFeatures StandardPairwiseCountsFeatures::computeFeatures(ImgFeatures& X, ImgLabeling& Y, set<int> action)
	{
		int numNodes = X.getNumNodes();
		int featureDim = X.getFeatureDim();
		int numClasses = Global::settings->CLASSES.numClasses();

		int unaryFeatDim = 1+featureDim;
		int pairwiseFeatDim = 1;
		int numPairs = (numClasses*(numClasses+1))/2;

		VectorXd phi = VectorXd::Zero(featureSize(X, Y, action));
		
		VectorXd unaryTerm = computeUnaryTerm(X, Y);
		VectorXd pairwiseTerm = computePairwiseTerm(X, Y);

		phi.segment(0, numClasses*unaryFeatDim) = unaryTerm;
		phi.segment(numClasses*unaryFeatDim, numPairs*pairwiseFeatDim) = pairwiseTerm;

		return RankFeatures(phi);
	}

	int StandardPairwiseCountsFeatures::featureSize(ImgFeatures& X, ImgLabeling& Y, set<int> action)
	{
		int numNodes = X.getNumNodes();
		int featureDim = X.getFeatureDim();
		int unaryFeatDim = 1+featureDim;
		int pairwiseFeatDim = 1;
		int numClasses = Global::settings->CLASSES.numClasses();
		int numPairs = (numClasses*(numClasses+1))/2;

		return numClasses*unaryFeatDim + numPairs*pairwiseFeatDim;
	}
	
	VectorXd StandardPairwiseCountsFeatures::computePairwiseTerm(ImgFeatures& X, ImgLabeling& Y)
	{
		const int numNodes = X.getNumNodes();
		const int numClasses = Global::settings->CLASSES.numClasses();
		const int featureDim = X.getFeatureDim();
		const int pairwiseFeatDim = 1;
		int numPairs = (numClasses*(numClasses+1))/2;
		
		VectorXd phi = VectorXd::Zero(numPairs*pairwiseFeatDim);

		int numEdges = 0;
		for (int node1 = 0; node1 < numNodes; node1++)
		{
			if (X.graph.adjList.count(node1) == 0)
				continue;

			// get neighbors (ending nodes) of starting node
			NeighborSet_t neighbors = X.graph.adjList[node1];
			const int numNeighbors = neighbors.size();
			for (NeighborSet_t::iterator it = neighbors.begin(); it != neighbors.end(); ++it)
			{
				int node2 = *it;
				numEdges++;

				// get node features and label
				VectorXd nodeFeatures1 = X.graph.nodesData.row(node1);
				int nodeLabel1 = Y.getLabel(node1);

				VectorXd nodeFeatures2 = X.graph.nodesData.row(node2);
				int nodeLabel2 = Y.getLabel(node2);

				int classIndex = -1;
				VectorXd edgeFeatureVector = computePairwiseFeatures(nodeFeatures1, nodeFeatures2, nodeLabel1, nodeLabel2, classIndex);
				phi.segment(classIndex*pairwiseFeatDim, pairwiseFeatDim) += edgeFeatureVector; // contrast sensitive pairwise potential
			}
		}

		phi = 1.0/numEdges * phi;

		return phi;
	}

	VectorXd StandardPairwiseCountsFeatures::computePairwiseFeatures(VectorXd& nodeFeatures1, VectorXd& nodeFeatures2, 
		int nodeLabel1, int nodeLabel2, int& classIndex)
	{
		int node1ClassIndex = Global::settings->CLASSES.getClassIndex(nodeLabel1);
		int node2ClassIndex = Global::settings->CLASSES.getClassIndex(nodeLabel2);
		int numClasses = Global::settings->CLASSES.numClasses();

		int i = min(node1ClassIndex, node2ClassIndex);
		int j = max(node1ClassIndex, node2ClassIndex);

		classIndex = (numClasses*(numClasses+1)-(numClasses-i)*(numClasses-i+1))/2+(numClasses-1-j);

		VectorXd one = VectorXd::Ones(1);
		return one;
	}

	/**************** Standard Confidences Unary and Co-occurence Counts Pairwise Features ****************/

	StandardConfPairwiseCountsFeatures::StandardConfPairwiseCountsFeatures()
	{
	}

	StandardConfPairwiseCountsFeatures::~StandardConfPairwiseCountsFeatures()
	{
	}

	RankFeatures StandardConfPairwiseCountsFeatures::computeFeatures(ImgFeatures& X, ImgLabeling& Y, set<int> action)
	{
		int numNodes = X.getNumNodes();
		int featureDim = X.getFeatureDim();
		int numClasses = Global::settings->CLASSES.numClasses();

		int unaryFeatDim = 1;
		int pairwiseFeatDim = 1;
		int numPairs = (numClasses*(numClasses+1))/2;

		VectorXd phi = VectorXd::Zero(featureSize(X, Y, action));
		
		VectorXd unaryTerm = computeUnaryTerm(X, Y);
		VectorXd pairwiseTerm = computePairwiseTerm(X, Y);

		phi.segment(0, numClasses*unaryFeatDim) = unaryTerm;
		phi.segment(numClasses*unaryFeatDim, numPairs*pairwiseFeatDim) = pairwiseTerm;

		return RankFeatures(phi);
	}

	int StandardConfPairwiseCountsFeatures::featureSize(ImgFeatures& X, ImgLabeling& Y, set<int> action)
	{
		int numNodes = X.getNumNodes();
		int featureDim = X.getFeatureDim();
		int unaryFeatDim = 1;
		int pairwiseFeatDim = 1;
		int numClasses = Global::settings->CLASSES.numClasses();
		int numPairs = (numClasses*(numClasses+1))/2;

		return numClasses*unaryFeatDim + numPairs*pairwiseFeatDim;
	}
	
	VectorXd StandardConfPairwiseCountsFeatures::computePairwiseTerm(ImgFeatures& X, ImgLabeling& Y)
	{
		const int numNodes = X.getNumNodes();
		const int numClasses = Global::settings->CLASSES.numClasses();
		const int featureDim = X.getFeatureDim();
		const int pairwiseFeatDim = 1;
		int numPairs = (numClasses*(numClasses+1))/2;
		
		VectorXd phi = VectorXd::Zero(numPairs*pairwiseFeatDim);

		int numEdges = 0;
		for (int node1 = 0; node1 < numNodes; node1++)
		{
			if (X.graph.adjList.count(node1) == 0)
				continue;

			// get neighbors (ending nodes) of starting node
			NeighborSet_t neighbors = X.graph.adjList[node1];
			const int numNeighbors = neighbors.size();
			for (NeighborSet_t::iterator it = neighbors.begin(); it != neighbors.end(); ++it)
			{
				int node2 = *it;
				numEdges++;

				// get node features and label
				VectorXd nodeFeatures1 = X.graph.nodesData.row(node1);
				int nodeLabel1 = Y.getLabel(node1);

				VectorXd nodeFeatures2 = X.graph.nodesData.row(node2);
				int nodeLabel2 = Y.getLabel(node2);

				int classIndex = -1;
				VectorXd edgeFeatureVector = computePairwiseFeatures(nodeFeatures1, nodeFeatures2, nodeLabel1, nodeLabel2, classIndex);
				phi.segment(classIndex*pairwiseFeatDim, pairwiseFeatDim) += edgeFeatureVector; // contrast sensitive pairwise potential
			}
		}

		phi = 1.0/numEdges * phi;

		return phi;
	}

	VectorXd StandardConfPairwiseCountsFeatures::computePairwiseFeatures(VectorXd& nodeFeatures1, VectorXd& nodeFeatures2, 
		int nodeLabel1, int nodeLabel2, int& classIndex)
	{
		int node1ClassIndex = Global::settings->CLASSES.getClassIndex(nodeLabel1);
		int node2ClassIndex = Global::settings->CLASSES.getClassIndex(nodeLabel2);
		int numClasses = Global::settings->CLASSES.numClasses();

		int i = min(node1ClassIndex, node2ClassIndex);
		int j = max(node1ClassIndex, node2ClassIndex);

		classIndex = (numClasses*(numClasses+1)-(numClasses-i)*(numClasses-i+1))/2+(numClasses-1-j);

		VectorXd one = VectorXd::Ones(1);
		return one;
	}

	/**************** Dense CRF Features ****************/

	DenseCRFFeatures::DenseCRFFeatures()
	{
	}

	DenseCRFFeatures::~DenseCRFFeatures()
	{
	}

	RankFeatures DenseCRFFeatures::computeFeatures(ImgFeatures& X, ImgLabeling& Y, set<int> action)
	{
		int numNodes = X.getNumNodes();
		int featureDim = X.getFeatureDim();
		int numClasses = Global::settings->CLASSES.numClasses();
		int numPairs = (numClasses*(numClasses+1))/2;

		int unaryFeatDim = 1;
		int pairwiseFeatDim = 2;

		VectorXd phi = VectorXd::Zero(featureSize(X, Y, action));
		
		VectorXd unaryTerm = computeUnaryTerm(X, Y);
		VectorXd pairwiseTerm = computePairwiseTerm(X, Y);

		phi.segment(0, numClasses*unaryFeatDim) = unaryTerm;
		phi.segment(numClasses*unaryFeatDim, numPairs*pairwiseFeatDim) = pairwiseTerm;

		return RankFeatures(phi);
	}

	int DenseCRFFeatures::featureSize(ImgFeatures& X, ImgLabeling& Y, set<int> action)
	{
		int numNodes = X.getNumNodes();
		int featureDim = X.getFeatureDim();
		int unaryFeatDim = 1;
		int pairwiseFeatDim = 2;
		int numClasses = Global::settings->CLASSES.numClasses();
		int numPairs = (numClasses*(numClasses+1))/2;

		return numClasses*unaryFeatDim + numPairs*pairwiseFeatDim;
	}
	
	VectorXd DenseCRFFeatures::computeUnaryTerm(ImgFeatures& X, ImgLabeling& Y)
	{
		if (!Y.confidencesAvailable)
		{
			LOG(ERROR) << "confidences not available for unary potential.";
			abort();
		}

		const int numNodes = X.getNumNodes();
		const int numClasses = Global::settings->CLASSES.numClasses();
		const int featureDim = X.getFeatureDim();
		const int unaryFeatDim = 1;
		
		VectorXd phi = VectorXd::Zero(numClasses*unaryFeatDim);

		// unary potential
		for (int node = 0; node < numNodes; node++)
		{
			// get node features and label
			VectorXd nodeFeatures = X.graph.nodesData.row(node);
			int nodeLabel = Y.getLabel(node);

			// map node label to indexing value in phi vector
			int classIndex = Global::settings->CLASSES.getClassIndex(nodeLabel);

			// assignment
			phi(classIndex*unaryFeatDim) += 1-Y.confidences(node, classIndex);
		}

		phi = 1.0/X.getNumNodes() * phi;

		return phi;
	}

	VectorXd DenseCRFFeatures::computePairwiseTerm(ImgFeatures& X, ImgLabeling& Y)
	{
		const int numNodes = X.getNumNodes();
		const int numClasses = Global::settings->CLASSES.numClasses();
		const int featureDim = X.getFeatureDim();
		const int pairwiseFeatDim = 2;
		const int numPairs = (numClasses*(numClasses+1))/2;
		
		VectorXd phi = VectorXd::Zero(numPairs*pairwiseFeatDim);

		int numEdges = 0;
		for (int node1 = 0; node1 < numNodes; node1++)
		{
			for (int node2 = node1+1; node2 < numNodes; node2++)
			{
				numEdges++;

				// get node features and label
				VectorXd nodeFeatures1 = X.graph.nodesData.row(node1);
				double nodeLocationX1 = X.getNodeLocationX(node1);
				double nodeLocationY1 = X.getNodeLocationY(node1);
				int nodeLabel1 = Y.getLabel(node1);

				VectorXd nodeFeatures2 = X.graph.nodesData.row(node2);
				double nodeLocationX2 = X.getNodeLocationX(node2);
				double nodeLocationY2 = X.getNodeLocationY(node2);
				int nodeLabel2 = Y.getLabel(node2);

				int classIndex = -1;
				VectorXd edgeFeatureVector = computePairwiseFeatures(nodeFeatures1, nodeFeatures2, 
					nodeLocationX1, nodeLocationY1, nodeLocationX2, nodeLocationY2, 
					nodeLabel1, nodeLabel2, classIndex);
				phi.segment(classIndex*pairwiseFeatDim, pairwiseFeatDim) += edgeFeatureVector; // contrast sensitive pairwise potential
			}
		}

		phi = 1.0/numEdges * phi;

		return phi;
	}

	VectorXd DenseCRFFeatures::computePairwiseFeatures(VectorXd& nodeFeatures1, VectorXd& nodeFeatures2, 
		double nodeLocationX1, double nodeLocationY1, double nodeLocationX2, double nodeLocationY2, 
		int nodeLabel1, int nodeLabel2, int& classIndex)
	{
		const double THETA_ALPHA = 0.025;
		const double THETA_BETA = 0.025;
		const double THETA_GAMMA = 0.025;

		int node1ClassIndex = Global::settings->CLASSES.getClassIndex(nodeLabel1);
		int node2ClassIndex = Global::settings->CLASSES.getClassIndex(nodeLabel2);
		int numClasses = Global::settings->CLASSES.numClasses();

		int i = min(node1ClassIndex, node2ClassIndex);
		int j = max(node1ClassIndex, node2ClassIndex);

		classIndex = (numClasses*(numClasses+1)-(numClasses-i)*(numClasses-i+1))/2+(numClasses-1-j);

		// phi features depend on labels
		if (nodeLabel1 != nodeLabel2)
		{
			VectorXd potential = VectorXd::Zero(2);

			double locationDistance = pow(nodeLocationX1-nodeLocationX2,2)+pow(nodeLocationY1-nodeLocationY2,2);
			VectorXd featureDiff = nodeFeatures1 - nodeFeatures2;
			double featureDistance = featureDiff.squaredNorm();

			double appearanceTerm = exp(-locationDistance/(2*pow(THETA_ALPHA,2)) - featureDistance/(2*pow(THETA_BETA,2)));
			double smoothnessTerm = exp(-locationDistance/(2*pow(THETA_GAMMA,2)));

			potential(0) = appearanceTerm;
			potential(1) = smoothnessTerm;

			return potential;
		}
		else
		{
			VectorXd potential = VectorXd::Zero(2);

			double locationDistance = pow(nodeLocationX1-nodeLocationX2,2)+pow(nodeLocationY1-nodeLocationY2,2);
			VectorXd featureDiff = nodeFeatures1 - nodeFeatures2;
			double featureDistance = featureDiff.squaredNorm();

			double appearanceTerm = 1-exp(-locationDistance/(2*pow(THETA_ALPHA,2)) - featureDistance/(2*pow(THETA_BETA,2)));
			double smoothnessTerm = 1-exp(-locationDistance/(2*pow(THETA_GAMMA,2)));

			potential(0) = appearanceTerm;
			potential(1) = smoothnessTerm;

			return potential;
		}
	}
}