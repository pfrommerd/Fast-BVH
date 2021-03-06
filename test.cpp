#include <cstdio>
#include <vector>
#include <cstdlib>
#include <string>
#include "BVH.h"
#include "ABVH.h"
#include "objects/Sphere.h"
#include "objects/Box.h"
#include "Stopwatch.h"

// Return a random number in [0,1]
float rand01() {
  return rand() * (1.f / RAND_MAX);
}

// Return a random vector with each component in the range [-1,1]
Vector3 randVector3() {
  return Vector3(rand01(), rand01(), rand01())*2.f - Vector3(1,1,1);
}

template<typename H>
void render_image(std::vector<Object *> &objects, int width, int height, float *pixels,
                    Vector3 camera_position, Vector3 camera_dir, Vector3 camera_u, Vector3 camera_v) {
  // Compute a BVH for this object set
  H bvh(&objects);
  Stopwatch timer;
  for(size_t i=0; i<width; ++i) {
    for(size_t j=0; j<height; ++j) {
      size_t index = 3*(width * j + i);

      float u = (i+.5f) / (float)(width-1) - .5f;
      float v = (height-1-j+.5f) / (float)(height-1) - .5f;
      float fov = .5f / tanf( 70.f * 3.14159265*.5f / 180.f);

      // This is only valid for square aspect ratio images
      Ray ray(camera_position, normalize(u*camera_u + v*camera_v + fov*camera_dir));

      IntersectionInfo I;
      bool hit = bvh.getIntersection(ray, &I, false);

      if(!hit) {
        pixels[index] = pixels[index+1] = pixels[index+2] = 0.f;
      } else {

        // Just for fun, we'll make the color based on the normal
        const Vector3 normal = I.object->getNormal(I);
        const Vector3 color(fabs(normal.x), fabs(normal.y), fabs(normal.z));

        pixels[index  ] = color.x;
        pixels[index+1] = color.y;
        pixels[index+2] = color.z;
      }
    }
  }
  printf("Render time: %f\n", timer.read());
}

void write_image(int width, int height, float *pixels, std::string fileName) {
  printf("Writing out image file: \"%s\"\n", fileName.c_str());
  FILE *image = fopen(fileName.c_str(), "w");
  fprintf(image, "P6\n%d %d\n255\n", width, height);
  for(size_t j=0; j<height; ++j) {
    for(size_t i=0; i<width; ++i) {
      size_t index = 3*(width * j + i);
      unsigned char r = std::max(std::min(pixels[index  ]*255.f, 255.f), 0.f);
      unsigned char g = std::max(std::min(pixels[index+1]*255.f, 255.f), 0.f);
      unsigned char b = std::max(std::min(pixels[index+2]*255.f, 255.f), 0.f);
      fprintf(image, "%c%c%c", r,g,b);
    }
  }
  fclose(image);
}

void create_random_spheres(std::vector<Object*> *objects) {
    const unsigned int N = 1000000;
    printf("Constructing %d random spheres...\n", N);
    for(size_t i=0; i < N; ++i) {
        objects->push_back(new Sphere(randVector3(), .01f));
    }
}

void create_wall_spheres(std::vector<Object*> *objects) {
    const unsigned int w = 10, h = 10;
    const unsigned int sd = w < h ? w : h;
    for (size_t x = 0; x < w; x++) {
        for (size_t y = 0; y < w; y++) {
            objects->push_back(new Sphere(
                        Vector3(1.0/w * x, 1.0/h * y, 0), 1.0/sd));
        }
    }
}
void create_wall_boxes(std::vector<Object*> *objects) {
    objects->push_back(new Box(Vector3(0, 0, 0),
                               Vector3(0.5, 0.5, 0.5)));
}
void create_bboxes(std::vector<Object*> *objects, const BVH &t, int level) {
//  objects->push_back(new Box(t.flatTree[1].bbox));
  objects->push_back(new Box(t.flatTree[t.flatTree[0].rightOffset].bbox));
}

int main(int argc, char **argv) {
  // If we want different results
  // each time
  srand(time(NULL));

  // Create a million spheres packed in the space of a cube
  std::vector<Object*> objects;
  //create_random_spheres(&objects);
  //create_wall_spheres(&objects);
  create_wall_spheres(&objects);
  /*
  BVH t(&objects);
  // Extract the bounding boxes
  objects.clear();
  create_bboxes(&objects, t, 1);
  //*/

  // Create a camera from position and focus point
  Vector3 camera_position(1.6, 1.3, 1.6);
  Vector3 camera_focus(0,0,0);
  Vector3 camera_up(0,1,0);

  // Camera tangent space
  Vector3 camera_dir = normalize(camera_focus - camera_position);
  Vector3 camera_u = normalize(camera_dir ^ camera_up);
  Vector3 camera_v = normalize(camera_u ^ camera_dir);
  
  // Allocate space for some image pixels
  const unsigned int width=512, height=512;
  float* regular_pixels = new float[width*height*3];
  float* new_pixels = new float[width*height*3];

  printf("\n\n");
  printf("Rendering image new (%dx%d)...\n", width, height);
  render_image<ABVH>(objects, width, height, new_pixels,
          camera_position, camera_dir, camera_u, camera_v);
  printf("\n\n");
  printf("Rendering image regular (%dx%d)...\n", width, height);
  render_image<BVH>(objects, width, height, regular_pixels,
          camera_position, camera_dir, camera_u, camera_v);
  printf("\n\n");

  // Output image file (PPM Format)
  write_image(width, height, regular_pixels, "regular.ppm");
  write_image(width, height, new_pixels, "new.ppm");

  // Cleanup
  delete regular_pixels;
  delete new_pixels;
}
