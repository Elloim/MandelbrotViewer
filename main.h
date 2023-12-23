/*
 * name : main.h
 * auteur : PETIT Eloi
 * date : 2023 nov. 21
 */


#ifndef main_h
#define main_h
#define pi 3.14159

// Definition struct

// Definition fonctions

void error_callback(int error, const char* description);

void printMsPerFrame(double* LastTime, int* nbFrames);

void moveAround(GLFWwindow* window, float * data, long double* xmin, long double* xmax, long double* ymin, long double *ymax, long double xscale, long double yscale, double prevmouseX, double prevmouseY, double mouseX, double mouseY);

// Definition variables globales

int width;
int height;

/*int gradient_points[][3] = {
		{66,  30,    1},
		{25,  7,    26},
		{9,   120,  47},
		{4,   230,  73},
		{0,   100, 100},
		{12,  44,  138},
		{24,  82,  177},
		{57,  125, 209},
		{134, 181, 229},
		{211, 236, 248},
		{241, 233, 191},
		{248, 201,  95},
		{255, 20,    0},
		{204, 10,    0},
		{34,  7,   230},
		{50,  52,  120}
};*/


/*int gradient_points[][3] = {
		{241, 233, 191},
		{5,  221,    245},
		{209,   246,  26},
		{249,  129, 37},
		{207,   137, 242},
		{255,  77,  196},
		{101,  218,  12},	
		{213, 174, 143},
		
		{173, 103,  242},
		{201, 14,    14},
};*/

int * cells_to_update;
int nb_cells_to_update;

int global_count;
int cell_number;
int cell_number_row;
int cell_number_col;
int cell_pixel_width;
int cell_pixel_height;
pthread_mutex_t global_count_mutex;

#endif
