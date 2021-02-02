#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "shader.h"
#include "camera.h"
#include "model.h"
#include <iostream>
#include <vector>
#include <string>
#include <ctime>


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
void loadPBRtextures();
unsigned int loadTexture(const char *path);
void initCylinder();  //根据圆柱体信息初始化圆柱体,载入VAO,VBO和顶点数量
int FirstUnusedParticle();  //找到particles数组中第一个消亡的粒子下标
void initParticle(int index); //根据数组下标初始化粒子


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
const int angleStep = 2;			//切分角度增量
const double lengthStep = 0.001;	//切分长度增量
const double radiusStep = 0.001;	//半径增量
vector<int> radiusArray;			//半径数组,均为radiusStep的整数倍
vector<int> radiusMinArray;			//半径最小值数组,规定半径的最小值,用于贝塞尔曲线的限制
vector<glm::vec3> allPoints;		//所有点数组,包括顶点、法向量和纹理坐标
vector<unsigned int> indices;		//索引数据

unsigned int cylinderVAO;
unsigned int cylinderVBO;
unsigned int vertexNum; //顶点数量


//鼠标对应的裁剪坐标(标准化设备坐标,范围为-1~1),开始时刀尖在圆心处
const double clipX0 = 0.62, clipY0 = -0.2;  //刀尖初始位置
double clipX = clipX0, clipY = clipY0;  //刀尖新位置
bool isCut = false;  //是否被切削
int mode = 0;  //模式,0表示非切削模式,可以指定贝塞尔曲线,1表示切削模式


//粒子系统
struct Particle {
	glm::vec3 position, velocity;
	float life;

	Particle() :position(0.0f), velocity(0.0f), life(1.0f) {}
};
vector<Particle> particles;			//粒子数组
vector<glm::mat4> modelMatrices;	//模型矩阵数组
const int PARTICLE_NUM = 2000;		//粒子总数
const int NEW_PARTICLE_NUM = 8;		//新产生的粒子数
const double GRAVITY = 9.8 / 2.0;	//重力
const double xzVelorityMax = 0.6;	//x,z方向上的速度最大值
const double fadeMax = 0.01;		//生命周期每帧减缓的最大值
bool isLeft = false;				//粒子x方向的速度是否向左


//Bezier曲线
vector<glm::vec2> BezierPoints;     //约束点
vector<glm::vec2> BezierCurvePoints;//曲线点,固定为1001个

