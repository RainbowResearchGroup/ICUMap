/*
 *
 * Copyright (c) 2014, Laurens van der Maaten (Delft University of Technology)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by the Delft University of Technology.
 * 4. Neither the name of the Delft University of Technology nor the names of 
 *    its contributors may be used to endorse or promote products derived from 
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY LAURENS VAN DER MAATEN ''AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO 
 * EVENT SHALL LAURENS VAN DER MAATEN BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 */


/* This code was adopted with minor modifications from Steve Hanov's great tutorial at http://stevehanov.ca/blog/index.php?id=130 */

#include <stdlib.h>
#include <algorithm>
#include <vector>
#include <stdio.h>
#include <queue>
#include <limits>
#include <cmath>
#include <set>

#ifndef VPTREE_H
#define VPTREE_H

class DistanceParams {
public:
    double lengthWeight;
    double deathWeight;
    double cardinalWeight;
    bool* cardinals;

    DistanceParams(double lengthWeight, double deathWeight, double cardinalWeight, bool* cardinals) {
        this->lengthWeight = lengthWeight;
        this->deathWeight = deathWeight;
        this->cardinalWeight = cardinalWeight;
        this->cardinals = cardinals;
    }
};

class DataPoint
{
    int _ind;

public:
    double* _x;
    int _D;
    int _id;
    int _next;
    DistanceParams* distanceMetaData;
    DataPoint() {
        _D = 1;
        _ind = -1;
        _x = NULL;
        _id = NULL;
        _next = NULL;
    }
    DataPoint(int D, int ind, double* x, DistanceParams* meta) {
        _D = D - 2;
        _ind = ind;
        _id = (int) x[_D];
        _next = (int) x[_D + 1];
        distanceMetaData = meta;
        _x = (double*) malloc(_D * sizeof(double));
        for(int d = 0; d < _D; d++) _x[d] = x[d];
    }
    DataPoint(const DataPoint& other) {                     // this makes a deep copy -- should not free anything
        if(this != &other) {
            _D = other.dimensionality();
            _ind = other.index();
            _id = other.id();
            _next = other.next();
            distanceMetaData = other.distanceMetaData;
            _x = (double*) malloc(_D * sizeof(double));
            for(int d = 0; d < _D; d++) _x[d] = other.x(d);
        }
    }
    ~DataPoint() { if(_x != NULL) free(_x); }
    DataPoint& operator= (const DataPoint& other) {         // asignment should free old object
        if(this != &other) {
            if(_x != NULL) free(_x);
            _D = other.dimensionality();
            _ind = other.index();
            _id = other.id();
            _next = other.next();
            distanceMetaData = other.distanceMetaData;
            _x = (double*) malloc(_D * sizeof(double));
            for(int d = 0; d < _D; d++) _x[d] = other.x(d);
        }
        return *this;
    }
    int index() const { return _ind; }
    int dimensionality() const { return _D; }
    int id() const { return _id; }
    int next() const { return _next; }
    double x(int d) const { return _x[d]; }
};

double euclidean_distance(const DataPoint &t1, const DataPoint &t2) {
    double dd = .0;
    double* x1 = t1._x;
    double* x2 = t2._x;
    double diff;
    for(int d = 0; d < t1._D; d++) {
        if (t1.distanceMetaData->cardinals[d]) {
            if (x1[d] != x2[d]) dd += t1.distanceMetaData->cardinalWeight;
        } else {
            diff = (x1[d] - x2[d]);
            dd += diff * diff;
        }
    }
    return sqrt(dd);
}

double euclidean_trajectory_distance(const DataPoint &t1, const DataPoint &t2) {
    double distance = euclidean_distance(t1, t2);

    if (t1.next() != NULL && t1.next() == t2.id()) {
        return t1.distanceMetaData->lengthWeight; // * distance;
    } else {
        return distance;
    }
}

inline double min(double a, double b) {
    return a < b ? a : b;
}

