#include "GLWidget.h"

using namespace std;


/**
 * Constructor.
 * @param {QWidget*} parent Parent QWidget this QWidget is contained in.
 */
GLWidget::GLWidget( QWidget* parent ) : QGLWidget( parent ) {
	srand( (unsigned) time( 0 ) );

	mCL = new CL();
	this->initTargetTexture();
	mCL->loadProgram( Cfg::get().value<string>( Cfg::OPENCL_PROGRAM ) );
	mCL->createKernel( "pathTracing" );

	mModelMatrix = glm::mat4( 1.0f );
	mProjectionMatrix = glm::perspective(
		Cfg::get().value<float>( Cfg::PERS_FOV ),
		Cfg::get().value<float>( Cfg::WINDOW_WIDTH ) / Cfg::get().value<float>( Cfg::WINDOW_HEIGHT ),
		Cfg::get().value<float>( Cfg::PERS_ZNEAR ),
		Cfg::get().value<float>( Cfg::PERS_ZFAR )
	);

	mDoRendering = false;
	mFrameCount = 0;
	mPreviousTime = 0;
	mCamera = new Camera( this );
	mSampleCount = 0;
	mSelectedLight = -1;

	// mTimer = new QTimer( this );
	// connect( mTimer, SIGNAL( timeout() ), this, SLOT( update() ) );

	mTimeSinceStart = boost::posix_time::microsec_clock::local_time();

	this->startRendering();
}


/**
 * Destructor.
 */
GLWidget::~GLWidget() {
	this->stopRendering();
}


/**
 * Calculate the matrices for view, model, model-view-projection and normals.
 */
void GLWidget::calculateMatrices() {
	if( !mDoRendering ) {
		return;
	}

	mSampleCount = 0;

	glm::vec3 e = mCamera->getEye_glmVec3();
	glm::vec3 c = mCamera->getCenter_glmVec3();
	glm::vec3 u = mCamera->getUp_glmVec3();
	// printf( "e: %f, %f, %f\n", e[0], e[1], e[2] );
	// printf( "c: %f, %f, %f\n", c[0], c[1], c[2] );
	// printf( "u: %f, %f, %f\n", u[0], u[1], u[2] );

	mViewMatrix = glm::lookAt( e, e - c, u );


	// glm::vec3 z = glm::normalize( e - c );
	// glm::vec3 x = glm::normalize( glm::cross( u, z ) );
	// glm::vec3 y = glm::normalize( glm::cross( z, x ) );

	// glm::mat4 m(
	// 	x[0], x[1], x[2], 0.0f,
	// 	y[0], y[1], y[2], 0.0f,
	// 	z[0], z[1], z[2], 0.0f,
	// 	0.0f, 0.0f, 0.0f, 1.0f
	// );

	// glm::mat4 t(
	// 	1.0f, 0.0f, 0.0f, -e[0],
	// 	0.0f, 1.0f, 0.0f, -e[1],
	// 	0.0f, 0.0f, 1.0f, -e[2],
	// 	0.0f, 0.0f, 0.0f, 1.0f
	// );

	// mViewMatrix = m * t;


	// glm::mat4 v = mViewMatrix;
	// printf( "v: %f, %f, %f, %f\n", v[0][0], v[0][1], v[0][2], v[0][3] );
	// printf( "v: %f, %f, %f, %f\n", v[1][0], v[1][1], v[1][2], v[1][3] );
	// printf( "v: %f, %f, %f, %f\n", v[2][0], v[2][1], v[2][2], v[2][3] );
	// printf( "v: %f, %f, %f, %f\n", v[3][0], v[3][1], v[3][2], v[3][3] );

	mModelViewProjectionMatrix = mProjectionMatrix * mViewMatrix * mModelMatrix;

	// glm::mat4 v = mModelViewProjectionMatrix;
	// printf( "v0: %f, %f, %f, %f\n", v[0][0], v[0][1], v[0][2], v[0][3] );
	// printf( "v1: %f, %f, %f, %f\n", v[1][0], v[1][1], v[1][2], v[1][3] );
	// printf( "v2: %f, %f, %f, %f\n", v[2][0], v[2][1], v[2][2], v[2][3] );
	// printf( "v3: %f, %f, %f, %f\n", v[3][0], v[3][1], v[3][2], v[3][3] );
}


