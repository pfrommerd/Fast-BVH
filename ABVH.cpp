#include <algorithm>
#include <iostream>

#include "ABVH.h"
#include "Log.h"
#include "Stopwatch.h"

//! Node for storing state information during traversal.
struct ABVHTraversal {
  uint32_t i; // Node
  float mint; // Minimum hit time for this node.
  ABVHTraversal() { }
  ABVHTraversal(int _i, float _mint) : i(_i), mint(_mint) { }
};

//! - Compute the nearest intersection of all objects within the tree.
//! - Return true if hit was found, false otherwise.
//! - In the case where we want to find out of there is _ANY_ intersection at all,
//!   set occlusion == true, in which case we exit on the first hit, rather
//!   than find the closest.
bool ABVH::getIntersection(const Ray& ray, IntersectionInfo* intersection, bool occlusion) const {
  intersection->t = 999999999.f;
  intersection->object = NULL;
  int32_t closer, other;

  // Working set
  ABVHTraversal todo[64];
  int32_t stackptr = 0;

  // "Push" on the root node to the working set
  todo[stackptr].i = 0;
  todo[stackptr].mint = -9999999.f;

  while(stackptr>=0) {
    // Pop off the next node to work on.
    int ni = todo[stackptr].i;
    float near = todo[stackptr].mint;
    stackptr--;
    ABVHFlatNode &node(flatTree[ ni ]);

    // If this node is further than the closest found intersection, continue
    if(near > intersection->t && near > intersection->ft)
      continue;

    // Is leaf -> Intersect
    if( node.rightOffset == 0 ) {
      for(uint32_t o=0;o<node.nPrims;++o) {
        IntersectionInfo current;
        current.ft = node.tfar;

        const Object* obj = (*build_prims)[node.start+o];
        bool hit = obj->getIntersection(ray, &current);

        if (hit) {
          // If we're only looking for occlusion, then any hit is good enough
          if(occlusion) {
            return true;
          }

          // Otherwise, keep the closest intersection only
          if (current.t < intersection->t) {
            *intersection = current;
          }
        }
      }

    } else { // Not a leaf
      ABVHFlatNode *left = &flatTree[ni + 1];
      ABVHFlatNode *right = &flatTree[ni + node.rightOffset];

      bool ur = right->rightOffset == 0 || !right->hit;
      bool ul = left->rightOffset == 0 || !left->hit;
      bool lp = left->hit;
      bool rp = right->hit;
      if (ul)
          left->hit = left->bbox.intersect(ray, &(left->tnear), &(left->tfar));
      if (ur)
          right->hit = right->bbox.intersect(ray, &(right->tnear), &(right->tfar));
      ul = !ul  && (ur && right->hit != rp);
      ur = !ur && (ul && left->hit != lp);
      if (ul)
          left->hit = left->bbox.intersect(ray, &(left->tnear), &(left->tfar));
      if (ur)
          right->hit = right->bbox.intersect(ray, &(right->tnear), &(right->tfar));

      // If both have not intersected, we need
      // to reset the parent's hit field so that
      // it is re-checked
      if(!left->hit && !right->hit) node.hit = false;
      else if(left->hit && right->hit) {
        // Did we hit both nodes?
        // We assume that the left child is a closer hit...
        closer = ni+1;
        other = ni+node.rightOffset;

        // ... If the right child was actually closer, swap the relevant values.
        if(right->tnear < left->tnear) {
          std::swap(left, right);
          std::swap(closer, other);
        }

        // It's possible that the nearest object is still in the other side, but we'll
        // check the further-away node later...
        // Push the farther first
        todo[++stackptr] = ABVHTraversal(other, right->tnear);

        // And now the closer (with overlap test)
        todo[++stackptr] = ABVHTraversal(closer, left->tnear);
      } else if (left->hit) {
        todo[++stackptr] = ABVHTraversal(ni+1, left->tnear);
      } else if(right->hit) {
        todo[++stackptr] = ABVHTraversal(ni+node.rightOffset, right->tnear);
      }
    }
  }

  // If we hit something,
  if(intersection->object != NULL)
    intersection->hit = ray.o + ray.d * intersection->t;

  return intersection->object != NULL;
}

ABVH::~ABVH() {
  delete[] flatTree;
}