//template<double (*distance)( const DataPoint&, const DataPoint& )>
class ShortenedGraph {
    static double** distances;
    static int count;
    static int N;
public:
    ShortenedGraph(std::vector<DataPoint> dataPoints, int N, bool deathAsADistinctLocation) {
        ShortenedGraph::N = N;
        distances = new double*[N];
        for (int i = 0; i < N; i++) {
            distances[i] = new double[N];
        }

        double totalDistance = 0;
        int comparisons = 0;

        for (int i = 0; i < N; i++) {
            if (i % 500 == 0) {
                printf("Computing distances: %i of %i\n", i, N);
            }
            for (int j = 0; j < N; j++) {
                if (i == j) {
                    distances[i][j] = 0;
                } else if (deathAsADistinctLocation && (i == N - 1 || j == N - 1)) {
                    // (N - 1) is the last row/column in the distance matrix. This is where
                    // "death" is located. Assign a fixed distance to this point from nodes which
                    // have mortality marked as true and their next trajectory pointer doesn't
                    // point to a node i.e. from nodes which are the last state recorded for a
                    // dead patient. All other distances can be set to infinite; the graph search
                    // step will take care of the rest.
                    if (
                            (i == N - 1 && dataPoints[j]._x[15] == 1 && dataPoints[j].next() == 0) ||
                            (j == N - 1 && dataPoints[i]._x[15] == 1 && dataPoints[i].next() == 0)
                        ) {
                        distances[i][j] = 1.0;
                    } else {
                        distances[i][j] = DBL_MAX;
                    }
                } else {
                    double a = euclidean_trajectory_distance(dataPoints[i], dataPoints[j]);
                    double b = euclidean_trajectory_distance(dataPoints[j], dataPoints[i]);

                    distances[i][j] = a < b ? a : b;
                    // TODO: Average instead of min? I think averaging === min with float *= 2, so doesn't matter

                    totalDistance += distances[i][j];
                    comparisons += 1;
                }
            }

        }

        printf("\nTotal Distance: %f\n", totalDistance);
        printf("Comparisons: %i\n", comparisons);
        printf("Average distance: %f\n\n", totalDistance / ((float) comparisons));

        for (int k = 0; k < N; k++) {
            if (k % 10 == 0) {
                printf("Shortest path hunting: %i of %i\n", k, N);
            }
            for (int i = 0; i < N; i++) {
                for (int j = 0; j < N; j++) {
                    if (distances[i][j] > distances[i][k] + distances[k][j]) {
                        distances[i][j] = distances[i][k] + distances[k][j];
                    }
                }
            }
        }

        printf("Shortest path hunting: %i of %i\n", N, N);
    }

    static double euclidean_shortest_trajectory_distance(const DataPoint &t1, const DataPoint &t2) {
        if (count++ % 100000 == 0) {
            printf("Count: %i\n", count);
        }
        return distances[t1._id - 1][t2._id - 1];
    }

    static double shortened_distance(const DataPoint &t1, const DataPoint &t2) {
        return distances[t1._id - 1][t2._id - 1];
    }

    static double euclidean_shortest_trajectory_distance_dynamic(const DataPoint &t1, const DataPoint &t2) {
        printf("Compute\n");
        int source = t1._id - 1;
        int target = t2._id - 1;

        // Create queue of nodes to visit next.
        // Initialise with all nodes adjacent to this one
        std::set<std::pair<double, int>> queue;
        for (int j = 0; j < N; j++) {
            queue.insert(std::make_pair(distances[source][j], j));
        }

        while (true) {
            double d = (*queue.begin()).first;
            int y = (*queue.begin()).second;

//            printf("%f %i\n", d, y);

            queue.erase(*queue.begin());

            if (distances[source][y] < d) {
                // There's already a shorter path to this node i.e. we've visited this before
                // (either in this run of the function or a previous). Therefore no need to keep searching here.
                continue;
            }

            distances[source][y] = d;

            if (y == target) {
                printf("Done\n");
                return d; // = distances[source][y] = distances[source][target]
            }

            for (int j = 0; j < N; j++) {
                if (j != y) {
                    queue.insert(std::make_pair(distances[source][y] + distances[y][j], j));
                }
            }
        }

    }
};

double** ShortenedGraph::distances = NULL;
int ShortenedGraph::count = 0;
int ShortenedGraph::N = 0;

double distance_select(const DataPoint &t1, const DataPoint &t2) {
    if (t1.distanceMetaData->lengthWeight == 1.0) {
        return euclidean_distance(t1, t2);
    } else {
        return ShortenedGraph::shortened_distance(t1, t2);
    }
}

template<typename T, double (*distance)( const T&, const T& )>
class VpTree
{
public:
    
    // Default constructor
    VpTree() : _root(0) {}
    
    // Destructor
    ~VpTree() {
        delete _root;
    }

    // Function to create a new VpTree from data
    void create(const std::vector<T>& items) {
        delete _root;
        _items = items;
        _root = buildFromPoints(0, items.size());
    }
    