/**
 * The camera has changed. Handle it.
 */
void GLWidget::cameraUpdate() {
	this->calculateMatrices();
	this->update(); // TODO: remove when timer in use again
}


/**
 * Delete data (buffers, textures) of the old model.
 */
void GLWidget::deleteOldModel() {
	// Delete old vertex array buffers
	if( mVA.size() > 0 ) {
		glDeleteBuffers( mVA.size(), &mVA[0] );
		glDeleteBuffers( 1, &mIndexBuffer );

		map<GLuint, GLuint>::iterator texIter = mTextureIDs.begin();
		while( texIter != mTextureIDs.end() ) {
			glDeleteTextures( 1, &((*texIter).second) );
			texIter++;
		}
	}
}


glm::vec3 GLWidget::getEyeRay( glm::mat4 matrix, glm::vec3 eye, float x, float y ) {
	glm::vec4 tmp = matrix * glm::vec4( x, y, 0.0f, 1.0f );
	glm::vec3 result( tmp[0] / tmp[3], tmp[1] / tmp[3], tmp[2] / tmp[3] );

	return glm::normalize( result - eye );
}


glm::mat4 GLWidget::getJitterMatrix( glm::vec3 v ) {
	glm::mat4 jitter = glm::mat4( 1.0f );

	jitter[0][3] = v[0];
	jitter[1][3] = v[1];
	jitter[2][3] = v[2];

	return glm::inverse( jitter * mModelViewProjectionMatrix );
}


/**
 * Initialize OpenGL and start rendering.
 */
void GLWidget::initializeGL() {
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );

	glEnable( GL_DEPTH_TEST );
	glEnable( GL_MULTISAMPLE );

	glEnable( GL_ALPHA_TEST );
	glAlphaFunc( GL_ALWAYS, 0.0f );

	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	this->initGlew();
	// this->initTargetTexture();

	GLfloat vertices[8] = {
		-1.0f, -1.0f,
		-1.0f, +1.0f,
		+1.0f, -1.0f,
		+1.0f, +1.0f
	};

	glGenBuffers( 1, &mVertexBuffer );
	glBindBuffer( GL_ARRAY_BUFFER, mVertexBuffer );
	glBufferData( GL_ARRAY_BUFFER, sizeof( vertices ), &vertices, GL_STATIC_DRAW );

	// glGenFramebuffers( 1, &mFramebuffer );
	// glBindFramebuffer( GL_FRAMEBUFFER, mFramebuffer );

	Logger::logInfo( string( "[OpenGL] Version " ).append( (char*) glGetString( GL_VERSION ) ) );
	Logger::logInfo( string( "[OpenGL] GLSL " ).append( (char*) glGetString( GL_SHADING_LANGUAGE_VERSION ) ) );
}


void GLWidget::initGlew() {
	GLenum err = glewInit();

	if( err != GLEW_OK ) {
		Logger::logError( string( "[GLEW] Init failed: " ).append( (char*) glewGetErrorString( err ) ) );
		exit( 1 );
	}
	Logger::logInfo( string( "[GLEW] Version " ).append( (char*) glewGetString( GLEW_VERSION ) ) );
}


/**
 * Load and compile the shader.
 */
