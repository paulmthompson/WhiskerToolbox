#ifndef __kdtree_HPP
#define __kdtree_HPP

//
// Kd-Tree implementation.
//
// Copyright: Christoph Dalitz, 2018-2023
//            Jens Wilberg, 2018
// Version:   1.3
// License:   BSD style license
//            (see the file LICENSE for details)
//

#include <cstdlib>
#include <queue>
#include <vector>

#include "CoreGeometry/points.hpp"

namespace Kdtree {

template<typename T>
using CoordPoint = Point2D<T>;
typedef std::vector<double> WeightVector;

// for passing points to the constructor of kdtree
template<typename T>
struct KdNode {
  CoordPoint<T> point;
  void* data;
  int index;
  KdNode(const CoordPoint<T>& p, void* d = NULL, int i = -1) {
    point = p;
    data = d;
    index = i;
  }
  KdNode() { data = NULL; }
};
template<typename T>
using KdNodeVector = std::vector<KdNode<T>>;

// base function object for search predicate in knn search
// returns true when the given KdNode is an admissible neighbor
// To define an own search predicate, derive from this class
// and overwrite the call operator operator()
template<typename T>
struct KdNodePredicate {
  virtual ~KdNodePredicate() {}
  virtual bool operator()(const KdNode<T>&) const { return true; }
};

//--------------------------------------------------------
// private helper classes used internally by KdTree
//
// the internal node structure used by kdtree
template<typename T>
class kdtree_node;
// base class for different distance computations
template<typename T>
class DistanceMeasure;
// helper class for priority queue in k nearest neighbor search
class nn4heap {
 public:
  size_t dataindex;  // index of actual kdnode in *allnodes*
  double distance;   // distance of this neighbor from *point*
  nn4heap(size_t i, double d) {
    dataindex = i;
    distance = d;
  }
};
class compare_nn4heap {
 public:
  bool operator()(const nn4heap& n, const nn4heap& m) {
    return (n.distance < m.distance);
  }
};
  typedef std::priority_queue<nn4heap, std::vector<nn4heap>, compare_nn4heap> SearchQueue;
//--------------------------------------------------------

// kdtree class
template<typename T>
class KdTree {
 private:
  // recursive build of tree
  kdtree_node<T>* build_tree(size_t depth, size_t a, size_t b);
  // helper variable for keeping track of subtree bounding box
  CoordPoint<T> lobound, upbound;
  // helper variable to check the distance method
  int distance_type;
  bool neighbor_search(const CoordPoint<T>& point, kdtree_node<T>* node, size_t k, SearchQueue* neighborheap);
  void range_search(const CoordPoint<T>& point, kdtree_node<T>* node, double r, std::vector<size_t>* range_result);
  bool bounds_overlap_ball(const CoordPoint<T>& point, double dist,
                           kdtree_node<T>* node);
  bool ball_within_bounds(const CoordPoint<T>& point, double dist,
                          kdtree_node<T>* node);
  // class implementing the distance computation
  DistanceMeasure<T>* distance;
  // search predicate in knn searches
  KdNodePredicate<T>* searchpredicate;

 public:
  KdNodeVector<T> allnodes;
  size_t dimension;
  kdtree_node<T>* root;
  // distance_type can be 0 (max), 1 (city block), or 2 (euklid [squared])
  KdTree(const KdNodeVector<T>* nodes, int distance_type = 2);
  ~KdTree();
  void set_distance(int distance_type, const WeightVector* weights = NULL);
  void k_nearest_neighbors(const CoordPoint<T>& point, size_t k,
                           KdNodeVector<T>* result, KdNodePredicate<T>* pred = NULL);
  void range_nearest_neighbors(const CoordPoint<T>& point, double r,
                               KdNodeVector<T>* result);
};

}  // end namespace Kdtree

#endif
