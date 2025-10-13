//
// Kd-Tree implementation.
//
// Copyright: Christoph Dalitz, 2018-2023
//            Jens Wilberg, 2018
// Version:   1.3
// License:   BSD style license
//            (see the file LICENSE for details)
//

#include "KdTree.hpp"
#include <math.h>
#include <algorithm>
#include <limits>
#include <stdexcept>

#include "CoreGeometry/points.hpp"

namespace Kdtree {

//--------------------------------------------------------------
// function object for comparing only dimension d of two vecotrs
//--------------------------------------------------------------
template<typename T>
class compare_dimension {
 public:
  compare_dimension(size_t dim) { d = dim; }
  bool operator()(const KdNode<T>& p, const KdNode<T>& q) {
    if (d == 0)
      return (p.point.x < q.point.x);
    else
      return (p.point.y < q.point.y);
  }
  size_t d;
};

//--------------------------------------------------------------
// internal node structure used by kdtree
//--------------------------------------------------------------
template<typename T>
class kdtree_node {
 public:
  kdtree_node() {
    dataindex = cutdim = 0;
    loson = hison = (kdtree_node<T>*)NULL;
  }
  ~kdtree_node() {
    if (loson) delete loson;
    if (hison) delete hison;
  }
  // index of node data in kdtree array "allnodes"
  size_t dataindex;
  // cutting dimension
  size_t cutdim;
  // value of point
  // double cutval; // == point[cutdim]
  CoordPoint<T> point;
  //  roots of the two subtrees
  kdtree_node<T> *loson, *hison;
  // bounding rectangle of this node's subtree
  CoordPoint<T> lobound, upbound;
};

//--------------------------------------------------------------
// different distance metrics
//--------------------------------------------------------------
template<typename T>
class DistanceMeasure {
 public:
  DistanceMeasure() {}
  virtual ~DistanceMeasure() {}
  virtual double distance(const CoordPoint<T>& p, const CoordPoint<T>& q) = 0;
  virtual double coordinate_distance(T x, T y, size_t dim) = 0;
};
// Maximum distance (Linfinite norm)
template<typename T>
class DistanceL0 : virtual public DistanceMeasure<T> {
  WeightVector* w;

 public:
  DistanceL0(const WeightVector* weights = NULL) {
    if (weights)
      w = new WeightVector(*weights);
    else
      w = (WeightVector*)NULL;
  }
  ~DistanceL0() {
    if (w) delete w;
  }
  double distance(const CoordPoint<T>& p, const CoordPoint<T>& q) {
    double dist, test;
    if (w) {
      dist = (*w)[0] * fabs(p.x - q.x);
      test = (*w)[1] * fabs(p.y - q.y);
      if (test > dist) dist = test;
    } else {
      dist = fabs(p.x - q.x);
      test = fabs(p.y - q.y);
      if (test > dist) dist = test;
    }
    return dist;
  }
  double coordinate_distance(T x, T y, size_t dim) {
    if (w)
      return (*w)[dim] * fabs(x - y);
    else
      return fabs(x - y);
  }
};
// Manhatten distance (L1 norm)
template<typename T>
class DistanceL1 : virtual public DistanceMeasure<T> {
  WeightVector* w;

 public:
  DistanceL1(const WeightVector* weights = NULL) {
    if (weights)
      w = new WeightVector(*weights);
    else
      w = (WeightVector*)NULL;
  }
  ~DistanceL1() {
    if (w) delete w;
  }
  double distance(const CoordPoint<T>& p, const CoordPoint<T>& q) {
    double dist = 0.0;
    if (w) {
      dist += (*w)[0] * fabs(p.x - q.x);
      dist += (*w)[1] * fabs(p.y - q.y);
    } else {
      dist += fabs(p.x - q.x);
      dist += fabs(p.y - q.y);
    }
    return dist;
  }
  double coordinate_distance(T x, T y, size_t dim) {
    if (w)
      return (*w)[dim] * fabs(x - y);
    else
      return fabs(x - y);
  }
};
// Euklidean distance (L2 norm) (squared)
template<typename T>
class DistanceL2 : virtual public DistanceMeasure<T> {
  WeightVector* w;