void GLWidget::initShaders() {
	string shaderPath = Cfg::get().value<string>( Cfg::SHADER_PATH );
	shaderPath.append( Cfg::get().value<string>( Cfg::SHADER_NAME ) );

	glDeleteProgram( mGLProgramTracer );

	mGLProgramTracer = glCreateProgram();
	mShaderVert = glCreateShader( GL_VERTEX_SHADER );
	mShaderFrag = glCreateShader( GL_FRAGMENT_SHADER );

	this->loadShader( mShaderVert, shaderPath + string( ".vert" ) );
	this->loadShader( mShaderFrag, shaderPath +string( ".frag" ) );

	glLinkProgram( mGLProgramTracer );
	glUseProgram( mGLProgramTracer );

	glDetachShader( mGLProgramTracer, mShaderVert );
	glDeleteShader( mShaderVert );

	glDetachShader( mGLProgramTracer, mShaderFrag );
	glDeleteShader( mShaderFrag );

	mVertexAttribute = glGetAttribLocation( mGLProgramTracer, "vertex" );
	glEnableVertexAttribArray( mVertexAttribute );
}


void GLWidget::initTargetTexture() {
	size_t w = 256;
	size_t h = 256;

	mTextureOut = vector<cl_float>( w * h * 4 );

	mKernelArgTextureIn = mCL->createImageReadOnly( w, h, &mTextureOut[0] );
	mKernelArgTextureOut = mCL->createImageWriteOnly( w, h );

	glGenTextures( 1, &mTargetTexture );
}


/**
 * Check, if QGLWidget is currently rendering.
 * @return {bool} True, if is rendering, false otherwise.
 */
bool GLWidget::isRendering() {
	return ( mDoRendering /*&& mTimer->isActive()*/ );
}


/**
 * Load 3D model and start rendering it.
 * @param {string} filepath Path to the file, without file name.
 * @param {string} filename Name of the file.
 */
void GLWidget::loadModel( string filepath, string filename ) {
	this->deleteOldModel();

	ModelLoader* ml = new ModelLoader();

	ml->loadModel( filepath, filename );
	mIndices = ml->mIndices;
	mVertices = ml->mVertices;
	// mNormals = ml->mNormals;

	// OpenCL buffers
	mBufferIndices = mCL->createBuffer( mIndices, sizeof( cl_uint ) * mIndices.size() );
	mBufferVertices = mCL->createBuffer( mVertices, sizeof( cl_float ) * mVertices.size() );

	mBufferEye = mCL->createEmptyBuffer( sizeof( cl_float ) * 3 );
	mBufferRay00 = mCL->createEmptyBuffer( sizeof( cl_float ) * 3 );
	mBufferRay01 = mCL->createEmptyBuffer( sizeof( cl_float ) * 3 );
	mBufferRay10 = mCL->createEmptyBuffer( sizeof( cl_float ) * 3 );
	mBufferRay11 = mCL->createEmptyBuffer( sizeof( cl_float ) * 3 );


	this->initShaders();

	// Ready
	this->startRendering();
}


/**
 * Load a GLSL shader and attach it to the program.
 * @param {GLuint}     shader ID of the shader.
 * @param {std:string} path   File path and name.
 */
void GLWidget::loadShader( GLuint shader, string path ) {
	string shaderString = utils::loadFileAsString( path.c_str() );

	shaderString = this->shaderReplacePlaceholders( shaderString );

	const GLchar* shaderSource = shaderString.c_str();
	const GLint shaderLength = shaderString.size();

	glShaderSource( shader, 1, &shaderSource, &shaderLength );
	glCompileShader( shader );

	GLint status;
	glGetShaderiv( shader, GL_COMPILE_STATUS, &status );

	if( status != GL_TRUE ) {
		char logBuffer[1000];
		glGetShaderInfoLog( shader, 1000, 0, logBuffer );
		Logger::logError( path + string( "\n" ).append( logBuffer ) );
		exit( 1 );
	}

	glAttachShader( mGLProgramTracer, shader );
}


/**
 * Set a minimum width and height for the QWidget.
 * @return {QSize} Minimum width and height.
 */
QSize GLWidget::minimumSizeHint() const {
	return QSize( 50, 50 );
}


