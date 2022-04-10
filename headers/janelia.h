#ifndef JANELIA_H
#define JANELIA_H

#include <cstdint>
#include <vector>
#include <memory>

enum SEED_METHOD_ENUM { SEED_ON_MHAT_CONTOURS, SEED_ON_GRID, SEED_EVERYWHERE };
#define bigReal 1.E38f

struct Whisker_Seg {

    int id;
    int time;
    int len;
    std::vector<float> x;
    std::vector<float> y;
    std::vector<float> thick;
    std::vector<float> scores;

    Whisker_Seg(int n) {
        len = n;
        x = std::vector<float>(n);
        y = std::vector<float>(n);
        thick = std::vector<float>(n);
        scores = std::vector<float>(n);
    }
};

template <typename T>
struct Image
{
    int width;
    int height;
    std::vector<T> array;

    Image<T>() {
        width = 0;
        height = 0;
        std::vector<T> array{};
    }

    Image<T>(int w, int h) {
        width=w;
        height=h;
        array = std::vector<T>(h*w,0);
    }
    Image<T>(int w, int h,std::vector<T> img) {
        width=w;
        height=h;
        array = img;
    }
};

struct Seed {
    int xpnt;
    int ypnt;
    int xdir;
    int ydir;
};


struct  Line_Params {
    float  offset;
    float  angle;
    float  width;
    float  score;
  };

struct seedrecord
{
    int idx;
    float score;

