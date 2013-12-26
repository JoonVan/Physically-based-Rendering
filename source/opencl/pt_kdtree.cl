/**
 * Check all faces of a leaf node for intersections with the given ray.
 * @param {const int}             nodeIndex
 * @param {const int}             faceIndex
 * @param {const float4*}         origin
 * @param {const float4*}         dir
 * @param {const global float*}   scVertices
 * @param {const global uint*}    scFaces
 * @param {const global int*}     kdNodeFaces
 * @param {const float}           entryDistance
 * @param {float*}                exitDistance
 * @param {hit_t*}                result
 */
void checkFaces(
	const int nodeIndex, const int faceIndex, const float4 origin, const float4 dir,
	const int numFaces, const global float* kdNodeFaces,
	const float entryDistance, float* exitDistance, const float boxExitLimit, hit_t* result
) {
	float4 a = (float4)( 0.0f );
	float4 b = (float4)( 0.0f );
	float4 c = (float4)( 0.0f );
	float4 prefetch_a = (float4)( 0.0f );
	float4 prefetch_b = (float4)( 0.0f );
	float4 prefetch_c = (float4)( 0.0f );
	float r;

	uint j = faceIndex;

	prefetch_a.x = kdNodeFaces[j];
	prefetch_a.y = kdNodeFaces[j + 1];
	prefetch_a.z = kdNodeFaces[j + 2];
	prefetch_b.x = kdNodeFaces[j + 3];
	prefetch_b.y = kdNodeFaces[j + 4];
	prefetch_b.z = kdNodeFaces[j + 5];
	prefetch_c.x = kdNodeFaces[j + 6];
	prefetch_c.y = kdNodeFaces[j + 7];
	prefetch_c.z = kdNodeFaces[j + 8];

	for( uint i = 0; i < numFaces; i++ ) {
		j = faceIndex + i * 10 + 10;
		a = prefetch_a;
		b = prefetch_b;
		c = prefetch_c;

		prefetch_a.x = kdNodeFaces[j];
		prefetch_a.y = kdNodeFaces[j + 1];
		prefetch_a.z = kdNodeFaces[j + 2];
		prefetch_b.x = kdNodeFaces[j + 3];
		prefetch_b.y = kdNodeFaces[j + 4];
		prefetch_b.z = kdNodeFaces[j + 5];
		prefetch_c.x = kdNodeFaces[j + 6];
		prefetch_c.y = kdNodeFaces[j + 7];
		prefetch_c.z = kdNodeFaces[j + 8];

		r = checkFaceIntersection(
			origin, dir, a, b, c,
			entryDistance, fmin( *exitDistance, boxExitLimit )
		);

		if( r > -1.0f ) {
			*exitDistance = r;

			if( result->t > r || result->t < -1.0f ) {
				result->t = r;
				result->nodeIndex = nodeIndex;
				result->faceIndex = (uint) kdNodeFaces[j - 1];
			}
		}
	}

	result->position = ( result->t > -1.0f )
	                 ? fma( result->t, dir, origin )
	                 : result->position;
}


/**
 * Check all faces of a leaf node for intersections with the given ray to test if it can reach a light source.
 * If a hit is found between origin and light source, the function returns immediately.
 * @param  {const int}             nodeIndex
 * @param  {const int}             faceIndex
 * @param  {const float4*}         origin
 * @param  {const float4*}         dir
 * @param  {const global float*} scVertices
 * @param  {const global uint*}  scFaces
 * @param  {const global int*}   kdNodeFaces
 * @param  {const float}           entryDistance
 * @param  {float*}                exitDistance
 * @return {bool}                                True, if a hit is detected between origin and light, false otherwise.
 */
bool checkFacesForShadow(
	const int nodeIndex, const int faceIndex, const float4* origin, const float4* dir,
	const int numFaces, const global float* kdNodeFaces,
	const float entryDistance, float* exitDistance
) {
	float4 a = (float4)( 0.0f );
	float4 b = (float4)( 0.0f );
	float4 c = (float4)( 0.0f );
	float4 prefetch_a = (float4)( 0.0f );
	float4 prefetch_b = (float4)( 0.0f );
	float4 prefetch_c = (float4)( 0.0f );
	float r;

	uint j = faceIndex;

	prefetch_a.x = kdNodeFaces[j];
	prefetch_a.y = kdNodeFaces[j + 1];
	prefetch_a.z = kdNodeFaces[j + 2];
	prefetch_b.x = kdNodeFaces[j + 3];
	prefetch_b.y = kdNodeFaces[j + 4];
	prefetch_b.z = kdNodeFaces[j + 5];
	prefetch_c.x = kdNodeFaces[j + 6];
	prefetch_c.y = kdNodeFaces[j + 7];
	prefetch_c.z = kdNodeFaces[j + 8];

	for( uint i = 0; i < numFaces; i++ ) {
		j = faceIndex + i * 10 + 10;
		a = prefetch_a;
		b = prefetch_b;
		c = prefetch_c;

		prefetch_a.x = kdNodeFaces[j];
		prefetch_a.y = kdNodeFaces[j + 1];
		prefetch_a.z = kdNodeFaces[j + 2];
		prefetch_b.x = kdNodeFaces[j + 3];
		prefetch_b.y = kdNodeFaces[j + 4];
		prefetch_b.z = kdNodeFaces[j + 5];
		prefetch_c.x = kdNodeFaces[j + 6];
		prefetch_c.y = kdNodeFaces[j + 7];
		prefetch_c.z = kdNodeFaces[j + 8];

		r = checkFaceIntersection(
			*origin, *dir, a, b, c, entryDistance, *exitDistance
		);

		if( r > -1.0f ) {
			*exitDistance = r;

			if( r <= 1.0f ) {
				return true;
			}
		}
	}

	return false;
}


