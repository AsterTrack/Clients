/**
AsterTrack Optical Tracking System
Copyright (C)  2025 Seneral <contact@seneral.dev> and contributors

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "visualisation.hpp"
#include "sharedGL.hpp"

#include "util/log.hpp"
#include "util/blocked_vector.hpp"

#include "util/eigenutil.hpp"

#include <cassert>


/* Variables */

static GLuint visTempVBO; // Line buffer for uploading visualisation lines to GPU

static Eigen::Projective3f vpMat;
static Eigen::Projective3f projectionMat;
static Eigen::Isometry3f postProjectionMat;
static Eigen::Isometry3f viewMat;
static Eigen::Isometry3f id = Eigen::Isometry3f::Identity();
static float viewportZoom;


/* Functions */

void initVisualisation()
{
	initSharedGL();

	// Init mesh buffers
	glGenBuffers(1, &visTempVBO);
}

void cleanVisualisation()
{
	glDeleteBuffers(1, &visTempVBO);

	cleanSharedGL();
}

std::array<float,2> getPointSizeRange()
{
	std::array<float,2> pointSizeRange;
	glGetFloatv(GL_POINT_SIZE_RANGE, pointSizeRange.data());
	return pointSizeRange;
}

void visSetupView(const Eigen::Projective3f &projection, const Eigen::Isometry3f &view, Eigen::Vector2i viewport)
{
	projectionMat = projection;
	viewMat = view;
	postProjectionMat.setIdentity();
	vpMat = projection*view;
	viewportZoom = 1.0f;
}

void visSetupCamera(const Eigen::Isometry3f &postProjection, const CameraCalib &calib, const CameraMode &mode, Eigen::Vector2i viewport)
{
	projectionMat = calib.projection.cast<float>();
	viewMat = calib.view.cast<float>();
	postProjectionMat = postProjection;
	vpMat = postProjection * calib.camera.cast<float>();
	viewportZoom = std::abs(postProjection(0,0));
}

void visSetupProjection(const Eigen::Isometry3f &projection, Eigen::Vector2i viewport)
{
	projectionMat.setIdentity();
	viewMat.setIdentity();
	postProjectionMat = projection;
	vpMat = projection;
	viewportZoom = std::abs(projection(0,0));
}

/*
 * Visualisation functions for both 2D and 3D views
 */

void visualiseSkybox(float time)
{
	skyShader->use();
	glUniformMatrix4fv(skyShader->uProjAdr, 1, GL_FALSE, projectionMat.data());
	glUniformMatrix4fv(skyShader->uModelAdr, 1, GL_FALSE, viewMat.data()); // Misuse of model mat

	glUniform1f(skyTimeAdr, time);

	// Fixed sun pos (time = 50.0f)
	glUniform2f(skySunAdr, std::sin(50 * 0.01f), std::cos(50 * 0.01f));

	xyPlaneMesh->draw();
}

void visualiseFloor(Color color)
{
	flatUniformColorShader->use();
	glUniform4f(flatUniformColorShader->uColorAdr, color.r, color.g, color.b, color.a);
	glUniformMatrix4fv(flatUniformColorShader->uProjAdr, 1, GL_FALSE, vpMat.data());
	Eigen::Affine3f scale = Eigen::Affine3f::Identity()*Eigen::Scaling(10.0f);
	glUniformMatrix4fv(flatUniformColorShader->uModelAdr, 1, GL_FALSE, scale.matrix().data());
	xyPlaneMesh->draw();
}

void updatePointsVBO(unsigned int &VBO, const std::vector<VisPoint> &points)
{
	// Generate Vertex Buffer Object if not yet done
	if (VBO == 0)
		glGenBuffers(1, &VBO);

	// Load vertex data
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(VisPoint) * points.size(), points.data(), GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	int error = glGetError();
	while (error != GL_NO_ERROR)
	{
		LOG(LGUI, LError, "GL error %d!\n", error);
		error = glGetError();
	}
}

