
#include "detector_bank.h"

#include <cmath>
#include <cfloat>

int DetectorBank::compute_number_steps( Range r )
{  return lround( (r.max - r.min) / r.step ) + 1;
}

int DetectorBank::Get_Detector(const int ioffset, const int iwidth, const int iangle  )
{ return iangle  * this->bank.strides_px[1]
                                + iwidth  * this->bank.strides_px[2]
                                + ioffset * this->bank.strides_px[3];
}

LineDetector::LineDetector() {

}

LineDetector::LineDetector(JaneliaConfig config) {

    this->off = Range { -1.0,1.0,config._offset_step};
    this->ang = Range { -M_PI/4.0,M_PI/4.0, M_PI/4.0/config._angle_step};
    this->wid = Range { config._width_min,config._width_max, config._width_step  };
    Build_Line_Detectors(config._tlen, 2*config._tlen+3 );
}

HalfSpaceDetector::HalfSpaceDetector() {

}

HalfSpaceDetector::HalfSpaceDetector(JaneliaConfig config) {
    this->norm = -1;
    this->off = Range { -1.0,1.0,config._offset_step};
    this->ang = Range { -M_PI/4.0,  M_PI/4.0, M_PI/4.0/config._angle_step };
    this->wid = Range {config._width_min, config._width_max, config._width_step};
    Build_Half_Space_Detectors(config._tlen,2*config._tlen+3);

    { float *m = this->bank.data.data() + Get_Detector(0,0,0);
      int n = (2*config._tlen+3)*(2*config._tlen+3);
      while(n--)
        this->norm += m[n];
    }
}


void LineDetector::Build_Line_Detectors(float length,int supportsize ) {

     int noff =  compute_number_steps( this->off ),
         nwid =  compute_number_steps( this->wid ),
         nang =  compute_number_steps( this->ang );
     std::array<int,5> shape = { supportsize,
                      supportsize,
                      noff,
                      nwid,
                      nang};
      this->bank = Array(shape, sizeof(float)); // This array is always size 5 for the number of dimensions

     { int    o,a,w;
       for( o = 0; o < noff; o++ )
       { //point anchor = {supportsize/2.0, o*off.step + off.min + supportsize/2.0};
         point anchor = {supportsize/2.0f, supportsize/2.0f};
         for( a = 0; a < nang; a++ )
           for( w = 0; w < nwid; w++ )
           {
               float *bank_i = this->bank.data.data() + Get_Detector(o,w,a);
             Render_Line_Detector(
                 o*off.step + off.min,                       //offset (before rotation)
                 length,                                     //length,
                 a*ang.step + ang.min,                       //angle,
                 w*wid.step + wid.min,                       //width,
                 anchor,                                     //anchor,
                 bank_i,            //image
                 this->bank.strides_px.data() + 3);                      //strides
           }
       }
     }
}

