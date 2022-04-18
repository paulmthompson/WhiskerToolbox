#ifndef JANELIA_H
#define JANELIA_H

#include <cstdint>
#include <vector>
#include <memory>

#include "detector_bank.h"

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

struct Hist {
    uint8_t h; // Histogram
    float th; //Slopes
    float s; // Stats
    bool mask; // Mask of seeds
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

struct record
{
    float x;
    float y;
    float thick;
    float score;
};

struct Interval
{
    double min;
    double max;
 };

class JaneliaTracker {

public:
    JaneliaTracker();
   std::vector<Whisker_Seg> find_segments(int iFrame, Image<uint8_t>& image, const Image<uint8_t> &bg);
   JaneliaConfig config;
   LineDetector bank;
   HalfSpaceDetector half_space_bank;
private:

    void compute_seed_from_point_field_on_grid(const Image<uint8_t>& image, Image<uint8_t>& h, Image<float>& th, Image<float>& s);
    Seed* compute_seed_from_point( const Image<uint8_t>& image, int p, int maxr );
    Seed *compute_seed_from_point_ex(const Image<uint8_t>& image, int p, int maxr, float *out_m, float *out_stat);
    Line_Params line_param_from_seed(const Seed *s);
    float eval_line(Line_Params *line, const Image<uint8_t>& image, int p);
    float round_anchor_and_offset( Line_Params *line, int *p, int stride );
    std::vector<int>* get_offset_list(const Image<uint8_t>& image, int support, float angle, int p, int *npx );
    static bool _cmp_seed_scores(seedrecord a, seedrecord b);

    bool is_small_angle(const float angle );
    bool is_angle_leftward(const float angle );

    Whisker_Seg trace_whisker(Seed *s, Image<uint8_t>& image);
    void initialize_paramater_ranges( Line_Params *line, Interval *roff, Interval *rang, Interval *rwid);

    float threshold_two_means( uint8_t *array, size_t size );
    float eval_half_space( Line_Params *line, const Image<uint8_t>& image, int p, float *rr, float *ll );
    int move_line( Line_Params *line, int *p, int stride, int direction );
    int adjust_line_start(Line_Params *line, const Image<uint8_t>& image, int *pp,
                                   Interval *roff, Interval *rang, Interval *rwid);
    bool is_change_too_big( Line_Params *new_line, Line_Params *old, const float alim, const float wlim, const float olim);
    bool is_local_area_trusted( Line_Params *line, Image<uint8_t>& image, int p );
    bool is_local_area_trusted_conservative( Line_Params *line, Image<uint8_t>& image, int p );
    int threshold_bottom_fraction_uint8( const Image<uint8_t>& im );
    static bool outofbounds(const int q, const int cwidth, const int cheight);
    void compute_dxdy( Line_Params *line, float *dx, float *dy );

    //New
    double calculate_whisker_length(Whisker_Seg& w);
    void eliminate_redundant(std::vector<Whisker_Seg>& w_segs);


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

#define SWAP(a,b) (( (a)==(b) ) || ( ((a) ^= (b)), ((b) ^= (a)), ((a) ^=(b) ))  )

#endif // JANELIA_H