void visualisePointsVBOSprites(unsigned int VBO, unsigned int count, bool round, float sizeFactor)
{
	if (VBO == 0)
		return;

	if (round)
	{
		flatRoundPointShader->use();
		glUniformMatrix4fv(flatRoundPointShader->uProjAdr, 1, GL_FALSE, vpMat.data());
		glUniformMatrix4fv(flatRoundPointShader->uModelAdr, 1, GL_FALSE, id.matrix().data());
		glUniform1f(roundSizeAdr, sizeFactor*viewportZoom*pointSizeCorrection);
	}
	else
	{
	 	flatSquarePointShader->use();
		glUniformMatrix4fv(flatSquarePointShader->uProjAdr, 1, GL_FALSE, vpMat.data());
		glUniformMatrix4fv(flatSquarePointShader->uModelAdr, 1, GL_FALSE, id.matrix().data());
		glUniform1f(squareSizeAdr, sizeFactor*viewportZoom*pointSizeCorrection);
	}

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VisPoint), (void *)offsetof(VisPoint, pos));
	glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VisPoint), (void *)offsetof(VisPoint, color));
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(VisPoint), (void *)offsetof(VisPoint, size));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(3);
	glDrawArrays(GL_POINTS, 0, count);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	int error = glGetError();
	while (error != GL_NO_ERROR)
	{
		LOG(LGUI, LError, "GL error %d!\n", error);
		error = glGetError();
	}
}

void visualisePointsSprites(const std::vector<VisPoint> &points, bool round)
{
	if (points.empty()) return;
	static unsigned int DynamicPointsVBO;
	updatePointsVBO(DynamicPointsVBO, points);
	visualisePointsVBOSprites(DynamicPointsVBO, points.size(), round);
}

void visualisePointsSpheres(const std::vector<VisPoint> &points)
{
	if (points.empty()) return;
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	flatUniformColorShader->use();
	glUniformMatrix4fv(flatUniformColorShader->uProjAdr, 1, GL_FALSE, vpMat.data());

	spherePointMesh->prepare();
	for (int i = 0; i < points.size(); i++)
	{
		auto &pt = points[i];
		if (pt.color.a == 0) continue;
		Eigen::Affine3f model = Eigen::Translation3f(pt.pos) * Eigen::Scaling(pt.size);
		glUniformMatrix4fv(flatUniformColorShader->uModelAdr, 1, GL_FALSE, model.matrix().data());
		Color col = pt.color;
		glUniform4f(flatUniformColorShader->uColorAdr, col.r, col.g, col.b, col.a);
		spherePointMesh->drawPart();
	}
	spherePointMesh->cleanup();

	glDisable(GL_CULL_FACE);
}

void visualisePointsSpheresDepthSorted(const std::vector<VisPoint> &points)
{
	if (points.empty()) return;
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	flatUniformColorShader->use();
	glUniformMatrix4fv(flatUniformColorShader->uProjAdr, 1, GL_FALSE, vpMat.data());

	spherePointMesh->prepare();
	// Opaque first, and register transparent
	thread_local std::vector<std::pair<int, float>> transparentOrder;
	transparentOrder.clear();
	for (int i = 0; i < points.size(); i++)
	{
		auto &pt = points[i];
		if (pt.color.a == 0) continue;
		if (pt.color.a < 255)
		{ // Register transparent
			float dist = (viewMat * pt.pos).z();
			transparentOrder.emplace_back(i, dist-pt.size);
		}
		else
		{ // Draw opaque
			Eigen::Affine3f model = Eigen::Translation3f(pt.pos) * Eigen::Scaling(pt.size);
			glUniformMatrix4fv(flatUniformColorShader->uModelAdr, 1, GL_FALSE, model.matrix().data());
			Color col = pt.color;
			glUniform4f(flatUniformColorShader->uColorAdr, col.r, col.g, col.b, col.a);
			spherePointMesh->drawPart();
		}
	}
	// Sort and draw transparent
	std::sort(transparentOrder.begin(), transparentOrder.end(), 
		[&](auto &a, auto &b) { return a.second > b.second; });
	for (auto &p : transparentOrder)
	{
		auto &pt = points[p.first];
		Eigen::Affine3f model = Eigen::Translation3f(pt.pos) * Eigen::Scaling(pt.size);
		glUniformMatrix4fv(flatUniformColorShader->uModelAdr, 1, GL_FALSE, model.matrix().data());
		Color col = pt.color;
		glUniform4f(flatUniformColorShader->uColorAdr, col.r, col.g, col.b, col.a);
		spherePointMesh->drawPart();
	}
	spherePointMesh->cleanup();

	glDisable(GL_CULL_FACE);
}