/**
 * Traverse down the kd-tree to find a leaf node the given ray intersects.
 * @param  {int}                     nodeIndex
 * @param  {const global kdNonLeaf*} kdNonLeaves
 * @param  {const float4}            hitNear
 * @return {int}
 */
int goToLeafNode( int nodeIndex, const global kdNonLeaf* kdNonLeaves, const float4 hitNear ) {
	float4 split = kdNonLeaves[nodeIndex].split;
	int4 children = kdNonLeaves[nodeIndex].children;
	int axis = split.w;
	const float hitPos[3] = { hitNear.x, hitNear.y, hitNear.z };

	while( true ) {
		float splitPos[3] = { split.x, split.y, split.z };
		nodeIndex = ( hitPos[axis] < splitPos[axis] ) ? children.x : children.y;

		if(
			( nodeIndex == children.x && children.z == 1 ) ||
			( nodeIndex == children.y && children.w == 1 )
		) {
			return nodeIndex;
		}

		axis = MOD_3[axis + 1];
		children = kdNonLeaves[nodeIndex].children;
		split = kdNonLeaves[nodeIndex].split;
	}

	return -1;
}



/**
 * Find the closest hit of the ray with a surface.
 * Uses stackless kd-tree traversal.
 * @param {const float4*}       origin
 * @param {const float4*}       dir
 * @param {int}                 nodeIndex
 * @param {const global float*} kdNodeSplits
 * @param {const global float*} kdNodeBB
 * @param {const global int*}   kdNodeMeta
 * @param {const global float*} kdNodeFaces
 * @param {const global int*}   kdNodeRopes
 * @param {hit_t*}              result
 * @param {const int}           bounce
 * @param {const int}           kdRoot
 * @param {const float}         entryDistance
 * @param {const float}         exitDistance
 */
void traverseKdTree(
	const float4 origin, const float4 dir, int nodeIndex,
	const global kdNonLeaf* kdNonLeaves,
	const global kdLeaf* kdLeaves, const global float* kdNodeFaces,
	hit_t* result, const int bounce, float entryDistance, float exitDistance
) {
	kdLeaf currentNode;
	int8 ropes;
	int exitRope = 0;
	float boxExitLimit = 0.0f;

	while( nodeIndex >= 0 && entryDistance < exitDistance ) {
		currentNode = kdLeaves[nodeIndex];
		ropes = currentNode.ropes;
		boxExitLimit = getBoxExitLimit( origin, dir, currentNode.bbMin, currentNode.bbMax );

		checkFaces(
			nodeIndex, ropes.s6, origin, dir, ropes.s7, kdNodeFaces,
			entryDistance, &exitDistance, boxExitLimit, result
		);

		// Exit leaf node
		updateEntryDistanceAndExitRope(
			origin, dir, currentNode.bbMin, currentNode.bbMax,
			&entryDistance, &exitRope
		);

		nodeIndex = ( (int*) &ropes )[exitRope];
		nodeIndex = ( nodeIndex < 1 )
		          ? -nodeIndex - 1
		          : goToLeafNode( nodeIndex - 1, kdNonLeaves, fma( entryDistance, dir, origin ) );
	}
}


/**
 * Test if from the current hit location there is an unobstracted direct path to the light source.
 * @param  {const float4*}       origin
 * @param  {const float4*}       dir
 * @param  {int}                 nodeIndex
 * @param  {const global float*} kdNodeSplits
 * @param  {const global float*} kdNodeBB
 * @param  {const global int*}   kdNodeMeta
 * @param  {const global float*} kdNodeFaces
 * @param  {const global int*}   kdNodeRopes
 * @param  {const float}         exitDistance
 * @return {bool}
 */
bool shadowTestIntersection(
	const float4* origin, const float4* dir, int nodeIndex,
	const global kdNonLeaf* kdNonLeaves,
	const global kdLeaf* kdLeaves, const global float* kdNodeFaces
) {
	kdLeaf currentNode;
	int8 ropes;
	int exitRope;
	float entryDistance = 0.0f;
	float exitDistance = 1.0f;

	while( nodeIndex >= 0 && entryDistance < exitDistance ) {
		currentNode = kdLeaves[nodeIndex];
		ropes = currentNode.ropes;

		if( checkFacesForShadow(
			nodeIndex, ropes.s6, origin, dir, ropes.s7, kdNodeFaces,
			entryDistance, &exitDistance
		) ) {
			return true;
		}

		// Exit leaf node
		updateEntryDistanceAndExitRope(
			*origin, *dir, currentNode.bbMin, currentNode.bbMax,
			&entryDistance, &exitRope
		);

		nodeIndex = ( (int*) &ropes )[exitRope];
		nodeIndex = ( nodeIndex < 1 )
		          ? -nodeIndex - 1
		          : goToLeafNode( nodeIndex - 1, kdNonLeaves, fma( entryDistance + EPSILON, *dir, *origin ) );
	}

	return false;
}