/**
 * Move the camera or if selected a light.
 * @param {const int} key Key code.
 */
void GLWidget::moveCamera( const int key ) {
	if( !this->isRendering() ) {
		return;
	}

	switch( key ) {

		case Qt::Key_W:
			if( mSelectedLight == -1 ) { mCamera->cameraMoveForward(); }
			else { mLights[mSelectedLight].position[0] += 0.5f; }
			break;

		case Qt::Key_S:
			if( mSelectedLight == -1 ) { mCamera->cameraMoveBackward(); }
			else { mLights[mSelectedLight].position[0] -= 0.5f; }
			break;

		case Qt::Key_A:
			if( mSelectedLight == -1 ) { mCamera->cameraMoveLeft(); }
			else { mLights[mSelectedLight].position[2] += 0.5f; }
			break;

		case Qt::Key_D:
			if( mSelectedLight == -1 ) { mCamera->cameraMoveRight(); }
			else { mLights[mSelectedLight].position[2] -= 0.5f; }
			break;

		case Qt::Key_Q:
			if( mSelectedLight == -1 ) { mCamera->cameraMoveUp(); }
			else { mLights[mSelectedLight].position[1] += 0.5f; }
			break;

		case Qt::Key_E:
			if( mSelectedLight == -1 ) { mCamera->cameraMoveDown(); }
			else { mLights[mSelectedLight].position[1] -= 0.5f; }
			break;

		case Qt::Key_R:
			mCamera->cameraReset();
			break;

	}
}


/**
 * Draw the scene.
 */