static void setupMesh(const VisPoint *data, unsigned int count)
{
	flatVertColorShader->use();
	glUniformMatrix4fv(flatVertColorShader->uProjAdr, 1, GL_FALSE, vpMat.data());
	glUniformMatrix4fv(flatVertColorShader->uModelAdr, 1, GL_FALSE, id.matrix().data());
	glBindBuffer(GL_ARRAY_BUFFER, visTempVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(VisPoint) * count, data, GL_STREAM_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VisPoint), (void *)offsetof(VisPoint, pos));
	glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VisPoint), (void *)offsetof(VisPoint, color));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
}

void visualiseLines(const std::vector<std::pair<VisPoint, VisPoint>> &lines, float size)
{
	if (lines.empty()) return;
	setupMesh((VisPoint*)lines.data(), lines.size()*2);
	glLineWidth(size);
	glDrawArrays(GL_LINES, 0, (int)lines.size()*2);
}

void visualiseLines(const std::vector<VisPoint> &lineVerts, float size)
{
	if (lineVerts.empty()) return;
	setupMesh(lineVerts.data(), lineVerts.size());
	glLineWidth(size);
	glDrawArrays(GL_LINE_LOOP, 0, (int)lineVerts.size());
}

void visualiseMesh(const std::vector<VisPoint> &vertices, unsigned int mode)
{
	// Assert static inputs are what we expected - else, change calling code
	static_assert(6 == GL_TRIANGLE_FAN);
	if (vertices.empty()) return;
	setupMesh((VisPoint*)vertices.data(), vertices.size());
	glDrawArrays(mode, 0, (int)vertices.size());
}

void visualiseCamera(Eigen::Isometry3f camera, Color color)
{
	flatUniformColorShader->use();
	glUniform4f(flatUniformColorShader->uColorAdr, color.r, color.g, color.b, color.a);
	glUniformMatrix4fv(flatUniformColorShader->uProjAdr, 1, GL_FALSE, vpMat.data());
	glUniformMatrix4fv(flatUniformColorShader->uModelAdr, 1, GL_FALSE, camera.matrix().data());
	cameraMesh->draw();
}

void visualisePose(const Eigen::Isometry3f &pose, Color color, float scale, float lineWidth)
{
	flatUniformColorShader->use();
	glUniform4f(flatUniformColorShader->uColorAdr, color.r, color.g, color.b, color.a);
	glUniformMatrix4fv(flatUniformColorShader->uProjAdr, 1, GL_FALSE, vpMat.data());
	Eigen::Affine3f model = pose*Eigen::Scaling(scale);
	glUniformMatrix4fv(flatUniformColorShader->uModelAdr, 1, GL_FALSE, model.matrix().data());

	glLineWidth(lineWidth);
	coordinateOriginMesh->draw();
}


/*
 * Visualisation functions for 2D views exclusively
 */

void loadGrayscaleFrame(unsigned int &frame, const uint8_t *data, int width, int height)
{
	if (frame == 0)
		glGenTextures(1, &frame);
	glBindTexture(GL_TEXTURE_2D, frame);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
//	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
//	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
//	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	GLfloat blank[] = { 0, 0, 0, 0 }; // Default value, but make sure
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, blank);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, data);
	glBindTexture(GL_TEXTURE_2D, 0);
	int error = glGetError();
	while (error != GL_NO_ERROR)
	{
		LOG(LGUI, LError, "GL error %d!\n", error);
		error = glGetError();
	}
}

