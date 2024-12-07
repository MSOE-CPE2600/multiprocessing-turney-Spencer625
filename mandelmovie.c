/**
 **********************************************************
 * @file    mandelmovie.c
 * @author  Spencer Thacker <thackers@msoe.edu>
 * @date    11/22/2024
 * @brief   Creates a movie om mandel images
 * @note    gcc mandelmovie.c jpegrw.c -o mandelmovie -ljpeg -lpthread -lm
 * @note    ffmpeg -i frame_%d.jpg mandel.mpg
 **********************************************************
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include "jpegrw.h"
#include <pthread.h>


static void compute_image(imgRawImage *img, double xmin, double xmax, double ymin, double ymax, int max, int num_threads);
static int iteration_to_color(int i, int max);
static int iterations_at_point(double x, double y, int max);
static void show_help();

typedef struct {
    imgRawImage *img;
    int start_row;
    int end_row;
    double xmin;
    double xmax;
    double ymin;
    double ymax;
    int max;
} ThreadData;

int main(int argc, char *argv[]) {
    char c;

    // Default configuration values, an "infinite" loop in seahorse valley
    int image_width = 1920; // Default width of the image
    int image_height = 1080; // Default hight of the image
    double xcenter = -0.743291891; // Default X coordinate of image center 
    double ycenter = -0.131240553; // Default Y coordinate of image center
    double xscale = 0.005; // Default initial scale of the image
    double yscale = 0; // calc later
    double zoom_factor = 9.5; // Default zoom coefficient
    double x_pan_factor = 0; // Default pan in the X diriction coefficnet
    double y_pan_factor = 0; // Default pan in the Y diriction coefficnet
    int total_frames = 50; // Default number of frames
    int max = 1000; // Default number of iterations per point
    int num_children = 4;  // Default number of child processes
    int num_threads = 4; // Default number of threads 

    // Parse command-line options
    while ((c = getopt(argc, argv, "x:y:s:X:Y:z:W:H:m:c:t:f:h")) != -1) {
        switch (c) {
            case 'x': // X coordinate of image center
                xcenter = atof(optarg);
                break;
            case 'y': // Y coordinate of image center
                ycenter = atof(optarg);
                break;
            case 's': // Initial scale of the image
                xscale = atof(optarg);
                break;
            case 'X': // Pan in the X diriction coefficnet
                x_pan_factor = atof(optarg);
                break;
            case 'Y': // Pan in the Y diriction coefficnet
                y_pan_factor = atof(optarg);
                break;
            case 'z': // Zoom coefficient
                zoom_factor = atof(optarg);
                break;
            case 'W': // Width of the image in pixels
                image_width = atoi(optarg);
                break;
            case 'H': // Height of the image in pixels
                image_height = atoi(optarg);
                break;
            case 'm': // Max iterations per point
                max = atoi(optarg);
                break;
            case 'c': // Number of child processes
                num_children = atoi(optarg);
                break;
            case 't': // Number of threads
                num_threads = atoi(optarg);
                if (num_threads < 1 || num_threads > 20) {
                    printf("Number of threads must be between 1 and 20.\n");
                    exit(1);
                }
                break;
            case 'f': // Number of frames
                total_frames = atoi(optarg);
                break;
            case 'h': // Shows the help message
                show_help();
                exit(0);
                break;
            default: // Unrecognized command
                printf("Unknown option: -%c\n", c);
                show_help();
                exit(1);
        }
    }


    // Semaphore to limit the number of concurrent processes
    sem_t *sem = sem_open("/mandel_sem", O_CREAT, 0644, num_children);
    if (sem == SEM_FAILED) {
        printf("An error has occurred when using Semaphore!\n");
        exit(1);
    }

    // Start of image generation
    for (int frame = 0; frame < total_frames; ++frame) {
        sem_wait(sem); // Wait for a semaphore slot

        if (fork() == 0) {
            // Child process

            // Compute the scale for the current frame
            double scale_ratio = pow(zoom_factor, (double)frame / (total_frames - 1));
            xscale = xscale / scale_ratio;

            // Calculate y scale based on x scale (settable) and image sizes in X and Y (settable)
            yscale = xscale * image_height / image_width;

            // Pan using given pan factors
            xcenter += x_pan_factor * (double)frame;
            ycenter += y_pan_factor * (double)frame;

            // Generate the filename
            char outfile[256];
            snprintf(outfile, sizeof(outfile), "frame_%d.jpg", frame);

            // Display Current configuration of the frame
            printf("Generating frame %d: xcenter=%lf ycenter=%lf xscale=%lf\n", frame, xcenter, ycenter, xscale);

            // Create a raw image of the appropriate size.
	        imgRawImage* img = initRawImage(image_width,image_height);

	        // Fill it with a black
	        setImageCOLOR(img,0);

	        // Compute the Mandelbrot image
	        compute_image(img, xcenter - xscale / 2, xcenter + xscale / 2, ycenter - yscale / 2, ycenter + yscale / 2, max, num_threads);
	        // Save the image in the stated file.
	        storeJpegImageFile(img,outfile);

	        // free the mallocs
	        freeRawImage(img);

            sem_post(sem); // Release semaphore
            exit(0);       // Exit child process
        }
    }

    // Wait for all child processes
    while (wait(NULL) > 0);

    // Clean up semaphore
    sem_close(sem);
    sem_unlink("/mandel_sem");

    return 0;
}

/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/

int iterations_at_point( double x, double y, int max )
{
	double x0 = x;
	double y0 = y;

	int iter = 0;

	while( (x*x + y*y <= 4) && iter < max ) {

		double xt = x*x - y*y + x0;
		double yt = 2*x*y + y0;

		x = xt;
		y = yt;

		iter++;
	}

	return iter;
}

/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/

void *compute_region(void *arg) {
    // Cast the argument to a ThreadData pointer to access thread-specific data
    ThreadData *data = (ThreadData *)arg;

    // Extract the imgRawImage pointer from the ThreadData structure
    imgRawImage *img = (*data).img;

    // Iterate over the range of rows assigned to this thread
    for (int j = (*data).start_row; j < (*data).end_row; j++) {
        // Iterate over all columns in the image
        for (int i = 0; i < (*img).width; i++) {
            // Calculate the x-coordinate in the fractal's space corresponding to the current column
            double x = (*data).xmin + i * ((*data).xmax - (*data).xmin) / (*img).width; 
            
            // Calculate the y-coordinate in the fractal's space corresponding to the current row
            double y = (*data).ymin + j * ((*data).ymax - (*data).ymin) / (*img).height; 
            
            // Determine the number of iterations for the point (x, y) in the fractal computation
            int iters = iterations_at_point(x, y, (*data).max); 
            
            // Map the iteration count to a color and set the pixel color in the image
            setPixelCOLOR(img, i, j, iteration_to_color(iters, (*data).max));
        }
    }
    return NULL;
}

void compute_image(imgRawImage *img, double xmin, double xmax, double ymin, double ymax, int max, int num_threads) {
    // Array to hold thread identifiers
    pthread_t threads[num_threads];
    
    // Array to hold thread-specific data for each thread
    ThreadData thread_data[num_threads];

    // Divide the image height into base rows per thread
    int rows_per_thread = (*img).height / num_threads;

    // Calculate the number of leftover rows that need to be handled by the first threads
    int leftover_rows = (*img).height % num_threads;

    // Create threads and assign each a portion of the computation
    int current_start_row = 0;  // Keep track of the current starting row

    for (int t = 0; t < num_threads; t++) {
        // Calculate the number of rows for this thread
        int rows_for_thread = rows_per_thread;

        // If there are leftover rows, give the first 'leftover_rows' threads one extra row
        if (t < leftover_rows) {
            rows_for_thread++;  // Give one extra row to this thread
        }

        // Calculate the start and end rows for this thread
        thread_data[t] = (ThreadData){
            .img = img, // Pointer to the shared image
            .start_row = current_start_row, // Starting row for this thread
            .end_row = current_start_row + rows_for_thread, // Ending row for this thread
            .xmin = xmin, // Minimum x-coordinate of the fractal region
            .xmax = xmax, // Maximum x-coordinate of the fractal region
            .ymin = ymin, // Minimum y-coordinate of the fractal region
            .ymax = ymax, // Maximum y-coordinate of the fractal region
            .max = max // Maximum number of iterations for the fractal
        };

        // Update the starting row for the next thread
        current_start_row += rows_for_thread;

        // Create a thread and pass it the compute_region function and its data
        pthread_create(&threads[t], NULL, compute_region, &thread_data[t]);
    }

    // Wait for all threads to complete before proceeding
    for (int t = 0; t < num_threads; t++) {
        pthread_join(threads[t], NULL);
    }
}




/*
Convert a iteration number to a color.
Here, we just scale to gray with a maximum of imax.
Modify this function to make more interesting colors.
*/
int iteration_to_color( int iters, int max )
{
	int color = 0xFFFFFF*iters/(double)max;
	return color;
}

// Show help message
void show_help() {
    printf("Options:\n");
    printf("-x <coord>   X coordinate of image center (default=-0.743291891;)\n");
    printf("-y <coord>   Y coordinate of image center (default=-0.131240553)\n");
    printf("-s <scale>   Initial scale of the image (default=0.005)\n");
    printf("-z <zoom>    Zoom coefficient (default= 9.5)\n");
    printf("-X <num>     Pan in the X diriction coefficnet (default= 0.001)\n");
    printf("-Y <num>     Pan in the Y diriction coefficnet (default= 0.001)\n");
    printf("-W <pixels>  Width of the image in pixels (default=1920)\n");
    printf("-H <pixels>  Height of the image in pixels (default=1080)\n");
    printf("-m <max>     Max iterations per point (default=1000)\n");
    printf("-c <num>     Number of child processes (default=4)\n");
    printf("-t <num>     Number of threads (default=5)\n");
    printf("-h           Show this help message\n");
}
