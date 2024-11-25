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

static void compute_image(imgRawImage *img, double xmin, double xmax, double ymin, double ymax, int max);
static int iteration_to_color(int i, int max);
static int iterations_at_point(double x, double y, int max);
static void show_help();

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

    // Parse command-line options
    while ((c = getopt(argc, argv, "x:y:s:X:Y:z:W:H:m:c:f:h")) != -1) {
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
	        compute_image(img,xcenter-xscale/2,xcenter+xscale/2,ycenter-yscale/2,ycenter+yscale/2,max);

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

void compute_image(imgRawImage* img, double xmin, double xmax, double ymin, double ymax, int max )
{
	int i,j;

	int width = img->width;
	int height = img->height;

	// For every pixel in the image...

	for(j=0;j<height;j++) {

		for(i=0;i<width;i++) {

			// Determine the point in x,y space for that pixel.
			double x = xmin + i*(xmax-xmin)/width;
			double y = ymin + j*(ymax-ymin)/height;

			// Compute the iterations at that point.
			int iters = iterations_at_point(x,y,max);

			// Set the pixel in the bitmap.
			setPixelCOLOR(img,i,j,iteration_to_color(iters,max));
		}
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
    printf("-c <num>     Number of child processes (default=5)\n");
    printf("-h           Show this help message\n");
}
