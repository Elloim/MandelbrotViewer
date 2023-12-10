/*
 * name : main.c
 * auteur : PETIT Eloi
 * date : 2023 déc. 10
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/glew.h>
#include <cglm/cglm.h>
#include <cglm/mat4.h>

#include "main.h"

float cubeVerts[] = {
	 -0.5, -0.5, -0.5, 0., 0., -1., 1.0, 0.0, 0.0, //0
	  0.5, -0.5, -0.5, 0., 0., -1., 1.0, 0.0, 0.0, //1
	  0.5,  0.5, -0.5, 0., 0., -1., 1.0, 0.0, 0.0, //2
	  0.5,  0.5, -0.5, 0., 0., -1., 1.0, 0.0, 0.0, //2
	 -0.5,  0.5, -0.5, 0., 0., -1., 1.0, 0.0, 0.0, //3
	 -0.5, -0.5, -0.5, 0., 0., -1., 1.0, 0.0, 0.0, //0
	 
	 -0.5f, -0.5f,  0.5f, 0., 0., 1., .0f, .0f, 1.0f, //4
     0.5f, -0.5f,  0.5f, 0., 0., 1., .0f, .0f, 1.0f, //5
     0.5f,  0.5f,  0.5f, 0., 0., 1., .0f, .0f, 1.0f, //6
     0.5f,  0.5f,  0.5f, 0., 0., 1., .0f, .0f, 1.0f, //6
    -0.5f,  0.5f,  0.5f, 0., 0., 1., .0f, .0f, 1.0f, //7
    -0.5f, -0.5f,  0.5f, 0., 0., 1., .0f, .0f, 1.0f, //4

    -0.5f,  0.5f,  0.5f, -1., 0., 0., .0f, 1.0f, .0f, //7
    -0.5f,  0.5f, -0.5f, -1., 0., 0., .0f, 1.0f, .0f, //3
    -0.5f, -0.5f, -0.5f, -1., 0., 0., .0f, 1.0f, .0f, //0
    -0.5f, -0.5f, -0.5f, -1., 0., 0., .0f, 1.0f, .0f, //0
    -0.5f, -0.5f,  0.5f, -1., 0., 0., .0f, 1.0f, .0f, //4
    -0.5f,  0.5f,  0.5f, -1., 0., 0., .0f, 1.0f, .0f, //7

     0.5f,  0.5f,  0.5f, 1., 0., 0., 1.0f, 1.0f, .0f, //6
     0.5f,  0.5f, -0.5f, 1., 0., 0., 1.0f, 1.0f, .0f, //2
     0.5f, -0.5f, -0.5f, 1., 0., 0., 1.0f, 1.0f, .0f, //1
     0.5f, -0.5f, -0.5f, 1., 0., 0., 1.0f, 1.0f, .0f, //1
     0.5f, -0.5f,  0.5f, 1., 0., 0., 1.0f, 1.0f, .0f, //5
     0.5f,  0.5f,  0.5f, 1., 0., 0., 1.0f, 1.0f, .0f, //6

    -0.5f, -0.5f, -0.5f, 0., -1., 0., 1.0f, 1.0f, 1.0f, //0
     0.5f, -0.5f, -0.5f, 0., -1., 0., 1.0f, 1.0f, 1.0f, //1
     0.5f, -0.5f,  0.5f, 0., -1., 0., 1.0f, 1.0f, 1.0f, //5
     0.5f, -0.5f,  0.5f, 0., -1., 0., 1.0f, 1.0f, 1.0f, //5
    -0.5f, -0.5f,  0.5f, 0., -1., 0., 1.0f, 1.0f, 1.0f, //4
    -0.5f, -0.5f, -0.5f, 0., -1., 0., 1.0f, 1.0f, 1.0f, //0

    -0.5f,  0.5f, -0.5f, 0., 1., 0., 1.0f, .0f, 1.0f, //3
     0.5f,  0.5f, -0.5f, 0., 1., 0., 1.0f, .0f, 1.0f, //2
     0.5f,  0.5f,  0.5f, 0., 1., 0., 1.0f, .0f, 1.0f, //6
     0.5f,  0.5f,  0.5f, 0., 1., 0., 1.0f, .0f, 1.0f, //6
    -0.5f,  0.5f,  0.5f, 0., 1., 0., 1.0f, .0f, 1.0f, //7
    -0.5f,  0.5f, -0.5f, 0., 1., 0., 1.0f, .0f, 1.0f //3
};

GLuint elements[] = {
	0, 1, 2, 2, 3, 0,
	4, 5, 6, 6, 7, 4,
	7, 3, 0, 0, 4, 7,
	6, 2, 1, 1, 5, 6,
	0, 1, 5, 5, 4, 0,
	3, 2, 6, 6, 7, 3
};


char* loadShaderContent(const char* filename) {
	FILE * file;
	long size;
	char* shaderContent;

	file = fopen(filename, "rb");
	if (file == NULL) {
		fprintf(stderr, "Error loading shader from file %s\n", filename);
		return "";
	}
	
	fseek(file, 0L, SEEK_END);
	size  = ftell(file) + 1;
	printf("Size : %lo\n", size);
	fclose(file);

	file = fopen(filename, "r");

	shaderContent = memset(malloc(size), '\0', size);
	if (!fread(shaderContent, 1, size-1, file)) {
		fprintf(stderr, "Error loading shader from file %s : fread failed\n", filename);
	}
	fclose(file);

	printf("%s\n", shaderContent);
	
	return shaderContent;
}

void error_callback(int error, const char* description) {
	fprintf(stderr, "Erreur num %d : %s\n", error, description);
}


void cameraTranslate(vec3 translation, float delta) {
	glm_vec3_scale(translation, delta, translation);
	glm_vec3_add(pos, translation, pos);
}

void mouse_callback(GLFWwindow* window, double nxpos, double nypos) {
	angles[0] += -(nxpos - xpos)/100.0;
	angles[1] += (nypos - ypos)/100.0;
	if (-pi/2 > angles[1]) {
		
		angles[1] = -pi/2;
	}
	if (angles[1] > pi/2) {
		angles[1] = pi/2;
	}
	cameraRotate(0.1);	
	printf("angle : %lf\n", angles[1]);
	xpos = nxpos;
	ypos = nypos;
	//glfwSetCursorPos(window, width/2, height/2);
}

void cameraRotate(float delta) {
	direction[0] = -cosf(angles[0]) * cosf(angles[1]);
	direction[1] = -sinf(angles[1]);
	direction[2] = sinf(angles[0]) * cosf(angles[1]);
	glm_vec3_normalize(direction);
	//glm_vec3_rotate(direction, (float)angles[0], cameraUp);
	//glm_vec3_rotate(direction, (float)angles[1], cameraRight);
	glm_vec3_cross(up, direction, cameraRight);
	glm_vec3_normalize(cameraRight);
	glm_vec3_scale(cameraRight, 1., cameraRight);
	glm_vec3_cross(direction, cameraRight, cameraUp);
	
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	vec3 translation = {0., 0., 0.};
	switch (key) {
		case 87:
			glm_vec3_copy(direction, translation);
			translation[1] = 0;
			break;
		case 65:
			glm_vec3_copy(cameraRight, translation);
			translation[1] = 0.;
			break;
		case 68:
			glm_vec3_copy(cameraRight, translation);
			translation[1] = 0.;	
			glm_vec3_scale(translation, -1., translation);
			break;
		case 83:
			glm_vec3_copy(direction, translation);
			translation[1] = 0.;
			glm_vec3_scale(translation, -1., translation);
			break;
		case GLFW_KEY_SPACE:
			translation[1] = 1.;
			break;
		case 81:
			translation[1] = -1.;
			break;
	}
	
	
	cameraTranslate(translation, 0.5f);
	printf("Key pressed : %d, action : %d\n", key, action);
}

void printMsPerFrame(double* LastTime, int* nbFrames) {
	double current = glfwGetTime();
	(*nbFrames)++;
	
	if (current - *LastTime >= 1.0) {
		printf("%f ms/frames\n", 1000.0/(double)(*nbFrames));
		*nbFrames = 0;
		*LastTime = glfwGetTime();
	}
}



int main() {

	mat4 proj, view, model, lightModel, normalModel, scaleModel;

	mat4 models[300*300];

	int indice = 0;
	for (int i = 0; i < 300; i++) {
		for (int j = 0; j < 300; j++) {
			vec3 offset = {(float)i - 100, 0., (float)j - 100};
			glm_translate_make(models[indice], offset);
			indice++;
		}
	}
	
	GLFWwindow* window;
	
	if (!glfwInit()) {
		printf("Erreur initialisation\n");
		return -1;
	}
	
	glfwWindowHint(GLFW_SAMPLES, 4);
	

	window = glfwCreateWindow(width, height, "Tankistan", NULL, NULL);
	;
	if (!window) {
		printf("Erreur creation contexte\n");
		return -1;
	}

	glfwMakeContextCurrent(window);
	
	GLenum err = glewInit();
	if (GLEW_OK != err) { 
		fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}
	
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_FRAMEBUFFER_SRGB);
	glEnable(GL_MULTISAMPLE);;

	
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPos(window, width/2, height/2);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetKeyCallback(window, key_callback);
	
	glfwSwapInterval(0);
	
	// OpenGL	
	GLuint vbo, vao, ebo, lvbo, lvao, cubeVertexShader, cubeFragmentShader, lightVertexShader, lightFragmentShader, shaderProgram, lightShaderProgram, uniLightColor, uniLightPos, uniAmbiant, uniCameraPos, posAttrib, colorAttrib, normalAttrib, uniModel, uniView, uniProj;
	GLint shader_status, uniColor;
	
	// Lightsource
	glGenVertexArrays(1, &lvao);
	glBindVertexArray(lvao);
	
	
	glGenBuffers(1, &lvbo);
	glBindBuffer(GL_ARRAY_BUFFER, lvbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);
	
	// Cubes
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);

	GLuint modelsBuffer;
	glGenBuffers(1, &modelsBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, modelsBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(models), models, GL_STATIC_DRAW);	

	// EBO
	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);

	// Shaders	
	const char * lightcubeVertex_shader_text = (const char *) loadShaderContent(lightCubeVertexFilename);
	lightVertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(lightVertexShader, 1, &lightcubeVertex_shader_text, NULL);
	glCompileShader(lightVertexShader);
	free((void*)lightcubeVertex_shader_text);
	
	glGetShaderiv(lightVertexShader, GL_COMPILE_STATUS, &shader_status);
	if (shader_status == GL_FALSE) {
		fprintf(stderr, "Erreur compilation light shader vertex\n");
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}
	
	const char * lightFragment_shader_text = (const char *) loadShaderContent(lightFragmentFilename);
	lightFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(lightFragmentShader, 1, &lightFragment_shader_text, NULL);
	glCompileShader(lightFragmentShader);
	free((void*)lightFragment_shader_text);
	
	glGetShaderiv(lightFragmentShader, GL_COMPILE_STATUS, &shader_status);
	if (shader_status == GL_FALSE) {
		fprintf(stderr, "Erreur compilation light shader fragment\n");
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}
		
	// Cube	
	const char * cubeVertex_shader_text = (const char *) loadShaderContent(cubeVertexFilename);
	cubeVertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(cubeVertexShader, 1, &cubeVertex_shader_text, NULL);
	glCompileShader(cubeVertexShader);
	free((void *)cubeVertex_shader_text);
	
	glGetShaderiv(cubeVertexShader, GL_COMPILE_STATUS, &shader_status);
	if (shader_status == GL_FALSE) {
		fprintf(stderr, "Erreur compilation shader vertex\n");
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}

	const char * fragment_shader_text = loadShaderContent(fragmentFilename);
	cubeFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(cubeFragmentShader, 1, &fragment_shader_text, NULL);
	glCompileShader(cubeFragmentShader);
	free((void *)fragment_shader_text);
	
	glGetShaderiv(cubeFragmentShader, GL_COMPILE_STATUS, &shader_status);
	if (shader_status == GL_FALSE) {
		fprintf(stderr, "Erreur compilation shader fragment\n");
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}
	
	glBindVertexArray(lvao);
	lightShaderProgram = glCreateProgram();
	glAttachShader(lightShaderProgram, lightVertexShader);
	glAttachShader(lightShaderProgram, lightFragmentShader);
	glLinkProgram(lightShaderProgram);
	glUseProgram(lightShaderProgram);
	glBindBuffer(GL_ARRAY_BUFFER, lvbo);

	GLuint unilightModel = glGetUniformLocation(lightShaderProgram, "model");
	GLuint unilightView = glGetUniformLocation(lightShaderProgram, "view");
	GLuint unilightProj = glGetUniformLocation(lightShaderProgram, "proj");	
	
	posAttrib = glGetAttribLocation(lightShaderProgram, "position");
	glEnableVertexAttribArray(posAttrib);
	glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 9*sizeof(float), 0);
	
	glBindVertexArray(vao);
	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, cubeVertexShader);
	glAttachShader(shaderProgram, cubeFragmentShader);
	glLinkProgram(shaderProgram);
	glUseProgram(shaderProgram);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	
	posAttrib = glGetAttribLocation(shaderProgram, "position");
	glEnableVertexAttribArray(posAttrib);
	glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 9*sizeof(float), 0);
	
	normalAttrib = glGetAttribLocation(shaderProgram, "normal");
	glEnableVertexAttribArray(normalAttrib);
	glVertexAttribPointer(normalAttrib, 3, GL_FLOAT, GL_FALSE, 9*sizeof(float), (void*)(3*sizeof(float)));
	
	uniView = glGetUniformLocation(shaderProgram, "view");
	uniProj = glGetUniformLocation(shaderProgram, "proj");
	uniLightColor = glGetUniformLocation(shaderProgram, "lightColor");
	uniLightPos = glGetUniformLocation(shaderProgram, "lightPos");
	uniAmbiant = glGetUniformLocation(shaderProgram, "ambiant");
	uniCameraPos = glGetUniformLocation(shaderProgram, "cameraPos");
	
	colorAttrib = glGetAttribLocation(shaderProgram, "color");
	glEnableVertexAttribArray(colorAttrib);
	glVertexAttribPointer(colorAttrib, 3, GL_FLOAT, GL_FALSE, 9*sizeof(float), (void*)(6*sizeof(float)));

	GLuint modelAttrib = glGetAttribLocation(shaderProgram, "model");
	glBindBuffer(GL_ARRAY_BUFFER, modelsBuffer);
	glEnableVertexAttribArray(modelAttrib);
	glVertexAttribPointer(modelAttrib, 4, GL_FLOAT, GL_FALSE, 4*sizeof(vec4), (void*)0);
	glEnableVertexAttribArray(modelAttrib+1);
	glVertexAttribPointer(modelAttrib+1, 4, GL_FLOAT, GL_FALSE, 4*sizeof(vec4), (void*)(1*sizeof(vec4)));
	glEnableVertexAttribArray(modelAttrib+2);
	glVertexAttribPointer(modelAttrib+2, 4, GL_FLOAT, GL_FALSE, 4*sizeof(vec4), (void*)(2*sizeof(vec4)));
	glEnableVertexAttribArray(modelAttrib+3);
	glVertexAttribPointer(modelAttrib+3, 4, GL_FLOAT, GL_FALSE, 4*sizeof(vec4), (void*)(3*sizeof(vec4)));

	glVertexAttribDivisor(modelAttrib, 1);
	glVertexAttribDivisor(modelAttrib+1, 1);
	glVertexAttribDivisor(modelAttrib+2, 1);
	glVertexAttribDivisor(modelAttrib+3, 1);

	//glBindVertexArray(0);

	
	double LastTime = glfwGetTime();
	int nbFrames = 0;
	int count = 0;
	vec3 scale = {2.0, 2.0, 2.0};
	//glm_rotate_atm(model, pos, 0.0, up);
	glm_scale_make(scaleModel, scale);
	//scaling fucks. up the light
	//glm_mat4_mul(model, scaleModel, model);
	glm_translate_make(lightModel, lightPos);
	
	while (!glfwWindowShouldClose(window)) {
		float ratio;
		count++;
		//count = 0;
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		ratio = width / (float) height;
		
		glClearColor(0.0, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		glm_vec3_add(pos, direction, cameraDirection);
		glm_lookat(pos, cameraDirection, cameraUp, view);
		glm_perspective(3.14/2, ratio, 0.1, 1000.0, proj);
		/*glm_rotate_at(model, eye, 0.01, up);
		glm_rotate_at(model, eye, 0.002, side);
		glm_rotate_at(model, eye, 0.005, forward);
		glm_mat4_transpose_to(model, normalModel);
		glm_mat4_inv(normalModel, normalModel);
		*/
		glViewport(0, 0, width, height);
		glUseProgram(lightShaderProgram);
		glBindVertexArray(lvao);
		//glBindBuffer(GL_ARRAY_BUFFER, lvbo);
		glUniformMatrix4fv(unilightView, 1, GL_FALSE, view[0]);
		glUniformMatrix4fv(unilightProj, 1, GL_FALSE, proj[0]);	
		glUniformMatrix4fv(unilightModel, 1, GL_FALSE, lightModel[0]);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		
		glUseProgram(shaderProgram);
		glBindVertexArray(vao);
		//glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glUniformMatrix4fv(uniView, 1, GL_FALSE, view[0]);
		glUniformMatrix4fv(uniProj, 1, GL_FALSE, proj[0]);
		glUniform3fv(uniLightColor, 1, lightColor);
		glUniform3fv(uniLightPos, 1, lightPos);
		glUniform3fv(uniAmbiant, 1, ambiant);
		glUniform3fv(uniCameraPos, 1, pos);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 36, 200*200);
				
		glfwSwapBuffers(window);
		glfwPollEvents();
		printMsPerFrame(&LastTime, &nbFrames);

	}
	
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
};