    // Function that uses the tree to find the k nearest neighbors of target
    void search(const T& target, int k, std::vector<T>* results, std::vector<double>* distances)
    {
        
        // Use a priority queue to store intermediate results on
        std::priority_queue<HeapItem> heap;
        
        // Variable that tracks the distance to the farthest point in our results
        _tau = DBL_MAX;
        
        // Perform the search
        search(_root, target, k, heap);
        
        // Gather final results
        results->clear(); distances->clear();
        while(!heap.empty()) {
            results->push_back(_items[heap.top().index]);
            distances->push_back(heap.top().dist);
            heap.pop();
        }
        
        // Results are in reverse order
        std::reverse(results->begin(), results->end());
        std::reverse(distances->begin(), distances->end());
    }
    
private:
    std::vector<T> _items;
    double _tau;
    
    // Single node of a VP tree (has a point and radius; left children are closer to point than the radius)
    struct Node
    {
        int index;              // index of point in node
        double threshold;       // radius(?)
        Node* left;             // points closer by than threshold
        Node* right;            // points farther away than threshold
        
        Node() :
        index(0), threshold(0.), left(0), right(0) {}
        
        ~Node() {               // destructor
            delete left;
            delete right;
        }
    }* _root;
    
    
    // An item on the intermediate result queue
    struct HeapItem {
        HeapItem( int index, double dist) :
        index(index), dist(dist) {}
        int index;
        double dist;
        bool operator<(const HeapItem& o) const {
            return dist < o.dist;
        }
    };
    
    // Distance comparator for use in std::nth_element
    struct DistanceComparator
    {
        const T& item;
        DistanceComparator(const T& item) : item(item) {}
        bool operator()(const T& a, const T& b) {
            return distance(item, a) < distance(item, b);
        }
    };
    
    // Function that (recursively) fills the tree
    Node* buildFromPoints( int lower, int upper )
    {
        if (upper == lower) {     // indicates that we're done here!
            return NULL;
        }
        
        // Lower index is center of current node
        Node* node = new Node();
        node->index = lower;
        
        if (upper - lower > 1) {      // if we did not arrive at leaf yet
            
            // Choose an arbitrary point and move it to the start
            int i = (int) ((double)rand() / RAND_MAX * (upper - lower - 1)) + lower;
            std::swap(_items[lower], _items[i]);
            
            // Partition around the median distance
            int median = (upper + lower) / 2;
            std::nth_element(_items.begin() + lower + 1,
                             _items.begin() + median,
                             _items.begin() + upper,
                             DistanceComparator(_items[lower]));
            
            // Threshold of the new node will be the distance to the median
            node->threshold = distance(_items[lower], _items[median]);
            
            // Recursively build tree
            node->index = lower;
            node->left = buildFromPoints(lower + 1, median);
            node->right = buildFromPoints(median, upper);
        }
        
        // Return result
        return node;
    }
    
    // Helper function that searches the tree    
    void search(Node* node, const T& target, int k, std::priority_queue<HeapItem>& heap)
    {
        if(node == NULL) return;     // indicates that we're done here
        
        // Compute distance between target and current node
        double dist = distance(_items[node->index], target);

        // If current node within radius tau
        if(dist < _tau) {
            if(heap.size() == k) heap.pop();                 // remove furthest node from result list (if we already have k results)
            heap.push(HeapItem(node->index, dist));           // add current node to result list
            if(heap.size() == k) _tau = heap.top().dist;     // update value of tau (farthest point in result list)
        }
        
        // Return if we arrived at a leaf
        if(node->left == NULL && node->right == NULL) {
            return;
        }
        
        // If the target lies within the radius of ball
        if(dist < node->threshold) {
            if(dist - _tau <= node->threshold) {         // if there can still be neighbors inside the ball, recursively search left child first
                search(node->left, target, k, heap);
            }
            
            if(dist + _tau >= node->threshold) {         // if there can still be neighbors outside the ball, recursively search right child
                search(node->right, target, k, heap);
            }
        
        // If the target lies outsize the radius of the ball
        } else {
            if(dist + _tau >= node->threshold) {         // if there can still be neighbors outside the ball, recursively search right child first
                search(node->right, target, k, heap);
            }
            
            if (dist - _tau <= node->threshold) {         // if there can still be neighbors inside the ball, recursively search left child
                search(node->left, target, k, heap);
            }
        }
    }
};
            
#endif