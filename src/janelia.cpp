
#include "janelia.h"

#include <cmath>
#include <cfloat>
#include <algorithm>
#include <numeric>
#include <stdio.h>

JaneliaTracker::JaneliaTracker()
{
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

    bank = Array();
    half_space_bank = Array();
}

std::vector<Whisker_Seg> JaneliaTracker::find_segments(int iFrame, Image<uint8_t>& image, Image<uint8_t>& bg) {
    static Image<uint8_t> h = Image<uint8_t>(); // histogram from compute_seed_from_point_field_windowed_on_contour
    static Image<float> th = Image<float>(); // slopes
    static Image<float> s = Image<float>(); // stats
    static Image<uint8_t> mask = Image<uint8_t>(); // Mask for keeping track of seed points
    static int sarea = 0;

    int  area = image.width * image.height;
//    Object_Map  *omap;
    std::vector<Whisker_Seg> wsegs = {};
    size_t max_segs= 0;
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

    // Get contours, and compute correlations on perimeters
    switch(this->_seed_method)
    {
    case SEED_ON_MHAT_CONTOURS: {

    }
    case SEED_ON_GRID:
    {
        compute_seed_from_point_field_on_grid(image,h,th,s);
    }
    case SEED_EVERYWHERE:
    {

    }
    default:
    {}
    }
    { int i = sarea;
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
      { if( sa[i] > this->_seed_thres)
        { maska[i] = 1;
          nseeds ++;
        }
      }
      {
          std::vector<seedrecord> scores(sizeof(seedrecord) * nseeds);
          int stride = image.width;
          Line_Params line;
            int j = 0;

            i = sarea;
            while( i-- )
            { if( maska[i]==1 )
              { Seed seed = { i%stride,
                    i/stride,
                    (int) std::round(100 * cos( tha[i] )),
                    (int) std::round(100 * sin( tha[i] )) };

                line = line_param_from_seed( &seed );

                scores[j].score = eval_line( &line, image, i );

                scores[j].idx   = i;
                j++;
              }
            }

            sort(scores.begin(),scores.end(), JaneliaTracker::_cmp_seed_scores);

            j = nseeds;
            while(j--)
            { i = scores[j].idx;
              if( maska[i]==1 )
              {
                Seed seed = { i%stride,
                  i/stride,
                  (int) std::round(100 * cos( tha[i] )),
                  (int) std::round(100 * sin( tha[i] )) };

                auto w = trace_whisker( &seed, image );
                if(w.len == 0)
                { SWAP(seed.xdir,seed.ydir);
                  w = trace_whisker( &seed, image ); // try again at a right angle...sometimes when we're off by one the slope estimate is perpendicular to the whisker.
                }
                if (calculate_whisker_length(w) > this->_min_length)
                {
                  w.time = iFrame;
                  w.id  = n_segs;
                  wsegs.push_back(std::move(w));
                } // ... if w
              } // ... if maska[i]
            }
            scores.clear();
          }
    }
    eliminate_redundant(wsegs);
    return wsegs;
}

 bool JaneliaTracker::_cmp_seed_scores(seedrecord a, seedrecord b)
{
  float d = a.score - b.score;
  return (d < 0);
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

void JaneliaTracker::eliminate_redundant(std::vector<Whisker_Seg> w_segs) {

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
            if (min_cor < this->_redundancy_thres) {
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

void JaneliaTracker::compute_seed_from_point_field_on_grid(Image<uint8_t>& image, Image<uint8_t>& hist, Image<float>& slopes, Image<float>& stats) {
    int a = image.width * image.height;
    int stride = image.width;
    auto&  h = hist.array;
    auto& sl = slopes.array;
    auto& st = stats.array;

    float   m,stat;

    // horizontal lines
        int x,y;
      int p,newp,i;
      Seed *s;

      for( x=0; x<stride; x++ )
      { for( y=0; y<image.height; y += this->_lattice_spacing )
        { newp = x+y*stride;
          p = newp;
          for( i=0; i < this->_maxiter; i++ )
          { p = newp;
            s = compute_seed_from_point_ex(image, x+y*stride, this->_maxr, &m, &stat);
            if( !s ) break;
            newp = s->xpnt + stride * s->ypnt;
            if ( newp == p || stat < this->_iteration_thres )
              break;
          }
          if( s && stat > this->_accum_thres)
          { h[p] ++;
            sl[p] += m;
            st[p] += stat;
          }
        }
      }

    // Vertical lines
    { int x,y;
      int p,newp,i;
      Seed *s;
      for( x=0; x<stride; x+=this->_lattice_spacing  )
      { for( y=0; y<image.height; y ++ )
        { newp = x+y*stride;
          p = newp;
          for( i=0; i < this->_maxr; i++ ) // Max iter?
          { p = newp;
            s = compute_seed_from_point_ex(image, x+y*stride, this->_maxr, &m, &stat);
            if( !s ) break;
            newp = s->xpnt + stride * s->ypnt;
            if ( newp == p || stat < this->_iteration_thres)
              break;
          }
          if( s && stat > this->_accum_thres)
          { h[p] ++;
            sl[p] += m;
            st[p] += stat;
          }
        }
      }
    }

}

Seed* JaneliaTracker::compute_seed_from_point_ex( Image<uint8_t>& image, int p, int maxr, float *out_m, float *out_stat)
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
int tp, cx,cy,x,y;
cx=cy=0;
x = p%stride;
y = p/stride;

if( (x < maxr)                   || // Computation isn't valid for boundary
    (x >=(image.width  - maxr)) ||
    (y < maxr)                   ||
    (y >=(image.height - maxr)) )
{ *out_m = 0.0;
  *out_stat = 0.0;
  return nullptr;
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

return &myseed;
}

Line_Params JaneliaTracker::line_param_from_seed(const Seed *s) {
    Line_Params line;
     const double hpi = M_PI/4.0;
     const double ain = hpi/this->_angle_step;
       line.offset = .5;
       { if( s->xdir < 0 ) // flip so seed points along positive x
         { line.angle  = std::round(atan2(-1.0f* s->ydir,-1.0f* s->xdir) / ain) * ain;
         } else {
           line.angle  = std::round(atan2(s->ydir,s->xdir) / ain) * ain;
         }
       }
       line.width  = 2.0;
       return line;
}

float JaneliaTracker::eval_line(Line_Params *line, Image<uint8_t>& image, int p) {
    int i;
    const int support  = 2*this->_tlen + 3;
    int npxlist;

      std::vector<int> pxlist;
      //float *weights, coff;
      float coff;
      int bank_i;

      float  r,l,q,s       = 0.0;
      static float lastscore = 0.0;
      static float bg     = -1.0; //background is bright
      static void *lastim = NULL;

      // compute a nearby anchor

      coff      = round_anchor_and_offset( line, &p, image.width );
      pxlist    = get_offset_list( image, support, line->angle, p, &npxlist );

      bank_i = get_nearest_from_line_detector_bank ( coff, line->width, line->angle );

      {
        auto& parray = image.array;
        s = 0.0;
        i = npxlist;
        while(i--)
          //s += parray[pxlist[2*i]] * weights[pxlist[2*i+1]];
           s += parray[pxlist[2*i]] * this->bank.data[bank_i+pxlist[2*i+1]];
      }

      return -s;
}

float JaneliaTracker::round_anchor_and_offset( Line_Params *line, int *p, int stride )
/* rounds pixel anchor, p, to pixel nearest center of line detector (rx,ry)
** returns best offset to line
**
** This moves the center of the detector a little bit since the line
**  is a bit overconstrained.  However, the size of the error can be
**  bounded to less than the pixel size (proof?).
*/
{ float ex,ey,rx,ry,px,py;
  float ppx, ppy, drx, dry, t;     // ox, oy;
  ex  = cos(line->angle + M_PI/2); // unit vector normal to line
  ey  = sin(line->angle + M_PI/2);
  px  = (*p % stride );            // current anchor
  py  = (*p / stride );
  rx  = px + ex * line->offset;    // current position
  ry  = py + ey * line->offset;
  ppx = round( rx );               // round to nearest pixel as anchor
  ppy = round( ry );
  drx = rx - ppx;                  // ppx - rx;       // dr: vector from pp to r
  dry = ry - ppy;                  // ppy - ry;
  t   = drx*ex + dry*ey;           // dr dot e (projection along normal to line)

  // Max error is ~0.6 px

  //if( ppx != px || ppy != py)
  //{
   // float rx2 = ppx + t * ex,
    //    ry2 = ppy + t * ey;
   // float err = sqrt((rx2-rx)*(rx2-rx) + (ry2-ry)*(ry2-ry));
   // printf("(%3d, %3d) off: %5.5f --> (%3d, %3d) off: %5.5f\terr:%g\n",
   //			(int)px,(int)py,line->offset, (int)ppx, (int)ppy,t, err);
  //}
  *p = ((int)ppx) + stride*( (int) ppy );
  return t;
}

std::vector<int> JaneliaTracker::get_offset_list( Image<uint8_t>& image, int support, float angle, int p, int *npx )
  /* returns a static buffer with *npx integer pairs.  The integer pairs are
   * indices into the image and weight arrays such that:
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
{ static std::vector<int> pxlist = {};
  static int snpx = 0;
  static int lastp = -1;
  static int last_issmallangle = -1;
  int i,j, issa;
  int half = support / 2;
  int px = p%(image.width),
      py = p/(image.width);
  int ioob = 2*support*support; // index for out-of-bounds pixels

  //pxlist is a minimum of 2*support*support.
  if (pxlist.size() < (2*support*support)) {
      pxlist.resize((int)std::round(1.25 * 2 * support * support + 64));
  }

  issa = is_small_angle( angle );
  if( p != lastp || issa != last_issmallangle ) //recompute only if neccessary
  { int tx,ty,ww,hh,ox,oy;                     //  Neglects to check if support has changed
    //float angle = line->angle;
    ww = image.width;
    hh = image.height;
    ox = px - half;
    oy = py - half;
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
            { pxlist[ snpx++ ] = ww * ty + tx;    // image   pixel address
              pxlist[ snpx++ ] = support * i + j; // weights pixel address
            }
          }
        }
        //oob
        for( j=0; j<support; j++ )
        { tx = ox + j;
          if( (ty<0) || (ty>=hh) || (tx < 0) || (tx>=ww) ) //out of bounds
          {
            pxlist[ ioob-- ] = ww * std::min(std::max(0,ty),hh-1) + std::min(std::max(0,tx),ww-1); // clamps to border
            pxlist[ ioob-- ] = support * i + j;
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
            { pxlist[ snpx++ ] = ww * ty + tx;
              pxlist[ snpx++ ] = support * i + j;
            }
          }
        }

    // put out of bounds pixels at end
        for( j=0; j<support; j++ )
        { ty = oy + j;
          if( (ty<0) || (ty>=hh) || (tx < 0) || (tx>=ww) ) //out of bounds
          {
            pxlist[ ioob-- ] = ww * std::min(std::max(0,ty),hh-1) + std::min(std::max(0,tx),ww-1); // clamps to border
            pxlist[ ioob-- ] = support * i + j;
          }
        }
      }
    } // end if/ angle check
  }
  *npx = snpx/2;
  return pxlist;
}

int JaneliaTracker::is_small_angle( float angle )
 /* true iff angle is in [-pi/4,pi/4) or [3pi/4,5pi/4) */
{ static const float qpi = M_PI/4.0;
 static const float hpi = M_PI/2.0;
 int n = floorf( (angle-qpi)/hpi );
 return  (n % 2) != 0;
}

int JaneliaTracker::is_angle_leftward( float angle )
 /* true iff angle is in left half plane */
{ //static const float qpi = M_PI/4.0;
 static const float hpi = M_PI/2.0;
 int n = floorf( (angle-hpi)/M_PI );
 return  (n % 2) == 0;
}

int JaneliaTracker::get_nearest_from_line_detector_bank(float offset, float width, float angle)
{ int o,a,w;
  Range orng, arng, wrng;
  //Array *bank = get_harmonic_line_detector_bank( &orng, &wrng, &arng );
  get_line_detector_bank( &orng, &wrng, &arng );

  //If the angle is > 45 deg, fetch the detector
  //  that, when transposed, will be correct.
  //  This mechanism lets me store only half the detectors.
  if( !is_small_angle( angle ) )  // if large angle then transpose
  { angle = 3.0*M_PI/2.0 - angle; //   to small ones ( <45deg )
  //offset = -offset;			  // The transpose is a rotation and flip
  }								  // T = R(3pi/2) Flip
  WRAP_ANGLE_2PI( angle );        // rotating as angle -= 3pi/2 also flips the offset
                                  //    so it doesn't need to be done explicitly
  //Lines are left right symmetric
  //This allows us to store only half the angles (again)
  if( is_angle_leftward(angle) )
  { WRAP_ANGLE_HALF_PLANE( angle );
    offset = -offset;
  }

  o = lround( ( offset - orng.min ) / orng.step );
  a = lround( (  angle - arng.min ) / arng.step );
  w = lround( (  width - wrng.min ) / wrng.step );

  return Get_Line_Detector( this->bank, o, w, a );
}

int JaneliaTracker::compute_number_steps( Range *r )
{  return lround( (r->max - r->min) / r->step ) + 1;
}

void JaneliaTracker::Render_Line_Detector( float offset,
                           float length,
                           float angle,
                           float width,
                           point anchor,
                           float *image, int *strides  )
/*
 * `strides` should be an array of size ndim+1 (for 2d, size=3)
 *           with { width*height*channels, width*channels, channels }
 *           For now assume channels == 1.
 *  `anchor` The center is at anchor.
 */
{ point prim[4];
  const float thick = 0.7;
  const float r = 1.0;
  //const float area = 4*thick*length;
  //length -=2;

  { point off = { 0.0, offset + width/2.0f + r*thick/2.0f   };
    Simple_Line_Primitive(prim, off, length, r*thick   );
    rotate( prim, 4, angle );
    translate( prim, 4, anchor );
    Sum_Pixel_Overlap( (float*) prim, 4, -1.0/r, image, strides );
  }
  { point off = { 0.0, offset + width/2.0f - thick/2.0f };
    Simple_Line_Primitive(prim, off, length/r, thick );
    rotate( prim, 4, angle );
    translate( prim, 4, anchor );
    Sum_Pixel_Overlap( (float*) prim, 4,  r, image, strides );
  }
  { point off = { 0.0, offset - width/2.0f + thick/2.0f };
    Simple_Line_Primitive(prim, off, length/r, thick );
    rotate( prim, 4, angle );
    translate( prim, 4, anchor );
    Sum_Pixel_Overlap( (float*) prim, 4,  r, image, strides );
  }
  { point off = { 0.0, offset - width/2.0f - r*thick/2.0f  };
    Simple_Line_Primitive(prim, off, length, r*thick   );
    rotate( prim, 4, angle );
    translate( prim, 4, anchor );
    Sum_Pixel_Overlap( (float*) prim, 4, -1.0f/r, image, strides );
  }
  return;
}

void JaneliaTracker::Simple_Line_Primitive( point *verts, point offset, float length, float thick )
{ point v0 = { offset.x - length,  offset.y - thick},
       v1 = { offset.x + length,  offset.y - thick},
       v2 = { offset.x + length,  offset.y + thick},
       v3 = { offset.x - length,  offset.y + thick};
 verts[0] = v0;
 verts[1] = v1;
 verts[2] = v2;
 verts[3] = v3;
}

void JaneliaTracker::rotate( point *pbuf, int n, float angle)
  /* positive angle rotates counter clockwise */
{ float s = sin(angle),
        c = cos(angle);
  point *p = pbuf + n;
  while( p-- > pbuf )
  { float x = p->x,
          y = p->y;
    p->x =  x*c - y*s;
    p->y =  x*s + y*c;
  }
}

void JaneliaTracker::translate( point* pbuf, int n, point ori)
{ point *p = pbuf + n;
  while( p-- > pbuf )
  { p->x =  p->x + ori.x;
    p->y =  p->y + ori.y;
  }
  return;
}

void JaneliaTracker::Sum_Pixel_Overlap( float *xy, int n, float gain, float *grid, int *strides )
/* `xy`      is an array of `n` vertices arranged in (x,y) pairs.
 * `n`       the number of vertices in `xy`.
 * `grid`    should point to the origin pixel in an image buffer in the
 *           channel of interest.
 * `strides` should be an array of size ndim+1 (for 2d, size=3)
 *           with { width*height*channels, width*channels, channels }
 *           For now assume channels == 1.
 */
{
  /*
   * 1. Find which pixels are involved (simplest: all of them).
   *    a. restrict to bbox
   *    b. try to only do the expensive stuff on "interesting pixels"
   *       i.e. pixels the line crosses.
   *       The others are either all the way inside or all the way outside
   *       which a quick(?) even-odd rule test can check
   *    c. A more interesting approach would be to generalize the intersect
   *       computation for arbitrary tiling/subdivisions of the plane.
   *       This would reduce duplicate effort on shared edges.
   * 2. Convert pixels into paramterized polygons (4-tuples of vertices)
   *                      4-tuples of vertices    - simplest
   * 3. For each pixel, compute intersection area
   */
  float pxverts[8];
  int px,ix,iy;
  unsigned minx, maxx, miny, maxy;

  // Compute AABB
  minx = array_min_f32u( xy  , 2*n, 2, 0.0                      );
  maxx = array_max_f32u( xy  , 2*n, 2, strides[1]-1             );
  miny = array_min_f32u( xy+1, 2*n, 2, 0.0                      );
  maxy = array_max_f32u( xy+1, 2*n, 2, strides[0]/strides[1] - 1);

  for( ix = minx; ix <= maxx; ix++ )
  { for( iy = miny; iy <= maxy; iy ++ )
    { px = iy*strides[1] + ix;
      pixel_to_vertex_array( px, strides[1], pxverts );
      *(grid+px) += gain * inter( (point*) xy, n, (point*) pxverts, 4 );
    }
  }
  return;
}

void JaneliaTracker::pixel_to_vertex_array(int p, int stride, float *v)
{ float x = p%stride;
 float y = p/stride;
 v[0] = x;      v[1] = y;
 v[2] = x+1.0f; v[3] = y;
 v[4] = x+1.0f; v[5] = y+1.0f;
 v[6] = x;      v[7] = y+1.0f;
 return;
}

unsigned JaneliaTracker::array_max_f32u ( float *buf, int size, int step, float bound )
{ float *t = buf + size;
 float r = 0.0f;
 while( (t -= step) >= buf )
   r = std::max( r, ceilf(*t) );
 return (unsigned) std::min(r,bound);
}

unsigned JaneliaTracker::array_min_f32u ( float *buf, int size, int step, float bound )
{ float *t = buf + size;
 float r = FLT_MAX;
 while( (t -= step) >= buf )
   r = std::min( r, floorf(*t) );
 return (unsigned) std::max(r,bound);
}

float JaneliaTracker::inter(point * a, int na, point * b, int nb)
{ //vertex ipa[na+1], ipb[nb+1];
  vertex *ipa,*ipb;
  box B = {{bigReal, bigReal}, {-bigReal, -bigReal}};
  double ascale;

  if(na < 3 || nb < 3) return 0;
  ipa = (vertex*) malloc( sizeof(vertex) * (na+1) );
  ipb = (vertex*) malloc( sizeof(vertex) * (nb+1) );

  range(&B, a, na);
  range(&B, b, nb);

  ascale = fit(&B, a, na, ipa, 0);
  ascale = fit(&B, b, nb, ipb, 2);

  { long long s = 0;
    int j, k;

    /*
     * Look for crossings, add contributions from crossings and track winding
     * */
    for(j=0; j<na; ++j)
      for(k=0; k<nb; ++k)
        if(ovl(ipa[j].rx, ipb[k].rx) && ovl(ipa[j].ry, ipb[k].ry)) // if edges have overlapping bounding boxes...
        {
          long long a1 = -area(ipa[j  ].ip, ipb[k].ip, ipb[k+1].ip),
             a2 =  area(ipa[j+1].ip, ipb[k].ip, ipb[k+1].ip);
          { int o = a1<0;
            if(o == a2<0) //if there's a crossing...
            {
              long long a3 = area(ipb[k].ip, ipa[j].ip, ipa[j+1].ip),
                 a4 = -area(ipb[k+1].ip, ipa[j].ip, ipa[j+1].ip);
              if(a3<0 == a4<0)  //if still consistent with a crossing...
              { if(o) cross(&s, &ipa[j], &ipa[j+1], &ipb[k], &ipb[k+1], a1, a2, a3, a4);
                else  cross(&s, &ipb[k], &ipb[k+1], &ipa[j], &ipa[j+1], a3, a4, a1, a2);
              }
            }
          }
        }
    /* Add contributions from non-crossing edges */
    inness(&s, ipa, na, ipb, nb);
    inness(&s, ipb, nb, ipa, na);
    free(ipa);
    free(ipb);
    return s/ascale;
  }
}

int JaneliaTracker::ovl(rng p, rng q)
/* True if intervals intersect */
{ return p.mn < q.mx && q.mn < p.mx;
}

void JaneliaTracker::bdr(float * X, float y)
{ *X = *X<y ? *X:y;
}

void JaneliaTracker::bur(float * X, float y)
{ *X = *X>y ? *X:y;
}

void JaneliaTracker::range(box *B, point * x, int c)
{ while(c--)
 { bdr(&B->min.x, x[c].x); bur(&B->max.x, x[c].x);
   bdr(&B->min.y, x[c].y); bur(&B->max.y, x[c].y);
 }
}

void JaneliaTracker::cntrib(long long *s, ipoint f, ipoint t, short w)
/* Integrand for the line integral.  Google `Green's theorem polygon area` for
* functional form.
*/
{ *s += (long long)w*(t.x-f.x)*(t.y+f.y)/2;
}

long long JaneliaTracker::area(ipoint a, ipoint p, ipoint q)
/* Compute the area of the triangle (apq) */
{ return (long long)p.x*q.y - (long long)p.y*q.x +
   (long long)a.x*(p.y - q.y) + (long long)a.y*(q.x - p.x);
}

void JaneliaTracker::cross(long long *s, vertex * a, vertex * b, vertex * c, vertex * d,
    double a1, double a2, double a3, double a4)
{ /* Interpolate to intersection and add contributions from each half edge */
  float r1=a1/((float)a1+a2), r2 = a3/((float)a3+a4);
  /*
  cntrib(s, (ipoint){a->ip.x + r1*(b->ip.x-a->ip.x),
                     a->ip.y + r1*(b->ip.y-a->ip.y)},
      b->ip, 1);
  cntrib(s, d->ip, (ipoint){
      c->ip.x + r2*(d->ip.x - c->ip.x),
      c->ip.y + r2*(d->ip.y - c->ip.y)}, 1);
  */
  { ipoint p = {  static_cast<long>(a->ip.x + r1*(b->ip.x-a->ip.x)),
                  static_cast<long>(a->ip.y + r1*(b->ip.y-a->ip.y))};
    cntrib(s, p, b->ip, 1 );
  }
  { ipoint p = {
      static_cast<long>(c->ip.x + r2*(d->ip.x - c->ip.x)),
      static_cast<long>(c->ip.y + r2*(d->ip.y - c->ip.y))};
    cntrib(s, d->ip, p, 1 );
  }
  ++a->in; /* Track winding numbers...these show up later in `inness` */
  --c->in;
}

void JaneliaTracker::inness(long long *sarea, vertex * P, int cP, vertex * Q, int cQ)
{ int s=0, c=cQ;
  ipoint p = P[0].ip;
  int j;
  while(c--) //Compute winding of P[0] wrt Q
    if(Q[c].rx.mn < p.x && p.x < Q[c].rx.mx) //Bounds check x-interval only
    {  //use area to determine P[0] left of Q[c] edge
      int sgn = 0 < area(p, Q[c].ip, Q[c+1].ip);   // 0 or 1. 1 if left of Q[c] and Q[c] moves right
      // only count cw and moving right or ccw and moving left
      s += sgn != Q[c].ip.x < Q[c+1].ip.x ? 0 : (sgn?-1:1); //add winding
    }
  for(j=0; j<cP; ++j)
  { if(s)
      cntrib(sarea, P[j].ip, P[j+1].ip, s);
    s += P[j].in;
  }
}

double JaneliaTracker::fit(box *B, point * x, int cx, vertex * ix, int fudge)
  /* Fits points to an integral lattice.
   *
   * Converts floating point coords to an integer representation.  The bottom
   * three bits beyond the significance of the floating point and are used to
   * offset points to guarantee resolution of degeneracies.  This is similar to
   * the method described in:
   *
   * Edelsbrunner, H. and Mücke, E. P. Simulation of simplicity
   * ACM Trans. Graph. 9, 1 (1990), 66-104.
   * http://doi.acm.org/10.1145/77635.77639
   */
{ const float gamut = 500000000., mid = gamut/2.;
  float rngx = B->max.x - B->min.x, sclx = gamut/rngx,
        rngy = B->max.y - B->min.y, scly = gamut/rngy;
  int c=cx;
  while(c--)
  { ix[c].ip.x = (long)((x[c].x - B->min.x)*sclx - mid)&~7|fudge|c&1;
    ix[c].ip.y = (long)((x[c].y - B->min.y)*scly - mid)&~7|fudge;
  }
  ix[0].ip.y += cx&1;
  ix[cx] = ix[0];

  c=cx;
  while(c--)
  { rng xl = { ix[c  ].ip.x, ix[c+1].ip.x },
        xh = { ix[c+1].ip.x, ix[c  ].ip.x },
        yl = { ix[c  ].ip.y, ix[c+1].ip.y },
        yh = { ix[c+1].ip.y, ix[c  ].ip.y };
    ix[c].rx = ix[c].ip.x < ix[c+1].ip.x ? xl : xh;
//      (rng){ ix[c  ].ip.x, ix[c+1].ip.x }:
//      (rng){ ix[c+1].ip.x, ix[c  ].ip.x };
    ix[c].ry = ix[c].ip.y < ix[c+1].ip.y ? yl : yh;
//      (rng){ ix[c  ].ip.y, ix[c+1].ip.y}:
//      (rng){ ix[c+1].ip.y, ix[c  ].ip.y};
    ix[c].in=0;
  }
  return sclx*scly;
}

Whisker_Seg JaneliaTracker::trace_whisker(Seed *s, Image<uint8_t>& image)
{ typedef struct { float x; float y; float thick; float score; } record;
    static std::vector<record> ldata = {};
    static std::vector<record> rdata = {};
  static size_t maxldata = 0, maxrdata = 0;

  int nleft = 0, nright = 0;
  float x,y,dx,dy,newoff;
  int cwidth  = image.width,
      cheight = image.height;
  Line_Params line,rline,oldline;
  int trusted = 1;

  const double hpi = M_PI/4.0;
  const double ain = hpi/this->_angle_step;
  const double rad = 45./hpi;
  const double sigmin = (2*this->_tlen+1)*this->_min_signal;// + 255.00;

  x = s->xpnt;
  y = s->ypnt;
  { int      q,p = x + cwidth*y;
    int      oldp;
    Interval roff, rang, rwid;

    /*
     *  init
     */

    if( !ldata.empty() ) ldata.resize(maxldata);
    if( !rdata.empty() ) rdata.resize(maxrdata);

    line = line_param_from_seed( s );

    initialize_paramater_ranges( &line, &roff, &rang, &rwid);

    //Must start in a trusted area
    if( !is_local_area_trusted_conservative( &line, image, p ) )
      return (Whisker_Seg(0));

    line.score = eval_line( &line, image, p );
    adjust_line_start(&line,image,&p,&roff,&rang,&rwid);

    size_t newsize = (size_t) (1.25 * (nleft+1) + 64);
    ldata.resize(newsize);
    maxldata = newsize;
    compute_dxdy( &line, &dx, &dy);
    { record trec = {p%cwidth + dx, p/cwidth + dy, line.width, line.score };
      ldata[nleft++] = trec;
    }

    q = p;
    rline = line;

    /*
     * Move forward from seed
     */
    while( line.score > sigmin )
    { move_line( &line, &p, cwidth, 1 );
      if( outofbounds(p, cwidth, cheight) ) break;
      line.score = eval_line( &line, image, p );
      oldline = line;
      oldp    = p;
      trusted = adjust_line_start(&line,image,&p,&roff,&rang,&rwid);
      { int nmoves = 0;
        trusted = trusted && is_local_area_trusted( &line, image, p );
        while( !trusted /*&& score > sigmin*/ && nmoves < this->_half_space_tunneling_max_moves)
        { oldline = line; oldp = p;
          move_line( &line, &p, cwidth, 1 );
          nmoves ++;
          if( outofbounds(p, cwidth, cheight) ) break;
          trusted = is_local_area_trusted( &line, image, p );
          trusted &= adjust_line_start(&line,image,&p,&roff,&rang,&rwid);
          if(trusted && line.score < sigmin)
          { // check to see if a line can be reaquired
            Seed *sd = compute_seed_from_point( image, p, 3.0);
            if(sd)
            { line = line_param_from_seed(sd);
              if( line.angle * oldline.angle < 0.0 ) //make sure points in same direction
                line.angle *= -1.0;
            }
            line.score = eval_line( &line, image, p );
            trusted = adjust_line_start(&line,image,&p,&roff,&rang,&rwid);
            if( !trusted ||
                line.score < sigmin ||
                !is_local_area_trusted( &line, image, p ) ||
                is_change_too_big(&line,&oldline, 2*this->_max_delta_angle, 10.0, 10.0) )
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

      size_t newsize = (size_t) (1.25 * (nleft+1) + 64);
      ldata.resize(newsize);
      maxldata = newsize;
      compute_dxdy( &line, &dx, &dy);
      { record trec = {p%cwidth + dx, p/cwidth + dy, line.width, line.score };
        ldata[nleft++] = trec;
      }
    }

    /*
     * Move backward from seed
     */
    line = rline;
    p = q;
    nright = 0;
    while( line.score > sigmin )
    { move_line( &line, &p, cwidth, -1 );
      if( outofbounds(p, cwidth, cheight) ) break;
      line.score = eval_line( &line, image, p );
      trusted = adjust_line_start(&line,image,&p,&roff,&rang,&rwid);

      { int nmoves = 0;
        trusted = trusted && is_local_area_trusted( &line, image, p );
        //float score = line.score;
        while( !trusted /*&& score > sigmin*/ && nmoves < this->_half_space_tunneling_max_moves )
        { oldline = line; oldp = p;
          move_line( &line, &p, cwidth, -1 );
          nmoves ++;
          if( outofbounds(p, cwidth, cheight) ) break;
          trusted = is_local_area_trusted( &line, image, p );
          trusted &= adjust_line_start(&line,image,&p,&roff,&rang,&rwid);
          if(trusted && line.score < sigmin)
          { // check to see if a line can be reaquired
            Seed *sd = compute_seed_from_point( image, p, 3.0); // this will often pop the line back on
            if(sd) // else just use last line
            {
              line = line_param_from_seed(sd);
              if( line.angle * oldline.angle < 0.0 ) //make sure points in same direction
                line.angle *= -1.0;
            }
            line.score = eval_line( &line, image, p );
            trusted = adjust_line_start(&line,image,&p,&roff,&rang,&rwid);
            if( !trusted ||
                line.score < sigmin ||
                ! is_local_area_trusted( &line, image, p )  ||
                is_change_too_big(&line,&oldline, 2*this->_max_delta_angle, 10.0, 10.0 ) )
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

      size_t newsize = (size_t) (1.25 * (nright+1) + 64);
      rdata.resize(newsize);
      maxrdata = newsize;
      compute_dxdy( &line, &dx, &dy);
      { record trec = {p%cwidth + dx, p/cwidth + dy, line.width, line.score };
        rdata[nright++] = trec;
      }
    }
  }

  /*
   * Copy results into a whisker segment
   */
  if( nright+nleft > 2*this->_tlen )
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
  { return (Whisker_Seg(0));
  }
}

void JaneliaTracker::initialize_paramater_ranges( Line_Params *line, Interval *roff, Interval *rang, Interval *rwid)
{
    rwid->min = 0.5;
    rwid->max = 3.0;
    roff->min = -2.5;
    roff->max =  2.5;
    rang->min = line->angle - M_PI;
    rang->max = line->angle + M_PI;
}

int JaneliaTracker::is_local_area_trusted_conservative( Line_Params *line, Image<uint8_t>& image, int p )
{ float q,r,l;
  static float thresh = -1.0;
  static std::vector<uint8_t> lastim = {};
  q = eval_half_space( line, image, p, &r, &l );

  if( thresh < 0.0 || lastim != image.array) /* recomputes when image changes */
  { //thresh = mean_uint8( image );
    thresh = threshold_two_means( image.array.data(), image.width*image.height );
    lastim = image.array;
  }
  if( ((r < thresh) && (l < thresh )) ||
      (fabs(q) > this->_half_space_assymetry) )
  { return 0;
  } else
  { return 1;
  }
}

float JaneliaTracker::threshold_two_means( uint8_t *array, size_t size )
{ size_t i;
  std::vector<size_t> hist(256);
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

float JaneliaTracker::eval_half_space( Line_Params *line, Image<uint8_t>& image, int p, float *rr, float *ll )
{ int i,support  = 2*this->_tlen + 3;
  int npxlist, a = support*support;

  std::vector<int> pxlist;
  float coff;
  float leftnorm;
  int lefthalf, righthalf;
  float rightnorm;
  float  r,l,q       = 0.0;
  //static void *lastim = NULL;
  //static float bg = -1.0;

  // Out of bounds will be clamped to a constant corresponding to the bright
  // background
  //if( bg < 0.0 || lastim != image->array) /* memoize - recomputes when image changes */
  //{ bg = threshold_upper_fraction_uint8(image);
  //  lastim = image->array;
  //}

  coff = round_anchor_and_offset( line, &p, image.width );
  pxlist    = get_offset_list( image, support, line->angle, p, &npxlist );
  lefthalf  = get_nearest_from_half_space_detector_bank( coff, line->width, line->angle, &leftnorm );
  righthalf  = get_nearest_from_half_space_detector_bank( -coff, line->width, line->angle, &rightnorm );
  {
    auto& parray = image.array;
    i = a; //npxlist;
    l = 0.0;
    r = 0.0;
    while(i--)
    {
      l += parray[pxlist[2*i]] * this->half_space_bank.data[lefthalf+pxlist[2*i+1]];
      r += parray[pxlist[2*i]] * this->half_space_bank.data[righthalf+pxlist[2*i+1]];
      //l += parray[pxlist[2*i]] * lefthalf[pxlist[2*i+1]];
      //r += parray[pxlist[2*i]] * righthalf[a - pxlist[2*i+1]];
    }

  }

  // Take averages

  q = (r-l)/(r+l);
  r /= rightnorm;
  l /= leftnorm;

  //
  // Finish up
  //
  *ll = l; *rr = r;
  return q;
}

int JaneliaTracker::get_nearest_from_half_space_detector_bank(float offset, float width, float angle, float *norm)
{ int o,a,w;
  Range orng, arng, wrng;
  get_half_space_detector_bank( &orng, &wrng, &arng, norm );

  if( !is_small_angle( angle ) )  // if large angle then transpose
  { angle = 3.0*M_PI/2.0 - angle; //   to small ones ( <45deg )
  //offset = -offset;
  }
  WRAP_ANGLE_2PI( angle );

  //sometimes need to flip the line upside down
  if( is_angle_leftward(angle) )
  { WRAP_ANGLE_HALF_PLANE( angle );
    offset = -offset;
  }

  o = lround( ( offset - orng.min ) / orng.step );
  a = lround( (  angle - arng.min ) / arng.step );
  w = lround( (  width - wrng.min ) / wrng.step );

  return Get_Half_Space_Detector(half_space_bank, o, w, a );
}

// I think that this is the finding the index of the element to start with for future calculations?
int JaneliaTracker::Get_Line_Detector(Array& lbank, int ioffset, int iwidth, int iangle  )
{ return iangle  * lbank.strides_px[1]
                                + iwidth  * lbank.strides_px[2]
                                + ioffset * lbank.strides_px[3];
}

int JaneliaTracker::Get_Half_Space_Detector(Array& hbank, int ioffset, int iwidth, int iangle  )
{ return   iangle  * hbank.strides_px[1]
                                + iwidth  * hbank.strides_px[2]
                                + ioffset * hbank.strides_px[3];
}

void JaneliaTracker::get_line_detector_bank(Range *off, Range *wid, Range *ang)
{
  static Range o,a,w;
  if( this->bank.data.empty() )
  {
    Range v[3] = {{ -1.0,       1.0,         this->_offset_step},          //offset
                  { -M_PI/4.0,  M_PI/4.0,    M_PI/4.0/this->_angle_step},  //angle
                  {  this->_width_min, this->_width_max,   this->_width_step  }};         //width
    o = v[0];
    a = v[1];
    w = v[2];
      this->bank = Build_Line_Detectors( o, w, a, this->_tlen, 2*this->_tlen+3 );

    printf("Built Line Detector Bank");
    fflush(stdout);
  }
  *off = o; *ang = a; *wid = w;


}

void JaneliaTracker::get_half_space_detector_bank(Range *off, Range *wid, Range *ang, float *norm)
{
  static float sum = -1.0;
  static Range o,a,w;
  if( this->half_space_bank.data.empty() )
  {
    Range v[3] = {{ -1.0,       1.0,         this->_offset_step},          //offset
                  { -M_PI/4.0,  M_PI/4.0,    M_PI/4.0/this->_angle_step },  //angle
                  {  this->_width_min, this->_width_max,   this->_width_step}};         //width
    o = v[0];
    a = v[1];
    w = v[2];

    this->half_space_bank = Build_Half_Space_Detectors( o, w, a, this->_tlen, 2*this->_tlen+3 );

    { float *m = this->half_space_bank.data.data() + Get_Half_Space_Detector(this->half_space_bank,0,0,0);
      int n = (2*this->_tlen+3)*(2*this->_tlen+3);
      while(n--)
        sum += m[n];
    }
    printf("Build Half Space Detector Bank");
    fflush(stdout);
  }
  *off = o; *ang = a; *wid = w; *norm = sum;

}

Array JaneliaTracker::Build_Half_Space_Detectors( Range off,
                                   Range wid,
                                   Range ang,
                                   float length,
                                   int supportsize )
{
  int noff =  compute_number_steps( &off ),
      nwid =  compute_number_steps( &wid ),
      nang =  compute_number_steps( &ang );
  std::vector<int> shape = { supportsize,
                   supportsize,
                   noff,
                   nwid,
                   nang};
  Array newbank = Array(shape, 5, sizeof(float));

  { int    o,a,w;
    for( o = 0; o < noff; o++ )
    { //point anchor = {supportsize/2.0, o*off.step + off.min + supportsize/2.0};
      point anchor = {supportsize/2.0f, supportsize/2.0f};
      for( a = 0; a < nang; a++ )
        for( w = 0; w < nwid; w++ )
        {
            float *bank_i = newbank.data.data() + Get_Half_Space_Detector(newbank, o,w,a);
          Render_Half_Space_Detector(
              o*off.step + off.min,                       //offset (before rotation)
              length,                                     //length,
              a*ang.step + ang.min,                       //angle,
              w*wid.step + wid.min,                       //width,
              anchor,                                     //anchor,
              bank_i,      //image
             newbank.strides_px.data() + 3);                      //strides
        }
    }
  }

  return newbank;
}

Array JaneliaTracker::Build_Line_Detectors( Range off,
                            Range wid,
                            Range ang,
                            float length,
                            int supportsize )
{
 int noff =  compute_number_steps( &off ),
     nwid =  compute_number_steps( &wid ),
     nang =  compute_number_steps( &ang );
 std::vector<int> shape = { supportsize,
                  supportsize,
                  noff,
                  nwid,
                  nang};
  Array newbank = Array(shape, 5, sizeof(float));

 { int    o,a,w;
   for( o = 0; o < noff; o++ )
   { //point anchor = {supportsize/2.0, o*off.step + off.min + supportsize/2.0};
     point anchor = {supportsize/2.0f, supportsize/2.0f};
     for( a = 0; a < nang; a++ )
       for( w = 0; w < nwid; w++ )
       {
           float *bank_i = newbank.data.data() + Get_Line_Detector( newbank, o,w,a);
         Render_Line_Detector(
             o*off.step + off.min,                       //offset (before rotation)
             length,                                     //length,
             a*ang.step + ang.min,                       //angle,
             w*wid.step + wid.min,                       //width,
             anchor,                                     //anchor,
             bank_i,            //image
             newbank.strides_px.data() + 3);                      //strides
       }
   }
 }

 return newbank;
}

void JaneliaTracker::Render_Half_Space_Detector( float offset,
                                 float length,
                                 float angle,
                                 float width,
                                 point anchor,
                                 float *image, int *strides  )
/*
 * `strides` should be an array of size ndim+1 (for 2d, size=3)
 *           with { width*height*channels, width*channels, channels }
 *           For now assume channels == 1.
 *  `anchor` The center is at anchor.
 */
{
  float thick = length;
  float density = 1.0f;

  { point prim[4];
    point off = { 0.0, offset + thick /*+ width/2.0 */   };
    Simple_Line_Primitive(prim, off, 2*length, thick   );
    rotate( prim, 4, angle );
    translate( prim, 4, anchor );
    Sum_Pixel_Overlap( (float*) prim, 4, density, image, strides );
  }

  //{ point prim[4];
  //  point off = { 0.0, offset - thick - width/2.0   };
  //  Simple_Line_Primitive(prim, off, 2*length, thick   );
  //  rotate( prim, 4, angle );
  //  translate( prim, 4, anchor );
  //  Sum_Pixel_Overlap( (float*) prim, 4, -density, image, strides );
  //}

  { point prim[12];
    int npoint = 12;
    point off = { 0.0, offset };
    Simple_Circle_Primitive(prim,npoint,off,length, 1);
    rotate( prim, npoint, angle );
    translate( prim, npoint, anchor );
    Multiply_Pixel_Overlap( (float*) prim, npoint, density, 0, image, strides );
  }

  return;
}

void JaneliaTracker::Simple_Circle_Primitive( point *verts, int npoints, point center, float radius, int direction)
{ int i = npoints;
  float k = direction*2*M_PI/(float)npoints;
  while(i--)
  { point p = { static_cast<float>(center.x + radius * cos( k*i )),
                static_cast<float>(center.y + radius * sin( k*i ))};
    verts[i] = p;
  }
}

void JaneliaTracker::Multiply_Pixel_Overlap( float *xy, int n, float gain, float boundary, float *grid, int *strides )
/* `xy`      is an array of `n` vertices arranged in (x,y) pairs.
 * `n`       the number of vertices in `xy`.
 * `grid`    should point to the origin pixel in an image buffer in the
 *           channel of interest.
 * `strides` should be an array of size ndim+1 (for 2d, size=3)
 *           with { width*height*channels, width*channels, channels }
 *           For now assume channels == 1.
 */
{ float pxverts[8];
  int px,ix,iy;
  unsigned minx, maxx, miny, maxy;

  // Compute AABB
  minx = array_min_f32u( xy  , 2*n, 2, 0.0                      );
  maxx = array_max_f32u( xy  , 2*n, 2, strides[1]-1             );
  miny = array_min_f32u( xy+1, 2*n, 2, 0.0                      );
  maxy = array_max_f32u( xy+1, 2*n, 2, strides[0]/strides[1] - 1);

  // multiply by overlaps
  for( ix = minx; ix <= maxx; ix++ )
  { for( iy = miny; iy <= maxy; iy ++ )
    { px = iy*strides[1] + ix;
      pixel_to_vertex_array( px, strides[1], pxverts );
      *(grid+px) *= gain * inter( (point*) xy, n, (point*) pxverts, 4 );
    }
  }

  // everything outside of the AABB gets multiplied times boundary
  { float *p;
    for( iy = 0; iy < strides[0]/strides[1]; iy++ )
    { p = grid + iy*strides[1];
      for( ix = 0; ix < strides[1]; ix++ )
        if( (ix<minx) || (maxx<ix) ||
            (iy<miny) || (maxy<iy) )
          *(p + ix) *= boundary;
    }
  }
  return;
}

int JaneliaTracker::move_line( Line_Params *line, int *p, int stride, int direction )
{ float lx,ly,ex,ey,rx0,ry0,rx1,ry1;
  float ppx, ppy, drx, dry, t, ox, oy;
  float th = line->angle;
  lx  = cos(th);           // unit vector along direction of line
  ly  = sin(th);
  ex  = cos(th + M_PI/2);  // unit vector normal to line
  ey  = sin(th + M_PI/2);
  rx0 = (*p % stride ) + ex * line->offset; // current position
  ry0 = (*p / stride ) + ey * line->offset;
  rx1 = rx0 + direction * lx;        // step to next position
  ry1 = ry0 + direction * ly;
  ppx = round( rx1 );    // round to nearest pixel as anchor
  ppy = round( ry1 );    //   (largest error ~0.6 px and lies along direction of line)
  drx = rx1 - ppx; //ppx - rx1; //rx0 - ppx;       // vector from pp to r1
  dry = ry1 - ppy; //ppy - ry1; //ry0 - ppy;
  t   = drx*ex + dry*ey; // dr dot l

  line->offset = t;
  *p = ((int)ppx) + stride*( (int) ppy );
  return *p;
}

int JaneliaTracker::adjust_line_start(Line_Params *line, Image<uint8_t> &image, int *pp,
                               Interval *roff, Interval *rang, Interval *rwid)
{ double hpi = acos(0.)/2.;
  double ain = hpi/this->_angle_step;
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
    { line->offset -= this->_offset_step;
      v = eval_line(line,image,p);
    } while( fabs(v - last) < 1e-5 && line->offset >= roff->min);
    if (         (v - best) > 1e-5 && line->offset >= roff->min)
    { best   =  v;
      better =  1;
    }
    else
    { line->offset = x;
      do
      { line->offset += this->_offset_step;
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
    { line->width -= this->_width_step;
      v = eval_line(line,image,p);
    } while( fabs(v - last) < 1e-5 && line->width >= rwid->min);
    if (         (v - best) > 1e-5 && line->width >= rwid->min)
    { best   =  v;
      better =  1;
    }
    else
    { line->width = x;
      do
      { line->width += this->_width_step;
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


  if( is_change_too_big( &backup, line, this->_max_delta_angle, this->_max_delta_width, this->_max_delta_offset) )
  {
    *line = backup; //No adjustment
    return 0;
  }

  *pp = p;
  return trusted;
}

int JaneliaTracker::is_change_too_big( Line_Params *new_line, Line_Params *old, float alim, float wlim, float olim)
{ float dth = old->angle - new_line->angle,
        dw  = old->width - new_line->width,
        doff= old->offset- new_line->offset;
  if( ( fabs((dth*180.0/M_PI)) >  alim ) ||
      ( fabs(dw)               >  wlim ) ||
      ( fabs(doff)             >  olim ) )
  {
    return 1;
  }
  return 0;
}

int JaneliaTracker::is_local_area_trusted( Line_Params *line, Image<uint8_t>& image, int p )
{ float q,r,l;
  static float thresh = -1.0;
  static std::vector<uint8_t> lastim = {};
  q = eval_half_space( line, image, p, &r, &l );

  if( thresh < 0.0 || lastim != image.array) /* recomputes when image changes */
  { thresh = threshold_bottom_fraction_uint8(image);//,HALF_SPACE_FRACTION_DARK );
    lastim = image.array;
  }

  if( ((r < thresh) && (l < thresh )) ||
      (fabs(q) > this->_half_space_assymetry))
  { return 0;
  } else
  { return 1;
  }
}

int JaneliaTracker::threshold_bottom_fraction_uint8( Image<uint8_t>& im ) //, float fraction )
{ float acc, mean, lm;
  int a,i, count;
  std::vector<uint8_t> d;

  d = im.array;
  a = i = im.width * im.height;
  acc = 0.0f;
  while(i--)
    acc += d[i];
  mean = acc / (float)a;

  i = a;
  acc = 0.0f;
  count = 0;
  while(i--)
  { float v = d[i];
    if( v<mean )
    { acc += v;
      count ++;
    }
  }
  lm = acc / (float) count;

  return (int) lm;
}

int JaneliaTracker::outofbounds(int q, int cwidth, int cheight)
{ int x = q%cwidth;
  int y = q/cwidth;
  return (x < 1 || x >= cwidth-1 ||y < 1 || y >= cheight-1);
}

void JaneliaTracker::compute_dxdy( Line_Params *line, float *dx, float *dy )
{float ex,ey;
  ex  = cos(line->angle + M_PI/2);  // unit vector normal to line
  ey  = sin(line->angle + M_PI/2);
  *dx = ex * line->offset; // current position
  *dy = ey * line->offset;
}

Seed* JaneliaTracker::compute_seed_from_point( Image<uint8_t>& image, int p, int maxr )
{ float m, stat;
  return compute_seed_from_point_ex( image, p, maxr, &m, &stat);
}