void loadVectorField(unsigned int &frame, const std::vector<Eigen::Vector2f> &data, int width, int height)
{
	if (frame == 0)
		glGenTextures(1, &frame);
	glBindTexture(GL_TEXTURE_2D, frame);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, width, height, 0,  GL_RG,  GL_FLOAT, data.data());
	glBindTexture(GL_TEXTURE_2D, 0);
	int error = glGetError();
	while (error != GL_NO_ERROR)
	{
		LOG(LGUI, LError, "GL error %d!\n", error);
		error = glGetError();
	}
}

void deleteFrameTexture(unsigned int frame)
{
	glDeleteTextures(1, &frame);
}

void showGrayscaleFrame(unsigned int frame, const Eigen::Affine3f &projection,
	Color color, float alpha, float brightness, float contrast)
{
	// Setup shader uniforms
	imageShader->use();
	glUniformMatrix4fv(imageShader->uProjAdr, 1, GL_FALSE, vpMat.data());
	glUniformMatrix4fv(imageShader->uModelAdr, 1, GL_FALSE, projection.data());
	static GLint uAdjustAdr = glGetUniformLocation(imageShader->ID, "adjust");
	glUniform4f(uAdjustAdr, brightness, contrast, 0.0f, alpha);
	glUniform4f(imageShader->uColorAdr, color.r, color.g, color.b, color.a);
	glUniform1i(imageShader->uImageAdr, 0);
	// Set frame texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, frame);
	GLenum error = glGetError();
	while (error != GL_NO_ERROR)
	{
		LOG(LGUI, LError, "GL error %d!\n", error);
		error = glGetError();
	}
	// Frame
	xyPlaneMesh->draw();
	glBindTexture(GL_TEXTURE_2D, 0);
}

/**
 * Render a previously uploaded grayscale frame denoted by its handle
 */