void GLWidget::paintGL() {
	if( !mDoRendering || mVertices.size() <= 0 ) {
		return;
	}

	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );


	// Light(s)

	// char lightName1[20];
	// char lightName2[20];

	// for( uint i = 0; i < mLights.size(); i++ ) {
	// 	snprintf( lightName1, 20, "light%uData1", i );
	// 	snprintf( lightName2, 20, "light%uData2", i );

	// 	float lightData1[16] = {
	// 		mLights[i].position[0], mLights[i].position[1], mLights[i].position[2], mLights[i].position[3],
	// 		mLights[i].diffuse[0], mLights[i].diffuse[1], mLights[i].diffuse[2], mLights[i].diffuse[3],
	// 		mLights[i].specular[0], mLights[i].specular[1], mLights[i].specular[2], mLights[i].specular[3],
	// 		mLights[i].constantAttenuation, mLights[i].linearAttenuation, mLights[i].quadraticAttenuation, mLights[i].spotCutoff
	// 	};
	// 	float lightData2[4] = {
	// 		mLights[i].spotExponent, mLights[i].spotDirection[0], mLights[i].spotDirection[1], mLights[i].spotDirection[2]
	// 	};
	// }


	boost::posix_time::time_duration msdiff = boost::posix_time::microsec_clock::local_time() - mTimeSinceStart;
	cl_float timeSinceStart = msdiff.total_milliseconds() * 0.001f;


	// // Jittering for anti-aliasing
	// glm::vec3 v = glm::vec3(
	// 	rand() / (float) RAND_MAX * 2.0f - 1.0f,
	// 	rand() / (float) RAND_MAX * 2.0f - 1.0f,
	// 	0.0f
	// );
	// glm::mat4 jitter = this->getJitterMatrix( v ) * ( 1.0f / 256.0f );

	// printf( "ray00: %f, %f, %f\n", ray00[0], ray00[1], ray00[2] );
	// printf( "ray01: %f, %f, %f\n", ray01[0], ray01[1], ray01[2] );
	// printf( "ray10: %f, %f, %f\n", ray10[0], ray10[1], ray10[2] );
	// printf( "ray11: %f, %f, %f\n", ray11[0], ray11[1], ray11[2] );


	uint i = 0;
	vector<cl_mem> clBuffers;
	glm::vec3 eye = mCamera->getEye_glmVec3();
	// glm::vec4 eye4 = mModelViewProjectionMatrix * glm::vec4( eye[0], eye[1], eye[2], 1.0f );
	// eye[0] = eye4[0];
	// eye[1] = eye4[1];
	// eye[2] = eye4[2];

	cl_float textureWeight = mSampleCount / (cl_float) ( mSampleCount + 1 );

	glm::mat4 jitter = glm::inverse( mModelViewProjectionMatrix );

	glm::vec3 ray00 = this->getEyeRay( jitter, eye, -1.0f, -1.0f );
	glm::vec3 ray01 = this->getEyeRay( jitter, eye, -1.0f, +1.0f );
	glm::vec3 ray10 = this->getEyeRay( jitter, eye, +1.0f, -1.0f );
	glm::vec3 ray11 = this->getEyeRay( jitter, eye, +1.0f, +1.0f );


	mCL->setKernelArg( i, sizeof( cl_mem ), &mBufferIndices );
	mCL->setKernelArg( ++i, sizeof( cl_mem ), &mBufferVertices );

	mCL->updateBuffer( mBufferEye, sizeof( cl_float ) * 3, &eye[0] );
	mCL->updateBuffer( mBufferRay00, sizeof( cl_float ) * 3, &ray00[0] );
	mCL->updateBuffer( mBufferRay01, sizeof( cl_float ) * 3, &ray01[0] );
	mCL->updateBuffer( mBufferRay10, sizeof( cl_float ) * 3, &ray10[0] );
	mCL->updateBuffer( mBufferRay11, sizeof( cl_float ) * 3, &ray11[0] );

	mCL->setKernelArg( ++i, sizeof( cl_mem ), &mBufferEye );
	mCL->setKernelArg( ++i, sizeof( cl_mem ), &mBufferRay00 );
	mCL->setKernelArg( ++i, sizeof( cl_mem ), &mBufferRay01 );
	mCL->setKernelArg( ++i, sizeof( cl_mem ), &mBufferRay10 );
	mCL->setKernelArg( ++i, sizeof( cl_mem ), &mBufferRay11 );

	mCL->setKernelArg( ++i, sizeof( cl_float ), &textureWeight );
	mCL->setKernelArg( ++i, sizeof( cl_float ), &timeSinceStart );

	cl_uint numIndices = mIndices.size();
	mCL->setKernelArg( ++i, sizeof( cl_uint ), &numIndices );

	mCL->updateImageWriteOnly( 256, 256, &mTextureOut[0] );

	mCL->setKernelArg( ++i, sizeof( cl_mem ), &mKernelArgTextureIn );
	mCL->setKernelArg( ++i, sizeof( cl_mem ), &mKernelArgTextureOut );

	mCL->execute();
	mCL->readImageOutput( 256, 256, &mTextureOut[0] );
	mCL->finish();


	this->paintScene();


	this->showFPS();
}


/**
 * Draw the main objects of the scene.
 */
void GLWidget::paintScene() {
	// glBindTexture( GL_TEXTURE_2D, mTargetTextures[0] );
	// glBindFramebuffer( GL_FRAMEBUFFER, mFramebuffer );
	// glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTargetTextures[1], 0 );
	// glVertexAttribPointer( mVertexAttribute, 2, GL_FLOAT, false, 0, 0 );
	// glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
	// glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	glBindTexture( GL_TEXTURE_2D, mTargetTexture );
	// glBindFramebuffer( GL_FRAMEBUFFER, mFramebuffer );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_FLOAT, &mTextureOut[0] );

	// glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTargetTexture, 0 );

	glVertexAttribPointer( mVertexAttribute, 2, GL_FLOAT, false, 0, 0 );
	glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
	glBindTexture( GL_TEXTURE_2D, 0 );

	// if( glGetError() != 0 ) { cout << gluErrorString( glGetError() ) << endl; }

	// reverse( mTargetTextures.begin(), mTargetTextures.end() );
	mSampleCount++;

	glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
}


