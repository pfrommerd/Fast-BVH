#ifndef IntersectionInfo_h_
#define IntersectionInfo_h_

class Object;

struct IntersectionInfo {
  float t; // Intersection distance along the ray
  float ft; // Furthest possible intersection in this parent node
  const Object* object; // Object that was hit
  Vector3 hit; // Location of the intersection
};

#endif