void showGrayscaleFrameUndistorted(unsigned int frame, unsigned int undistortionTex,
	const Bounds2f &bounds, const CameraMode &mode, Eigen::Vector2f viewportScale,
	Color color, float alpha, float brightness, float contrast)
{// Setup shader uniforms
	undistortTexShader->use();
	glUniformMatrix4fv(undistortTexShader->uProjAdr, 1, GL_FALSE, vpMat.data());
	Eigen::Isometry3f model = Eigen::Isometry3f::Identity();
	model.matrix().diagonal().head<2>() = viewportScale;
	glUniformMatrix4fv(undistortTexShader->uModelAdr, 1, GL_FALSE, model.data());
	static GLint uAdjustAdr = glGetUniformLocation(undistortTexShader->ID, "adjust");
	glUniform4f(uAdjustAdr, brightness, contrast, 0.0f, alpha);
	glUniform4f(undistortTexShader->uColorAdr, color.r, color.g, color.b, color.a);
	glUniform1i(undistortTexShader->uImageAdr, 0);
	// Set special undistortion uniforms
	static GLint uBoundsAdr = glGetUniformLocation(undistortTexShader->ID, "bounds");
	static GLint uSizeAdr = glGetUniformLocation(undistortTexShader->ID, "size");
	static GLint uUndistortTexAdr = glGetUniformLocation(undistortTexShader->ID, "undistortTex");
	glUniform4f(uBoundsAdr, bounds.minX, bounds.minY, bounds.maxX, bounds.maxY);
	glUniform2f(uSizeAdr, mode.sizeW, mode.sizeH);
	glUniform1i(uUndistortTexAdr, 1);
	// Set frame texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, frame);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, undistortionTex);
	GLenum error = glGetError();
	while (error != GL_NO_ERROR)
	{
		LOG(LGUI, LError, "GL error %d!\n", error);
		error = glGetError();
	}
	// Frame
	xyPlaneMesh->draw();
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void showGrayscaleFrameUndistorted(unsigned int frame,
	const Bounds2f &bounds, const CameraMode &mode, const CameraCalib &calib, Eigen::Vector2f viewportScale,
	Color color, float alpha, float brightness, float contrast)
{
	// Setup shader uniforms
	undistortAlgShader->use();
	glUniformMatrix4fv(undistortAlgShader->uProjAdr, 1, GL_FALSE, vpMat.data());
	Eigen::Isometry3f model = Eigen::Isometry3f::Identity();
	model.matrix().diagonal().head<2>() = viewportScale;
	glUniformMatrix4fv(undistortAlgShader->uModelAdr, 1, GL_FALSE, model.data());
	static GLint uAdjustAdr = glGetUniformLocation(undistortAlgShader->ID, "adjust");
	glUniform4f(uAdjustAdr, brightness, contrast, 0.0f, alpha);
	glUniform4f(undistortAlgShader->uColorAdr, color.r, color.g, color.b, color.a);
	glUniform1i(undistortAlgShader->uImageAdr, 0);
	// Set special undistortion uniforms
	static GLint uUVScaleAdr = glGetUniformLocation(undistortAlgShader->ID, "uvScale");
	static GLint uBoundsAdr = glGetUniformLocation(undistortAlgShader->ID, "bounds");
	static GLint uSizeAdr = glGetUniformLocation(undistortAlgShader->ID, "size");
	static GLint uPrincipalAdr = glGetUniformLocation(undistortAlgShader->ID, "principal");
	static GLint uDistKAdr = glGetUniformLocation(undistortAlgShader->ID, "distK");
	static GLint uDistPAdr = glGetUniformLocation(undistortAlgShader->ID, "distP");
	glUniform2f(uUVScaleAdr, viewportScale.x(), viewportScale.y());
	glUniform4f(uBoundsAdr, bounds.minX, bounds.minY, bounds.maxX, bounds.maxY);
	glUniform2f(uSizeAdr, mode.sizeW, mode.sizeH);
	glUniform2f(uPrincipalAdr, calib.principalPoint.x(), calib.principalPoint.y());
	glUniform3f(uDistKAdr, calib.distortion.k1, calib.distortion.k2, calib.distortion.k3);
	glUniform2f(uDistPAdr, calib.distortion.p1, calib.distortion.p2);
	// Set frame texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, frame);
	GLenum error = glGetError();
	while (error != GL_NO_ERROR)
	{
		LOG(LGUI, LError, "GL error %d!\n", error);
		error = glGetError();
	}
	// Frame
	xyPlaneMesh->draw();
}

void showCircleWithCenter(Eigen::Vector2f pos, float size, Color color, float crossSize)
{
	const int SEG = 20;
	std::array<Eigen::Vector2f, SEG> ellipse;
	for (int i = 0; i < SEG; i++)
	{
		float p = 2*(float)PI*(float)i/SEG;
		ellipse[i].x() = (std::cos(p)*size + pos.x());
		ellipse[i].y() = (std::sin(p)*size + pos.y());
	}

	glLineWidth(2.0f);

	flatUniformColorShader->use();
	glUniformMatrix4fv(flatUniformColorShader->uProjAdr, 1, GL_FALSE, vpMat.data());
	glUniformMatrix4fv(flatUniformColorShader->uModelAdr, 1, GL_FALSE, id.matrix().data());

	glUniform4f(flatUniformColorShader->uColorAdr, color.r, color.g, color.b, color.a);
	glBindBuffer(GL_ARRAY_BUFFER, visTempVBO);

	glBufferData(GL_ARRAY_BUFFER, sizeof(Eigen::Vector2f) * ellipse.size(), &ellipse[0], GL_STREAM_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Eigen::Vector2f), (void *)0);
	glEnableVertexAttribArray(0);
	glDrawArrays(GL_LINE_LOOP, 0, (int)ellipse.size());

	// Draw cross at center
	if (crossSize > 0.0f)
	{
		Eigen::Vector2f axisX = Eigen::Vector2f::UnitX()*crossSize, axisY = Eigen::Vector2f::UnitY()*crossSize;
		std::vector<Eigen::Vector2f> cross { pos - axisX, pos + axisX, pos - axisY, pos + axisY };

		glBufferData(GL_ARRAY_BUFFER, sizeof(Eigen::Vector2f) * cross.size(), &cross[0], GL_STREAM_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Eigen::Vector2f), (void *)0);
		glEnableVertexAttribArray(0);
		glDrawArrays(GL_LINES, 0, 4);
	}
}