    seedrecord()
    {
        idx = 0;
        score = 0;
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

struct Interval
{
    double min;
    double max;
 };

struct Array
{
  std::vector<float> data;
  std::vector<int> strides_bytes;
  std::vector<int> strides_px;
  std::vector<int> shape;
  int ndim;

  Array() {

  }

  Array(std::vector<int> shape_in , int ndim_in, int bytesperpixel ) {
      int i = ndim_in;
      ndim = ndim_in;
      shape = std::vector<int>(ndim);
      strides_bytes = std::vector<int>(ndim+1);
      strides_px = std::vector<int>(ndim+1);

      strides_bytes[ndim] = bytesperpixel;
      strides_px[ndim] = 1;

      while(i--)                                            // For shape = (w,h,d):
      { strides_bytes[i] = strides_bytes[i+1] * shape_in[ndim-1 - i];//   strides = (whd, wh, w, 1)
        strides_px[i] = strides_bytes[i] / bytesperpixel;
        shape[i]   = shape_in[i];
      }
      data = std::vector<float>(strides_bytes[0]);
  }
};


class JaneliaTracker {

public:
    JaneliaTracker();
   std::vector<Whisker_Seg> find_segments(int iFrame, Image<uint8_t>& image, Image<uint8_t> &bg);
private:
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
    Array bank;
    Array half_space_bank;

    void compute_seed_from_point_field_on_grid(Image<uint8_t>& image, Image<uint8_t>& h, Image<float>& th, Image<float>& s);
    Seed* compute_seed_from_point( Image<uint8_t>& image, int p, int maxr );
    Seed *compute_seed_from_point_ex( Image<uint8_t>& image, int p, int maxr, float *out_m, float *out_stat);
    Line_Params line_param_from_seed(const Seed *s);
    float eval_line(Line_Params *line, Image<uint8_t>& image, int p);
    float round_anchor_and_offset( Line_Params *line, int *p, int stride );
    std::vector<int> get_offset_list( Image<uint8_t>& image, int support, float angle, int p, int *npx );
    static bool _cmp_seed_scores(seedrecord a, seedrecord b);

    int get_nearest_from_line_detector_bank(float offset, float width, float angle);
    int get_nearest_from_half_space_detector_bank(float offset, float width, float angle, float *norm);

    void get_line_detector_bank(Range *off, Range *wid, Range *ang);
    void get_half_space_detector_bank(Range *off, Range *wid, Range *ang, float *norm);


    Array Build_Line_Detectors( Range off,Range wid,Range ang,float length,int supportsize );
    Array Build_Half_Space_Detectors( Range off,
                                       Range wid,
                                       Range ang,
                                       float length,
                                       int supportsize );


    int Get_Line_Detector(Array& lbank, int ioffset, int iwidth, int iangle  );
    int Get_Half_Space_Detector(Array& hbank, int ioffset, int iwidth, int iangle  );

    void Render_Line_Detector( float offset,
                               float length,
                               float angle,
                               float width,
                               point anchor,
                               float* image, int *strides  );
    void Render_Half_Space_Detector( float offset,
                                     float length,
                                     float angle,
                                     float width,
                                     point anchor,
                                     float *image, int *strides  );

    void Simple_Circle_Primitive( point *verts, int npoints, point center, float radius, int direction);
    void Simple_Line_Primitive( point *verts, point offset, float length, float thick );
    void rotate( point *pbuf, int n, float angle);
    void translate( point* pbuf, int n, point ori);
    void Sum_Pixel_Overlap( float *xy, int n, float gain, float *grid, int *strides );
    void Multiply_Pixel_Overlap( float *xy, int n, float gain, float boundary, float *grid, int *strides );
    void pixel_to_vertex_array(int p, int stride, float *v);
    unsigned array_max_f32u ( float *buf, int size, int step, float bound );
    unsigned array_min_f32u ( float *buf, int size, int step, float bound );
    float inter(point * a, int na, point * b, int nb);
    int ovl(rng p, rng q);
    void bdr(float * X, float y);
    void bur(float * X, float y);
    void range(box *B, point * x, int c);
    void cntrib(long long *s, ipoint f, ipoint t, short w);
    long long area(ipoint a, ipoint p, ipoint q);
    void cross(long long *s, vertex * a, vertex * b, vertex * c, vertex * d,
        double a1, double a2, double a3, double a4);
    double fit(box *B, point * x, int cx, vertex * ix, int fudge);

    void inness(long long *sarea, vertex * P, int cP, vertex * Q, int cQ);

    int is_small_angle( float angle );
    int is_angle_leftward( float angle );
    int compute_number_steps( Range *r );

    Whisker_Seg trace_whisker(Seed *s, Image<uint8_t>& image);
    void initialize_paramater_ranges( Line_Params *line, Interval *roff, Interval *rang, Interval *rwid);
    int is_local_area_trusted_conservative( Line_Params *line, Image<uint8_t>& image, int p );
    float threshold_two_means( uint8_t *array, size_t size );
    float eval_half_space( Line_Params *line, Image<uint8_t>& image, int p, float *rr, float *ll );
    int move_line( Line_Params *line, int *p, int stride, int direction );
    int adjust_line_start(Line_Params *line, Image<uint8_t>& image, int *pp,
                                   Interval *roff, Interval *rang, Interval *rwid);
    int is_change_too_big( Line_Params *new_line, Line_Params *old, float alim, float wlim, float olim);
    int is_local_area_trusted( Line_Params *line, Image<uint8_t>& image, int p );
    int threshold_bottom_fraction_uint8( Image<uint8_t>& im );
    static int outofbounds(int q, int cwidth, int cheight);
    void compute_dxdy( Line_Params *line, float *dx, float *dy );


};

//  uint8_t val = *( ((uint8_t*) image->array) + tp);

#define _COMPUTE_SEED_FROM_POINT_HELPER(BEST,BP)                    \
{ int tp = x+cx + image.width * (y+cy);                            \
  uint8_t val = image.array[tp];                                   \
  if(   val <= BEST )                                               \
  { BP = tp;                                                        \
    BEST =  val;                                                    \
  }                                                                 \
}

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

#define SWAP(a,b) (( (a)==(b) ) || ( ((a) ^= (b)), ((b) ^= (a)), ((a) ^=(b) ))  )

#endif // JANELIA_H
