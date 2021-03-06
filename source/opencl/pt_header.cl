#define ACCEL_STRUCT #ACCEL_STRUCT#
#define ANTI_ALIASING #ANTI_ALIASING#
#define BRDF #BRDF#
#define BVH_NUM_NODES #BVH_NUM_NODES#
#define BVH_TEX_DIM #BVH_TEX_DIM#
#define EPSILON5 0.00001f
#define EPSILON7 0.0000001f
#define EPSILON10 0.0000000001f
#define IMG_HEIGHT #IMG_HEIGHT#
#define IMG_WIDTH #IMG_WIDTH#
#define MAX_ADDED_DEPTH #MAX_ADDED_DEPTH#
#define MAX_DEPTH #MAX_DEPTH#
#define NI_AIR 1.00028f
#define NUM_LIGHTS #NUM_LIGHTS#
#define PHONGTESS #PHONGTESS#
#define PHONGTESS_ALPHA #PHONGTESS_ALPHA#
#define PI_X2 6.28318530718f
#define SAMPLES #SAMPLES#
#define SHADOW_RAYS #SHADOW_RAYS#
#define SKY_LIGHT #SKY_LIGHT#


// Only used inside kernel.
typedef struct {
	float3 origin;
	float3 dir;
	float3 normal;
	float t;
	int hitFace;
} ray4;

// Only used inside kernel.
typedef struct {
	float3 n1; // Normal of plane 1
	float3 n2; // Normal of plane 2
	float o1;  // Distance of plane 1 to the origin
	float o2;  // Distance of plane 2 to the origin
} rayPlanes;

// Passed from outside.
typedef struct {
	float3 eye;
	float3 w;
	float3 u;
	float3 v;
	int2 focusPoint;
	float2 lense; // x: focal length; y: aperture
} camera;

typedef struct {
	uint4 vertices; // w: material
	uint4 normals;
} face_t;

typedef struct {
	float4 pos;
	float4 rgb;
	float4 data; // x: type
} light_t;


// BVH
#if ACCEL_STRUCT == 0

	typedef struct {
		float4 bbMin; // w: face index
		float4 bbMax; // w: face index or next node to visit
	} bvhNode;

	typedef struct {
		global const bvhNode* bvh;
		global const light_t* lights;
		global const uint4* facesV;
		global const uint4* facesN;
		global const float4* vertices;
		global const float4* normals;
		float4 debugColor;
	} Scene;

#endif


// Schlick
#if BRDF == 0

	typedef struct {
		float4 data;
		// data.s0: d
		// data.s1: Ni
		// data.s2: p
		// data.s3: rough
		float4 rgbDiff;
		float4 rgbSpec;
	} material;

// Shirley-Ashikhmin
#elif BRDF == 1

	typedef struct {
		float8 data;
		// data.s0: d
		// data.s1: Ni
		// data.s2: nu
		// data.s3: nv
		// data.s4: Rs
		// data.s5: Rd
		float4 rgbDiff;
		float4 rgbSpec;
	} material;

#endif