void showSolidEllipse(Eigen::Vector2f pos, Eigen::Vector2f size, Color color)
{
	const int SEG = 20;
	std::array<Eigen::Vector2f, SEG+1> ellipse;
	ellipse[0] = pos;
	for (int i = 1; i <= SEG; i++)
	{
		float p = 2*(float)PI*(float)(i-1)/(SEG-1);
		ellipse[i].x() = (std::cos(p)*size.x() + pos.x());
		ellipse[i].y() = (std::sin(p)*size.y() + pos.y());
	}

	flatUniformColorShader->use();
	glUniformMatrix4fv(flatUniformColorShader->uProjAdr, 1, GL_FALSE, id.data());
	glUniformMatrix4fv(flatUniformColorShader->uModelAdr, 1, GL_FALSE, id.data());

	glUniform4f(flatUniformColorShader->uColorAdr, color.r, color.g, color.b, color.a);
	glBindBuffer(GL_ARRAY_BUFFER, visTempVBO);

	glBufferData(GL_ARRAY_BUFFER, sizeof(Eigen::Vector2f) * ellipse.size(), &ellipse[0], GL_STREAM_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Eigen::Vector2f), (void *)0);
	glEnableVertexAttribArray(0);
	glDrawArrays(GL_TRIANGLE_FAN, 0, (int)ellipse.size());
}

void visualisePoints2D(const std::vector<Eigen::Vector2f> &points2D, Color color, float size, float depth, bool round)
{
	thread_local std::vector<VisPoint> vertices;
	vertices.clear();
	for (const auto &pt : points2D)
		vertices.emplace_back(Eigen::Vector3f(pt.x(), pt.y(), 1-depth), color, size);
	visualisePointsSprites(vertices, round);
}

template<class it_type>
void visualisePoints2D(it_type pts_begin, it_type pts_end, Color color, float size, float depth, bool round)
{
	thread_local std::vector<VisPoint> vertices;
	vertices.clear();
	while (pts_begin != pts_end)
	{
		vertices.emplace_back(Eigen::Vector3f(pts_begin->x(), pts_begin->y(), 1-depth), color, size);
		pts_begin++;
	}
	visualisePointsSprites(vertices, round);
}
template void visualisePoints2D(BlockedVector<Eigen::Vector2f>::const_iterator pts_begin, BlockedVector<Eigen::Vector2f>::const_iterator pts_end, Color color, float size, float depth, bool round);
template void visualisePoints2D(std::vector<Eigen::Vector2f>::const_iterator pts_begin, std::vector<Eigen::Vector2f>::const_iterator pts_end, Color color, float size, float depth, bool round);


void updatePixelVBO(unsigned int &VBO, const std::vector<uint8_t> &pixels2D, Eigen::Vector2i sizeImg, Eigen::Vector2f center, float pixelStride, float pixelSize, Color colorA, Color colorB, float depth)
{
	Eigen::Vector2f bottomLeft(
		center.x() + (-sizeImg.x()/2.0f + 0.5f) * pixelStride,
		center.y() + (-sizeImg.y()/2.0f + 0.5f) * pixelStride);
	// Pack vertex data
	assert(pixels2D.size() == sizeImg.prod());
	thread_local std::vector<VisPoint> vertices;
	vertices.resize(sizeImg.prod());
	for (int y = 0; y < sizeImg.y(); y++)
	{
		for (int x = 0; x < sizeImg.x(); x++)
		{
			int index = y*sizeImg.x()+x;
			VisPoint &vert = vertices[index];
			vert.pos.x() = bottomLeft.x()+x*pixelStride;
			vert.pos.y() = bottomLeft.y()+y*pixelStride;
			vert.pos.z() = 1-depth;
			vert.color = lerp(colorA, colorB, pixels2D[index]/255.0f);
			vert.size = pixelSize;

		}
	}

	updatePointsVBO(VBO, vertices);
}