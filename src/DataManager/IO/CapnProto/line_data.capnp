
@0xbebb4833460d7148;  # Unique file ID

struct Point {
  x @0 :Float32;
  y @1 :Float32;
}

struct Line {
  points @0 :List(Point);
}

struct TimeLine {
  time @0 :Int32;
  lines @1 :List(Line);
}

struct LineDataProto {
  timeLines @0 :List(TimeLine);
  imageWidth @1 :UInt32 = 0;
  imageHeight @2 :UInt32 = 0;
}