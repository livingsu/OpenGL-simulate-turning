#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "shader.h"
#include "camera.h"
#include "model.h"
#include "skybox.h"
#include <iostream>
#include <vector>
#include <string>
#include <ctime>


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
void loadPBRtextures();
unsigned int loadTexture(const char *path);
void initCylinder();  //根据圆柱体信息初始化圆柱体,载入VAO,VBO和顶点数量

// 屏幕宽高
unsigned int WIN_WIDTH = 800;
unsigned int WIN_HEIGHT = 600;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 1.0f));

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// pbr材质
const unsigned int PBR_TYPES = 2;
unsigned int PBR_type = 0;
string PBRtypes[PBR_TYPES] = { "rusted_iron","wood" };
unsigned int albedo[PBR_TYPES];
unsigned int normal[PBR_TYPES];
unsigned int metallic[PBR_TYPES];
unsigned int roughness[PBR_TYPES];
unsigned int ao[PBR_TYPES];


//圆柱体信息,注意应该用double而不是float,避免精度不够导致除法出错
const double cylinderRadius = 0.4;
const double cylinderLength = 1.6;
const int angleStep = 2; //切分角度增量
const double lengthStep = 0.001;  //切分长度增量
const double radiusStep = 0.001; //半径增量
vector<int> radiusArray;  //半径数组,均为radiusStep的整数倍
vector<glm::vec3> allPoints;  //所有点数组,包括顶点、法向量和纹理坐标
vector<unsigned int> indices;  //索引数据


unsigned int cylinderVAO;
unsigned int cylinderVBO;
unsigned int vertexNum; //顶点数量


//鼠标对应的裁剪坐标(标准化设备坐标,范围为-1~1),开始时刀尖在圆心处
const double clipX0 = 0.62, clipY0 = -0.2;  //刀尖初始位置
double clipX = clipX0, clipY = clipY0;  //刀尖新位置


//粒子系统
struct Particle {
	glm::vec3 position, velocity;
	float life;

	Particle() :position(0.0f), velocity(0.0f), life(0.0f) {}
};
vector<Particle> particles;  //粒子数组
const int PARTICLE_NUM = 1000; //粒子总数