void LineDetector::Render_Line_Detector( float offset,
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
{ std::array<point,4> prim ={};
  const float thick = 0.7;
  const float r = 1.0;
  //const float area = 4*thick*length;
  //length -=2;

  { point off = { 0.0, offset + width/2.0f + r*thick/2.0f   };
    Simple_Line_Primitive(prim, off, length, r*thick   );
    rotate( prim, angle );
    translate( prim, anchor );
    Sum_Pixel_Overlap( prim, -1.0/r, image, strides );
  }
  { point off = { 0.0, offset + width/2.0f - thick/2.0f };
    Simple_Line_Primitive(prim, off, length/r, thick );
    rotate( prim, angle );
    translate( prim, anchor );
    Sum_Pixel_Overlap( prim, r, image, strides );
  }
  { point off = { 0.0, offset - width/2.0f + thick/2.0f };
    Simple_Line_Primitive(prim, off, length/r, thick );
    rotate( prim, angle );
    translate( prim, anchor );
    Sum_Pixel_Overlap( prim, r, image, strides );
  }
  { point off = { 0.0, offset - width/2.0f - r*thick/2.0f  };
    Simple_Line_Primitive(prim, off, length, r*thick   );
    rotate( prim, angle );
    translate( prim, anchor );
    Sum_Pixel_Overlap(prim,-1.0f/r, image, strides );
  }
  return;
}

void HalfSpaceDetector::Build_Half_Space_Detectors(float length,int supportsize )
{
  int noff =  compute_number_steps(this->off ),
      nwid =  compute_number_steps(this->wid ),
      nang =  compute_number_steps(this->ang );
  std::array<int,5> shape = { supportsize,
                   supportsize,
                   noff,
                   nwid,
                   nang};
  this->bank = Array(shape, sizeof(float));

  { int    o,a,w;
    for( o = 0; o < noff; o++ )
    { //point anchor = {supportsize/2.0, o*off.step + off.min + supportsize/2.0};
      point anchor = {supportsize/2.0f, supportsize/2.0f};
      for( a = 0; a < nang; a++ )
        for( w = 0; w < nwid; w++ )
        {
            float *bank_i = this->bank.data.data() + Get_Detector(o,w,a);
          Render_Half_Space_Detector(
              o*off.step + off.min,                       //offset (before rotation)
              length,                                     //length,
              a*ang.step + ang.min,                       //angle,
              w*wid.step + wid.min,                       //width,
              anchor,                                     //anchor,
              bank_i,      //image
             this->bank.strides_px.data() + 3);                      //strides
        }
    }
  }

}


void HalfSpaceDetector::Render_Half_Space_Detector( float offset,
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

  { std::array<point,4> prim = {};
    point off = { 0.0, offset + thick /*+ width/2.0 */   };
    Simple_Line_Primitive(prim, off, 2*length, thick   );
    rotate( prim, angle );
    translate( prim, anchor );
    Sum_Pixel_Overlap(prim, density, image, strides );
  }

  //{ point prim[4];
  //  point off = { 0.0, offset - thick - width/2.0   };
  //  Simple_Line_Primitive(prim, off, 2*length, thick   );
  //  rotate( prim, 4, angle );
  //  translate( prim, 4, anchor );
  //  Sum_Pixel_Overlap( (float*) prim, 4, -density, image, strides );
  //}

  { std::array<point,12> prim ={};
    point off = { 0.0, offset };
    Simple_Circle_Primitive(prim,off,length, 1);
    rotate( prim, angle );
    translate( prim, anchor );
    Multiply_Pixel_Overlap( prim, density, 0, image, strides );
  }

  return;
}

int DetectorBank::get_nearest(float offset, float width, float angle)
{
   auto is_small_angle = [](const float angle )
     /* true iff angle is in [-pi/4,pi/4) or [3pi/4,5pi/4) */
    { static const float qpi = M_PI/4.0;
     static const float hpi = M_PI/2.0;
     int n = floorf( (angle-qpi)/hpi );
     return  (n % 2) != 0;
    };
  if( !is_small_angle( angle ) )  // if large angle then transpose
  { angle = 3.0*M_PI/2.0 - angle; //   to small ones ( <45deg )
  //offset = -offset;
  }

  // Make sure angle is between 0 and 2 Pi
  while( (angle)    < -M_PI )
    (angle)    +=   2*M_PI;
  while( (angle)    >= M_PI )
    (angle)    -=   2*M_PI;

  auto is_angle_leftward = [](const float angle )
          /* true iff angle is in left half plane */
         { //static const float qpi = M_PI/4.0;
          static const float hpi = M_PI/2.0;
          int n = floorf( (angle-hpi)/M_PI );
          return  (n % 2) == 0;
         };
  //sometimes need to flip the line upside down
  if( is_angle_leftward(angle) )
  {
    //Wrap the angle in the appropriate half plane
    while( (angle)    < M_PI/2.0 )
       (angle)    +=   M_PI;
    while( (angle)    >= M_PI/2.0 )
       (angle)    -=   M_PI;

    offset = -offset;
  }

  int o = lround( ( offset - this->off.min ) / this->off.step );
  int a = lround( (  angle - this->ang.min ) / this->ang.step );
  int w = lround( (  width - this->wid.min ) / this->wid.step );

  return Get_Detector(o, w, a );
}

template <size_t N>
void Simple_Circle_Primitive( std::array<point,N>& verts, point center, float radius, int direction)
{
  float k = direction*2*M_PI/(float)verts.size();
  for (int i = 0; i < verts.size(); i++) {
      point p = { static_cast<float>(center.x + radius * cos( k*i )),
                      static_cast<float>(center.y + radius * sin( k*i ))};
      verts[i] = p;
  }
}

template <size_t N>
void Multiply_Pixel_Overlap( std::array<point,N>& xy, float gain, float boundary, float *grid, int *strides )
/* `xy`      is an array of `n` vertices arranged in (x,y) pairs.
 * `n`       the number of vertices in `xy`.
 * `grid`    should point to the origin pixel in an image buffer in the
 *           channel of interest.
 * `strides` should be an array of size ndim+1 (for 2d, size=3)
 *           with { width*height*channels, width*channels, channels }
 *           For now assume channels == 1.
 */
{ std::array<point,4> pxverts ={};
  int px,ix,iy;

  auto[minx,maxx,miny,maxy] = f32_min_max(xy,strides);

  // multiply by overlaps
  for( ix = minx; ix <= maxx; ix++ )
  { for( iy = miny; iy <= maxy; iy ++ )
    { px = iy*strides[1] + ix;
      pixel_to_vertex_array( px, strides[1], pxverts );
      *(grid+px) *= gain * inter( xy, pxverts);
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

template <size_t N>
void Simple_Line_Primitive( std::array<point,N>& verts, const point offset, const float length, const float thick )
{
 verts[0] = { offset.x - length,  offset.y - thick};
 verts[1] = { offset.x + length,  offset.y - thick};
 verts[2] = { offset.x + length,  offset.y + thick};
 verts[3] = { offset.x - length,  offset.y + thick};
}

template <size_t N>
void rotate( std::array<point,N>& pbuf, const float angle)
  /* positive angle rotates counter clockwise */
{ float s = sin(angle),
        c = cos(angle);
    for (auto& p : pbuf) {
        float x = p.x;
        float y = p.y;
        p.x =  x*c - y*s;
        p.y =  x*s + y*c;
    }
}

template <size_t N>
void translate( std::array<point,N>& pbuf, const point ori)
{
    for (auto& p : pbuf) {
        p.x =  p.x + ori.x;
        p.y =  p.y + ori.y;
    }
}

template <size_t N>
void Sum_Pixel_Overlap(std::array<point,N>& xy, const float gain, float *grid, int *strides )
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
  std::array<point,4> pxverts ={};
  int px,ix,iy;

  auto[minx,maxx,miny,maxy] = f32_min_max(xy,strides);

  for( ix = minx; ix <= maxx; ix++ )
  { for( iy = miny; iy <= maxy; iy ++ )
    { px = iy*strides[1] + ix;
      pixel_to_vertex_array( px, strides[1], pxverts );
      *(grid+px) += gain * inter(xy, pxverts);
    }
  }
  return;
}

void pixel_to_vertex_array(const int p, const int stride, std::array<point,4>& v)
{ float x = p%stride;
 float y = p/stride;
 v[0] = {x,y};
 v[1] = {x+1.0f, y};
 v[2] = {x+1.0f, y+1.0f};
 v[3] = {x, y+1.0f};
 return;
}

template <size_t N>
std::array<int,4> f32_min_max(std::array<point,N>& points,int * bound) {
    float miny = FLT_MAX, minx = FLT_MAX, maxy=0.0f, maxx = 0.0f;

    for (auto p : points) {
        if (p.x < minx) {
            minx = p.x;
        }
        if (p.x > maxx) {
            maxx = p.x;
        }
        if (p.y < miny) {
            miny = p.y;
        }
        if (p.y > maxy) {
            maxy = p.y;
        }
    }

    return std::array<int,4>{std::max((int)minx,0),
                std::min((int)maxx,bound[1]-1),
                std::max((int)miny,0),
                std::min((int)maxy,bound[0]/bound[1]-1)};
}

template <size_t N>
float inter(std::array<point, N>& a, std::array<point,4>& b)
{ //vertex ipa[na+1], ipb[nb+1];
  //vertex *ipa,*ipb;
  box B = {{bigReal, bigReal}, {-bigReal, -bigReal}};
  double ascale;

  std::array<vertex,N+1> ipa = {};
  std::array<vertex,5> ipb = {};

  range(B, a);
  range(B, b);

  ascale = fit(B, a,ipa, 0);
  ascale = fit(B, b,ipb, 2);

  { long long s = 0;
    int j, k;

    auto ovl = [](const rng p, const rng q)
    /* True if intervals intersect */
    { return p.mn < q.mx && q.mn < p.mx;
    };

    /*
     * Look for crossings, add contributions from crossings and track winding
     * */
    for(j=0; j<a.size(); ++j)
      for(k=0; k<b.size(); ++k)
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
              { if(o) s+=cross(ipa[j], ipa[j+1], ipb[k], ipb[k+1], a1, a2, a3, a4);
                else  s+=cross(ipb[k], ipb[k+1], ipa[j], ipa[j+1], a3, a4, a1, a2);
              }
            }
          }
        }
    /* Add contributions from non-crossing edges */
    s += inness(ipa, ipb);
    s += inness(ipb, ipa);
    return s/ascale;
  }
}

