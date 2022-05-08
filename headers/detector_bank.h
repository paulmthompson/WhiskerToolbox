#ifndef DETECTOR_BANK_H
#define DETECTOR_BANK_H

#include <vector>
#include <unordered_map>

#define bigReal 1.E38f

enum SEED_METHOD_ENUM { SEED_ON_MHAT_CONTOURS, SEED_ON_GRID, SEED_EVERYWHERE };

struct JaneliaConfig {
    SEED_METHOD_ENUM _seed_method;
    int _lattice_spacing;
    int _maxr;
    int _maxiter;
    float _iteration_thres;
    float _accum_thres;
    float _seed_thres;
    float _angle_step;
    int _tlen;
    float _offset_step;
    float _width_min;
    float _width_max;
    float _width_step;
    float _min_signal;
    float _half_space_assymetry;
    float _max_delta_angle;
    int _half_space_tunneling_max_moves;
    float _max_delta_width;
    float _max_delta_offset;
    float _min_length;
    float _redundancy_thres;

    JaneliaConfig() {
        _seed_method = SEED_ON_GRID;
        _lattice_spacing = 50;
        _maxr = 4;
        _maxiter = 1;
        _iteration_thres = 0.0;
        _accum_thres = 0.99;
        _seed_thres = 0.99;
        _angle_step = 18.0;
        _tlen = 8;
        _offset_step = 0.1;
        _width_min = 0.4;
        _width_max = 6.5;
        _width_step = 0.2;
        _min_signal = 5.0;
        _half_space_assymetry = 0.25;
        _max_delta_angle = 10.1;
        _half_space_tunneling_max_moves = 50;
        _max_delta_width = 6.0;
        _max_delta_offset = 6.0;
        _min_length = 100.0;
        _redundancy_thres = 20.0;
    }
};

struct Range
{
    double min;
    double max;
    double step;
};

struct point
{
    float x;
    float y;
};

struct ipoint
{
    long x;
    long y;
};

struct rng
{
    long mn;
    long mx;
};

struct box
{
    point min;
    point max;
};

struct vertex
{
    ipoint ip;
    rng rx;
    rng ry;
    short in;
};

struct Array
{
  int ndim;
  std::array<int,6> strides_bytes;
  std::array<int,6> strides_px;
  std::array<int,5> shape;
  std::vector<float> data;
  //std::unordered_map<int,float> data;

  Array() {

  }
  // ndim_in is always 5
  Array(std::array<int,5>& shape_in , int bytesperpixel ) {
      int i = 5;
      ndim = 5;
      shape = {};
      strides_bytes = {};
      strides_px = {};

      strides_bytes[ndim] = bytesperpixel;
      strides_px[ndim] = 1;

      while(i--)                                            // For shape = (w,h,d):
      { strides_bytes[i] = strides_bytes[i+1] * shape_in[ndim-1 - i];//   strides = (whd, wh, w, 1)
        strides_px[i] = strides_bytes[i] / bytesperpixel;
        shape[i]   = shape_in[i];
      }
      data = std::vector<float>(strides_bytes[0]);
      //data.reserve(strides_bytes[0]);
  }
};


class DetectorBank {
public:
    Array bank;
    Range off;
    Range wid;
    Range ang;

    int compute_number_steps( Range r );
    int get_nearest(float offset, float width, float angle);
    int Get_Detector(int ioffset, int iwidth, int iangle  );
protected:

};

class LineDetector : public DetectorBank {
public:
    LineDetector();
    LineDetector(JaneliaConfig config);

private:
    void Build_Line_Detectors(float length,int supportsize );
    void Render_Line_Detector(float offset,float length,
                                float angle,
                                float width,
                                point anchor,
                                float *image, int *strides);
};

class HalfSpaceDetector : public DetectorBank {
public:
    HalfSpaceDetector();
    HalfSpaceDetector(JaneliaConfig config);
    float norm;

private:
    void Build_Half_Space_Detectors(float length, int supportsize);
    void Render_Half_Space_Detector( float offset,
                                     float length,
                                     float angle,
                                     float width,
                                     point anchor,
                                     float *image, int *strides  );

};

#define WRAP_ANGLE_HALF_PLANE(th) \
  while( (th)    < M_PI/2.0 )     \
    (th)    +=   M_PI;            \
  while( (th)    >= M_PI/2.0 )    \
    (th)    -=   M_PI;
#define WRAP_ANGLE_2PI(th)        \
  while( (th)    < -M_PI )        \
    (th)    +=   2*M_PI;          \
  while( (th)    >= M_PI )        \
    (th)    -=   2*M_PI;

template <std::size_t N>
void Simple_Line_Primitive( std::array<point,N>& verts, point offset, float length, float thick );

template <std::size_t N>
void rotate( std::array<point,N>& pbuf, float angle);

template <std::size_t N>
void translate(std::array<point,N>& pbuf, point ori);

template <std::size_t N>
void Sum_Pixel_Overlap(std::array<point,N>& xy, float gain, float *grid, int *strides );

void pixel_to_vertex_array(int p, int stride, std::array<point,4>& v);

template <std::size_t N>
float inter(std::array<point, N>& a, std::array<point,4>& b);

template <std::size_t N>
void range(box& B, std::array<point,N>& x);

void cntrib(long long *s, ipoint f, ipoint t, short w);
long long area(const ipoint a, const ipoint p, const ipoint q);
void cross(long long *s, vertex * a, vertex * b, vertex * c, vertex * d,
    double a1, double a2, double a3, double a4);


template <std::size_t N>
double fit(box& B, std::array<point,N>& x, vertex * ix, int fudge);

void inness(long long *sarea, vertex * P, int cP, vertex * Q, int cQ);

template <std::size_t N>
void Simple_Circle_Primitive(std::array<point,N>& verts, point center, float radius, int direction);

template <std::size_t N>
void Multiply_Pixel_Overlap(std::array<point,N>& xy, float gain, float boundary, float *grid, int *strides );

#endif // DETECTOR_BANK_H
