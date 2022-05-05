
#include "detector_bank.h"

#include "qmath.h"

#define _USE_MATH_DEFINES

#include <cmath>
#include <cfloat>

int DetectorBank::compute_number_steps( Range r )
{  return lround( (r.max - r.min) / r.step ) + 1;
}

int DetectorBank::Get_Detector(int ioffset, int iwidth, int iangle  )
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
  WRAP_ANGLE_2PI( angle );

  auto is_angle_leftward = [](const float angle )
          /* true iff angle is in left half plane */
         { //static const float qpi = M_PI/4.0;
          static const float hpi = M_PI/2.0;
          int n = floorf( (angle-hpi)/M_PI );
          return  (n % 2) == 0;
         };
  //sometimes need to flip the line upside down
  if( is_angle_leftward(angle) )
  { WRAP_ANGLE_HALF_PLANE( angle );
    offset = -offset;
  }

  int o = lround( ( offset - this->off.min ) / this->off.step );
  int a = lround( (  angle - this->ang.min ) / this->ang.step );
  int w = lround( (  width - this->wid.min ) / this->wid.step );

  return Get_Detector(o, w, a );
}

void Simple_Circle_Primitive( point *verts, int npoints, point center, float radius, int direction)
{ int i = npoints;
  float k = direction*2*M_PI/(float)npoints;
  while(i--)
  { point p = { static_cast<float>(center.x + radius * cos( k*i )),
                static_cast<float>(center.y + radius * sin( k*i ))};
    verts[i] = p;
  }
}

void Multiply_Pixel_Overlap( float *xy, int n, float gain, float boundary, float *grid, int *strides )
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

void Simple_Line_Primitive( point *verts, point offset, float length, float thick )
{ point v0 = { offset.x - length,  offset.y - thick},
       v1 = { offset.x + length,  offset.y - thick},
       v2 = { offset.x + length,  offset.y + thick},
       v3 = { offset.x - length,  offset.y + thick};
 verts[0] = v0;
 verts[1] = v1;
 verts[2] = v2;
 verts[3] = v3;
}

void rotate( point *pbuf, int n, float angle)
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

void translate( point* pbuf, int n, point ori)
{ point *p = pbuf + n;
  while( p-- > pbuf )
  { p->x =  p->x + ori.x;
    p->y =  p->y + ori.y;
  }
  return;
}

void Sum_Pixel_Overlap( float *xy, int n, float gain, float *grid, int *strides )
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

void pixel_to_vertex_array(int p, int stride, float *v)
{ float x = p%stride;
 float y = p/stride;
 v[0] = x;      v[1] = y;
 v[2] = x+1.0f; v[3] = y;
 v[4] = x+1.0f; v[5] = y+1.0f;
 v[6] = x;      v[7] = y+1.0f;
 return;
}

unsigned array_max_f32u ( float *buf, int size, int step, float bound )
{ float *t = buf + size;
 float r = 0.0f;
 while( (t -= step) >= buf )
   r = std::max( r, ceilf(*t) );
 return (unsigned) std::min(r,bound);
}

unsigned array_min_f32u ( float *buf, int size, int step, float bound )
{ float *t = buf + size;
 float r = FLT_MAX;
 while( (t -= step) >= buf )
   r = std::min( r, floorf(*t) );
 return (unsigned) std::max(r,bound);
}

float inter(point * a, int na, point * b, int nb)
{ //vertex ipa[na+1], ipb[nb+1];
  vertex *ipa,*ipb;
  box B = {{bigReal, bigReal}, {-bigReal, -bigReal}};
  double ascale;

  if(na < 3 || nb < 3) return 0;
  ipa = (vertex*) malloc( sizeof(vertex) * (na+1) );
  ipb = (vertex*) malloc( sizeof(vertex) * (nb+1) );

  range(B, a, na);
  range(B, b, nb);

  ascale = fit(B, a, na, ipa, 0);
  ascale = fit(B, b, nb, ipb, 2);

  { long long s = 0;
    int j, k;

    auto ovl = [](const rng p, const rng q)
    /* True if intervals intersect */
    { return p.mn < q.mx && q.mn < p.mx;
    };

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

void range(box& B, point * x, int c)
{
  auto bdr = [](float * X, float y)
  { *X = *X<y ? *X:y;
  };
  auto bur = [](float * X, float y)
  { *X = *X>y ? *X:y;
  };
    while(c--)
 { bdr(&B.min.x, x[c].x); bur(&B.max.x, x[c].x);
   bdr(&B.min.y, x[c].y); bur(&B.max.y, x[c].y);
 }
}

void cntrib(long long *s, ipoint f, ipoint t, short w)
/* Integrand for the line integral.  Google `Green's theorem polygon area` for
* functional form.
*/
{ *s += (long long)w*(t.x-f.x)*(t.y+f.y)/2;
}

long long area(const ipoint a, const ipoint p, const ipoint q)
/* Compute the area of the triangle (apq) */
{ return (long long)p.x*q.y - (long long)p.y*q.x +
   (long long)a.x*(p.y - q.y) + (long long)a.y*(q.x - p.x);
}

void cross(long long *s, vertex * a, vertex * b, vertex * c, vertex * d,
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

void inness(long long *sarea, vertex * P, int cP, vertex * Q, int cQ)
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

double fit(box& B, point * x, int cx, vertex * ix, int fudge)
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
  int c=cx;
  while(c--)
  { ix[c].ip.x = (long)((x[c].x - B.min.x)*sclx - mid)&~7|fudge|c&1;
    ix[c].ip.y = (long)((x[c].y - B.min.y)*scly - mid)&~7|fudge;
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