ABVH::ABVH(std::vector<Object*>* objects, uint32_t leafSize)
  : nNodes(0), nLeafs(0), leafSize(leafSize), build_prims(objects), flatTree(NULL) {
    Stopwatch sw;

    // Build the tree based on the input object data set.
    build();

    // Output tree build time and statistics
    double constructionTime = sw.read();
    LOG_STAT("Built BVH (%d nodes, with %d leafs) in %d ms", nNodes, nLeafs, (int)(1000*constructionTime));
  }

struct ABVHBuildEntry {
  // If non-zero then this is the index of the parent. (used in offsets)
  uint32_t parent;
  // The range of objects in the object list covered by this node.
  uint32_t start, end;
};

/*! Build the BVH, given an input data set
 *  - Handling our own stack is quite a bit faster than the recursive style.
 *  - Each build stack entry's parent field eventually stores the offset
 *    to the parent of that node. Before that is finally computed, it will
 *    equal exactly three other values. (These are the magic values Untouched,
 *    Untouched-1, and TouchedTwice).
 *  - The partition here was also slightly faster than std::partition.
 */
void ABVH::build() {
  ABVHBuildEntry todo[128];
  uint32_t stackptr = 0;
  const uint32_t Untouched    = 0xffffffff;
  const uint32_t TouchedTwice = 0xfffffffd;

  // Push the root
  todo[stackptr].start = 0;
  todo[stackptr].end = build_prims->size();
  todo[stackptr].parent = 0xfffffffc;
  stackptr++;

  ABVHFlatNode node;
  std::vector<ABVHFlatNode> buildnodes;
  buildnodes.reserve(build_prims->size()*2);

  while(stackptr > 0) {
    // Pop the next item off of the stack
    ABVHBuildEntry &bnode( todo[--stackptr] );
    uint32_t start = bnode.start;
    uint32_t end = bnode.end;
    uint32_t nPrims = end - start;

    nNodes++;
    node.start = start;
    node.nPrims = nPrims;
    node.rightOffset = Untouched;

    // Calculate the bounding box for this node
    BBox bb( (*build_prims)[start]->getBBox());
    BBox bc( (*build_prims)[start]->getCentroid());
    for(uint32_t p = start+1; p < end; ++p) {
      bb.expandToInclude( (*build_prims)[p]->getBBox());
      bc.expandToInclude( (*build_prims)[p]->getCentroid());
    }
    node.bbox = bb;

    // If the number of primitives at this point is less than the leaf
    // size, then this will become a leaf. (Signified by rightOffset == 0)
    if(nPrims <= leafSize) {
      node.rightOffset = 0;
      nLeafs++;
    }

    buildnodes.push_back(node);

    // Child touches parent...
    // Special case: Don't do this for the root.
    if(bnode.parent != 0xfffffffc) {
      buildnodes[bnode.parent].rightOffset --;

      // When this is the second touch, this is the right child.
      // The right child sets up the offset for the flat tree.
      if( buildnodes[bnode.parent].rightOffset == TouchedTwice ) {
        buildnodes[bnode.parent].rightOffset = nNodes - 1 - bnode.parent;
      }
    }

    // If this is a leaf, no need to subdivide.
    if(node.rightOffset == 0)
      continue;

    // Set the split dimensions
    uint32_t split_dim = bc.maxDimension();

    // Split on the center of the longest axis
    float split_coord = .5f * (bc.min[split_dim] + bc.max[split_dim]);

    // Partition the list of objects on this split
    uint32_t mid = start;
    for(uint32_t i=start;i<end;++i) {
      if( (*build_prims)[i]->getCentroid()[split_dim] < split_coord ) {
        std::swap( (*build_prims)[i], (*build_prims)[mid] );
        ++mid;
      }
    }

    // If we get a bad split, just choose the center...
    if(mid == start || mid == end) {
      mid = start + (end-start)/2;
    }

    // Push right child
    todo[stackptr].start = mid;
    todo[stackptr].end = end;
    todo[stackptr].parent = nNodes-1;
    stackptr++;

    // Push left child
    todo[stackptr].start = start;
    todo[stackptr].end = mid;
    todo[stackptr].parent = nNodes-1;
    stackptr++;
  }

  // Copy the temp node data to a flat array
  flatTree = new ABVHFlatNode[nNodes];
  for(uint32_t n=0; n<nNodes; ++n) 
    flatTree[n] = buildnodes[n];
}

