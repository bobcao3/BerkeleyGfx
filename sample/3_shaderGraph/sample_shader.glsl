#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 UV;

layout(binding = 0) uniform BuiltinConstants
{
  vec4 iMouse;
  vec3 iResolution;
  float iTime;
  float iTimeDelta;
  float iFrame;
};

// From shadertoy:

// The MIT License
// Copyright © 2015 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// Signed distance to a 2D cross. Produces exact interior distance,
// which doing the union of two rectangle SDFs doesn't. Also, it's cheaper,
// it's a single square root instead of two. Win Win!

// List of some other 2D distances: https://www.shadertoy.com/playlist/MXdSRf
//
// and www.iquilezles.org/www/articles/distfunctions2d/distfunctions2d.htm


float sdCross( in vec2 p, in vec2 b, float r ) 
{
  p = abs(p); p = (p.y>p.x) ? p.yx : p.xy;
    
	vec2  q = p - b;
  float k = max(q.y,q.x);
  vec2  w = (k>0.0) ? q : vec2(b.y-p.x,-k);
    
  return sign(k)*length(max(w,0.0)) + r;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	vec2 p = (2.0*fragCoord.xy-iResolution.xy)/iResolution.y;

  // size
	vec2 si = 0.5 + 0.3*cos( iTime + vec2(0.0,1.57) ); if( si.x<si.y ) si=si.yx;
    
  // corner radious
  float ra = 0.1*sin(iTime*1.2);
    
	float d = sdCross( p, si, ra );
    
  vec3 col = vec3(1.0) - sign(d)*vec3(0.1,0.4,0.7);
	col *= 1.0 - exp(-3.0*abs(d));
	col *= 0.8 + 0.2*cos(150.0*d);
	col = mix( col, vec3(1.0), 1.0-smoothstep(0.0,0.015,abs(d)) );

	fragColor = vec4(col,1.0);
}

// End code from shadertoy

void main()
{
  mainImage(outColor, gl_FragCoord.st);
}