template <size_t N>
void range(box& B, std::array<point,N>& x)
{
  auto bdr = [](float * X, const float y)
  { *X = *X<y ? *X:y;
  };
  auto bur = [](float * X, const float y)
  { *X = *X>y ? *X:y;
  };
  for (auto xx : x)
 { bdr(&B.min.x, xx.x); bur(&B.max.x, xx.x);
   bdr(&B.min.y, xx.y); bur(&B.max.y, xx.y);
 }
}

long long cntrib(const ipoint f, const ipoint t, const short w)
/* Integrand for the line integral.  Google `Green's theorem polygon area` for
* functional form.
*/
{ return (long long)w*(t.x-f.x)*(t.y+f.y)/2;}

long long area(const ipoint a, const ipoint p, const ipoint q)
/* Compute the area of the triangle (apq) */
{ return (long long)p.x*q.y - (long long)p.y*q.x +
   (long long)a.x*(p.y - q.y) + (long long)a.y*(q.x - p.x);
}

long long cross(vertex & a, vertex & b, vertex & c, vertex & d,
    const double a1, const double a2, const double a3, const double a4)
{ /* Interpolate to intersection and add contributions from each half edge */
  float r1=a1/((float)a1+a2), r2 = a3/((float)a3+a4);

  long long s = 0;
  { ipoint p = {  static_cast<long>(a.ip.x + r1*(b.ip.x-a.ip.x)),
                  static_cast<long>(a.ip.y + r1*(b.ip.y-a.ip.y))};
    s += cntrib(p, b.ip, 1 );
  }
  { ipoint p = {
      static_cast<long>(c.ip.x + r2*(d.ip.x - c.ip.x)),
      static_cast<long>(c.ip.y + r2*(d.ip.y - c.ip.y))};
    s += cntrib(d.ip, p, 1 );
  }
  ++a.in; /* Track winding numbers...these show up later in `inness` */
  --c.in;

  return s;
}