/**
 * Handle resizing of the QWidget by updating the viewport and perspective.
 * @param {int} width  New width of the QWidget.
 * @param {int} height New height of the QWidget.
 */
void GLWidget::resizeGL( int width, int height ) {
	glViewport( 0, 0, width, height );

	mProjectionMatrix = glm::perspective(
		Cfg::get().value<float>( Cfg::PERS_FOV ),
		width / (float) height,
		Cfg::get().value<float>( Cfg::PERS_ZNEAR ),
		Cfg::get().value<float>( Cfg::PERS_ZFAR )
	);

	this->calculateMatrices();
}


/**
 * Calculate the current framerate and show it in the status bar.
 */
void GLWidget::showFPS() {
	mFrameCount++;

	GLuint currentTime = glutGet( GLUT_ELAPSED_TIME );
	GLuint timeInterval = currentTime - mPreviousTime;

	if( timeInterval > 1000 ) {
		float fps = mFrameCount / (float) timeInterval * 1000.0f;
		mPreviousTime = currentTime;
		mFrameCount = 0;

		char statusText[40];
		snprintf( statusText, 40, "%.2f FPS (%d\u00D7%dpx)", fps, width(), height() );
		( (Window*) parentWidget() )->updateStatus( statusText );
	}
}


/**
 * Set size of the QWidget.
 * @return {QSize} Width and height of the QWidget.
 */
QSize GLWidget::sizeHint() const {
	return QSize(
		Cfg::get().value<uint>( Cfg::WINDOW_WIDTH ),
		Cfg::get().value<uint>( Cfg::WINDOW_HEIGHT )
	);
}


/**
 * Select the next light in the list.
 */
void GLWidget::selectNextLight() {
	if( mSelectedLight > -1 ) {
		mSelectedLight = ( mSelectedLight + 1 ) % mLights.size();
	}
}


/**
 * Select the previous light in the list.
 */
void GLWidget::selectPreviousLight() {
	if( mSelectedLight > -1 ) {
		mSelectedLight = ( mSelectedLight == 0 ) ? mLights.size() - 1 : mSelectedLight - 1;
	}
}


string GLWidget::shaderReplacePlaceholders( string shaderString ) {
	size_t posVertices = shaderString.find( "#NUM_VERTICES#" );
	if( posVertices != string::npos ) {
		char numVertices[20];
		snprintf( numVertices, 20, "%lu", mVertices.size() / 3 );
		shaderString.replace( posVertices, 14, numVertices );
	}

	size_t posNormals = shaderString.find( "#NUM_NORMALS#" );
	if( posNormals != string::npos ) {
		char numNormals[20];
		snprintf( numNormals, 20, "%lu", mNormals.size() / 3 );
		shaderString.replace( posNormals, 13, numNormals );
	}

	size_t posIndices = shaderString.find( "#NUM_INDICES#" );
	if( posIndices != string::npos ) {
		char numIndices[20];
		snprintf( numIndices, 20, "%lu", mIndices.size() / 3 );
		shaderString.replace( posIndices, 13, numIndices );
	}

	return shaderString;
}


/**
 * Start or resume rendering.
 */
void GLWidget::startRendering() {
	if( !mDoRendering ) {
		mDoRendering = true;
		float fps = Cfg::get().value<float>( Cfg::RENDER_INTERVAL );
		// mTimer->start( fps );
	}
}


/**
 * Stop rendering.
 */
void GLWidget::stopRendering() {
	if( mDoRendering ) {
		mDoRendering = false;
		// mTimer->stop();
		( (Window*) parentWidget() )->updateStatus( "Stopped." );
	}
}


/**
 * Switch between controlling the camera and the lights.
 */
void GLWidget::toggleLightControl() {
	if( mSelectedLight == -1 ) {
		if( mLights.size() > 0 ) {
			mSelectedLight = 0;
		}
	}
	else {
		mSelectedLight = -1;
	}
}
