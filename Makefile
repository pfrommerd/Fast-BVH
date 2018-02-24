simple-target:
	g++ -g test.cpp BBox.cpp BVH.cpp ABVH.cpp -O3 -msse3 -o test