int main()
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "LearnOpenGL", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// 配置openGL全局属性
	// -----------------------------
	glEnable(GL_DEPTH_TEST);


	// 创建,编译着色器
	// -------------------------
	Shader skyboxShader("skybox.vs", "skybox.fs");
	Shader cylinderShader_pbr("cylinder.vs", "cylinder_pbr.fs");
	Shader modelShader("model.vs", "model.fs");


	// 创建顶点数据,VAO,VBO
	// ----------------------------

	initCylinder();
	cout << "points:" << vertexNum << endl;


	// 加载纹理
	// -------------

	unsigned int myTexture = loadTexture("resources/textures/container.png");


	//测试pbr加载运行时间
	clock_t startTime, endTime;
	startTime = clock();//计时开始

	loadPBRtextures();

	endTime = clock();//计时结束
	cout << "The run time is: " << (double)(endTime - startTime) / (time_t)1000 << "s" << endl;


	Skybox mySkybox;


	// 着色器配置
	// --------------------
	skyboxShader.use();
	skyboxShader.setInt("skybox", 0);

	//pbr光照模型
	cylinderShader_pbr.use();
	cylinderShader_pbr.setInt("albedoMap", 0);
	cylinderShader_pbr.setInt("normalMap", 1);
	cylinderShader_pbr.setInt("metallicMap", 2);
	cylinderShader_pbr.setInt("roughnessMap", 3);
	cylinderShader_pbr.setInt("aoMap", 4);


	// pbr灯光
	// ----------
	glm::vec3 lightPositions[] = {
		glm::vec3(0.0f, 0.0f, 10.0f),glm::vec3(0.0f, 0.0f,-2.0f),
	};
	glm::vec3 lightColors[] = {
		glm::vec3(150.0f, 150.0f, 150.0f),glm::vec3(150.0f, 150.0f, 150.0f),
	};


	// 加载3d模型
	// -----------
	Model myModel("resources/models/turningTool/turningTool.3ds");



	// 模型、视图、投影矩阵
	// ------------------------

	glm::mat4 model, view, projection;
	// 旋转角度
	float angle = 0.0f;


	while (!glfwWindowShouldClose(window))
	{
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// 输入
		// -----
		processInput(window);

		// 渲染
		// ------
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


		// 设置矩阵
		// ---------
		model = glm::mat4(1.0f);
		view = camera.GetViewMatrix();
		float orthoSize = 1.0f;
		projection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, -orthoSize, orthoSize);


		// 画圆柱体：pbr模型
		// ------------------------
		//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		cylinderShader_pbr.use();
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.5f, 0.0f, 0.0f));
		//圆柱体旋转
		model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		cylinderShader_pbr.setMat4("model", model);
		cylinderShader_pbr.setMat4("view", view);
		cylinderShader_pbr.setMat4("projection", projection);
		cylinderShader_pbr.setVec3("viewPos", camera.Position);
		//设置pbr材质属性
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, albedo[PBR_type]);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, normal[PBR_type]);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, metallic[PBR_type]);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, roughness[PBR_type]);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, ao[PBR_type]);
		glBindVertexArray(cylinderVAO);
		glDrawElements(GL_TRIANGLES, vertexNum, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		//设置pbr的点光源位置(可为多个点光源,最多4个)
		for (unsigned int i = 0; i < sizeof(lightPositions) / sizeof(lightPositions[0]); ++i) {
			glm::vec3 newPos = lightPositions[i] + glm::vec3(sin(glfwGetTime() * 5.0) * 5.0, 0.0, 0.0);
			newPos = lightPositions[i];
			cylinderShader_pbr.setVec3("lightPositions[" + std::to_string(i) + "]", newPos);
			cylinderShader_pbr.setVec3("lightColors[" + std::to_string(i) + "]", lightColors[i]);
		}


		// 画3d模型
		// -------------
		modelShader.use();
		model = glm::mat4(1.0f);
		//直接设置最终坐标
		model = glm::translate(model, glm::vec3(clipX, clipY, 0.0f));
		//旋转到正确方位
		model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::scale(model, glm::vec3(0.02f));
		view = glm::mat4(1.0f);
		projection = glm::mat4(1.0f);
		modelShader.setMat4("model", model);
		modelShader.setMat4("view", view);
		modelShader.setMat4("projection", projection);
		myModel.Draw(modelShader);


		// 最后画天空盒子
		// --------------------
		skyboxShader.use();
		projection = glm::perspective(glm::radians(camera.Zoom), (float)WIN_WIDTH / (float)WIN_HEIGHT, 0.1f, 100.0f);
		skyboxShader.setMat4("view", glm::mat4(glm::mat3(camera.GetViewMatrix())));
		skyboxShader.setMat4("projection", projection);
		mySkybox.draw();


		//更新旋转角
		angle += 0.8f;

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteVertexArrays(1, &cylinderVAO);

	glfwTerminate();
	return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.ProcessKeyboard(RIGHT, deltaTime);


	if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
		PBR_type = 0;
	}
	if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
		PBR_type = 1;
	}
	if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
		PBR_type = 2;
	}
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	if (width == 0 || height == 0) {
		WIN_WIDTH = width;
		WIN_HEIGHT = height;
	}
	glViewport(0, 0, width, height);
}


void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	//将屏幕坐标转化为裁剪坐标(-1~1范围)
	double newClipX = xpos * 2.0 / (double)(WIN_WIDTH - 1) - 1.0;
	double newClipY = -ypos * 2.0 / (double)(WIN_HEIGHT - 1) + 1.0;

	if (newClipY > clipY0) { //模型不得高于圆心位置
		newClipY = clipY0;
	}

	static int moveNum = 0;
	static int cutNum = 0;

	//模拟切削
	if (newClipX < clipX0 || clipX < clipX0) { //车刀向左移动
		int zStart = (clipX0 - (clipX > newClipX ? clipX : newClipX)) / lengthStep; //圆柱体z轴方向下标
		int zEnd = (clipX0 - (clipX > newClipX ? newClipX : clipX)) / lengthStep;
		int zGap = zEnd - zStart;
		//开始部分对应的半径是radiusStep的整数倍
		int R_start = (clipY0 - (clipX > newClipX ? clipY : newClipY)) / radiusStep;
		int R_end = (clipY0 - (clipX > newClipX ? newClipY : clipY)) / radiusStep;
		int R_gap = R_end - R_start;

		int stacks = cylinderLength / lengthStep;
		if (zStart >= 0 && zStart <= stacks && zEnd >= 0 && zEnd <= stacks && zGap >= 0 && R_start >= 0) {  
			bool isCut = false;
			for (int i = zStart; i <= zEnd; ++i) {
				int newRadius;
				if (zGap == 0)
					newRadius = R_start;
				else
					newRadius = R_start + R_gap * (float)(i - zStart) / (float)zGap;

				if (newRadius < 0) newRadius = 0;
				if (radiusArray[i] > newRadius) {
					isCut = true;
					radiusArray[i] = newRadius;  //更新半径数组
				}
			}

			//更新VBO缓冲区
			glBindBuffer(GL_ARRAY_BUFFER, cylinderVBO);
			if (isCut) {
				int slices = 360 / angleStep;
				for (int i = 0; i <= slices; ++i) {
					for (int j = zStart; j <= zEnd; ++j) {
						int pointOffset = (i*(stacks + 1) + j) * 3;
						glm::vec3 newPoint = allPoints[pointOffset];
						float alpha = i * angleStep;
						float newR = radiusArray[j] * radiusStep;
						newPoint.x = newR * (float)glm::cos(glm::radians(alpha));
						newPoint.y = newR * (float)glm::sin(glm::radians(alpha));
						glBufferSubData(GL_ARRAY_BUFFER, 3 * sizeof(float) * pointOffset, 2 * sizeof(float), &newPoint);
						allPoints[pointOffset] = newPoint;
					}

				}
			}
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	}

	//车刀跟随鼠标移动
	clipX = newClipX;
	clipY = newClipY;
}