template <size_t M, size_t N>
long long inness(std::array<vertex,M>& P, std::array<vertex,N>& Q)
{ int s=0;
  ipoint p = P[0].ip;
  int j;

  int c = N-1;
  while(c--) //Compute winding of P[0] wrt Q
    if(Q[c].rx.mn < p.x && p.x < Q[c].rx.mx) //Bounds check x-interval only
    {  //use area to determine P[0] left of Q[c] edge
      int sgn = 0 < area(p, Q[c].ip, Q[c+1].ip);   // 0 or 1. 1 if left of Q[c] and Q[c] moves right
      // only count cw and moving right or ccw and moving left
      s += sgn != Q[c].ip.x < Q[c+1].ip.x ? 0 : (sgn?-1:1); //add winding
    }
  long long sarea = 0;
  for(j=0; j<(M-1); ++j)
  { if(s)
      sarea += cntrib(P[j].ip, P[j+1].ip, s);
    s += P[j].in;
  }
  return sarea;
}

template <size_t N, size_t M>
double fit(box& B, std::array<point,N>& x, std::array<vertex,M>& ix, int fudge)
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
  float rngx = B.max.x - B.min.x, sclx = gamut/rngx,
        rngy = B.max.y - B.min.y, scly = gamut/rngy;
  int c=x.size();
  while(c--)
  { ix[c].ip.x = (long)((x[c].x - B.min.x)*sclx - mid)&~7|fudge|c&1;
    ix[c].ip.y = (long)((x[c].y - B.min.y)*scly - mid)&~7|fudge;
  }
  ix[0].ip.y += x.size()&1;
  ix[x.size()] = ix[0];

  c=x.size();
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