unsigned int bezierVAO, bezierVBO, bezierCurveVAO, bezierCurveVBO;

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
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// 配置openGL全局属性
	// -----------------------------
	srand(glfwGetTime());
	glEnable(GL_DEPTH_TEST);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glad_glPointSize(4);
	glad_glLineWidth(2);


	// 粒子系统初始化
	// ----------------
	for (int i = 0; i < PARTICLE_NUM; ++i) {
		particles.push_back(Particle());
		initParticle(i);
	}


	// 创建顶点数据,VAO,VBO
	// ----------------------------

	initCylinder();
	cout << "points:" << vertexNum << endl;

	float backgroundVertex[] = {
		-1.0f,-1.0f,  0.0f,0.0f,
		 1.0f,-1.0f,  1.0f,0.0f,
		 1.0f, 1.0f,  1.0f,1.0f,

		-1.0f,-1.0f,  0.0f,0.0f,
		 1.0f, 1.0f,  1.0f,1.0f,
		-1.0f, 1.0f,  0.0f,1.0f,
	};

	float particleVertex[] = {
		0.0f,0.0f,0.0f,  0.0f,0.0f,
		0.5f,0.0f,0.0f,  1.0f,0.0f,
		0.5f,0.5f,0.0f,  1.0f,1.0f,

		0.0f,0.0f,0.0f,  0.0f,0.0f,
		0.5f,0.5f,0.0f,  1.0f,1.0f,
		0.0f,0.5f,0.0f,  1.0f,0.0f,
	};

	for (int i = 0; i < PARTICLE_NUM; ++i) {
		modelMatrices.push_back(glm::mat4(1.0f));
	}

	vector<float> particleLifes;
	for (int i = 0; i < PARTICLE_NUM; ++i) {
		particleLifes.push_back(particles[i].life);
	}

	//粒子系统
	unsigned int particleVAO, particleVBO, modelMatrixVBO;
	glGenVertexArrays(1, &particleVAO);
	glGenBuffers(1, &particleVBO);
	glGenBuffers(1, &modelMatrixVBO);
	glBindVertexArray(particleVAO);
	glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(particleVertex), particleVertex, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, modelMatrixVBO);
	glBufferData(GL_ARRAY_BUFFER, PARTICLE_NUM * sizeof(glm::mat4), &modelMatrices[0], GL_STREAM_DRAW);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(0));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4)));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(2 * sizeof(glm::vec4)));
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(3 * sizeof(glm::vec4)));
	glEnableVertexAttribArray(5);
	//对每个实例使用模型矩阵,而不是对每个顶点
	glVertexAttribDivisor(2, 1);
	glVertexAttribDivisor(3, 1);
	glVertexAttribDivisor(4, 1);
	glVertexAttribDivisor(5, 1);
	glVertexAttribDivisor(6, 1);
	glBindVertexArray(0);

	//背景图片
	unsigned int bgVAO, bgVBO;
	glGenVertexArrays(1, &bgVAO);
	glGenBuffers(1, &bgVBO);
	glBindVertexArray(bgVAO);
	glBindBuffer(GL_ARRAY_BUFFER, bgVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(backgroundVertex), backgroundVertex, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);

	//Bezier曲线
	glGenVertexArrays(1, &bezierVAO);
	glGenVertexArrays(1, &bezierCurveVAO);
	glGenBuffers(1, &bezierVBO);
	glGenBuffers(1, &bezierCurveVBO);

	//约束点
	glBindVertexArray(bezierVAO);
	glBindBuffer(GL_ARRAY_BUFFER, bezierVBO);
	glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(glm::vec2), nullptr, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);
	//Bezier曲线点
	glBindVertexArray(bezierCurveVAO);
	glBindBuffer(GL_ARRAY_BUFFER, bezierCurveVBO);
	glBufferData(GL_ARRAY_BUFFER, 1001 * sizeof(glm::vec2), nullptr, GL_DYNAMIC_DRAW);   //bezier曲线点固定为1001个
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);



	// 加载纹理
	// -------------
	unsigned int bgTexture = loadTexture("resources/textures/background.bmp");



	//测试pbr加载运行时间
	clock_t startTime, endTime;
	startTime = clock();//计时开始

	loadPBRtextures();

	endTime = clock();//计时结束
	cout << "The run time is: " << (double)(endTime - startTime) / (time_t)1000 << "s" << endl;



	// 着色器配置
	// --------------------
	Shader cylinderShader_pbr("cylinder.vs", "cylinder_pbr.fs");
	Shader modelShader("model.vs", "model.fs");
	Shader particleShader("particle.vs", "particle.fs");
	Shader bgShader("background.vs", "background.fs");
	Shader bezierShader("bezier.vs", "bezier.fs");

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
		glm::vec3(0.0f, 0.0f, 4.0f)
	};
	glm::vec3 lightColors[] = {  //光源颜色:150.0f表示rgb中的1.0f
		glm::vec3(150.0f, 150.0f, 150.0f),
	};


	// 加载3d模型:车刀
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
		projection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, orthoSize, -0.0f);
		projection = glm::mat4(1.0f);  //投影矩阵设置为单位矩阵,即为默认的正射投影

		// 画圆柱体：pbr模型
		// ------------------------
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
		//设置pbr的点光源位置(可为多个点光源,最多4个)
		for (unsigned int i = 0; i < sizeof(lightPositions) / sizeof(lightPositions[0]); ++i) {
			glm::vec3 newPos = lightPositions[i] + glm::vec3(sin(glfwGetTime() * 5.0) * 5.0, 0.0, 0.0);
			newPos = lightPositions[i];
			cylinderShader_pbr.setVec3("lightPositions[" + std::to_string(i) + "]", newPos);
			cylinderShader_pbr.setVec3("lightColors[" + std::to_string(i) + "]", lightColors[i]);
		}


		// 画粒子系统
		// -------------
		if (isCut) {
			particleShader.use();
			particleShader.setMat4("view", view);
			particleShader.setMat4("projection", projection);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, albedo[PBR_type]);
			glBindVertexArray(particleVAO);
			glBindBuffer(GL_ARRAY_BUFFER, modelMatrixVBO);

			int UsedParticle = 0;
			modelMatrices.clear();
			for (int i = 0; i < PARTICLE_NUM; ++i) {
				Particle p = particles[i];
				if ((isLeft&&p.velocity.x > 0) || (!isLeft&&p.velocity.x < 0)) {
					p.velocity.x = -p.velocity.x;
					p.position.x = -p.position.x;
				}

				if (p.life > 0.0f) {
					model = glm::mat4(1.0f);
					model = glm::translate(model, glm::vec3(clipX - clipX0 + 0.5, clipY - clipY0, 0.0f)); //粒子系统随鼠标移动而移动
					model = glm::translate(model, p.position);
					modelMatrices.push_back(model);
					UsedParticle++;
				}
			}
			glBufferSubData(GL_ARRAY_BUFFER, 0, UsedParticle * sizeof(glm::mat4), &modelMatrices[0]);
			//只画life>0.0f的粒子,用实例化提升性能(虽然并没有提升多少,因为glBufferSubData又成为了性能瓶颈)
			glDrawArraysInstanced(GL_TRIANGLES, 0, 6, UsedParticle);
			glBindVertexArray(0);
		}


		// 画3d模型
		// -------------
		modelShader.use();
		model = glm::mat4(1.0f);
		//直接设置最终坐标
		if (mode == 1) {
			model = glm::translate(model, glm::vec3(clipX, clipY, 0.0f));
		}
		else {
			model = glm::translate(model, glm::vec3(clipX0, clipY0, 0.0f));
		}
		//旋转到正确方位
		model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::scale(model, glm::vec3(0.02f));
		modelShader.setMat4("model", model);
		myModel.Draw(modelShader);


		// 画背景图片
		// -------------
		glDepthFunc(GL_LEQUAL);   //设置背景在所有物体的最后面(最大深度)绘制
		bgShader.use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, bgTexture);
		glBindVertexArray(bgVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
		glDepthFunc(GL_LESS);


		// 画bezier曲线
		// -----------------
		bezierShader.use();

		//画bezier曲线的约束点
		glBindVertexArray(bezierVAO);
		glDrawArrays(GL_POINTS, 0, BezierPoints.size());
		glBindVertexArray(0);

		//画bezier曲线
		if (BezierPoints.size() == 4) {
			glBindVertexArray(bezierCurveVAO);
			glDrawArrays(GL_LINE_STRIP, 0, BezierCurvePoints.size());
			glBindVertexArray(0);
		}


		// 更新操作
		// --------------

		//更新旋转角
		angle += 0.8f;

		//更新粒子系统

		//生成新粒子
		for (int i = 0; i < NEW_PARTICLE_NUM; ++i) {
			int unusedParticle = FirstUnusedParticle();
			initParticle(unusedParticle);
		}


		//更新所有粒子
		for (int i = 0; i < PARTICLE_NUM; ++i) {
			Particle &p = particles[i];
			float dt = ((double)(rand() % 10) / 10.0)*fadeMax;
			p.life -= dt;
			if (p.life > 0.0f) {
				p.position += p.velocity * dt;
				p.velocity.y -= GRAVITY * dt;
			}
		}

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteVertexArrays(1, &cylinderVAO);
	glDeleteVertexArrays(1, &particleVAO);
	glDeleteVertexArrays(1, &bgVAO);
	glDeleteVertexArrays(1, &bezierVAO);
	glDeleteVertexArrays(1, &bezierCurveVAO);
	glDeleteBuffers(1, &cylinderVBO);
	glDeleteBuffers(1, &particleVBO);
	glDeleteBuffers(1, &modelMatrixVBO);
	glDeleteBuffers(1, &bgVBO);
	glDeleteBuffers(1, &bezierVAO);
	glDeleteBuffers(1, &bezierCurveVAO);

	glfwTerminate();
	return 0;
}


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

	if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
		mode = 1;  //转化为切削模式
	}
	if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) {
		mode = 0;  //转化为画贝塞尔曲线模式
	}
}


