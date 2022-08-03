
#include "janelia.h"

#include <cmath>
#include <cfloat>
#include <algorithm>
#include <numeric>
#include <stdio.h>
#include <chrono>  // for high_resolution_clock
#include <iostream>

JaneliaTracker::JaneliaTracker()
{
    config = JaneliaConfig();
    bank = LineDetector();
    half_space_bank = HalfSpaceDetector();

    pxlist = std::vector<offset_pair>(1000);
}

std::vector<Whisker_Seg> JaneliaTracker::find_segments(int iFrame, Image<uint8_t>& image, const Image<uint8_t>& bg) {
    static Image<uint8_t> h = Image<uint8_t>(); // histogram from compute_seed_from_point_field_windowed_on_contour
    static Image<float> th = Image<float>(); // slopes
    static Image<float> s = Image<float>(); // stats
    static Image<uint8_t> mask = Image<uint8_t>(); // Mask for keeping track of seed points
    static int sarea = 0;

    //auto start = std::chrono::high_resolution_clock::now();

    int  area = image.width * image.height;
    std::vector<Whisker_Seg> wsegs = {};
    int n_segs=0;

    // Prepare
    if(( sarea != area ) )
    {
      h     = Image<uint8_t>(image.width, image.height );
      th    = Image<float>(image.width, image.height );
      s     = Image<float>(image.width, image.height );
      mask  = Image<uint8_t>(image.width, image.height );
      sarea = area;
    }

    //Reset static arrays to zero
    std::fill(h.array.begin(),h.array.end(),0);
    std::fill(th.array.begin(),th.array.end(),0);
    std::fill(s.array.begin(),s.array.end(),0);
    std::fill(mask.array.begin(),mask.array.end(),0);

    // Get contours, and compute correlations on perimeters
    switch(this->config._seed_method)
    {
    case SEED_ON_MHAT_CONTOURS: {

    }
    case SEED_ON_GRID:
    {
        compute_seed_from_point_field_on_grid(image,h,th,s); // 17 ms
    }
    case SEED_EVERYWHERE:
    {

    }
    default:
    {}
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    {
      int i = sarea;
      int nseeds = 0;
      auto&  sa = s.array;
      auto& tha = th.array;
      auto& ha = h.array;
      auto& maska = mask.array;

      // Compute means and mask
      while( i-- )
      { float n = (float) ha[i];
        if( n > 0.0f )
          tha[i] /= n;
      }
      i = sarea;
      while( i-- )
      { if( sa[i] > this->config._seed_thres)
        { maska[i] = 1;
          nseeds ++;
        }
      }
      {
          std::vector<seedrecord> scores(sizeof(seedrecord) * nseeds);
          int stride = image.width;
          Line_Params line;
            int j = 0;

            auto start = std::chrono::high_resolution_clock::now();
            i = sarea;
            while( i-- )
            { if( maska[i]==1 )
              { Seed seed = { i%stride,
                    i/stride,
                    (int) std::round(100 * cos( tha[i] )),
                    (int) std::round(100 * sin( tha[i] )) };

                line = line_param_from_seed( seed );

                scores[j].score = eval_line( &line, image, i );

                scores[j].idx   = i;
                j++;
              }
            }

            std::sort(scores.begin(),scores.end(),
                      [] (const seedrecord& a, const seedrecord& b){
                        return a.score < b.score;
            });

            auto t1 = std::chrono::high_resolution_clock::now();

            j = nseeds;
            while(j--)
            { i = scores[j].idx;
              if( maska[i]==1 )
              {
                Seed seed = { i%stride,
                  i/stride,
                  (int) std::round(100 * cos( tha[i] )),
                  (int) std::round(100 * sin( tha[i] )) };

                auto w = trace_whisker( seed, image );
                if(w.len == 0)
                { std::swap(seed.xdir,seed.ydir);
                  w = trace_whisker( seed, image ); // try again at a right angle...sometimes when we're off by one the slope estimate is perpendicular to the whisker.
                }
                if (w.len > this->config._min_length)
                {
                  w.time = iFrame;
                  w.id  = n_segs++;
                  wsegs.push_back(std::move(w));
                } // ... if w
              } // ... if maska[i]
            }
            scores.clear();
            auto t2 = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed1 = t1 - start;
            std::chrono::duration<double> elapsed2 = t2 - t1;
            //std::cout << "Elapsed time: " << elapsed1.count() << std::endl;
            //std::cout << "Elapsed time: " << elapsed2.count() << std::endl;
            //std::cout << "Num seeds: " << nseeds << std::endl;

          }
    }
    eliminate_redundant(wsegs);

    return wsegs;
}

double JaneliaTracker::calculate_whisker_length(Whisker_Seg& w) {

     double out = 0.0f;

     if (w.len > 0)
     {
        for (int i=1; i<w.x.size(); i++) {
            out += sqrt(pow(w.x[i]-w.x[i-1],2) + pow(w.y[i]-w.y[i-1],2));
        }
     }

     return out;
 }

void JaneliaTracker::eliminate_redundant(std::vector<Whisker_Seg>& w_segs) {

    int i = 0;

    while (i < w_segs.size())
    {
        auto& w2_x = w_segs[i].x;
        auto& w2_y = w_segs[i].y;

        double min_cor = 10000.0;

        for (int j = 0; j<w_segs.size(); j++)
        {
            if (j != i)
            {
                auto& w1_x = w_segs[j].x;
                auto& w1_y = w_segs[j].y;

                double mycor = 0.0;
                for (int k = 1; k<21; k++) {
                    mycor += sqrt(pow(w1_x[w1_x.size() - k]-w2_x[w2_x.size() - k],2) + pow(w1_y[w1_y.size() - k]-w2_y[w2_y.size() - k],2));
                }
                if (mycor < min_cor) {
                    min_cor = mycor;
                }
            }
            if (min_cor < this->config._redundancy_thres) {
                double w1_score = std::accumulate(w_segs[j].scores.begin(),w_segs[j].scores.end(),0);
                double w2_score = std::accumulate(w_segs[i].scores.begin(),w_segs[i].scores.end(),0);

                if (w1_score > w2_score)
                {
                    w_segs.erase(w_segs.begin()+i);
                } else {
                    w_segs.erase(w_segs.begin()+j);
                }

                i = 1;
                break;
            }
        }
        i++;
    }

}

void JaneliaTracker::compute_seed_from_point_field_on_grid(const Image<uint8_t>& image, Image<uint8_t>& hist, Image<float>& slopes, Image<float>& stats) {
    int a = image.width * image.height;
    int stride = image.width;
    auto&  h = hist.array;
    auto& sl = slopes.array;
    auto& st = stats.array;

    float   m,stat;

    // horizontal lines
        int x,y;
      int p,newp,i;
      std::optional<Seed> s;
      for( x=0; x<stride; x++ )
      { for( y=0; y<image.height; y += this->config._lattice_spacing )
        { newp = x+y*stride;
          p = newp;
          for( i=0; i < this->config._maxiter; i++ )
          { p = newp;
            s = compute_seed_from_point_ex(image, x+y*stride, this->config._maxr, &m, &stat);
            if( !s.has_value() ) break;
            newp = s->xpnt + stride * s->ypnt;
            if ( newp == p || stat < this->config._iteration_thres )
              break;
          }
          if( s.has_value() && stat > this->config._accum_thres)
          { h[p] ++;
            sl[p] += m;
            st[p] += stat;
          }
        }
      }

    // Vertical lines
    { int x,y;
      int p,newp,i;
      for( x=0; x<stride; x+=this->config._lattice_spacing  )
      { for( y=0; y<image.height; y ++ )
        { newp = x+y*stride;
          p = newp;
          for( i=0; i < this->config._maxr; i++ ) // Max iter?
          { p = newp;
            std::optional<Seed> s = compute_seed_from_point_ex(image, x+y*stride, this->config._maxr, &m, &stat);
            if( !s ) break;
            newp = s->xpnt + stride * s->ypnt;
            if ( newp == p || stat < this->config._iteration_thres)
              break;
          }
          if( s && stat > this->config._accum_thres)
          { h[p] ++;
            sl[p] += m;
            st[p] += stat;
          }
        }
      }
    }

}

std::optional<Seed> JaneliaTracker::compute_seed_from_point_ex(const Image<uint8_t>& image, int p, int maxr, float *out_m, float *out_stat)
/* Specific for uint8 */
{ static Seed myseed;
static const float eps = 1e-3;
int i = -1, rnpoints = 0, lnpoints = 0;
int stride = image.width;
float lsx   = 0.0, /* statistics for left corner cut: (ab,cd) grouping */
      lsy   = 0.0,
      lsxy  = 0.0,
      lsxx  = 0.0,
      lsyy  = 0.0;
float rsx   = 0.0, /* statistics for right corner cut: (ad,bc) grouping */
      rsy   = 0.0,
      rsxy  = 0.0,
      rsxx  = 0.0,
      rsyy  = 0.0;
/* Spiral out from center
 * Collect pixels minimal over set of pixels with same L0 distance from p
 * Form seed by computing center and slope of collected pixels
 * Analyze eigenvalues from covariance of minima positions
 * Exclude center.
 */
int cx=0;
int cy=0;
int x = p%stride;
int y = p/stride;

if( (x < maxr)                   || // Computation isn't valid for boundary
    (x >=(image.width  - maxr)) ||
    (y < maxr)                   ||
    (y >=(image.height - maxr)) )
{ *out_m = 0.0;
  *out_stat = 0.0;
  return std::nullopt;
}

while( i++ < maxr)
{ int abp,bbp,cbp,dbp, bp = -1;              //best points
  uint8_t abest,bbest,cbest,dbest, best = 255; //best mins
  int j,maxj;

  abp = -1;
  abest = 255;
  j = maxj = 2*i;
  // do one loop of the spiral
  //for(j=0;j<maxj;j++)
  while( j-- )
  { --cy;
    _COMPUTE_SEED_FROM_POINT_HELPER(abest,abp);
  }
  bbp = -1;
  bbest  = 255;
  //for(j=0;j<maxj;j++)
  j = maxj;
  while( j-- )
  { --cx;
    _COMPUTE_SEED_FROM_POINT_HELPER(bbest,bbp);
  }
  cbest = 255;
  cbp = -1;
  j = maxj;
  while( j-- )
  //for(j=0;j<maxj;j++)
  { ++cy;
    _COMPUTE_SEED_FROM_POINT_HELPER(cbest,cbp);
  }
  dbest = 255;
  dbp = -1;
  //for(j=0;j<maxj;j++)
  j = maxj;
  while( j-- )
  { ++cx;
    _COMPUTE_SEED_FROM_POINT_HELPER(dbest,dbp);
  }
  cx++; cy++;


  /*
   * a:     top  edge
   * b:     left edge
   * c:     bottom edge
   * d:     right edge
   */
  /* Integrate statistics for (ab,cd) grouping */
  bp = (abest < bbest) ? abp : bbp;  // (ab)
  if( bp >= 0 )
  { int tx = bp%stride,
        ty = bp/stride;

    lsx += tx;
    lsy += ty;
    lsxy += tx*ty;
    lsxx += tx*tx;
    lsyy += ty*ty;
    lnpoints++;
  }
  bp = (cbest < dbest) ? cbp : dbp; // (cd)
  if( bp > 0 )
  { int tx = bp%stride,
        ty = bp/stride;

    lsx += tx;
    lsy += ty;
    lsxy += tx*ty;
    lsxx += tx*tx;
    lsyy += ty*ty;
    lnpoints++;
  }
  /* Integrate statistics for (ad,cb) grouping */
  bp = (abest < dbest) ? abp : dbp; //(ad)
  if( bp >= 0 )
  { int tx = bp%stride,
        ty = bp/stride;

    rsx += tx;
    rsy += ty;
    rsxy += tx*ty;
    rsxx += tx*tx;
    rsyy += ty*ty;
    rnpoints++;
  }
  bp = (cbest < bbest) ? cbp : bbp; //(cb)
  if( bp > 0 )
  { int tx = bp%stride,
        ty = bp/stride;

        rsx += tx;
        rsy += ty;
        rsxy += tx*ty;
        rsxx += tx*tx;
        rsyy += ty*ty;
        rnpoints++;
  }
} // end search

/* How well do the collected points distribute in a line?
 * Measure the slope
 * */
{ float rm,rnum,lm,lnum; //,rr2,lr2;
  float lstat,rstat;
  //lnum = lnpoints*lsxy - lsx*lsy;          //numerator - for linear regression
  //lm = atan2( lnum,  lnpoints*lsxx - lsx*lsx); //slope angle  - by linear regression
  //lr2 = lnum * lm / ( lnpoints*lsyy - lsy*lsy + eps); // pierson's correlation coefficient
  if( lnpoints <= 3 )
  { lstat = 0.0f;
    lm = 0.0f;
  } else
  { //principle components
    float cxx,cxy,cyy,trace,det,desc,eig0,eig1,stat;
    float n = lnpoints, n2 = lnpoints*lnpoints;
    cxx = lsxx/n - lsx*lsx/n2;
    cxy = lsxy/n - lsx*lsy/n2;
    cyy = lsyy/n - lsy*lsy/n2;
    trace = cxx + cyy;
    det   = cxx*cyy - cxy*cxy;
    desc  = trace*trace - 4.0f*det; // when is this negative? Both eig's imaginary?

    desc = sqrtf(desc);
    eig0  = 0.5f*( trace + desc ); // eig0 > eig1
    eig1  = 0.5f*( trace - desc );
    lstat  = 1.0f - eig1/eig0;
    lm   = atan2( cxx - eig0, -cxy );

  }

  //rnum = rnpoints*rsxy - rsx*rsy;          //numerator - for linear regression
  //rm = atan2(rnum , rnpoints*rsxx - rsx*rsx); //slope angle    - by linear regression
  //rr2 = rnum * rm / ( rnpoints*rsyy - rsy*rsy + eps);  // pierson's correlation coefficient
  if( rnpoints <= 3 )
  { rstat = 0.0f;
  } else
  { //principle components
    float cxx,cxy,cyy,trace,det,desc,eig0,eig1,stat;
    float n = rnpoints, n2 = rnpoints*rnpoints;
    cxx = rsxx/n - rsx*rsx/n2;
    cxy = rsxy/n - rsx*rsy/n2;
    cyy = rsyy/n - rsy*rsy/n2;
    trace = cxx + cyy;
    det   = cxx*cyy - cxy*cxy;
    desc  = trace*trace - 4.0f*det; // when is this negative? Both eig's imaginary?

    desc = sqrtf(desc);
    eig0  = 0.5f*( trace + desc );
    eig1  = 0.5f*( trace - desc );
    rstat  = 1.0f - eig1/eig0;
    rm   = atan2( cxx - eig0, -cxy );

  }

  // choose the set that collected the most line-like distribution
  { float norm;
    if( lstat > rstat )
    { myseed.xpnt = (int) lsx/lnpoints;
      myseed.ypnt = (int) lsy/lnpoints;
      myseed.xdir = 100*cos(lm);
      myseed.ydir = (int) (100*sin(lm));

      norm = 1.0; //_compute_seed_from_point_eigennorm( lm ); // weights by length of line in square
      *out_m = lm;
      *out_stat = lstat/( norm*norm ); // normalize by length squared since the eigenvalue is variance
    } else
    { myseed.xpnt = (int) rsx/rnpoints;
      myseed.ypnt = (int) rsy/rnpoints;
      myseed.xdir = 100*cos(rm);
      myseed.ydir = (int) (100*sin(rm));

      norm = 1.0; //_compute_seed_from_point_eigennorm( rm );
      *out_m = rm;
      *out_stat = rstat/( norm * norm );
    }
  }
}

return myseed;
}

Line_Params JaneliaTracker::line_param_from_seed(const Seed s) {
    Line_Params line;
     const double hpi = M_PI/4.0;
     const double ain = hpi/this->config._angle_step;
       line.offset = .5;
       { if( s.xdir < 0 ) // flip so seed points along positive x
         { line.angle  = std::round(atan2(-1.0f* s.ydir,-1.0f* s.xdir) / ain) * ain;
         } else {
           line.angle  = std::round(atan2(s.ydir,s.xdir) / ain) * ain;
         }
       }
       line.width  = 2.0;
       return line;
}

float JaneliaTracker::eval_line(Line_Params *line, const Image<uint8_t>& image, int p) {

    const int support  = 2*this->config._tlen + 3;
    int npxlist;

    // compute a nearby anchor

    float coff;
    std::tie(coff, p)      = round_anchor_and_offset( *line, p, image.width );
    get_offset_list( image, support, line->angle, p, &npxlist );

    int bank_i = bank.get_nearest(coff, line->width, line->angle);

    auto& parray = image.array;
    float s = 0.0;
    int i = 0;
    while(i < npxlist) {
        s += parray[this->pxlist[i].image_ind] * this->bank.bank.data[bank_i + this->pxlist[i].weight_ind];
        //    s += parray[(*pxlist)[2*i]] * this->bank.bank.data[bank_i+(*pxlist)[2*i+1]];
        i++;
    }

    return -s; // Return the line score
}

std::pair<float,int> JaneliaTracker::round_anchor_and_offset( const Line_Params line, const int p, const int stride )
/* rounds pixel anchor, p, to pixel nearest center of line detector (rx,ry)
** returns best offset to line
**
** This moves the center of the detector a little bit since the line
**  is a bit overconstrained.  However, the size of the error can be
**  bounded to less than the pixel size (proof?).
*/
{
  float ex  = cos(line.angle + M_PI/2); // unit vector normal to line
  float ey  = sin(line.angle + M_PI/2);
  float px  = (p % stride );            // current anchor
  float py  = (p / stride );
  float rx  = px + ex * line.offset;    // current position
  float ry  = py + ey * line.offset;
  float ppx = round( rx );               // round to nearest pixel as anchor
  float ppy = round( ry );
  float drx = rx - ppx;                  // ppx - rx;       // dr: vector from pp to r
  float dry = ry - ppy;                  // ppy - ry;
  float t   = drx*ex + dry*ey;           // dr dot e (projection along normal to line)

  // Max error is ~0.6 px

  //if( ppx != px || ppy != py)
  //{
   // float rx2 = ppx + t * ex,
    //    ry2 = ppy + t * ey;
   // float err = sqrt((rx2-rx)*(rx2-rx) + (ry2-ry)*(ry2-ry));
   // printf("(%3d, %3d) off: %5.5f --> (%3d, %3d) off: %5.5f\terr:%g\n",
   //			(int)px,(int)py,line->offset, (int)ppx, (int)ppy,t, err);
  //}
  return std::make_pair(t,((int)ppx) + stride*( (int) ppy ));
}

void JaneliaTracker::get_offset_list(const Image<uint8_t>& image, const int support, const float angle, int p, int *npx )
  /* The integer pairs are indices into the image and weight arrays such that:
   *
   * The following will perform the correlation of filter and image centered at
   * p in the * image (with the center of the filter as its origin).
   *
   *     int *pairs = get_offset_list(image, support, line, p, &npx);
   *     float score = 0.0;
   *     while(npx--)
   *      score += image->array[ pairs[2*npx] * filter[ pairs[2*npx+1] ]
   *
   * A list of out-of-bounds pixels of the weight array is stored in:
   *
   *     area = support * support
   *     pairs[ npx+1...a-1] // out of bound pairs, clamped to boarder
   *
   *     // integrate over out-of-bouds
   *     int *pairs = get_offset_list(image, support, line, p, &npx);
   *     while( npx++ <  area)
   *      score += image->array[ pairs[2*npx] * filter[ pairs[2*npx+1] ]
   *
   */
{
  static int snpx = 0;
  static int lastp = -1;
  static int last_issmallangle = -1;
  int i,j;
  int half = support / 2;
  int px = p%(image.width),
      py = p/(image.width);
  int ioob = support*support; // index for out-of-bounds pixels

  //pxlist is a minimum of 2*support*support.
  if (this->pxlist.size() < (2*support*support)) {
      this->pxlist.resize((int)std::round(1.25 * 2 * support * support + 64));
  }

  auto is_small_angle = [](const float angle )
  /* true iff angle is in [-pi/4,pi/4) or [3pi/4,5pi/4) */
  { static const float qpi = M_PI/4.0;
    static const float hpi = M_PI/2.0;
    int n = floorf( (angle-qpi)/hpi );
    return  (n % 2) != 0;
   };

  int issa = is_small_angle( angle );
  if( p != lastp || issa != last_issmallangle ) //recompute only if neccessary
  { int tx,ty;                    //  Neglects to check if support has changed
    //float angle = line->angle;
    int ww = image.width;
    int hh = image.height;
    int ox = px - half;
    int oy = py - half;
    lastp = p;
    last_issmallangle = issa;
    //lastangle = line->angle;

    snpx = 0;
    if( issa )
    { for( i=0; i<support; i++ )
      { ty = oy + i;
        if( (ty >= 0) && (ty < hh) )
        { for( j=0; j<support; j++ )
          { tx = ox + j;
            if( (tx >= 0) && (tx<ww) )
            { this->pxlist[ snpx++ ] = std::move(offset_pair(ww * ty + tx,support*i+j));    // image   pixel address
              //this->pxlist[ snpx++ ] = support * i + j; // weights pixel address
            }
          }
        }
        //oob
        for( j=0; j<support; j++ )
        { tx = ox + j;
          if( (ty<0) || (ty>=hh) || (tx < 0) || (tx>=ww) ) //out of bounds
          {
            this->pxlist[ ioob-- ] = std::move(offset_pair(ww * std::min(std::max(0,ty),hh-1) + std::min(std::max(0,tx),ww-1),
                                                     support * i + j)); // clamps to border
            //this->pxlist[ ioob-- ] = support * i + j;
          }
        }
      }

    } else // large angle, do transpose
    { for( i=0; i<support; i++ )
      { tx = ox + i;
        if( (tx >= 0) && (tx < ww) )
        { for( j=0; j<support; j++ )
          { ty = oy + j;
            if( (ty >= 0) && (ty<hh) )
            { this->pxlist[ snpx++ ] = std::move(offset_pair(ww * ty + tx,support * i + j));
              //this->pxlist[ snpx++ ] = support * i + j;
            }
          }
        }

    // put out of bounds pixels at end
        for( j=0; j<support; j++ )
        { ty = oy + j;
          if( (ty<0) || (ty>=hh) || (tx < 0) || (tx>=ww) ) //out of bounds
          {
            this->pxlist[ ioob-- ] = std::move(offset_pair(ww * std::min(std::max(0,ty),hh-1) + std::min(std::max(0,tx),ww-1),
                                                     support * i + j)); // clamps to border
            //this->pxlist[ ioob-- ] = support * i + j;
          }
        }
      }
    } // end if/ angle check
  }

  *npx = snpx;
}

Whisker_Seg JaneliaTracker::trace_whisker(Seed s, Image<uint8_t>& image)
{
  static std::vector<record> ldata(1000);
  static std::vector<record> rdata(1000);

  int nleft = 0, nright = 0;
  float dx,dy,newoff;
  int cwidth  = image.width,
      cheight = image.height;
  Line_Params line,rline,oldline;
  int trusted = 1;

  const double hpi = M_PI/4.0;
  const double ain = hpi/this->config._angle_step;
  const double rad = 45./hpi;
  const double sigmin = (2*this->config._tlen+1)*this->config._min_signal;// + 255.00;

  float x = s.xpnt;
  float y = s.ypnt;
  { int      q,p = x + cwidth*y;
    int      oldp;
    Interval roff, rang, rwid;

    auto compute_dxdy =[]( Line_Params *line, float *dx, float *dy )
    {
      float ex  = cos(line->angle + M_PI/2);  // unit vector normal to line
      float ey  = sin(line->angle + M_PI/2);
      *dx = ex * line->offset; // current position
      *dy = ey * line->offset;
    };

    auto outofbounds = [](const int q, const int cwidth, const int cheight)
    { int x = q%cwidth;
      int y = q/cwidth;
      return (x < 1 || x >= cwidth-1 ||y < 1 || y >= cheight-1);
    };

    /*
     *  init
     */
    std::fill(ldata.begin(),ldata.end(), record());
    std::fill(rdata.begin(),rdata.end(), record());

    line = line_param_from_seed( s );

    auto initialize_parameter_ranges = []( Line_Params *line, Interval *roff, Interval *rang, Interval *rwid)
    {
        rwid->min = 0.5;
        rwid->max = 3.0;
        roff->min = -2.5;
        roff->max =  2.5;
        rang->min = line->angle - M_PI;
        rang->max = line->angle + M_PI;
    };

    initialize_parameter_ranges( &line, &roff, &rang, &rwid);

    //Must start in a trusted area
    if( !is_local_area_trusted_conservative( &line, image, p ) )
      return (Whisker_Seg(0));

    line.score = eval_line( &line, image, p );
    adjust_line_start(&line,image,&p,&roff,&rang,&rwid);

    compute_dxdy( &line, &dx, &dy);

    //ldata[nleft++] = {p%cwidth + dx, p/cwidth + dy, line.width, line.score };
    ldata[nleft++] = record(p%cwidth + dx, p/cwidth + dy, line.width, line.score);

    q = p;
    rline = line;

    /*
     * Move forward from seed
     */
    while( line.score > sigmin )
    { p = move_line( &line, p, cwidth, 1 );
      if( outofbounds(p, cwidth, cheight) ) break;
      line.score = eval_line( &line, image, p );
      oldline = line;
      oldp    = p;
      trusted = adjust_line_start(&line,image,&p,&roff,&rang,&rwid);
      { int nmoves = 0;
        trusted = trusted && is_local_area_trusted( &line, image, p );
        while( !trusted /*&& score > sigmin*/ && nmoves < this->config._half_space_tunneling_max_moves)
        { oldline = line; oldp = p;
          p = move_line( &line, p, cwidth, 1 );
          nmoves ++;
          if( outofbounds(p, cwidth, cheight) ) break;
          trusted = is_local_area_trusted( &line, image, p );
          trusted &= adjust_line_start(&line,image,&p,&roff,&rang,&rwid);
          if(trusted && line.score < sigmin)
          { // check to see if a line can be reaquired
            std::optional<Seed> sd = compute_seed_from_point( image, p, 3.0);
            if(sd.has_value())
            { line = line_param_from_seed(sd.value());
              if( line.angle * oldline.angle < 0.0 ) //make sure points in same direction
                line.angle *= -1.0;
            }
            line.score = eval_line( &line, image, p );
            trusted = adjust_line_start(&line,image,&p,&roff,&rang,&rwid);
            if( !trusted ||
                line.score < sigmin ||
                !is_local_area_trusted( &line, image, p ) ||
                is_change_too_big(line,oldline, 2*this->config._max_delta_angle, 10.0, 10.0) )
            { trusted = 0;    // nothing found, back up
              break;
            }
          }
         // score *= HALF_SPACE_TUNNELING_DECAY;
        }
        if( !trusted )
        {   p = oldp;
            line = oldline;
            break;
        }
      }

      compute_dxdy( &line, &dx, &dy);

      if (nleft < ldata.size()) {
          ldata[nleft] = {p%cwidth + dx, p/cwidth + dy, line.width, line.score };
      } else {
          ldata.push_back({p%cwidth + dx, p/cwidth + dy, line.width, line.score });
      }
      nleft++;
    }

    /*
     * Move backward from seed
     */
    line = rline;
    p = q;
    nright = 0;
    while( line.score > sigmin )
    { p = move_line( &line, p, cwidth, -1 );
      if( outofbounds(p, cwidth, cheight) ) break;
      line.score = eval_line( &line, image, p );
      trusted = adjust_line_start(&line,image,&p,&roff,&rang,&rwid);

      { int nmoves = 0;
        trusted = trusted && is_local_area_trusted( &line, image, p );
        //float score = line.score;
        while( !trusted /*&& score > sigmin*/ && nmoves < this->config._half_space_tunneling_max_moves )
        { oldline = line; oldp = p;
          p = move_line( &line, p, cwidth, -1 );
          nmoves ++;
          if( outofbounds(p, cwidth, cheight) ) break;
          trusted = is_local_area_trusted( &line, image, p );
          trusted &= adjust_line_start(&line,image,&p,&roff,&rang,&rwid);
          if(trusted && line.score < sigmin)
          { // check to see if a line can be reaquired
            std::optional<Seed> sd = compute_seed_from_point( image, p, 3.0); // this will often pop the line back on
            if(sd.has_value()) // else just use last line
            {
              line = line_param_from_seed(sd.value());
              if( line.angle * oldline.angle < 0.0 ) //make sure points in same direction
                line.angle *= -1.0;
            }
            line.score = eval_line( &line, image, p );
            trusted = adjust_line_start(&line,image,&p,&roff,&rang,&rwid);
            if( !trusted ||
                line.score < sigmin ||
                ! is_local_area_trusted( &line, image, p )  ||
                is_change_too_big(line,oldline, 2*this->config._max_delta_angle, 10.0, 10.0 ) )
            { trusted = 0;  // nothing found, back up
              break;
            }
          }
        }
        if( !trusted )
        {   p = oldp;
            line = oldline;
            break;
        }
      }

      compute_dxdy( &line, &dx, &dy);

      if (nright < rdata.size()) {
          rdata[nright] = {p%cwidth + dx, p/cwidth + dy, line.width, line.score };
      } else {
          rdata.push_back({p%cwidth + dx, p/cwidth + dy, line.width, line.score });
      }
      nright++;
    }
  }

  /*
   * Copy results into a whisker segment
   */
  if( nright+nleft > 2*this->config._tlen )
  {
    Whisker_Seg wseg = Whisker_Seg(nright + nleft);
    int j=0, i = nright;
    while( i-- )  /* backward copy */
    { wseg.x     [j] = rdata[i].x;
      wseg.y     [j] = rdata[i].y;
      wseg.thick [j] = rdata[i].thick;
      wseg.scores[j] = rdata[i].score;
      j++;
    }
    for( i=0; i< nleft; i++ )
    { wseg.x     [j] = ldata[i].x;
      wseg.y     [j] = ldata[i].y;
      wseg.thick [j] = ldata[i].thick;
      wseg.scores[j] = ldata[i].score;
      j++;
    }
    return wseg;
  } else
  { return (Whisker_Seg());
  }
}

float threshold_two_means( uint8_t *array, size_t size )
{ size_t i;
    size_t hist[256] ={};
  uint8_t *cur = array + size;
  float num = 0.0,
        dom = 0.0,
        thresh,last,
        c[2] = {0.0,0.0};
  while(cur-- > array) ++hist[ *cur ];
  for(i=0;i<256;i++)
  { float v = hist[i];
    num += i*v;
    dom += v;
  }
  thresh = num/dom;    // the mean - compute this way bc we need to compute the hist anyway

  // update means
  do
  { last = thresh;
    num = dom = 0.0;
    for(i=0;i<thresh;i++)
    { float v = hist[i];
      num += i*v;
      dom += v;
    }
    c[0] = num/dom;
    num = dom = 0.0;
    for(;i<256;i++)
    { float v = hist[i];
      num += i*v;
      dom += v;
    }
    c[1] = num/dom;
    thresh = (c[1]+c[0])/2.0;
  } while( fabs(last - thresh) > 0.5 );
  //debug("Threshold: %f\n",thresh);
  return thresh;
}

int threshold_bottom_fraction_uint8( const Image<uint8_t>& im ) //, float fraction )
{
  auto& d = im.array;
  uint8_t mean = std::floor(std::accumulate(d.begin(),d.end(),0) / d.size());

  int i = im.width * im.height;;
  uint32_t acc = 0;
  int count = 0;
  while(i--)
  {
    if( d[i] <= mean )
    { acc += d[i];
      count ++;
    }
  }
  float lm = acc / count;

  return (int) lm;
}

bool JaneliaTracker::is_local_area_trusted( Line_Params *line, Image<uint8_t>& image, int p )
{ float q,r,l;
  static float thresh = -1.0;
  static std::vector<uint8_t>* lastim = {};
  q = eval_half_space( line, image, p, &r, &l );

  if( thresh < 0.0 || lastim != &image.array) /* recomputes when image changes */
  { thresh = threshold_bottom_fraction_uint8(image);//,HALF_SPACE_FRACTION_DARK );
    lastim = &image.array;
  }

  if( ((r < thresh) && (l < thresh )) ||
      (fabs(q) > this->config._half_space_assymetry))
  { return false;
  } else
  { return true;
  }
}

bool JaneliaTracker::is_local_area_trusted_conservative( Line_Params *line, Image<uint8_t>& image, int p )
{ float q,r,l;
  static float thresh = -1.0;
  static std::vector<uint8_t>* lastim = {};
  //static const std::vector<uint8_t>* lastim = {};
  q = eval_half_space( line, image, p, &r, &l );

  if( thresh < 0.0 || lastim != &image.array) /* recomputes when image changes */
  { //thresh = mean_uint8( image );
    thresh = threshold_two_means( image.array.data(), image.width*image.height );
    lastim = &image.array;
  }
  if( ((r < thresh) && (l < thresh )) ||
      (fabs(q) > this->config._half_space_assymetry) )
  { return false;
  } else
  { return true;
  }
}

float JaneliaTracker::eval_half_space( Line_Params *line, const Image<uint8_t>& image, int p, float *rr, float *ll )
{   int support  = 2*this->config._tlen + 3;
    int npxlist;

    float coff;
    std::tie(coff, p)  = round_anchor_and_offset( *line, p, image.width );
    get_offset_list( image, support, line->angle, p, &npxlist );
    //lefthalf  = get_nearest_from_half_space_detector_bank( coff, line->width, line->angle, &leftnorm );
    //righthalf  = get_nearest_from_half_space_detector_bank( -coff, line->width, line->angle, &rightnorm );
    int lefthalf = half_space_bank.get_nearest(coff,line->width,line->angle);
    int righthalf = half_space_bank.get_nearest(-coff,line->width,line->angle);

    auto& parray = image.array;
    int i = 0; //npxlist;
    float l = 0.0;
    float r = 0.0;
    while(i < npxlist)
    {
        l += parray[this->pxlist[i].image_ind] * this->half_space_bank.bank.data[lefthalf + this->pxlist[i].weight_ind];
        r += parray[this->pxlist[i].image_ind] * this->half_space_bank.bank.data[righthalf +this->pxlist[i].weight_ind]; // It doesn't work if I have the a term here. Why is that?
        i++;
        //l += parray[pxlist[2*i]] * lefthalf[pxlist[2*i+1]];
        //r += parray[pxlist[2*i]] * righthalf[a - pxlist[2*i+1]];
    }
    // Take averages
    float q = (r-l)/(r+l);
    r /= this->half_space_bank.norm;
    l /= this->half_space_bank.norm;
    //
    // Finish up
    //
    *ll = l; *rr = r;
    return q;
}

int JaneliaTracker::move_line( Line_Params* line, const int p, const int stride, const int direction )
{ float lx,ly,ex,ey,rx0,ry0,rx1,ry1;
  float ppx, ppy, drx, dry, t, ox, oy;
  float th = line->angle;
  lx  = cos(th);           // unit vector along direction of line
  ly  = sin(th);
  ex  = cos(th + M_PI/2);  // unit vector normal to line
  ey  = sin(th + M_PI/2);
  rx0 = (p % stride ) + ex * line->offset; // current position
  ry0 = (p / stride ) + ey * line->offset;
  rx1 = rx0 + direction * lx;        // step to next position
  ry1 = ry0 + direction * ly;
  ppx = round( rx1 );    // round to nearest pixel as anchor
  ppy = round( ry1 );    //   (largest error ~0.6 px and lies along direction of line)
  drx = rx1 - ppx; //ppx - rx1; //rx0 - ppx;       // vector from pp to r1
  dry = ry1 - ppy; //ppy - ry1; //ry0 - ppy;
  t   = drx*ex + dry*ey; // dr dot l

  line->offset = t;
  return ((int)ppx) + stride*( (int) ppy );
}

int JaneliaTracker::adjust_line_start(Line_Params *line, const Image<uint8_t> &image, int *pp,
                               Interval *roff, Interval *rang, Interval *rwid)
{ double hpi = acos(0.)/2.;
  double ain = hpi/this->config._angle_step;
  double rad = 45./hpi;
  int trusted = 1;

  double x, v, best, last;
  int    better;
  int p = *pp;

  double atest = line->angle;
  Line_Params backup = *line;

  better = 1;
  while (better)
  { better = 0;
    best = line->score;

    /* adjust angle */
    /* when the angle switches from small to large around 45 deg, the meaning
     * off the offset changes.  But at 45 deg, the x-offset and the y-offset
     * are the same.
     */
    last = best;
    x = line->angle;
    do
    { line->angle -= ain;
      v = eval_line(line,image,p);
    } while( fabs(v - last) < 1e-5 && line->angle >= rang->min);
    if (         (v - best) > 1e-5 && line->angle >= rang->min)
    { best   =  v;
      better =  1;
    }
    else
    { line->angle = x;
      do
      { line->angle += ain;
        v = eval_line(line,image,p);
      } while( fabs(v - last) < 1e-5  && line->angle <= rang->max);
      if (         (v - best) > 1e-5  && line->angle <= rang->max)
      { best   =  v;
        better =  1;
      }
      else
        line->angle = x;
    }

    /*
     * adjust offset
     * */
    last = best;
    x = line->offset;
    do
    { line->offset -= this->config._offset_step;
      v = eval_line(line,image,p);
    } while( fabs(v - last) < 1e-5 && line->offset >= roff->min);
    if (         (v - best) > 1e-5 && line->offset >= roff->min)
    { best   =  v;
      better =  1;
    }
    else
    { line->offset = x;
      do
      { line->offset += this->config._offset_step;
        v = eval_line(line,image,p);
      } while( fabs(v - last) < 1e-5 && line->offset <= roff->max);
      if (         (v - best) > 1e-5 && line->offset <= roff->max)
      { best   =  v;
        better =  1;
      }
      else
        line->offset = x;
    }

    /*
     * adjust width
     * */
    last = best;
    x = line->width;
    do
    { line->width -= this->config._width_step;
      v = eval_line(line,image,p);
    } while( fabs(v - last) < 1e-5 && line->width >= rwid->min);
    if (         (v - best) > 1e-5 && line->width >= rwid->min)
    { best   =  v;
      better =  1;
    }
    else
    { line->width = x;
      do
      { line->width += this->config._width_step;
        v = eval_line(line,image,p);
      } while( fabs(v - last) < 1e-5 && line->width <= rwid->max);
      if (         (v - best) > 1e-5 && line->width <= rwid->max)
      { best   =  v;
        better =  1;
      }
      else
        line->width = x;
    }

    line->score = best;
  }


  if( is_change_too_big( backup, *line, this->config._max_delta_angle, this->config._max_delta_width, this->config._max_delta_offset) )
  {
    *line = backup; //No adjustment
    return 0;
  }

  *pp = p;
  return trusted;
}

bool JaneliaTracker::is_change_too_big( const Line_Params new_line, const Line_Params old, const float alim, const float wlim, const float olim)
{ float dth = old.angle - new_line.angle,
        dw  = old.width - new_line.width,
        doff= old.offset- new_line.offset;
  if( ( fabs((dth*180.0/M_PI)) >  alim ) ||
      ( fabs(dw)               >  wlim ) ||
      ( fabs(doff)             >  olim ) )
  {
    return true;
  }
  return false;
}

std::optional<Seed> JaneliaTracker::compute_seed_from_point( const Image<uint8_t>& image, int p, int maxr )
{ float m, stat;
  return compute_seed_from_point_ex( image, p, maxr, &m, &stat);
}