 public:
  DistanceL2(const WeightVector* weights = NULL) {
    if (weights)
      w = new WeightVector(*weights);
    else
      w = (WeightVector*)NULL;
  }
  ~DistanceL2() {
    if (w) delete w;
  }
  double distance(const CoordPoint<T>& p, const CoordPoint<T>& q) {
    double dist = 0.0;
    if (w) {
      dist += (*w)[0] * (p.x - q.x) * (p.x - q.x);
      dist += (*w)[1] * (p.y - q.y) * (p.y - q.y);
    } else {
      dist += (p.x - q.x) * (p.x - q.x);
      dist += (p.y - q.y) * (p.y - q.y);
    }
    return dist;
  }
  double coordinate_distance(T x, T y, size_t dim) {
    if (w)
      return (*w)[dim] * (x - y) * (x - y);
    else
      return (x - y) * (x - y);
  }
};

//--------------------------------------------------------------
// destructor and constructor of kdtree
//--------------------------------------------------------------
template<typename T>
KdTree<T>::~KdTree() {
  if (root) delete root;
  delete distance;
}
// distance_type can be 0 (Maximum), 1 (Manhatten), or 2 (Euklidean [squared])
template<typename T>
KdTree<T>::KdTree(const KdNodeVector<T>* nodes, int distance_type /*=2*/) {
  size_t i;
  // copy over input data
  if (!nodes || nodes->empty())
    throw std::invalid_argument(
        "kdtree::KdTree(): argument nodes must not be empty");
  dimension = 2;
  allnodes = *nodes;
  // initialize distance values
  distance = NULL;
  this->distance_type = -1;
  set_distance(distance_type);
  // compute global bounding box
  lobound = nodes->begin()->point;
  upbound = nodes->begin()->point;
  for (i = 1; i < nodes->size(); i++) {
    if (allnodes[i].point.x < lobound.x) lobound.x = allnodes[i].point.x;
    if (allnodes[i].point.y < lobound.y) lobound.y = allnodes[i].point.y;
    if (allnodes[i].point.x > upbound.x) upbound.x = allnodes[i].point.x;
    if (allnodes[i].point.y > upbound.y) upbound.y = allnodes[i].point.y;
  }
  // build tree recursively
  root = build_tree(0, 0, allnodes.size());
}

// distance_type can be 0 (Maximum), 1 (Manhatten), or 2 (Euklidean [squared])
template<typename T>
void KdTree<T>::set_distance(int distance_type,
                          const WeightVector* weights /*=NULL*/) {
  if (distance) delete distance;
  this->distance_type = distance_type;
  if (distance_type == 0) {
    distance = (DistanceMeasure<T>*)new DistanceL0<T>(weights);
  } else if (distance_type == 1) {
    distance = (DistanceMeasure<T>*)new DistanceL1<T>(weights);
  } else {
    distance = (DistanceMeasure<T>*)new DistanceL2<T>(weights);
  }
}

//--------------------------------------------------------------
// recursive build of tree
// "a" and "b"-1 are the lower and upper indices
// from "allnodes" from which the subtree is to be built
//--------------------------------------------------------------
template<typename T>
kdtree_node<T>* KdTree<T>::build_tree(size_t depth, size_t a, size_t b) {
  size_t m;
  T temp, cutval;
  kdtree_node<T>* node = new kdtree_node<T>();
  node->lobound = lobound;
  node->upbound = upbound;
  node->cutdim = depth % dimension;
  if (b - a <= 1) {
    node->dataindex = a;
    node->point = allnodes[a].point;
  } else {
    m = (a + b) / 2;
    std::nth_element(allnodes.begin() + a, allnodes.begin() + m,
                     allnodes.begin() + b, compare_dimension<T>(node->cutdim));
    node->point = allnodes[m].point;
    if (node->cutdim == 0)
        cutval = allnodes[m].point.x;
    else
        cutval = allnodes[m].point.y;
    node->dataindex = m;
    if (m - a > 0) {
      if (node->cutdim == 0) {
        temp = upbound.x;
        upbound.x = cutval;
        node->loson = build_tree(depth + 1, a, m);
        upbound.x = temp;
      } else {
        temp = upbound.y;
        upbound.y = cutval;
        node->loson = build_tree(depth + 1, a, m);
        upbound.y = temp;
      }
    }
    if (b - m > 1) {
      if (node->cutdim == 0) {
        temp = lobound.x;
        lobound.x = cutval;
        node->hison = build_tree(depth + 1, m + 1, b);
        lobound.x = temp;
      } else {
        temp = lobound.y;
        lobound.y = cutval;
        node->hison = build_tree(depth + 1, m + 1, b);
        lobound.y = temp;
      }
    }
  }
  return node;
}

//--------------------------------------------------------------
// k nearest neighbor search
// returns the *k* nearest neighbors of *point* in O(log(n))
// time. The result is returned in *result* and is sorted by
// distance from *point*.
// The optional search predicate is a callable class (aka "functor")
// derived from KdNodePredicate. When Null (default, no search
// predicate is applied).
//--------------------------------------------------------------
template<typename T>
void KdTree<T>::k_nearest_neighbors(const CoordPoint<T>& point, size_t k,
                                 KdNodeVector<T>* result,
                                 KdNodePredicate<T>* pred /*=NULL*/) {
  size_t i;
  KdNode<T> temp;
  searchpredicate = pred;

  result->clear();
  if (k < 1) return;

  // collect result of k values in neighborheap
  SearchQueue* neighborheap = new SearchQueue();
  if (k > allnodes.size()) {
    // when more neighbors asked than nodes in tree, return everything
    k = allnodes.size();
    for (i = 0; i < k; i++) {
      if (!(searchpredicate && !(*searchpredicate)(allnodes[i])))
        neighborheap->push(
            nn4heap(i, distance->distance(allnodes[i].point, point)));
    }
  } else {
    neighbor_search(point, root, k, neighborheap);
  }

  // copy over result sorted by distance
  // (we must revert the vector for ascending order)
  while (!neighborheap->empty()) {
    i = neighborheap->top().dataindex;
    neighborheap->pop();
    result->push_back(allnodes[i]);
  }
  // beware that less than k results might have been returned
  k = result->size();
  for (i = 0; i < k / 2; i++) {
    temp = (*result)[i];
    (*result)[i] = (*result)[k - 1 - i];
    (*result)[k - 1 - i] = temp;
  }
  delete neighborheap;
}

//--------------------------------------------------------------
// range nearest neighbor search
// returns the nearest neighbors of *point* in the given range
// *r*. The result is returned in *result* and is sorted by
// distance from *point*.
//--------------------------------------------------------------
template<typename T>
void KdTree<T>::range_nearest_neighbors(const CoordPoint<T>& point, double r,
                                     KdNodeVector<T>* result) {
  result->clear();
  if (this->distance_type == 2) {
    // if euclidien distance is used the range must be squared because we
    // get squared distances from this implementation
    r *= r;
  }

  // collect result in range_result
  std::vector<size_t> range_result;
  range_search(point, root, r, &range_result);

  // copy over result
  for (std::vector<size_t>::iterator i = range_result.begin();
       i != range_result.end(); ++i) {
    result->push_back(allnodes[*i]);
  }

  // clear vector
  range_result.clear();
}

//--------------------------------------------------------------
// recursive function for nearest neighbor search in subtree
// under *node*. Stores result in *neighborheap*.
// returns "true" when no nearer neighbor elsewhere possible
//--------------------------------------------------------------
template<typename T>
bool KdTree<T>::neighbor_search(const CoordPoint<T>& point, kdtree_node<T>* node,
                             size_t k, SearchQueue* neighborheap) {
  double curdist, dist;

  curdist = distance->distance(point, node->point);
  if (!(searchpredicate && !(*searchpredicate)(allnodes[node->dataindex]))) {
    if (neighborheap->size() < k) {
      neighborheap->push(nn4heap(node->dataindex, curdist));
    } else if (curdist < neighborheap->top().distance) {
      neighborheap->pop();
      neighborheap->push(nn4heap(node->dataindex, curdist));
    }
  }

  T p_dim;
  T n_dim;

  if(node->cutdim == 0) {
    p_dim = point.x;
    n_dim = node->point.x;
  } else {
    p_dim = point.y;
    n_dim = node->point.y;
  }
  // first search on side closer to point
  if (p_dim < n_dim) {
    if (node->loson)
      if (neighbor_search(point, node->loson, k, neighborheap)) return true;
  } else {
    if (node->hison)
      if (neighbor_search(point, node->hison, k, neighborheap)) return true;
  }
  // second search on farther side, if necessary
  if (neighborheap->size() < k) {
    dist = std::numeric_limits<double>::max();
  } else {
    dist = neighborheap->top().distance;
  }
  if (p_dim < n_dim) {
    if (node->hison && bounds_overlap_ball(point, dist, node->hison))
      if (neighbor_search(point, node->hison, k, neighborheap)) return true;
  } else {
    if (node->loson && bounds_overlap_ball(point, dist, node->loson))
      if (neighbor_search(point, node->loson, k, neighborheap)) return true;
  }

  if (neighborheap->size() == k) dist = neighborheap->top().distance;
  return ball_within_bounds(point, dist, node);
}

//--------------------------------------------------------------
// recursive function for range search in subtree under *node*.
// Stores result in *range_result*.
//--------------------------------------------------------------
template<typename T>
void KdTree<T>::range_search(const CoordPoint<T>& point, kdtree_node<T>* node,
                          double r, std::vector<size_t>* range_result) {
  double curdist = distance->distance(point, node->point);
  if (curdist <= r) {
    range_result->push_back(node->dataindex);
  }
  if (node->loson != NULL && this->bounds_overlap_ball(point, r, node->loson)) {
    range_search(point, node->loson, r, range_result);
  }
  if (node->hison != NULL && this->bounds_overlap_ball(point, r, node->hison)) {
    range_search(point, node->hison, r, range_result);
  }
}

// returns true when the bounds of *node* overlap with the
// ball with radius *dist* around *point*
template<typename T>
bool KdTree<T>::bounds_overlap_ball(const CoordPoint<T>& point, double dist,
                                 kdtree_node<T>* node) {
  if (distance_type != 0) {
    double distsum = 0.0;
    if (point.x < node->lobound.x) {  // lower than low boundary
      distsum += distance->coordinate_distance(point.x, node->lobound.x, 0);
      if (distsum > dist) return false;
    } else if (point.x > node->upbound.x) {  // higher than high boundary
      distsum += distance->coordinate_distance(point.x, node->upbound.x, 0);
      if (distsum > dist) return false;
    }
    if (point.y < node->lobound.y) {  // lower than low boundary
      distsum += distance->coordinate_distance(point.y, node->lobound.y, 1);
      if (distsum > dist) return false;
    } else if (point.y > node->upbound.y) {  // higher than high boundary
      distsum += distance->coordinate_distance(point.y, node->upbound.y, 1);
      if (distsum > dist) return false;
    }
    return true;
  } else { // maximum distance needs different treatment
    double max_dist = 0.0;
    double curr_dist = 0.0;
    if (point.x < node->lobound.x) {  // lower than low boundary
      curr_dist = distance->coordinate_distance(point.x, node->lobound.x, 0);
    } else if (point.x > node->upbound.x) {  // higher than high boundary
      curr_dist = distance->coordinate_distance(point.x, node->upbound.x, 0);
    }
    if(curr_dist > max_dist) {
      max_dist = curr_dist;
    }
    if (max_dist > dist) return false;
    if (point.y < node->lobound.y) {  // lower than low boundary
      curr_dist = distance->coordinate_distance(point.y, node->lobound.y, 1);
    } else if (point.y > node->upbound.y) {  // higher than high boundary
      curr_dist = distance->coordinate_distance(point.y, node->upbound.y, 1);
    }
    if(curr_dist > max_dist) {
        max_dist = curr_dist;
    }
    if (max_dist > dist) return false;

    return true;
  }
}

// returns true when the bounds of *node* completely contain the
// ball with radius *dist* around *point*
template<typename T>
bool KdTree<T>::ball_within_bounds(const CoordPoint<T>& point, double dist,
                                kdtree_node<T>* node) {

  if (distance->coordinate_distance(point.x, node->lobound.x, 0) <= dist ||
      distance->coordinate_distance(point.x, node->upbound.x, 0) <= dist ||
      distance->coordinate_distance(point.y, node->lobound.y, 1) <= dist ||
      distance->coordinate_distance(point.y, node->upbound.y, 1) <= dist)
    return false;
  return true;
}

}  // namespace Kdtree

template class Kdtree::KdTree<float>;
template class Kdtree::KdTree<uint32_t>;