void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if (mode == 0) {
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
			if (clipX <= clipX0) {
				int size = BezierPoints.size();
				if (size == 0 || (size < 4 && BezierPoints[size - 1].x != clipX)) {  //不满4个约束点
					BezierPoints.push_back(glm::vec2(clipX, clipY));
					glBindBuffer(GL_ARRAY_BUFFER, bezierVBO);
					glBufferSubData(GL_ARRAY_BUFFER, 0, BezierPoints.size() * sizeof(glm::vec2), &BezierPoints[0]);
					glBindBuffer(GL_ARRAY_BUFFER, 0);

					if (BezierPoints.size() == 4) {  //已满4个约束点,添加曲线点
						float curveX, curveY;
						glm::vec2 p0 = BezierPoints[0], p1 = BezierPoints[1], p2 = BezierPoints[2], p3 = BezierPoints[3];
						for (float t = 0.0f; t < 1.0f; t += 0.001f) {
							curveX = p0.x * glm::pow((1 - t), 3) + 3 * p1.x * t * glm::pow((1 - t), 2) + 3 * p2.x * t * t * (1 - t) + p3.x * pow(t, 3);
							curveY = p0.y * glm::pow((1 - t), 3) + 3 * p1.y * t * glm::pow((1 - t), 2) + 3 * p2.y * t * t * (1 - t) + p3.y * pow(t, 3);
							BezierCurvePoints.push_back(glm::vec2(curveX, curveY));
						}
						glBindBuffer(GL_ARRAY_BUFFER, bezierCurveVBO);
						glBufferSubData(GL_ARRAY_BUFFER, 0, BezierCurvePoints.size() * sizeof(glm::vec2), &BezierCurvePoints[0]);
						glBindBuffer(GL_ARRAY_BUFFER, 0);


						//更新半径最小值数组
						int curveSize = BezierCurvePoints.size();
						int zIndex;  //沿圆柱体z轴方向上半径数组的下标
						int newR;

						for (int i = 0; i <= curveSize - 1; ++i) {  //由于曲线点之间的间隔小于radiusStep,故直接对每个曲线点更新半径最小值即可
							zIndex = (0.5f - BezierCurvePoints[i].x) / lengthStep;
							newR = (-BezierCurvePoints[i].y) / radiusStep;
							if (zIndex >= 0 && newR >= 0) {
								//更新radiusMinArray
								radiusMinArray[zIndex] = newR;
							}
						}
					}
				}
			}
		}

		if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {  //清空bezier约束点和曲线点
			BezierPoints.clear();
			BezierCurvePoints.clear();
		}
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

	//模拟切削
	if (mode == 1) {   //切削模式
		if (newClipY > clipY0) { //模型不得高于圆心位置
			newClipY = clipY0;
		}

		if (newClipX < clipX0 || clipX < clipX0) {
			int zStart = (clipX0 - (clipX > newClipX ? clipX : newClipX)) / lengthStep; //圆柱体z轴方向下标
			int zEnd = (clipX0 - (clipX > newClipX ? newClipX : clipX)) / lengthStep;
			int zGap = zEnd - zStart;
			//开始部分对应的半径是radiusStep的整数倍
			int R_start = (clipY0 - (clipX > newClipX ? clipY : newClipY)) / radiusStep;
			int R_end = (clipY0 - (clipX > newClipX ? newClipY : clipY)) / radiusStep;
			int R_gap = R_end - R_start;

			int stacks = cylinderLength / lengthStep;
			if (zStart >= 0 && zStart <= stacks && zEnd >= 0 && zEnd <= stacks && zGap >= 0 && R_start >= 0) {
				isCut = false;
				for (int i = zStart; i <= zEnd; ++i) {
					int newRadius;
					if (zGap == 0)
						newRadius = R_start;
					else
						newRadius = R_start + R_gap * (float)(i - zStart) / (float)zGap;

					if (newRadius < 0) newRadius = 0;
					//更新半径数组
					if (radiusArray[i] > newRadius&&radiusArray[i] > radiusMinArray[i]) {  //最小半径不能比现有半径小
						isCut = true;
						radiusArray[i] = newRadius < radiusMinArray[i] ? radiusMinArray[i] : newRadius; //新半径必须比现有半径小,且>=规定的最小半径
					}
				}

				//更新VBO缓冲区
				glBindBuffer(GL_ARRAY_BUFFER, cylinderVBO);
				if (isCut) {
					if (newClipX < clipX) { //车刀左移,粒子速度方向应向右
						isLeft = false;
					}
					else {
						isLeft = true;
					}

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


unsigned int loadTexture(char const * path)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	stbi_set_flip_vertically_on_load(true);
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
	stbi_set_flip_vertically_on_load(false);
	return textureID;
}



void initCylinder() {
	int slices = 360 / angleStep;  //围绕z轴的细分
	int stacks = cylinderLength / lengthStep;  //沿z轴的细分

	//初始化半径数组和半径限制数组
	unsigned int initRadius = cylinderRadius / radiusStep;
	for (int i = 0; i <= stacks; ++i) {
		radiusArray.push_back(initRadius);
	}

	for (int i = 0; i <= stacks; ++i) {
		radiusMinArray.push_back(0);  //开始时,半径最小均可以取0
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


int FirstUnusedParticle() {
	static int lastUsedParticle = 0;
	for (int i = lastUsedParticle; i < PARTICLE_NUM; ++i) {
		if (particles[i].life < 0.0f) {
			lastUsedParticle = i;
			return i;
		}
	}
	for (int i = 0; i < lastUsedParticle; ++i) {
		if (particles[i].life < 0.0f) {
			lastUsedParticle = i;
			return i;
		}
	}
	lastUsedParticle = 0;
	return 0;
}


void initParticle(int index) {
	Particle &p = particles[index];
	p.life = 1.0f;
	p.position = glm::vec3(0.0f, 0.0f, 0.0f);
	p.velocity.x = ((double)(rand() % 200 - 100) / 100.0)*xzVelorityMax;
	p.velocity.y = 0.0f;
	p.velocity.z = ((double)(rand() % 200 - 100) / 100.0)*xzVelorityMax;
}

