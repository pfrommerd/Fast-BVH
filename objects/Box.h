#ifndef Box_h_
#define Box_h_

#include "../Object.h"
#include "../Vector3.h"

struct Box : public Object {
    BBox bound;
    Box(const Vector3 &min, const Vector3 &max) : bound(min, max) { }
    Box(BBox b) : bound(b) {}

    bool getIntersection(const Ray& ray, IntersectionInfo* I) const {
        float tnear;
        float tfar;
        bool inter = bound.intersect(ray, &tnear, &tfar);
        if (!inter) return false;
        I->object = this;
        I->t = tnear;
        return true;
    }

    Vector3 getNormal(const IntersectionInfo & I) const {
        Vector3 bottomDiff = I.hit - bound.min;
        Vector3 topDiff = I.hit - bound.max;
        // Find the near-zero dimensions
#define EPSILON 0.000001
        if (abs(bottomDiff.x) < EPSILON) return Vector3(-1, 0, 0);
        else if (abs(bottomDiff.y) < EPSILON) return Vector3(0, -1, 0);
        else if (abs(bottomDiff.z) < EPSILON) return Vector3(0, 0, -1);
        else if (abs(topDiff.x) < EPSILON) return Vector3(1, 0, 0);
        else if (abs(topDiff.y) < EPSILON) return Vector3(0, 1, 0);
        else if (abs(topDiff.z) < EPSILON) return Vector3(0, 0, 1);
        else return Vector3(1, 1, 1);
        
    }

    BBox getBBox() const {
        return bound;
    }

    Vector3 getCentroid() const {
        return bound.min + 0.5 * bound.extent;
    }
};

#endif
