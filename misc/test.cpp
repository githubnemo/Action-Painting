#include <stdio.h>

#include <cv.h>
#include <highgui.h>

int main(void) {
	IplImage* img = cvLoadImage("background.jpg");
	int height, width, step, channels;
	uchar* data;

	height    = img->height;
	width     = img->width;
	step      = img->widthStep;
	channels  = img->nChannels;
	data      = (uchar *)img->imageData;
	printf("Processing a %dx%d image with %d channels, widthStep is %d\n",
			height,width,channels, step);


	for(int x=0; x < step; x++)
		data[100 * step + x] = 0;

	cv::Mat m = cv::Mat(img, true);
	IplImage img2 = m;
	img2.height = height;
	img2.width = width;
	img2.widthStep = step;

	for(int y=0; y < height; y++) {
		for(int x=0; x < width; x++) {
			int ownStep = (width * channels);
			ownStep += step - ownStep;
			img2.imageData[y * step + x] = data[y * ownStep + x];
		}
	}

	img2.imageData[0] = 0xFF;
	//img2.imageData[1] = 0xFF;
	//img2.imageData[2] = 0xFF;

	cvNamedWindow("image display", CV_WINDOW_AUTOSIZE);
	cvMoveWindow("image display", 100, 100);

	cvShowImage("image display", &img2);

	cvWaitKey(0);

	cvReleaseImage(&img);

	return 0;
}