void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll(yoffset);
}

// 根据类型加载pbr纹理
// ---------------------------------------------------
void loadPBRtextures()
{
	for (int type = 0; type < 1; ++type) {
		string folder = PBRtypes[type];
		string filepath1 = "resources/textures/pbr/" + folder + "/albedo.png";
		string filepath2 = "resources/textures/pbr/" + folder + "/normal.png";
		string filepath3 = "resources/textures/pbr/" + folder + "/metallic.png";
		string filepath4 = "resources/textures/pbr/" + folder + "/roughness.png";
		string filepath5 = "resources/textures/pbr/" + folder + "/ao.png";

		albedo[type] = loadTexture(filepath1.c_str());
		normal[type] = loadTexture(filepath2.c_str());
		metallic[type] = loadTexture(filepath3.c_str());
		roughness[type] = loadTexture(filepath4.c_str());
		ao[type] = loadTexture(filepath5.c_str());
	}
}


// utility function for loading a 2D texture from file
// ---------------------------------------------------
unsigned int loadTexture(char const * path)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
	if (data)
	{
		GLenum format;
		if (nrComponents == 1)
			format = GL_RED;
		else if (nrComponents == 3)
			format = GL_RGB;
		else if (nrComponents == 4)
			format = GL_RGBA;

		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
	}
	else
	{
		std::cout << "Texture failed to load at path: " << path << std::endl;
		stbi_image_free(data);
	}

	return textureID;
}



void initCylinder() {
	int slices = 360 / angleStep;  //围绕z轴的细分
	int stacks = cylinderLength / lengthStep;  //沿z轴的细分


	cout << slices << '\t' << stacks << endl;

	//初始化半径数组
	unsigned int initRadius = cylinderRadius / radiusStep;
	for (int i = 0; i <= stacks; ++i) {
		radiusArray.push_back(initRadius);
	}

	float R, alpha, x, y, z, texX, texY;

	for (int i = 0; i <= slices; i++) {
		for (int j = 0; j <= stacks; j++) {
			R = radiusArray[j] * radiusStep;
			alpha = i * angleStep;
			x = R * (float)glm::cos(glm::radians(alpha));
			y = R * (float)glm::sin(glm::radians(alpha));
			z = lengthStep * j;
			texX = (float)i / (float)slices;  //纹理坐标
			texY = (float)j / (float)stacks;

			//顶点
			glm::vec3 V(x, y, z);

			//侧面
			allPoints.push_back(V);
			allPoints.push_back(glm::vec3(V.x, V.y, 0.0f)); //法向量
			allPoints.push_back(glm::vec3(texX, texY, 0.0f));//纹理坐标(是2D,但由于allPoints只能接受3D,故多出没用的一个float)


			//添加索引坐标
			if (i < slices && j < stacks) {
				unsigned int leftDown, leftUp, rightDown, rightUp;
				leftDown = i * (stacks + 1) + j;
				leftUp = i * (stacks + 1) + (j + 1);
				rightDown = (i + 1) * (stacks + 1) + j;
				rightUp = (i + 1) * (stacks + 1) + (j + 1);

				indices.push_back(leftDown);
				indices.push_back(leftUp);
				indices.push_back(rightUp);
				indices.push_back(leftDown);
				indices.push_back(rightUp);
				indices.push_back(rightDown);
			}
		}
	}

	vertexNum = indices.size();

	unsigned int VAO, VBO, EBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*allPoints.size() * 3, &allPoints[0], GL_STREAM_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*vertexNum, &indices[0], GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	cylinderVAO = VAO;
	cylinderVBO = VBO;
}
