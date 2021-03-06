#ifndef ABVH_h
#define ABVH_h

#include "BBox.h"
#include <vector>
#include <stdint.h>
#include "Object.h"
#include "IntersectionInfo.h"
#include "Ray.h"

//! Node descriptor for the flattened tree
struct ABVHFlatNode {
  BBox bbox;
  uint32_t start, nPrims, rightOffset;
  // Last intersect info
  bool hit;
  float tnear;
  float tfar;
};

//! \author Brandon Pelfrey
//! A Bounding Volume Hierarchy system for fast Ray-Object intersection tests
class ABVH {
  uint32_t nNodes, nLeafs, leafSize;
  std::vector<Object*>* build_prims;

  //! Build the BVH tree out of build_prims
  void build();

  // Fast Traversal System
  ABVHFlatNode *flatTree;

  public:
  ABVH(std::vector<Object*>* objects, uint32_t leafSize=4);
  bool getIntersection(const Ray& ray, IntersectionInfo *intersection, bool occlusion) const ;

  ~ABVH();
};

#endif
